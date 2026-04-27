#pragma once

#include <cstdint>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct zwlr_layer_shell_v1;

namespace thumbboard::wayland {

class WaylandClient {
public:
    WaylandClient();
    ~WaylandClient();

    WaylandClient(const WaylandClient&) = delete;
    WaylandClient& operator=(const WaylandClient&) = delete;

    int display_fd() const;
    bool dispatch();
    bool flush();
    void roundtrip();

    wl_surface* create_surface();

    wl_display* display() const {
        return display_;
    }
    wl_compositor* compositor() const {
        return compositor_;
    }
    zwlr_layer_shell_v1* layer_shell() const {
        return layer_shell_;
    }

    // Public so they can be referenced in file-scope Wayland listener structs.
    static void registry_global(
        void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version
    );
    static void registry_global_remove(void* data, wl_registry* registry, uint32_t name);

private:
    void on_global(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);

    wl_display* display_ = nullptr;
    wl_registry* registry_ = nullptr;
    wl_compositor* compositor_ = nullptr;
    zwlr_layer_shell_v1* layer_shell_ = nullptr;
};

} // namespace thumbboard::wayland
