#include "monitor.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sched.h>
#include <termios.h>
#include <unistd.h>

#include "anomaly_detector.h"
#include "feature_window.h"
#include "serial_reader.h"

namespace {
constexpr int kIngestionPriority = 30;
constexpr int kProcessingPriority = 20;
constexpr int kDashboardPriority = 10;

std::uint64_t nowNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

void requestRealtime(const char* role, int priority) {
    sched_param parameters{};
    parameters.sched_priority = priority;
    const int result = pthread_setschedparam(pthread_self(), SCHED_FIFO,
                                              &parameters);
    if (result != 0) {
        std::cerr << "WARNING: " << role << " could not use SCHED_FIFO/"
                  << priority << ": " << std::strerror(result)
                  << "; continuing with normal scheduling\n";
    }
}

bool configureSerial(int fd, std::string& error) {
    termios settings{};
    if (tcgetattr(fd, &settings) != 0) {
        error = std::strerror(errno);
        return false;
    }
    settings.c_iflag &=
        ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    settings.c_oflag &= ~OPOST;
    settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    settings.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    settings.c_cflag |= CS8 | CLOCAL | CREAD;
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;
    if (cfsetispeed(&settings, B115200) != 0 ||
        cfsetospeed(&settings, B115200) != 0 ||
        tcsetattr(fd, TCSANOW, &settings) != 0) {
        error = std::strerror(errno);
        return false;
    }
    tcflush(fd, TCIFLUSH);
    return true;
}

struct HandState {
    explicit HandState(const HandConfig& value) : config(value), queue(256) {}
    HandConfig config;
    FrameQueue queue;
    std::atomic<std::uint64_t> frames{0};
    std::atomic<std::uint64_t> parse_errors{0};
    std::atomic<std::uint64_t> read_errors{0};
    std::atomic<std::uint64_t> last_frame_ns{0};
    std::atomic<bool> done{false};
    std::atomic<bool> failed{false};
    FeatureWindow window;
    std::unique_ptr<AnomalyDetector> detector;
    std::ofstream recording;
    std::size_t recorded = 0;
    AnomalyResult result;
    bool has_score = false;
    FeatureVector latest_features{};
    bool has_features = false;
    pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
};

struct Runtime {
    Runtime(const MonitorOptions& value)
        : options(value), left(value.left), right(value.right) {}
    MonitorOptions options;
    HandState left;
    HandState right;
    std::atomic<bool> processing_done{false};
};

bool prepareHand(HandState& hand, std::size_t k, double threshold) {
    if (!hand.config.baseline_path.empty()) {
        hand.detector.reset(new AnomalyDetector(k, threshold));
        BaselineLoadStats stats;
        std::string error;
        if (!hand.detector->loadBaseline(hand.config.baseline_path, stats,
                                         error)) {
            std::cerr << error << '\n';
            return false;
        }
    }
    if (!hand.config.record_path.empty()) {
        hand.recording.open(hand.config.record_path,
                            std::ios::out | std::ios::trunc);
        if (!hand.recording) {
            std::cerr << "cannot open baseline output: "
                      << hand.config.record_path << '\n';
            return false;
        }
    }
    return true;
}

void* ingest(void* argument) {
    HandState& hand = *static_cast<HandState*>(argument);
    requestRealtime(hand.config.name, kIngestionPriority);
    const int fd = open(hand.config.input.path.c_str(),
                        O_RDONLY | (hand.config.input.serial ? O_NOCTTY : 0));
    if (fd < 0) {
        std::cerr << hand.config.name << " open failed: "
                  << hand.config.input.path << ": " << std::strerror(errno)
                  << '\n';
        hand.failed = true;
        hand.done = true;
        return nullptr;
    }
    if (hand.config.input.serial) {
        std::string error;
        if (!configureSerial(fd, error)) {
            std::cerr << hand.config.name << " serial setup failed: " << error
                      << '\n';
            close(fd);
            hand.failed = true;
            hand.done = true;
            return nullptr;
        }
    }

    FrameStreamReader reader([&hand](const GloveFrame& frame) {
        hand.queue.push(frame);
        hand.last_frame_ns = frame.timestamp_ns;
    });
    char buffer[256];
    for (;;) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count > 0) {
            reader.consume(buffer, static_cast<std::size_t>(count), nowNs());
            hand.frames = reader.counters().frames;
            hand.parse_errors = reader.counters().parse_errors;
        } else if (count == 0) {
            reader.finish(nowNs());
            hand.parse_errors = reader.counters().parse_errors;
            if (hand.config.input.serial) hand.failed = true;
            break;
        } else if (errno != EINTR) {
            ++hand.read_errors;
            hand.failed = true;
            std::cerr << hand.config.name << " read failed: "
                      << std::strerror(errno) << '\n';
            break;
        }
    }
    close(fd);
    hand.done = true;
    return nullptr;
}

void processFrame(HandState& hand, const GloveFrame& frame) {
    hand.window.add(frame);
    if (!hand.window.ready()) return;
    const FeatureVector features = hand.window.features();
    if (hand.recording.is_open() && appendFeatureCsv(hand.recording, features)) {
        ++hand.recorded;
    }
    pthread_mutex_lock(&hand.result_mutex);
    hand.latest_features = features;
    hand.has_features = true;
    if (hand.detector) {
        hand.result = hand.detector->score(features);
        hand.has_score = true;
    }
    pthread_mutex_unlock(&hand.result_mutex);
}

