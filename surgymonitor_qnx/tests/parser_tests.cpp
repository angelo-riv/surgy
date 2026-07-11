#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "glove_parser.h"
#include "serial_reader.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool near(double left, double right) {
    return std::fabs(left - right) < 1e-9;
}
}  // namespace

int main() {
    GloveParser parser;
    GloveFrame frame;
    require(parser.parse("A0B1024C2048D3072E4095F512G512P0", 42, frame),
            "valid Alpha frame parses");
    require(frame.timestamp_ns == 42, "timestamp is preserved");
    require(near(frame.thumb, 0.0), "zero normalizes to zero");
    require(near(frame.pinky, 1.0), "4095 normalizes to one");
    require(!parser.parse("A0B1C2D3E4", 0, frame),
            "truncated Alpha prefix is rejected");
    require(!parser.parse("A0B1C2D3E5000F0G0P0", 0, frame),
            "out-of-range ADC value is rejected");
    require(!parser.parse("debug text", 0, frame), "debug text is rejected");

    std::vector<GloveFrame> frames;
    FrameStreamReader reader([&frames](const GloveFrame& value) {
        frames.push_back(value);
    });
    const std::string first = "A1B2C3D4E5F0G0P0\nA10B20";
    const std::string second = "C30D40E50F0G0P0\nmalformed\n";
    reader.consume(first.data(), first.size(), 100);
    require(frames.size() == 1, "partial frame remains buffered");
    reader.consume(second.data(), second.size(), 200);
    require(frames.size() == 2, "split frame is reassembled");
    require(reader.counters().frames == 2, "frame counter is correct");
    require(reader.counters().parse_errors == 1,
            "parse error counter is correct");

    FrameStreamReader incomplete([](const GloveFrame&) {});
    const std::string noNewline = "A1B2C3D4E5F0G0P0";
    incomplete.consume(noNewline.data(), noNewline.size(), 300);
    incomplete.finish(400);
    require(incomplete.counters().frames == 0,
            "unterminated data is not treated as a complete frame");
    require(incomplete.counters().parse_errors == 1,
            "unterminated data is counted as a parse error");

    std::cout << "All parser and chunking tests passed.\n";
    return 0;
}
