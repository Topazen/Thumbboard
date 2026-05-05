#include "daemon.hpp"

#include <SDL3/SDL.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

#include "../core/layout.hpp"
#include "../input/cursor_controller.hpp"
#include "../input/gamepad_manager.hpp"
#include "../render/egl_context.hpp"
#include "../render/renderer.hpp"
#include "../wayland/layer_shell_surface.hpp"
#include "../wayland/virtual_keyboard.hpp"
#include "../wayland/wayland_client.hpp"
#include "../wayland/xkb_context.hpp"

namespace thumbboard::app {

static int g_sig_write_fd = -1; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void Daemon::signal_handler(int signum) {
    if (g_sig_write_fd >= 0) {
        // NOLINTNEXTLINE(bugprone-signal-handler)
        const auto byte = static_cast<char>(signum);
        (void)write(g_sig_write_fd, &byte, 1);
    }
}

void Daemon::setup_signal_pipe() {
    std::array<int, 2> fds{};
    if (pipe2(fds.data(), O_CLOEXEC | O_NONBLOCK) != 0) {
        std::fprintf(stderr, "thumbboard: pipe2 failed: %s\n", std::strerror(errno));
        std::exit(1);
    }
    signal_pipe_read_fd_ = fds[0];
    signal_pipe_write_fd_ = fds[1];
    g_sig_write_fd = fds[1];

    struct sigaction sig_act{};
    sig_act.sa_handler = signal_handler;
    sigemptyset(&sig_act.sa_mask);
    sigaction(SIGINT, &sig_act, nullptr);
    sigaction(SIGTERM, &sig_act, nullptr);
}

Daemon::Daemon() {
    setup_signal_pipe();
    wayland_client_ = std::make_unique<wayland::WaylandClient>();
    layer_surface_ = std::make_unique<wayland::LayerShellSurface>(*wayland_client_);
    xkb_context_ = std::make_unique<wayland::XkbContext>("us");
    virtual_keyboard_ = std::make_unique<wayland::VirtualKeyboard>(
        wayland_client_->vk_manager(), wayland_client_->seat(), *xkb_context_
    );
    gamepad_manager_ = std::make_unique<input::GamepadManager>();
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

const core::Layout& Daemon::current_layout() const {
    switch (keyboard_state_.layer) {
    case core::Layer::Alpha:
        return *layout_alpha_;
    case core::Layer::Numeric:
        return *layout_num_;
    case core::Layer::Symbol:
        return *layout_sym_;
    }
    return *layout_alpha_;
}

void Daemon::toggle_shift() {
    switch (keyboard_state_.shift) {
    case core::ShiftState::Off:
        keyboard_state_.shift = core::ShiftState::OneShotArmed;
        break;
    case core::ShiftState::OneShotArmed:
        keyboard_state_.shift = core::ShiftState::CapsLock;
        break;
    case core::ShiftState::CapsLock:
        keyboard_state_.shift = core::ShiftState::Off;
        virtual_keyboard_->send_modifiers(0, 0, 0, 0);
        wayland_client_->flush();
        break;
    }
}

void Daemon::cycle_layer(int direction) {
    constexpr int kLayerCount = 3;
    const int current = static_cast<int>(keyboard_state_.layer);
    const int next = (current + direction + kLayerCount) % kLayerCount;
    keyboard_state_.layer = static_cast<core::Layer>(next);
    keyboard_state_.cursor = 0;
    cursor_controller_->set_layout(current_layout());
    renderer_->set_layout(current_layout(), keyboard_state_.layer);
}

void Daemon::commit_focused_key() {
    const auto& keys = current_layout().keys();
    const auto nkeys = static_cast<int>(keys.size());
    if (keyboard_state_.cursor < 0 || keyboard_state_.cursor >= nkeys) {
        return;
    }

    const core::Key& key = keys[static_cast<std::size_t>(keyboard_state_.cursor)];
    const std::string& sym = key.keysym.empty() ? key.label : key.keysym;

    // Special keys handled internally — do not forward to compositor.
    if (sym == "Shift_L") {
        toggle_shift();
        return;
    }
    if (sym == "layer_next") {
        cycle_layer(+1);
        return;
    }
    if (sym == "layer_prev") {
        cycle_layer(-1);
        return;
    }

    const uint32_t keycode = xkb_context_->keysym_to_keycode(sym);
    if (keycode == 0) {
        return;
    }

    const bool shifted = (keyboard_state_.shift != core::ShiftState::Off);
    if (shifted) {
        // Shift modifier bit 0 in the standard evdev XKB keymap.
        virtual_keyboard_->send_modifiers(1, 0, 0, 0);
        wayland_client_->flush();
    }

    virtual_keyboard_->press(keycode);
    wayland_client_->flush();
    virtual_keyboard_->release(keycode);
    wayland_client_->flush();

    if (shifted) {
        virtual_keyboard_->send_modifiers(0, 0, 0, 0);
        wayland_client_->flush();
        if (keyboard_state_.shift == core::ShiftState::OneShotArmed) {
            keyboard_state_.shift = core::ShiftState::Off;
        }
        // CapsLock persists until explicitly toggled off.
    }
}

void Daemon::run() {
    // Flush initial surface commit and wait for compositor configure.
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

    layout_alpha_ =
        std::make_unique<core::Layout>(core::Layout::load_from_file("data/layouts/qwerty.json"));
    layout_num_ = std::make_unique<core::Layout>(
        core::Layout::load_from_file("data/layouts/qwerty_num.json")
    );
    layout_sym_ = std::make_unique<core::Layout>(
        core::Layout::load_from_file("data/layouts/qwerty_sym.json")
    );

    cursor_controller_ = std::make_unique<input::CursorController>(*layout_alpha_);
    renderer_ = std::make_unique<render::Renderer>(*layout_alpha_, core::Layer::Alpha);
    renderer_->resize(layer_surface_->width(), layer_surface_->height());

    renderer_->draw(keyboard_state_);
    egl_context_->swap_buffers();
    wayland_client_->flush();

    std::array<pollfd, 3> fds{{
        {.fd = wayland_client_->display_fd(), .events = POLLIN, .revents = 0},
        {.fd = signal_pipe_read_fd_, .events = POLLIN, .revents = 0},
        {.fd = gamepad_manager_->event_fd(), .events = POLLIN, .revents = 0},
    }};

    while (true) {
        wayland_client_->flush();
        // 16 ms timeout keeps the cursor-repeat timer ticking at ~60 Hz.
        const int poll_ret = poll(fds.data(), static_cast<nfds_t>(fds.size()), 16);

        if (poll_ret < 0) {
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

        gamepad_manager_->pump();

        SDL_Event sdl_event;
        const uint64_t now_ms = SDL_GetTicks();
        while (SDL_PollEvent(&sdl_event)) {
            const auto event = input_mapper_.process(sdl_event, now_ms);

            switch (event.action) {
            case input::InputAction::SummonToggle:
                keyboard_state_.visible = !keyboard_state_.visible;
                break;

            case input::InputAction::CommitKey:
                if (keyboard_state_.visible) {
                    commit_focused_key();
                }
                break;

            case input::InputAction::ToggleShift:
                if (keyboard_state_.visible) {
                    toggle_shift();
                }
                break;

            case input::InputAction::LayerNext:
                if (keyboard_state_.visible) {
                    cycle_layer(+1);
                }
                break;

            case input::InputAction::LayerPrev:
                if (keyboard_state_.visible) {
                    cycle_layer(-1);
                }
                break;

            case input::InputAction::GamepadAdded:
                gamepad_manager_->try_open_gamepad(event.gamepad_id);
                break;

            case input::InputAction::GamepadRemoved:
                if (event.gamepad_id == gamepad_manager_->tracked_id()) {
                    gamepad_manager_->close_gamepad();
                }
                break;

            default:
                break;
            }
        }

        if (keyboard_state_.visible) {
            const auto new_cursor = cursor_controller_->update(
                input_mapper_.axis_x(),
                input_mapper_.axis_y(),
                keyboard_state_.cursor,
                SDL_GetTicks()
            );
            if (new_cursor.has_value()) {
                keyboard_state_.cursor = *new_cursor;
            }
        }

        renderer_->draw(keyboard_state_);
        egl_context_->swap_buffers();
    }
}

} // namespace thumbboard::app
