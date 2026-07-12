#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

#include <armadillo>

#include "feature_window.h"

struct BaselineLoadStats {
    std::size_t total_lines = 0;
    std::size_t blank_lines = 0;
    std::size_t vectors = 0;
    std::size_t malformed_rows = 0;
};

struct AnomalyResult {
    double score = 0.0;
    bool anomaly = false;
    std::size_t neighbours_used = 0;
};

class AnomalyDetector {
public:
    explicit AnomalyDetector(std::size_t k = 5, double threshold = 0.5);

    bool loadBaseline(const std::string& path, BaselineLoadStats& stats,
                      std::string& error);
    AnomalyResult score(const FeatureVector& features) const;
    std::size_t baselineSize() const;

private:
    std::size_t k_;
    double threshold_;
    arma::mat baseline_;
};

bool appendFeatureCsv(std::ostream& output, const FeatureVector& features);
