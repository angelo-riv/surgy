#pragma once

#include <array>
#include <cstddef>
#include <deque>

#include "glove_frame.h"

constexpr std::size_t kFingerCount = 5;
constexpr std::size_t kFeatureCount = 15;
constexpr std::size_t kDefaultWindowSize = 20;

using FeatureVector = std::array<double, kFeatureCount>;

class FeatureWindow {
public:
    explicit FeatureWindow(std::size_t capacity = kDefaultWindowSize);

    void add(const GloveFrame& frame);
    bool ready() const;
    std::size_t size() const;
    FeatureVector features() const;

private:
    static std::array<double, kFingerCount> values(const GloveFrame& frame);

    std::size_t capacity_;
    std::deque<GloveFrame> frames_;
};
