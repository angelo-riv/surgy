#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

#include "anomaly_detector.h"

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::string temporaryCsv(const std::string& contents) {
    char path[] = "/tmp/surgy-baseline-XXXXXX";
    const int fd = mkstemp(path);
    require(fd >= 0, "temporary baseline file is created");
    close(fd);
    std::ofstream output(path);
    output << contents;
    output.close();
    return path;
}

std::string row(double value) {
    std::string result;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        if (i > 0) result += ',';
        result += std::to_string(value);
    }
    return result + '\n';
}
}  // namespace

int main() {
    const std::string validPath = temporaryCsv(
        "\n" + row(0.00) + row(0.01) + row(0.02) + row(0.03) +
        row(0.04) + row(0.05));
    AnomalyDetector detector(5, 0.5);
    BaselineLoadStats stats;
    std::string error;
    require(detector.loadBaseline(validPath, stats, error),
            "valid baseline CSV loads");
    require(stats.vectors == 6 && stats.blank_lines == 1,
            "baseline counters are correct");

    FeatureVector near{};
    near.fill(0.025);
    FeatureVector far{};
    far.fill(1.0);
    const AnomalyResult nearResult = detector.score(near);
    const AnomalyResult farResult = detector.score(far);
    require(nearResult.neighbours_used == 5, "k=5 is used");
    require(nearResult.score < farResult.score,
            "near vector scores lower than distant vector");
    require(!nearResult.anomaly && farResult.anomaly,
            "threshold classification works");

    const std::string smallPath = temporaryCsv(row(0.0) + row(0.1));
    AnomalyDetector small(5, 10.0);
    require(small.loadBaseline(smallPath, stats, error),
            "small baseline loads");
    require(small.score(near).neighbours_used == 2,
            "baseline smaller than k uses all available vectors");

    const std::string malformedPath = temporaryCsv(row(0.0) + "bad,row\n");
    require(!detector.loadBaseline(malformedPath, stats, error),
            "malformed row is rejected");
    require(stats.malformed_rows == 1,
            "malformed row counter is incremented");

    const std::string shortPath = temporaryCsv("1,2,3\n");
    require(!detector.loadBaseline(shortPath, stats, error),
            "incorrect feature count is rejected");

    const std::string nonFinitePath = temporaryCsv(
        "nan,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n");
    require(!detector.loadBaseline(nonFinitePath, stats, error),
            "non-finite CSV value is rejected");

    unlink(validPath.c_str());
    unlink(smallPath.c_str());
    unlink(malformedPath.c_str());
    unlink(shortPath.c_str());
    unlink(nonFinitePath.c_str());
    std::cout << "All anomaly detector tests passed.\n";
    return 0;
}
