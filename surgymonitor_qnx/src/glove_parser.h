#pragma once

#include <cstdint>
#include <string>

#include "glove_frame.h"

class GloveParser {
public:
    bool parse(const std::string& line, std::uint64_t timestamp_ns,
               GloveFrame& frame) const;
};
