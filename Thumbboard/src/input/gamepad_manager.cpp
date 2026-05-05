#include "gamepad_manager.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace thumbboard::input {

// Called from SDL's joystick background thread when events are pushed.
// Writing one byte to the pipe wakes poll() on the main thread.
bool SDLCALL GamepadManager::event_watch(void* userdata, SDL_Event* event) {
    auto* self = static_cast<GamepadManager*>(userdata);
    switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        const char byte = 1;
        (void)write(self->pipe_write_fd_, &byte, 1);
        break;
    }
    default:
        break;
    }
    return true;
}

GamepadManager::GamepadManager() {
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "thumbboard: SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    std::array<int, 2> fds{};
    if (pipe2(fds.data(), O_CLOEXEC | O_NONBLOCK) != 0) {
        std::fprintf(stderr, "thumbboard: pipe2 for SDL events failed: %s\n", std::strerror(errno));
        SDL_Quit();
        return;
    }
    pipe_read_fd_ = fds[0];
    pipe_write_fd_ = fds[1];

    SDL_AddEventWatch(event_watch, this);

    // Open the first available gamepad at startup.
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids != nullptr) {
        if (count > 0) {
            try_open_gamepad(ids[0]);
        }
        SDL_free(ids);
    }
}

GamepadManager::~GamepadManager() {
    SDL_RemoveEventWatch(event_watch, this);
    close_gamepad();
    if (pipe_write_fd_ >= 0) {
        close(pipe_write_fd_);
    }
    if (pipe_read_fd_ >= 0) {
        close(pipe_read_fd_);
    }
    SDL_Quit();
}

void GamepadManager::try_open_gamepad(uint32_t gamepad_id) {
    if (gamepad_ != nullptr) {
        return;
    }
    SDL_Gamepad* pad = SDL_OpenGamepad(static_cast<SDL_JoystickID>(gamepad_id));
    if (pad == nullptr) {
        std::fprintf(stderr, "thumbboard: SDL_OpenGamepad failed: %s\n", SDL_GetError());
        return;
    }
    gamepad_ = pad;
    tracked_id_ = gamepad_id;
    std::fprintf(stderr, "thumbboard: gamepad connected: %s\n", SDL_GetGamepadName(pad));
}

void GamepadManager::close_gamepad() {
    if (gamepad_ != nullptr) {
        std::fprintf(stderr, "thumbboard: gamepad disconnected\n");
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
        tracked_id_ = 0;
    }
}

void GamepadManager::pump() const {
    // Drain the self-pipe so poll() doesn't re-fire on the same events.
    std::array<char, 64> buf{};
    while (read(pipe_read_fd_, buf.data(), buf.size()) > 0) {
    }
    // Pump SDL's internal event sources (main-thread safe).
    SDL_PumpEvents();
}

} // namespace thumbboard::input
