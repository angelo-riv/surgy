#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <pthread.h>
#include <string>

#include "glove_frame.h"

class FrameQueue {
public:
    explicit FrameQueue(std::size_t capacity = 256);
    ~FrameQueue();
    void push(const GloveFrame& frame);
    bool tryPop(GloveFrame& frame);
    bool empty() const;
    std::uint64_t dropped() const;

private:
    std::size_t capacity_;
    mutable pthread_mutex_t mutex_;
    std::deque<GloveFrame> frames_;
    std::uint64_t dropped_ = 0;
};

bool gloveOnline(std::uint64_t now_ns, std::uint64_t last_frame_ns,
                 std::uint64_t timeout_ns);

struct InputConfig {
    std::string path;
    bool serial = false;
};

struct HandConfig {
    const char* name = nullptr;
    InputConfig input;
    std::string baseline_path;
    std::string record_path;
};

struct MonitorOptions {
    HandConfig left;
    HandConfig right;
    std::size_t k = 5;
    double threshold = 0.5;
    std::uint64_t offline_timeout_ns = 1000000000ULL;
};

struct HandRunSummary {
    std::uint64_t frames = 0;
    std::uint64_t parse_errors = 0;
    std::uint64_t read_errors = 0;
    std::uint64_t dropped = 0;
    bool queue_drained = false;
    bool feature_window_ready = false;
};

struct MonitorRunSummary {
    HandRunSummary left;
    HandRunSummary right;
};

int runMonitor(const MonitorOptions& options,
               MonitorRunSummary* summary = nullptr);