void* process(void* argument) {
    Runtime& runtime = *static_cast<Runtime*>(argument);
    requestRealtime("processing", kProcessingPriority);
    for (;;) {
        bool worked = false;
        GloveFrame frame;
        while (runtime.left.queue.tryPop(frame)) {
            processFrame(runtime.left, frame);
            worked = true;
        }
        while (runtime.right.queue.tryPop(frame)) {
            processFrame(runtime.right, frame);
            worked = true;
        }
        if (runtime.left.done && runtime.right.done &&
            runtime.left.queue.empty() && runtime.right.queue.empty()) break;
        if (!worked) usleep(1000);
    }
    runtime.processing_done = true;
    return nullptr;
}

void printHand(const HandState& hand, std::uint64_t now,
               std::uint64_t timeout) {
    const bool online = gloveOnline(now, hand.last_frame_ns, timeout);
    std::cout << hand.config.name << " " << (online ? "ONLINE" : "OFFLINE")
              << " frames=" << hand.frames.load()
              << " parse_errors=" << hand.parse_errors.load()
              << " read_errors=" << hand.read_errors.load()
              << " dropped=" << hand.queue.dropped();
    pthread_mutex_lock(const_cast<pthread_mutex_t*>(&hand.result_mutex));
    if (hand.has_features) {
        std::cout << std::fixed << std::setprecision(3)
                  << " mean_thumb=" << hand.latest_features[0]
                  << " movement_thumb=" << hand.latest_features[5]
                  << " variability_thumb=" << hand.latest_features[10];
    }
    if (hand.has_score) {
        std::cout << std::fixed << std::setprecision(3)
                  << " score=" << hand.result.score << ' '
                  << (hand.result.anomaly ? "ANOMALY" : "NORMAL");
    }
    pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&hand.result_mutex));
    std::cout << '\n';
}

void* dashboard(void* argument) {
    Runtime& runtime = *static_cast<Runtime*>(argument);
    requestRealtime("dashboard", kDashboardPriority);
    while (!runtime.processing_done) {
        usleep(500000);
        if (!runtime.processing_done) {
            const std::uint64_t now = nowNs();
            printHand(runtime.left, now, runtime.options.offline_timeout_ns);
            printHand(runtime.right, now, runtime.options.offline_timeout_ns);
        }
    }
    return nullptr;
}
}  // namespace

int runMonitor(const MonitorOptions& options, MonitorRunSummary* summary) {
    Runtime runtime(options);
    if (!prepareHand(runtime.left, options.k, options.threshold) ||
        !prepareHand(runtime.right, options.k, options.threshold)) return 1;
    pthread_t leftThread{}, rightThread{}, processingThread{}, dashboardThread{};
    const int processingCreated =
        pthread_create(&processingThread, nullptr, process, &runtime);
    if (processingCreated != 0) {
        std::cerr << "processing pthread creation failed: "
                  << std::strerror(processingCreated) << '\n';
        return 1;
    }
    const int dashboardCreated =
        pthread_create(&dashboardThread, nullptr, dashboard, &runtime);
    const int leftCreated =
        pthread_create(&leftThread, nullptr, ingest, &runtime.left);
    if (leftCreated != 0) {
        std::cerr << "LEFT pthread creation failed: "
                  << std::strerror(leftCreated) << '\n';
        runtime.left.failed = true;
        runtime.left.done = true;
    }
    const int rightCreated =
        pthread_create(&rightThread, nullptr, ingest, &runtime.right);
    if (rightCreated != 0) {
        std::cerr << "RIGHT pthread creation failed: "
                  << std::strerror(rightCreated) << '\n';
        runtime.right.failed = true;
        runtime.right.done = true;
    }
    if (leftCreated == 0) pthread_join(leftThread, nullptr);
    if (rightCreated == 0) pthread_join(rightThread, nullptr);
    pthread_join(processingThread, nullptr);
    if (dashboardCreated == 0) {
        pthread_join(dashboardThread, nullptr);
    } else {
        std::cerr << "dashboard pthread creation failed: "
                  << std::strerror(dashboardCreated) << '\n';
    }

    // Finite sources reach this point only after both sources completed and the
    // processing thread observed both queues empty. Print one authoritative
    // final snapshot instead of an initial zero-frame dashboard race.
    const std::uint64_t now = nowNs();
    printHand(runtime.left, now, options.offline_timeout_ns);
    printHand(runtime.right, now, options.offline_timeout_ns);
    if (summary != nullptr) {
        summary->left = {runtime.left.frames, runtime.left.parse_errors,
                         runtime.left.read_errors, runtime.left.queue.dropped(),
                         runtime.left.queue.empty(), runtime.left.window.ready()};
        summary->right = {runtime.right.frames, runtime.right.parse_errors,
                          runtime.right.read_errors,
                          runtime.right.queue.dropped(),
                          runtime.right.queue.empty(),
                          runtime.right.window.ready()};
    }
    // A failed hand never stops the other thread, but the final exit status still
    // reports that the run was degraded once all active inputs have finished.
    return (runtime.left.failed || runtime.right.failed) ? 1 : 0;
}
