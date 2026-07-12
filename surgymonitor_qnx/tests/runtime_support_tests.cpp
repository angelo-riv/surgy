#include <cstdlib>
#include <iostream>

#include "monitor.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main() {
    FrameQueue left(2);
    FrameQueue right(2);
    GloveFrame first;
    first.timestamp_ns = 1;
    GloveFrame second;
    second.timestamp_ns = 2;
    GloveFrame third;
    third.timestamp_ns = 3;
    left.push(first);
    right.push(second);
    GloveFrame output;
    require(left.tryPop(output) && output.timestamp_ns == 1,
            "left queue transfers its frame independently");
    require(right.tryPop(output) && output.timestamp_ns == 2,
            "right queue transfers its frame independently");

    left.push(first);
    left.push(second);
    left.push(third);
    require(left.dropped() == 1, "bounded queue counts dropped oldest frames");
    require(left.tryPop(output) && output.timestamp_ns == 2,
            "bounded queue retains newer frames");

    require(gloveOnline(150, 100, 50), "frame at timeout boundary is online");
    require(!gloveOnline(151, 100, 50), "stale frame is offline");
    require(!gloveOnline(100, 0, 50), "glove with no valid frame is offline");

    std::cout << "All runtime support tests passed.\n";
    return 0;
}
