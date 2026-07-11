#include "feature_window.h"

#include <cmath>
#include <stdexcept>

FeatureWindow::FeatureWindow(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ < 2) {
        throw std::invalid_argument("feature window capacity must be at least 2");
    }
}

void FeatureWindow::add(const GloveFrame& frame) {
    frames_.push_back(frame);
    if (frames_.size() > capacity_) {
        frames_.pop_front();
    }
}

bool FeatureWindow::ready() const { return frames_.size() == capacity_; }

std::size_t FeatureWindow::size() const { return frames_.size(); }

FeatureVector FeatureWindow::features() const {
    if (!ready()) {
        throw std::logic_error("feature window is not full");
    }

    FeatureVector result{};
    std::array<double, kFingerCount> sums{};
    std::array<double, kFingerCount> movementSums{};

    for (std::size_t sample = 0; sample < frames_.size(); ++sample) {
        const auto current = values(frames_[sample]);
        for (std::size_t finger = 0; finger < kFingerCount; ++finger) {
            sums[finger] += current[finger];
            if (sample > 0) {
                const auto previous = values(frames_[sample - 1]);
                movementSums[finger] +=
                    std::fabs(current[finger] - previous[finger]);
            }
        }
    }

    for (std::size_t finger = 0; finger < kFingerCount; ++finger) {
        result[finger] = sums[finger] / static_cast<double>(frames_.size());
        result[kFingerCount + finger] =
            movementSums[finger] / static_cast<double>(frames_.size() - 1);
    }

    for (const GloveFrame& frame : frames_) {
        const auto current = values(frame);
        for (std::size_t finger = 0; finger < kFingerCount; ++finger) {
            const double difference = current[finger] - result[finger];
            result[2 * kFingerCount + finger] += difference * difference;
        }
    }
    for (std::size_t finger = 0; finger < kFingerCount; ++finger) {
        result[2 * kFingerCount + finger] = std::sqrt(
            result[2 * kFingerCount + finger] /
            static_cast<double>(frames_.size()));
    }
    return result;
}

std::array<double, kFingerCount> FeatureWindow::values(
    const GloveFrame& frame) {
    return {frame.thumb, frame.index, frame.middle, frame.ring, frame.pinky};
}
