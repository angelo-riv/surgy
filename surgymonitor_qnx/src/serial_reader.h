#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "glove_frame.h"
#include "glove_parser.h"

struct ReaderCounters {
    std::uint64_t frames = 0;
    std::uint64_t parse_errors = 0;
    std::uint64_t read_errors = 0;
};

class FrameStreamReader {
public:
    using FrameHandler = std::function<void(const GloveFrame&)>;

    explicit FrameStreamReader(FrameHandler handler);
    void consume(const char* data, std::size_t size, std::uint64_t timestamp_ns);
    void finish(std::uint64_t timestamp_ns);
    const ReaderCounters& counters() const;

private:
    void processLine(const std::string& line, std::uint64_t timestamp_ns);

    GloveParser parser_;
    FrameHandler handler_;
    std::string accumulator_;
    ReaderCounters counters_;
};

bool readFileFrames(const std::string& path, FrameStreamReader& reader,
                    std::string& error);
