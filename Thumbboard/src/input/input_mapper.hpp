#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

namespace thumbboard::input {

enum class InputAction : uint8_t {
    None,
    CommitKey,      // South (A/Cross): type the focused key
    SummonToggle,   // Start+Back held 250 ms: show or hide the keyboard
    ToggleShift,    // West (X/Square): cycle shift state
    LayerNext,      // Right shoulder (RB/R1): cycle layer forward
    LayerPrev,      // Left shoulder (LB/L1): cycle layer backward
    GamepadAdded,   // New gamepad connected
    GamepadRemoved, // Gamepad disconnected
};

struct InputEvent {
    InputAction action = InputAction::None;
    uint32_t gamepad_id = 0; // populated for GamepadAdded/Removed
};

// Translates raw SDL gamepad events into semantic InputEvents.
// Maintains internal state for the Start+Back summon combo.
class InputMapper {
public:
    InputMapper() = default;

    // Process one SDL event and update internal state.
    // Also checks whether the summon-combo hold time has elapsed.
    [[nodiscard]] InputEvent process(const SDL_Event& sdl_event, uint64_t now_ms);

    // Current left-stick values (updated by process()). Feed to CursorController.
    [[nodiscard]] int16_t axis_x() const {
        return axis_x_;
    }
    [[nodiscard]] int16_t axis_y() const {
        return axis_y_;
    }

private:
    void on_button_down(SDL_GamepadButton button, uint64_t now_ms, InputEvent& out);
    void on_button_up(SDL_GamepadButton button);

    int16_t axis_x_ = 0;
    int16_t axis_y_ = 0;
    bool start_held_ = false;
    bool back_held_ = false;
    uint64_t combo_start_ms_ = 0;
    bool combo_triggered_ = false;

    static constexpr uint64_t kSummonHoldMs = 250;
};

} // namespace thumbboard::input
