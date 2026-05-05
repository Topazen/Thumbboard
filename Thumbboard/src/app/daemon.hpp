#pragma once

#include <memory>

#include "../core/keyboard_state.hpp"
#include "../input/input_mapper.hpp"

namespace thumbboard::wayland {
class WaylandClient;
class LayerShellSurface;
class XkbContext;
class VirtualKeyboard;
} // namespace thumbboard::wayland

namespace thumbboard::core {
class Layout;
} // namespace thumbboard::core

namespace thumbboard::render {
class EglContext;
class Renderer;
} // namespace thumbboard::render

namespace thumbboard::input {
class GamepadManager;
class CursorController;
} // namespace thumbboard::input

namespace thumbboard::app {

class Daemon {
public:
    Daemon();
    ~Daemon();

    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;
    Daemon(Daemon&&) = delete;
    Daemon& operator=(Daemon&&) = delete;

    void run();

private:
    static void signal_handler(int signum);
    void setup_signal_pipe();

    void commit_focused_key();
    void toggle_shift();
    void cycle_layer(int direction); // +1 = next, -1 = prev
    [[nodiscard]] const core::Layout& current_layout() const;

    // Declaration order determines destruction order (reverse).
    std::unique_ptr<wayland::WaylandClient> wayland_client_;
    std::unique_ptr<wayland::LayerShellSurface> layer_surface_;
    std::unique_ptr<render::EglContext> egl_context_;
    std::unique_ptr<core::Layout> layout_alpha_;
    std::unique_ptr<core::Layout> layout_num_;
    std::unique_ptr<core::Layout> layout_sym_;
    std::unique_ptr<render::Renderer> renderer_;
    std::unique_ptr<wayland::XkbContext> xkb_context_;
    std::unique_ptr<wayland::VirtualKeyboard> virtual_keyboard_;
    std::unique_ptr<input::GamepadManager> gamepad_manager_;
    std::unique_ptr<input::CursorController> cursor_controller_;
    core::KeyboardState keyboard_state_;
    input::InputMapper input_mapper_;

    int signal_pipe_read_fd_ = -1;
    int signal_pipe_write_fd_ = -1;
};

} // namespace thumbboard::app
