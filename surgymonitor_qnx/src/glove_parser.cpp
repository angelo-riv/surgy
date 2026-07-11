#include "glove_parser.h"

#include <array>
#include <cctype>
#include <cstdlib>

namespace {
constexpr double kAdcMaximum = 4095.0;

bool parseField(const std::string& line, std::size_t& position, char tag,
                int& value) {
    if (position >= line.size() || line[position] != tag) {
        return false;
    }
    ++position;
    const std::size_t firstDigit = position;
    while (position < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[position]))) {
        ++position;
    }
    if (firstDigit == position) {
        return false;
    }

    const std::string token = line.substr(firstDigit, position - firstDigit);
    char* end = nullptr;
    const long parsed = std::strtol(token.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || parsed < 0 || parsed > 4095) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}
}  // namespace

bool GloveParser::parse(const std::string& input, std::uint64_t timestamp_ns,
                        GloveFrame& frame) const {
    std::string line = input;
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::array<int, 5> raw{};
    std::size_t position = 0;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (!parseField(line, position, static_cast<char>('A' + i), raw[i])) {
            return false;
        }
    }

    // Alpha frames continue with F/G/P and optional gesture fields. Requiring
    // F here rejects truncated A-E prefixes while preserving upstream syntax.
    if (position >= line.size() || line[position] != 'F') {
        return false;
    }

    frame.timestamp_ns = timestamp_ns;
    frame.thumb = raw[0] / kAdcMaximum;
    frame.index = raw[1] / kAdcMaximum;
    frame.middle = raw[2] / kAdcMaximum;
    frame.ring = raw[3] / kAdcMaximum;
    frame.pinky = raw[4] / kAdcMaximum;
    return true;
}
