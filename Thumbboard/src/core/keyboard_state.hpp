#pragma once

#include <cstdint>

namespace thumbboard::core {

enum class Layer : uint8_t { Alpha, Numeric, Symbol };
enum class ShiftState : uint8_t { Off, OneShotArmed, CapsLock };

struct KeyboardState {
    int        cursor  = 0;
    bool       visible = true;
    Layer      layer   = Layer::Alpha;
    ShiftState shift   = ShiftState::Off;
};

} // namespace thumbboard::core
