#pragma once

#include <memory>

namespace thumbboard::wayland {
class WaylandClient;
class LayerShellSurface;
} // namespace thumbboard::wayland

namespace thumbboard::render {
class EglContext;
} // namespace thumbboard::render

namespace thumbboard::app {

class Daemon {
public:
    Daemon();
    ~Daemon();

    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;

    void run();

private:
    static void signal_handler(int signum);
    void setup_signal_pipe();

    // Declaration order determines destruction order (reverse):
    // egl_context_ destructs first (before the surface it references),
    // then layer_surface_, then wayland_client_.
    std::unique_ptr<wayland::WaylandClient> wayland_client_;
    std::unique_ptr<wayland::LayerShellSurface> layer_surface_;
    std::unique_ptr<render::EglContext> egl_context_;

    int signal_pipe_read_fd_ = -1;
    int signal_pipe_write_fd_ = -1;
};

} // namespace thumbboard::app
