#include "virtual_keyboard.hpp"

#include <wayland-client.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "virtual-keyboard-unstable-v1-client-protocol.h"
#include "xkb_context.hpp"

namespace thumbboard::wayland {

static uint32_t now_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(
        static_cast<uint64_t>(ts.tv_sec) * 1000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL
    );
}

VirtualKeyboard::VirtualKeyboard(
    zwp_virtual_keyboard_manager_v1* manager, wl_seat* seat, const XkbContext& xkb
) {
    kbd_ = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(manager, seat);
    if (kbd_ == nullptr) {
        std::fprintf(stderr, "thumbboard: failed to create virtual keyboard\n");
        std::exit(1);
    }
    // WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 == 1
    zwp_virtual_keyboard_v1_keymap(kbd_, 1, xkb.keymap_fd(), xkb.keymap_size());
}

VirtualKeyboard::~VirtualKeyboard() {
    if (kbd_ != nullptr) {
        zwp_virtual_keyboard_v1_destroy(kbd_);
    }
}

void VirtualKeyboard::press(uint32_t keycode) {
    // WL_KEYBOARD_KEY_STATE_PRESSED == 1
    zwp_virtual_keyboard_v1_key(kbd_, now_ms(), keycode, 1);
}

void VirtualKeyboard::release(uint32_t keycode) {
    // WL_KEYBOARD_KEY_STATE_RELEASED == 0
    zwp_virtual_keyboard_v1_key(kbd_, now_ms(), keycode, 0);
}

void VirtualKeyboard::send_modifiers(
    uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group
) {
    zwp_virtual_keyboard_v1_modifiers(kbd_, depressed, latched, locked, group);
}

} // namespace thumbboard::wayland
