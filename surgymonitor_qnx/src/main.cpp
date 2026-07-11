#include <iomanip>
#include <iostream>
#include <string>

#include "feature_window.h"
#include "serial_reader.h"

namespace {
void usage(const char* program) {
    std::cerr << "Usage: " << program
              << " --left-file FILE --right-file FILE\n"
              << "       " << program << " --fake\n";
}

bool runHand(const char* name, const std::string& path) {
    GloveFrame latest;
    FeatureWindow window;
    FrameStreamReader reader([&latest, &window](const GloveFrame& frame) {
        latest = frame;
        window.add(frame);
    });
    std::string error;
    if (!readFileFrames(path, reader, error)) {
        std::cerr << error << '\n';
        return false;
    }
    const ReaderCounters& counts = reader.counters();
    std::cout << name << ": frames=" << counts.frames
              << " parse_errors=" << counts.parse_errors;
    if (counts.frames > 0) {
        std::cout << std::fixed << std::setprecision(3)
                  << " latest=[" << latest.thumb << ", " << latest.index
                  << ", " << latest.middle << ", " << latest.ring << ", "
                  << latest.pinky << ']';
    }
    if (window.ready()) {
        const FeatureVector features = window.features();
        std::cout << " mean_thumb=" << features[0]
                  << " movement_thumb=" << features[5]
                  << " variability_thumb=" << features[10];
    } else {
        std::cout << " feature_window=" << window.size() << '/'
                  << kDefaultWindowSize;
    }
    std::cout << '\n';
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    std::string left;
    std::string right;
    if (argc == 2 && std::string(argv[1]) == "--fake") {
        left = "data/left_normal.txt";
        right = "data/right_mixed.txt";
    } else {
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if ((option == "--left-file" || option == "--right-file") &&
                i + 1 < argc) {
                (option == "--left-file" ? left : right) = argv[++i];
            } else {
                usage(argv[0]);
                return 2;
            }
        }
    }
    if (left.empty() || right.empty()) {
        usage(argv[0]);
        return 2;
    }
    return runHand("LEFT", left) && runHand("RIGHT", right) ? 0 : 1;
}
