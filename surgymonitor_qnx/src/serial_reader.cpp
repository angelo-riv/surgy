#include "serial_reader.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace {
std::uint64_t monotonicNanoseconds() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
}  // namespace

FrameStreamReader::FrameStreamReader(FrameHandler handler)
    : handler_(handler) {}

void FrameStreamReader::consume(const char* data, std::size_t size,
                                std::uint64_t timestamp_ns) {
    accumulator_.append(data, size);
    std::size_t newline = 0;
    while ((newline = accumulator_.find('\n')) != std::string::npos) {
        const std::string line = accumulator_.substr(0, newline);
        accumulator_.erase(0, newline + 1);
        processLine(line, timestamp_ns);
    }
}

void FrameStreamReader::finish(std::uint64_t timestamp_ns) {
    (void)timestamp_ns;
    if (!accumulator_.empty()) {
        // EOF does not make a partial serial frame complete; only '\n' does.
        ++counters_.parse_errors;
        accumulator_.clear();
    }
}

const ReaderCounters& FrameStreamReader::counters() const { return counters_; }

void FrameStreamReader::processLine(const std::string& line,
                                    std::uint64_t timestamp_ns) {
    GloveFrame frame;
    if (!parser_.parse(line, timestamp_ns, frame)) {
        ++counters_.parse_errors;
        return;
    }
    ++counters_.frames;
    handler_(frame);
}

bool readFileFrames(const std::string& path, FrameStreamReader& reader,
                    std::string& error) {
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        error = path + ": " + std::strerror(errno);
        return false;
    }

    char buffer[37];  // Deliberately not aligned with Alpha frame boundaries.
    bool success = true;
    for (;;) {
        const ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            reader.consume(buffer, static_cast<std::size_t>(bytes),
                           monotonicNanoseconds());
        } else if (bytes == 0) {
            reader.finish(monotonicNanoseconds());
            break;
        } else if (errno != EINTR) {
            error = path + ": " + std::strerror(errno);
            success = false;
            break;
        }
    }
    close(fd);
    return success;
}
