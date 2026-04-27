#include "wayland_client.hpp"

#include <wayland-client.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace thumbboard::wayland {

static const wl_registry_listener kRegistryListener = {
    WaylandClient::registry_global,
    WaylandClient::registry_global_remove,
};

WaylandClient::WaylandClient() {
    display_ = wl_display_connect(nullptr);
    if (display_ == nullptr) {
        std::fprintf(
            stderr,
            "thumbboard: cannot connect to Wayland display — "
            "is WAYLAND_DISPLAY set?\n"
        );
        std::exit(1);
    }

    registry_ = wl_display_get_registry(display_);
    wl_registry_add_listener(registry_, &kRegistryListener, this);

    // First roundtrip: compositor sends all global events, on_global binds them.
    wl_display_roundtrip(display_);
    // Second roundtrip: ensures bind responses have arrived.
    wl_display_roundtrip(display_);

    if (layer_shell_ == nullptr) {
        std::fprintf(
            stderr,
            "thumbboard: compositor does not support "
            "wlr-layer-shell — requires Hyprland, Sway, river, "
            "labwc, or KDE Plasma 6\n"
        );
        std::exit(1);
    }
}

WaylandClient::~WaylandClient() {
    if (layer_shell_ != nullptr) {
        zwlr_layer_shell_v1_destroy(layer_shell_);
    }
    if (compositor_ != nullptr) {
        wl_compositor_destroy(compositor_);
    }
    if (registry_ != nullptr) {
        wl_registry_destroy(registry_);
    }
    if (display_ != nullptr) {
        wl_display_disconnect(display_);
    }
}

int WaylandClient::display_fd() const {
    return wl_display_get_fd(display_);
}

bool WaylandClient::dispatch() {
    return wl_display_dispatch(display_) >= 0;
}

bool WaylandClient::flush() {
    int ret = wl_display_flush(display_);
    // EAGAIN means the compositor's socket buffer is full; not fatal.
    return ret >= 0 || errno == EAGAIN;
}

void WaylandClient::roundtrip() {
    wl_display_roundtrip(display_);
}

wl_surface* WaylandClient::create_surface() {
    return wl_compositor_create_surface(compositor_);
}

void WaylandClient::registry_global(
    void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version
) {
    static_cast<WaylandClient*>(data)->on_global(registry, name, interface, version);
}

void WaylandClient::
    registry_global_remove(void* /*data*/, wl_registry* /*registry*/, uint32_t /*name*/) {}

void WaylandClient::on_global(
    wl_registry* registry, uint32_t name, const char* interface, uint32_t version
) {
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor_ = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4U))
        );
    } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        layer_shell_ = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, std::min(version, 1U))
        );
    }
}

} // namespace thumbboard::wayland
