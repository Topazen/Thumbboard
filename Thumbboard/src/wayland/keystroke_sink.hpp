#pragma once

#include <cstdint>

namespace thumbboard::wayland {

// Abstract keystroke output. Today: zwp_virtual_keyboard_v1.
// Future: zwp_input_method_v2 (see ADR 0001).
class KeystrokeSink {
public:
    virtual ~KeystrokeSink() = default;

    // Send a Linux evdev keycode press/release pair.
    virtual void press(uint32_t keycode) = 0;
    virtual void release(uint32_t keycode) = 0;
};

} // namespace thumbboard::wayland
