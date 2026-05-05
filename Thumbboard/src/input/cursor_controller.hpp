#pragma once

#include <cstdint>
#include <optional>

namespace thumbboard::core {
class Layout;
} // namespace thumbboard::core

namespace thumbboard::input {

// Converts gamepad left-stick axes into cell-snap cursor movement.
//
// Behavior: on first tilt the cursor moves immediately; after kInitialDelayMs
// it auto-repeats at kRepeatDelayMs. Changing direction resets the timer.
// Movement uses a nearest-neighbor spatial search over the layout's key centers,
// so it works correctly with non-uniform key sizes and staggered rows.
class CursorController {
public:
    explicit CursorController(const core::Layout& layout);

    void set_layout(const core::Layout& layout) { layout_ = &layout; }

    // Returns a new cursor index if the cursor moved, or nullopt.
    // axis_x / axis_y are raw SDL int16_t left-stick values (-32768..32767).
    // now_ms is SDL_GetTicks() or similar monotonic ms clock.
    [[nodiscard]] std::optional<int>
    update(int16_t axis_x, int16_t axis_y, int current_cursor, uint64_t now_ms);

private:
    [[nodiscard]] std::optional<int> find_neighbor(int from_idx, int move_dx, int move_dy) const;

    const core::Layout* layout_;

    int dir_x_ = 0;
    int dir_y_ = 0;
    uint64_t next_move_ms_ = 0;
    bool first_tilt_ = true;

    static constexpr float kDeadzone = 0.15F;
    static constexpr uint64_t kInitialDelayMs = 350;
    static constexpr uint64_t kRepeatDelayMs = 100;
};

} // namespace thumbboard::input
