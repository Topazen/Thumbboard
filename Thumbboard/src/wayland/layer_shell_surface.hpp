#pragma once

#include <cstdint>

#include "surface.hpp"

struct zwlr_layer_surface_v1;

namespace thumbboard::wayland {

class WaylandClient;

class LayerShellSurface final : public Surface {
public:
    explicit LayerShellSurface(WaylandClient& client, int height_px = 360);
    ~LayerShellSurface() override;

    LayerShellSurface(const LayerShellSurface&) = delete;
    LayerShellSurface& operator=(const LayerShellSurface&) = delete;

    void show() override;
    void hide() override;
    void on_configure(uint32_t serial, int width, int height) override;
    wl_surface* wl_surface_handle() override {
        return surface_;
    }
    bool supports_exclusive_zone() const override {
        return true;
    }

    bool is_configured() const {
        return configured_;
    }
    int width() const {
        return width_;
    }
    int height() const {
        return height_;
    }

    // Public so they can be referenced in file-scope Wayland listener structs.
    static void handle_configure(
        void* data,
        zwlr_layer_surface_v1* layer_surface,
        uint32_t serial,
        uint32_t width,
        uint32_t height
    );
    static void handle_closed(void* data, zwlr_layer_surface_v1* layer_surface);

private:
    wl_surface* surface_ = nullptr;
    zwlr_layer_surface_v1* layer_surface_ = nullptr;
    bool configured_ = false;
    int width_ = 0;
    int height_ = 0;
};

} // namespace thumbboard::wayland
