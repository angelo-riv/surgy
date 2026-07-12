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
    MonitorOptions options;
    options.left.name = "LEFT-TEST";
    options.right.name = "RIGHT-TEST";
    options.left.input = {"data/left_normal.txt", false};
    options.right.input = {"data/right_mixed.txt", false};

    MonitorRunSummary summary;
    require(runMonitor(options, &summary) == 0,
            "finite threaded monitor run succeeds");
    require(summary.left.frames == 24, "all left frames are processed");
    require(summary.right.frames == 24, "all right valid frames are processed");
    require(summary.left.parse_errors == 0,
            "left parse error count is preserved");
    require(summary.right.parse_errors == 1,
            "right malformed line is preserved in counters");
    require(summary.left.queue_drained && summary.right.queue_drained,
            "both queues drain before runtime shutdown");
    require(summary.left.feature_window_ready &&
                summary.right.feature_window_ready,
            "both final feature windows are produced");

    std::cout << "All threaded monitor runtime tests passed.\n";
    return 0;
}
