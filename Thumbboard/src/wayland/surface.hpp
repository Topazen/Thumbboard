#pragma once

#include <cstdint>

struct wl_surface;

namespace thumbboard::wayland {

class Surface {
public:
    virtual ~Surface() = default;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void on_configure(uint32_t serial, int width, int height) = 0;
    virtual wl_surface* wl_surface_handle() = 0;
    virtual bool supports_exclusive_zone() const = 0;
};

} // namespace thumbboard::wayland
