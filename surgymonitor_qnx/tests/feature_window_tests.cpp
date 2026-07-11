#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "feature_window.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool near(double left, double right, double tolerance = 1e-12) {
    return std::fabs(left - right) < tolerance;
}

GloveFrame uniform(double value) {
    GloveFrame frame;
    frame.thumb = value;
    frame.index = value;
    frame.middle = value;
    frame.ring = value;
    frame.pinky = value;
    return frame;
}
}  // namespace

int main() {
    FeatureWindow stable;
    for (std::size_t i = 0; i < kDefaultWindowSize; ++i) {
        stable.add(uniform(0.25));
    }
    require(stable.ready(), "20 samples make the default window ready");
    const FeatureVector stableFeatures = stable.features();
    for (std::size_t finger = 0; finger < kFingerCount; ++finger) {
        require(near(stableFeatures[finger], 0.25),
                "stable mean is correct");
        require(near(stableFeatures[kFingerCount + finger], 0.0),
                "stable movement is zero");
        require(near(stableFeatures[2 * kFingerCount + finger], 0.0),
                "stable variability is zero");
    }

    FeatureWindow ramp;
    for (std::size_t i = 0; i < kDefaultWindowSize; ++i) {
        ramp.add(uniform(static_cast<double>(i) / 19.0));
    }
    const FeatureVector rampFeatures = ramp.features();
    require(near(rampFeatures[0], 0.5), "ramp mean is correct");
    require(near(rampFeatures[5], 1.0 / 19.0),
            "average absolute per-frame movement is correct");
    require(rampFeatures[10] > 0.30 && rampFeatures[10] < 0.31,
            "ramp population standard deviation is correct");

    ramp.add(uniform(1.0));
    require(ramp.size() == kDefaultWindowSize,
            "sliding window retains only its configured capacity");

    FeatureWindow incomplete;
    bool threw = false;
    try {
        (void)incomplete.features();
    } catch (const std::logic_error&) {
        threw = true;
    }
    require(threw, "features cannot be calculated before the window is full");

    std::cout << "All feature window tests passed.\n";
    return 0;
}
