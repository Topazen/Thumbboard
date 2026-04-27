#include "daemon.hpp"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

#include "../render/egl_context.hpp"
#include "../wayland/layer_shell_surface.hpp"
#include "../wayland/wayland_client.hpp"

namespace thumbboard::app {

// Write end is stored here so the async-signal-safe handler can reach it.
static int g_sig_write_fd = -1; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void Daemon::signal_handler(int signum) {
    if (g_sig_write_fd >= 0) {
        // NOLINTNEXTLINE(bugprone-signal-handler)
        const auto byte = static_cast<char>(signum);
        (void)write(g_sig_write_fd, &byte, 1);
    }
}

void Daemon::setup_signal_pipe() {
    int fds[2]{};
    if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0) {
        std::fprintf(stderr, "thumbboard: pipe2 failed: %s\n", std::strerror(errno));
        std::exit(1);
    }
    signal_pipe_read_fd_ = fds[0];
    signal_pipe_write_fd_ = fds[1];
    g_sig_write_fd = fds[1];

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

Daemon::Daemon() {
    setup_signal_pipe();
    wayland_client_ = std::make_unique<wayland::WaylandClient>();
    layer_surface_ = std::make_unique<wayland::LayerShellSurface>(*wayland_client_);
}

Daemon::~Daemon() {
    g_sig_write_fd = -1;
    if (signal_pipe_write_fd_ >= 0) {
        close(signal_pipe_write_fd_);
    }
    if (signal_pipe_read_fd_ >= 0) {
        close(signal_pipe_read_fd_);
    }
}

void Daemon::run() {
    // Flush the initial surface commit and wait for the compositor configure.
    wayland_client_->flush();
    while (!layer_surface_->is_configured()) {
        wayland_client_->roundtrip();
    }

    egl_context_ = std::make_unique<render::EglContext>(
        wayland_client_->display(),
        layer_surface_->wl_surface_handle(),
        layer_surface_->width(),
        layer_surface_->height()
    );

    // Render one frame immediately so the surface gets a buffer attached.
    egl_context_->render_frame();
    wayland_client_->flush();

    struct pollfd fds[2]{
        {wayland_client_->display_fd(), POLLIN, 0},
        {signal_pipe_read_fd_, POLLIN, 0},
    };

    while (true) {
        wayland_client_->flush();
        const int ret = poll(fds, 2, -1);

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::fprintf(stderr, "thumbboard: poll error: %s\n", std::strerror(errno));
            break;
        }

        if ((fds[1].revents & POLLIN) != 0) {
            break; // signal received — clean exit
        }

        if ((fds[0].revents & POLLIN) != 0) {
            if (!wayland_client_->dispatch()) {
                break; // compositor disconnected
            }
        }

        egl_context_->render_frame();
    }
}

} // namespace thumbboard::app
