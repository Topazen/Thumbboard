#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct xkb_context;
struct xkb_keymap;

namespace thumbboard::wayland {

// Builds and serializes an xkb keymap for use with zwp_virtual_keyboard_v1.
class XkbContext {
public:
    explicit XkbContext(const std::string& layout = "us");
    ~XkbContext();

    XkbContext(const XkbContext&) = delete;
    XkbContext& operator=(const XkbContext&) = delete;

    // File descriptor of a memfd containing the serialized keymap.
    // Caller does NOT own this fd — XkbContext keeps it open for its lifetime.
    int keymap_fd() const {
        return fd_;
    }
    uint32_t keymap_size() const {
        return size_;
    }

    // Returns the Linux evdev keycode (keycode = xkb_keycode - 8) for the first
    // key that produces the named keysym at any level in layout group 0.
    // Returns 0 if the keysym is unknown or not present in the keymap.
    [[nodiscard]] uint32_t keysym_to_keycode(std::string_view name) const;

private:
    xkb_context* ctx_ = nullptr;
    xkb_keymap* keymap_ = nullptr;
    int fd_ = -1;
    uint32_t size_ = 0;
};

} // namespace thumbboard::wayland
