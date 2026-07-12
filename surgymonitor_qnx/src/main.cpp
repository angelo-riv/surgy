#include <cmath>
#include <iostream>
#include <string>

#include "monitor.h"

namespace {
void usage(const char* program) {
    std::cerr << "Usage: " << program
              << " --left DEVICE --right DEVICE [options]\n"
              << "       " << program
              << " --left-file FILE --right-file FILE [options]\n"
              << "       " << program << " --fake [options]\n"
              << "Options:\n"
              << "  --left-baseline CSV --right-baseline CSV\n"
              << "  --record-left-baseline CSV --record-right-baseline CSV\n"
              << "  --threshold NUMBER  (default 0.5)\n"
              << "  --k NUMBER          (default 5)\n"
              << "  --offline-timeout-ms NUMBER (default 1000)\n";
}

bool parseSize(const std::string& text, std::size_t& value) {
    try {
        std::size_t consumed = 0;
        const unsigned long parsed = std::stoul(text, &consumed);
        if (consumed != text.size() || parsed == 0) return false;
        value = static_cast<std::size_t>(parsed);
        return true;
    } catch (const std::exception&) { return false; }
}

bool parseNumber(const std::string& text, double& value) {
    try {
        std::size_t consumed = 0;
        value = std::stod(text, &consumed);
        return consumed == text.size() && std::isfinite(value) && value >= 0.0;
    } catch (const std::exception&) { return false; }
}
}  // namespace

int main(int argc, char** argv) {
    MonitorOptions options;
    options.left.name = "LEFT";
    options.right.name = "RIGHT";
    bool fake = false;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--fake") { fake = true; continue; }
        if (i + 1 >= argc) { usage(argv[0]); return 2; }
        const std::string value = argv[++i];
        if (option == "--left") {
            options.left.input = {value, true};
        } else if (option == "--right") {
            options.right.input = {value, true};
        } else if (option == "--left-file") {
            options.left.input = {value, false};
        } else if (option == "--right-file") {
            options.right.input = {value, false};
        } else if (option == "--left-baseline") {
            options.left.baseline_path = value;
        } else if (option == "--right-baseline") {
            options.right.baseline_path = value;
        } else if (option == "--record-left-baseline") {
            options.left.record_path = value;
        } else if (option == "--record-right-baseline") {
            options.right.record_path = value;
        } else if (option == "--k") {
            if (!parseSize(value, options.k)) { std::cerr << "invalid --k\n"; return 2; }
        } else if (option == "--threshold") {
            if (!parseNumber(value, options.threshold)) { std::cerr << "invalid --threshold\n"; return 2; }
        } else if (option == "--offline-timeout-ms") {
            std::size_t milliseconds = 0;
            if (!parseSize(value, milliseconds)) { std::cerr << "invalid offline timeout\n"; return 2; }
            options.offline_timeout_ns = milliseconds * 1000000ULL;
        } else { usage(argv[0]); return 2; }
    }
    if (fake) {
        if (options.left.input.path.empty()) options.left.input = {"data/left_normal.txt", false};
        if (options.right.input.path.empty()) options.right.input = {"data/right_mixed.txt", false};
    }
    if (options.left.input.path.empty() || options.right.input.path.empty()) {
        usage(argv[0]); return 2;
    }
    if ((!options.left.baseline_path.empty() && !options.left.record_path.empty()) ||
        (!options.right.baseline_path.empty() && !options.right.record_path.empty())) {
        std::cerr << "cannot load and record a baseline for the same hand\n";
        return 2;
    }
    return runMonitor(options);
}
