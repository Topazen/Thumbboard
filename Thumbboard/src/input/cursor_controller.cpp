#include "cursor_controller.hpp"

#include <cmath>
#include <limits>

#include "../core/layout.hpp"

namespace thumbboard::input {

CursorController::CursorController(const core::Layout& layout) : layout_(&layout) {}

std::optional<int>
CursorController::update(int16_t axis_x, int16_t axis_y, int current_cursor, uint64_t now_ms) {
    const float norm_x = static_cast<float>(axis_x) / 32767.0F;
    const float norm_y = static_cast<float>(axis_y) / 32767.0F;
    const float mag = std::sqrt((norm_x * norm_x) + (norm_y * norm_y));

    if (mag < kDeadzone) {
        dir_x_ = 0;
        dir_y_ = 0;
        next_move_ms_ = 0;
        first_tilt_ = true;
        return std::nullopt;
    }

    // Snap to the dominant axis: up/down/left/right only.
    int new_dx = 0;
    int new_dy = 0;
    if (std::abs(norm_x) >= std::abs(norm_y)) {
        new_dx = (norm_x > 0.0F) ? 1 : -1;
    } else {
        new_dy = (norm_y > 0.0F) ? 1 : -1;
    }

    if (new_dx != dir_x_ || new_dy != dir_y_) {
        dir_x_ = new_dx;
        dir_y_ = new_dy;
        next_move_ms_ = 0;
        first_tilt_ = true;
    }

    if (now_ms < next_move_ms_) {
        return std::nullopt;
    }

    const bool was_first = first_tilt_;
    first_tilt_ = false;
    next_move_ms_ = now_ms + (was_first ? kInitialDelayMs : kRepeatDelayMs);

    return find_neighbor(current_cursor, dir_x_, dir_y_);
}

std::optional<int> CursorController::find_neighbor(int from_idx, int move_dx, int move_dy) const {
    const auto& keys = layout_->keys();
    const auto nkeys = static_cast<int>(keys.size());
    if (from_idx < 0 || from_idx >= nkeys) {
        return std::nullopt;
    }

    const core::Key& src = keys[static_cast<std::size_t>(from_idx)];
    const float src_cx = src.x + (src.w * 0.5F);
    const float src_cy = src.y + (src.h * 0.5F);

    const float dir_fx = static_cast<float>(move_dx);
    const float dir_fy = static_cast<float>(move_dy);

    int best_idx = -1;
    float best_score = std::numeric_limits<float>::lowest();

    for (int idx = 0; idx < nkeys; ++idx) {
        if (idx == from_idx) {
            continue;
        }
        const core::Key& cand = keys[static_cast<std::size_t>(idx)];
        const float dcx = cand.x + (cand.w * 0.5F) - src_cx;
        const float dcy = cand.y + (cand.h * 0.5F) - src_cy;
        const float dist = std::sqrt((dcx * dcx) + (dcy * dcy));

        if (dist < 0.01F) {
            continue;
        }

        // Cosine of the angle between the movement direction and the vector to the candidate.
        const float dot = ((dcx * dir_fx) + (dcy * dir_fy)) / dist;
        if (dot <= 0.0F) {
            continue; // candidate is behind or perpendicular
        }

        // Reward alignment with direction; penalise distance.
        const float score = (dot * dot) / dist;
        if (score > best_score) {
            best_score = score;
            best_idx = idx;
        }
    }

    return (best_idx >= 0) ? std::optional<int>(best_idx) : std::nullopt;
}

} // namespace thumbboard::input
