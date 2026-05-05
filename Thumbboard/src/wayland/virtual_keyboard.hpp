#pragma once

#include <cstdint>

#include "keystroke_sink.hpp"

struct zwp_virtual_keyboard_v1;
struct zwp_virtual_keyboard_manager_v1;
struct wl_seat;

namespace thumbboard::wayland {

class XkbContext;

class VirtualKeyboard final : public KeystrokeSink {
public:
    VirtualKeyboard(
        zwp_virtual_keyboard_manager_v1* manager, wl_seat* seat, const XkbContext& xkb
    );
    ~VirtualKeyboard() override;

    VirtualKeyboard(const VirtualKeyboard&) = delete;
    VirtualKeyboard& operator=(const VirtualKeyboard&) = delete;

    void press(uint32_t keycode) override;
    void release(uint32_t keycode) override;
    void send_modifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group = 0);

private:
    zwp_virtual_keyboard_v1* kbd_ = nullptr;
};

} // namespace thumbboard::wayland
