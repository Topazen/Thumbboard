#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

namespace thumbboard::input {

// RAII wrapper for the SDL3 gamepad subsystem.
// Tracks the first connected gamepad and wires it into a poll(2) loop via a
// self-pipe: SDL's internal joystick thread writes to the pipe whenever a
// gamepad event is queued, waking poll() on the read end.
class GamepadManager {
public:
    GamepadManager();
    ~GamepadManager();

    GamepadManager(const GamepadManager&) = delete;
    GamepadManager& operator=(const GamepadManager&) = delete;
    GamepadManager(GamepadManager&&) = delete;
    GamepadManager& operator=(GamepadManager&&) = delete;

    // Readable fd that becomes POLLIN when gamepad events are waiting.
    // Add this to the poll(2) set.
    [[nodiscard]] int event_fd() const {
        return pipe_read_fd_;
    }

    // Drain the self-pipe and pump SDL's internal event sources.
    // Call each time poll() returns (on fd fire or timeout).
    void pump() const;

    [[nodiscard]] bool has_gamepad() const {
        return gamepad_ != nullptr;
    }
    [[nodiscard]] uint32_t tracked_id() const {
        return tracked_id_;
    }

    void try_open_gamepad(uint32_t gamepad_id);
    void close_gamepad();

private:
    static bool SDLCALL event_watch(void* userdata, SDL_Event* event);

    SDL_Gamepad* gamepad_ = nullptr;
    uint32_t tracked_id_ = 0;
    int pipe_read_fd_ = -1;
    int pipe_write_fd_ = -1;
};

} // namespace thumbboard::input
