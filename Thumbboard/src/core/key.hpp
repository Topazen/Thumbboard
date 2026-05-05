#pragma once

#include <string>

namespace thumbboard::core {

struct Key {
    std::string label;       // primary glyph drawn on the key
    std::string alt_label;   // optional secondary glyph (top-right corner)
    std::string keysym;      // xkb keysym name; empty → infer from label
    float x = 0.0F;          // grid-cell coords; multiplied by cell size
    float y = 0.0F;
    float w = 1.0F;
    float h = 1.0F;
};

} // namespace thumbboard::core
