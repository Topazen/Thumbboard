#include "input_mapper.hpp"

namespace thumbboard::input {

InputEvent InputMapper::process(const SDL_Event& sdl_event, uint64_t now_ms) {
    InputEvent result;

    switch (sdl_event.type) {
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (sdl_event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            axis_x_ = sdl_event.gaxis.value;
        }
        if (sdl_event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            axis_y_ = sdl_event.gaxis.value;
        }
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        on_button_down(static_cast<SDL_GamepadButton>(sdl_event.gbutton.button), now_ms, result);
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        on_button_up(static_cast<SDL_GamepadButton>(sdl_event.gbutton.button));
        break;

    case SDL_EVENT_GAMEPAD_ADDED:
        result.action = InputAction::GamepadAdded;
        result.gamepad_id = static_cast<uint32_t>(sdl_event.gdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        result.action = InputAction::GamepadRemoved;
        result.gamepad_id = static_cast<uint32_t>(sdl_event.gdevice.which);
        break;

    default:
        break;
    }

    // Check summon combo (Start + Back held for kSummonHoldMs).
    // Takes priority over other actions: if the combo fires, replace result.
    if (!combo_triggered_ && start_held_ && back_held_) {
        if ((now_ms - combo_start_ms_) >= kSummonHoldMs) {
            combo_triggered_ = true;
            result.action = InputAction::SummonToggle;
        }
    }

    return result;
}

void InputMapper::on_button_down(SDL_GamepadButton button, uint64_t now_ms, InputEvent& out) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        out.action = InputAction::CommitKey;
        break;

    case SDL_GAMEPAD_BUTTON_WEST:
        out.action = InputAction::ToggleShift;
        break;

    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        out.action = InputAction::LayerNext;
        break;

    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        out.action = InputAction::LayerPrev;
        break;

    case SDL_GAMEPAD_BUTTON_START:
        start_held_ = true;
        if (back_held_ && !combo_triggered_) {
            combo_start_ms_ = now_ms;
        }
        break;

    case SDL_GAMEPAD_BUTTON_BACK:
        back_held_ = true;
        if (start_held_ && !combo_triggered_) {
            combo_start_ms_ = now_ms;
        }
        break;

    default:
        break;
    }
}

void InputMapper::on_button_up(SDL_GamepadButton button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_START:
        start_held_ = false;
        combo_triggered_ = false;
        break;

    case SDL_GAMEPAD_BUTTON_BACK:
        back_held_ = false;
        combo_triggered_ = false;
        break;

    default:
        break;
    }
}

} // namespace thumbboard::input
