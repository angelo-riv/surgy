#pragma once

#include <cstdint>

struct GloveFrame {
    std::uint64_t timestamp_ns = 0;
    double thumb = 0.0;
    double index = 0.0;
    double middle = 0.0;
    double ring = 0.0;
    double pinky = 0.0;
};
