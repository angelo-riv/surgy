#include "anomaly_detector.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <mlpack.hpp>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
bool isBlank(const std::string& line) {
    return line.find_first_not_of(" \t\r") == std::string::npos;
}

bool parseRow(const std::string& line, FeatureVector& row,
              std::string& reason) {
    if (!line.empty() && line.back() == ',') {
        reason = "empty value after trailing comma";
        return false;
    }
    std::stringstream stream(line);
    std::string cell;
    std::size_t column = 0;
    while (std::getline(stream, cell, ',')) {
        if (column >= kFeatureCount) {
            reason = "expected exactly 15 features";
            return false;
        }
        try {
            std::size_t consumed = 0;
            const double value = std::stod(cell, &consumed);
            if (consumed != cell.size()) {
                reason = "invalid numeric value in column " +
                         std::to_string(column + 1);
                return false;
            }
            if (!std::isfinite(value)) {
                reason = "non-finite value in column " +
                         std::to_string(column + 1);
                return false;
            }
            row[column++] = value;
        } catch (const std::exception&) {
            reason = "invalid numeric value in column " +
                     std::to_string(column + 1);
            return false;
        }
    }
    if (column != kFeatureCount) {
        reason = "expected exactly 15 features, found " +
                 std::to_string(column);
        return false;
    }
    return true;
}
}  // namespace

AnomalyDetector::AnomalyDetector(std::size_t k, double threshold)
    : k_(k), threshold_(threshold) {
    if (k_ == 0) {
        throw std::invalid_argument("k must be at least 1");
    }
    if (!std::isfinite(threshold_) || threshold_ < 0.0) {
        throw std::invalid_argument("threshold must be finite and non-negative");
    }
}

bool AnomalyDetector::loadBaseline(const std::string& path,
                                   BaselineLoadStats& stats,
                                   std::string& error) {
    stats = {};
    baseline_.reset();
    std::ifstream input(path);
    if (!input) {
        error = "cannot open baseline: " + path;
        return false;
    }

    std::vector<FeatureVector> rows;
    std::string line;
    while (std::getline(input, line)) {
        ++stats.total_lines;
        if (isBlank(line)) {
            ++stats.blank_lines;
            continue;
        }
        FeatureVector row{};
        std::string reason;
        if (!parseRow(line, row, reason)) {
            ++stats.malformed_rows;
            error = path + ": line " + std::to_string(stats.total_lines) +
                    ": " + reason;
            return false;
        }
        rows.push_back(row);
        ++stats.vectors;
    }
    if (rows.empty()) {
        error = path + ": baseline contains no feature vectors";
        return false;
    }

    baseline_.set_size(kFeatureCount, rows.size());
    for (std::size_t column = 0; column < rows.size(); ++column) {
        for (std::size_t feature = 0; feature < kFeatureCount; ++feature) {
            baseline_(feature, column) = rows[column][feature];
        }
    }
    return true;
}

AnomalyResult AnomalyDetector::score(const FeatureVector& features) const {
    if (baseline_.n_cols == 0) {
        throw std::logic_error("cannot score without a loaded baseline");
    }
    arma::mat query(kFeatureCount, 1);
    for (std::size_t feature = 0; feature < kFeatureCount; ++feature) {
        if (!std::isfinite(features[feature])) {
            throw std::invalid_argument("live feature vector is non-finite");
        }
        query(feature, 0) = features[feature];
    }

    const std::size_t neighboursUsed =
        std::min(k_, static_cast<std::size_t>(baseline_.n_cols));
    mlpack::KNN knn(baseline_);
    arma::Mat<std::size_t> neighbours;
    arma::mat distances;
    knn.Search(query, neighboursUsed, neighbours, distances);

    AnomalyResult result;
    result.neighbours_used = neighboursUsed;
    result.score = arma::accu(distances.col(0)) /
                   static_cast<double>(neighboursUsed);
    result.anomaly = result.score > threshold_;
    return result;
}

std::size_t AnomalyDetector::baselineSize() const { return baseline_.n_cols; }

bool appendFeatureCsv(std::ostream& output, const FeatureVector& features) {
    output << std::setprecision(17);
    for (std::size_t i = 0; i < features.size(); ++i) {
        if (!std::isfinite(features[i])) {
            return false;
        }
        if (i > 0) {
            output << ',';
        }
        output << features[i];
    }
    output << '\n';
    return static_cast<bool>(output);
}
