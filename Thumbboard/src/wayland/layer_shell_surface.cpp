#include "layer_shell_surface.hpp"

#include <wayland-client.h>

#include <cstdio>
#include <cstdlib>

#include "wayland_client.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace thumbboard::wayland {

static const zwlr_layer_surface_v1_listener kLayerSurfaceListener = {
    LayerShellSurface::handle_configure,
    LayerShellSurface::handle_closed,
};

LayerShellSurface::LayerShellSurface(WaylandClient& client, int height_px) : height_(height_px) {
    surface_ = client.create_surface();

    layer_surface_ = zwlr_layer_shell_v1_get_layer_surface(
        client.layer_shell(), surface_, nullptr, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "thumbboard"
    );

    // width=0: compositor fills the full output width.
    zwlr_layer_surface_v1_set_size(layer_surface_, 0, static_cast<uint32_t>(height_px));
    zwlr_layer_surface_v1_set_anchor(
        layer_surface_,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
    );
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface_, height_px);
    zwlr_layer_surface_v1_add_listener(layer_surface_, &kLayerSurfaceListener, this);

    // Initial commit: no buffer yet; compositor responds with a configure event.
    wl_surface_commit(surface_);
}

LayerShellSurface::~LayerShellSurface() {
    if (layer_surface_ != nullptr) {
        zwlr_layer_surface_v1_destroy(layer_surface_);
    }
    if (surface_ != nullptr) {
        wl_surface_destroy(surface_);
    }
}

void LayerShellSurface::show() {
    // Will be implemented in M5 (daemon mode + IPC).
}

void LayerShellSurface::hide() {
    // Will be implemented in M5 (daemon mode + IPC).
}

void LayerShellSurface::on_configure(uint32_t serial, int width, int height) {
    if (width > 0) {
        width_ = width;
    }
    if (height > 0) {
        height_ = height;
    }
    zwlr_layer_surface_v1_ack_configure(layer_surface_, serial);
    wl_surface_commit(surface_);
    configured_ = true;
}

void LayerShellSurface::handle_configure(
    void* data,
    zwlr_layer_surface_v1* /*layer_surface*/,
    uint32_t serial,
    uint32_t width,
    uint32_t height
) {
    static_cast<LayerShellSurface*>(data)->on_configure(
        serial, static_cast<int>(width), static_cast<int>(height)
    );
}

void LayerShellSurface::handle_closed(void* /*data*/, zwlr_layer_surface_v1* /*layer_surface*/) {
    std::fprintf(stderr, "thumbboard: compositor closed the surface\n");
    std::exit(0);
}

} // namespace thumbboard::wayland
