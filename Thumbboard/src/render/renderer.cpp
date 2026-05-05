#include "renderer.hpp"

#include <GLES3/gl3.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "../core/keyboard_state.hpp"
#include "../core/layout.hpp"

namespace thumbboard::render {

namespace {

// Default theme constants (per docs/GUI.md §4 default.json). M7 will load
// these from JSON; for M2-M4 they are hardcoded so the demo stands on its own.
constexpr float kBgR = 0.055F;
constexpr float kBgG = 0.090F;
constexpr float kBgB = 0.130F;
constexpr float kBgA = 0.878F;                                     // 0xE0

constexpr std::array kKeyFill    = {0.102F, 0.145F, 0.200F, 1.0F}; // #1A2533
constexpr std::array kGlyphColor = {0.902F, 0.929F, 0.953F, 1.0F}; // #E6EDF3

// Shift-key highlight colors (one-shot vs. caps-lock).
constexpr std::array kShiftArmedFill = {0.24F, 0.42F, 0.55F, 1.0F}; // #3D6B8C
constexpr std::array kShiftCapsLockFill = {0.35F, 0.62F, 0.80F, 1.0F}; // #599ECC

constexpr float kKeyRadiusPx   = 12.0F;
constexpr float kKeyTopHighlight = 0.08F;

constexpr int kMarginPx    = 12; // left / right / bottom
constexpr int kTopMarginPx = 20; // top — extra space for the layer pill
constexpr int kKeyGapPx    = 8;
constexpr int kFontPixelSz = 24;
constexpr int kAltFontSz   = 13; // alt-glyph corner labels + layer pill
constexpr int kAltMarginPx = 3;  // padding between alt-glyph and key edge

// Resolve a data file relative to a few search roots.
std::filesystem::path data_path(const char* relative) {
    namespace fs = std::filesystem;
    if (const char* env = std::getenv("THUMBBOARD_DATA_DIR"); env != nullptr && *env != '\0') {
        fs::path candidate = fs::path(env) / relative;
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    for (const char* root : {"./data", "../data", "../../data"}) {
        fs::path candidate = fs::path(root) / relative;
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    return fs::path("./data") / relative;
}

struct PixelRect {
    float x = 0.0F;
    float y = 0.0F;
    float w = 0.0F;
    float h = 0.0F;
};

PixelRect pixel_rect_for_key(
    const core::Key& key, float cell_w, float cell_h, float origin_x, float origin_y
) {
    const float step_x = cell_w + static_cast<float>(kKeyGapPx);
    const float step_y = cell_h + static_cast<float>(kKeyGapPx);
    PixelRect rect;
    rect.x = origin_x + (key.x * step_x);
    rect.y = origin_y + (key.y * step_y);
    rect.w = (key.w * step_x) - static_cast<float>(kKeyGapPx);
    rect.h = (key.h * step_y) - static_cast<float>(kKeyGapPx);
    return rect;
}

} // namespace

Renderer::Renderer(const core::Layout& layout, core::Layer initial_layer)
    : layout_(&layout), current_layer_(initial_layer) {
    key_shader_ = std::make_unique<ShaderProgram>(
        data_path("shaders/key.vert"), data_path("shaders/key.frag")
    );
    text_shader_ = std::make_unique<ShaderProgram>(
        data_path("shaders/text.vert"), data_path("shaders/text.frag")
    );
    atlas_     = std::make_unique<FontAtlas>(std::string_view{}, kFontPixelSz);
    alt_atlas_ = std::make_unique<FontAtlas>(std::string_view{}, kAltFontSz);
}

Renderer::~Renderer() = default;

void Renderer::resize(int width, int height) {
    if (width == width_ && height == height_) {
        return;
    }
    width_  = width;
    height_ = height;
    glViewport(0, 0, width_, height_);
    rebuild_meshes();
}

void Renderer::set_layout(const core::Layout& layout, core::Layer layer) {
    layout_        = &layout;
    current_layer_ = layer;
    rebuild_meshes();
}

std::string_view Renderer::layer_label(core::Layer layer) {
    switch (layer) {
    case core::Layer::Alpha:   return "abc";
    case core::Layer::Numeric: return "123";
    case core::Layer::Symbol:  return "!#$";
    }
    return "abc";
}

void Renderer::rebuild_meshes() {
    if (width_ <= 0 || height_ <= 0 || layout_->cols() <= 0 || layout_->rows() <= 0) {
        return;
    }

    const int cols = layout_->cols();
    const int rows = layout_->rows();

    const float avail_w =
        static_cast<float>(width_ - (2 * kMarginPx) - ((cols - 1) * kKeyGapPx));
    const float avail_h =
        static_cast<float>(height_ - kTopMarginPx - kMarginPx - ((rows - 1) * kKeyGapPx));
    const float cell_w   = avail_w / static_cast<float>(cols);
    const float cell_h   = avail_h / static_cast<float>(rows);
    const float origin_x = static_cast<float>(kMarginPx);
    const float origin_y = static_cast<float>(kTopMarginPx);

    // Shared helper: append a pair of triangles for a quad.
    auto push_quad = [](std::vector<std::uint16_t>& idx, std::uint16_t base) {
        idx.push_back(base + 0);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
        idx.push_back(base + 0);
        idx.push_back(base + 2);
        idx.push_back(base + 3);
    };

    // ---- Key background mesh: 4 verts × 7 floats per key ----
    constexpr int kKFloats = 7; // pos.xy, local.xy, size_px.xy, key_idx
    std::vector<float>          kverts;
    std::vector<std::uint16_t>  kidx;
    kverts.reserve(layout_->keys().size() * 4 * static_cast<std::size_t>(kKFloats));
    kidx.reserve(layout_->keys().size() * 6);

    int key_index = 0;
    for (const core::Key& key : layout_->keys()) {
        const PixelRect rect = pixel_rect_for_key(key, cell_w, cell_h, origin_x, origin_y);
        const float xmin = std::round(rect.x);
        const float ymin = std::round(rect.y);
        const float xmax = std::round(rect.x + rect.w);
        const float ymax = std::round(rect.y + rect.h);
        const float kw   = xmax - xmin;
        const float kh   = ymax - ymin;
        const float kidxf = static_cast<float>(key_index);

        const auto base = static_cast<std::uint16_t>(kverts.size() / static_cast<std::size_t>(kKFloats));
        kverts.insert(kverts.end(), {xmin, ymin, -1.0F, -1.0F, kw, kh, kidxf});
        kverts.insert(kverts.end(), {xmax, ymin,  1.0F, -1.0F, kw, kh, kidxf});
        kverts.insert(kverts.end(), {xmax, ymax,  1.0F,  1.0F, kw, kh, kidxf});
        kverts.insert(kverts.end(), {xmin, ymax, -1.0F,  1.0F, kw, kh, kidxf});
        push_quad(kidx, base);
        ++key_index;
    }

    static const Mesh::Attribute kKeyAttribs[] = {
        {0, 2, 0},                               // a_pos
        {1, 2, sizeof(float) * 2},               // a_local
        {2, 2, sizeof(float) * 4},               // a_size_px
        {3, 1, sizeof(float) * 6},               // a_key_idx
    };
    key_mesh_.upload(kverts, kidx, sizeof(float) * kKFloats, kKeyAttribs);

    // ---- Text mesh: 4 verts × (pos.xy, uv.xy) per glyph ----
    constexpr int kTFloats = 4;
    static const Mesh::Attribute kTextAttribs[] = {
        {0, 2, 0},                 // a_pos
        {1, 2, sizeof(float) * 2}, // a_uv
    };

    std::vector<float>         tverts;
    std::vector<std::uint16_t> tidx;

    for (const core::Key& key : layout_->keys()) {
        if (key.label.empty() || key.label == " ") {
            continue;
        }
        const PixelRect rect = pixel_rect_for_key(key, cell_w, cell_h, origin_x, origin_y);
        const float cx = rect.x + (rect.w * 0.5F);
        const float cy = rect.y + (rect.h * 0.5F);

        const int text_w = atlas_->measure(key.label);
        const float baseline =
            cy + (static_cast<float>(kFontPixelSz) * 0.35F);
        float pen_x = cx - (static_cast<float>(text_w) * 0.5F);

        for (char ch : key.label) {
            const Glyph* glyph =
                atlas_->find(static_cast<std::uint32_t>(static_cast<unsigned char>(ch)));
            if (glyph == nullptr) {
                continue;
            }
            if (glyph->w > 0 && glyph->h > 0) {
                const float gx0 =
                    std::round(pen_x + static_cast<float>(glyph->bearing_x));
                const float gy0 =
                    std::round(baseline - static_cast<float>(glyph->bearing_y));
                const float gx1 = gx0 + static_cast<float>(glyph->w);
                const float gy1 = gy0 + static_cast<float>(glyph->h);
                const auto base =
                    static_cast<std::uint16_t>(tverts.size() / static_cast<std::size_t>(kTFloats));
                tverts.insert(tverts.end(), {gx0, gy0, glyph->u0, glyph->v0});
                tverts.insert(tverts.end(), {gx1, gy0, glyph->u1, glyph->v0});
                tverts.insert(tverts.end(), {gx1, gy1, glyph->u1, glyph->v1});
                tverts.insert(tverts.end(), {gx0, gy1, glyph->u0, glyph->v1});
                push_quad(tidx, base);
            }
            pen_x += static_cast<float>(glyph->advance_x);
        }
    }
    text_mesh_.upload(tverts, tidx, sizeof(float) * kTFloats, kTextAttribs);

    // ---- Alt-glyph mesh: top-right corner of each key, using alt_atlas_ ----
    std::vector<float>         averts;
    std::vector<std::uint16_t> aidx;

    for (const core::Key& key : layout_->keys()) {
        if (key.alt_label.empty()) {
            continue;
        }
        const PixelRect rect = pixel_rect_for_key(key, cell_w, cell_h, origin_x, origin_y);
        const int  alt_w   = alt_atlas_->measure(key.alt_label);
        const float text_right  =
            rect.x + rect.w - static_cast<float>(kAltMarginPx);
        float pen_ax = text_right - static_cast<float>(alt_w);
        const float baseline_ay =
            rect.y + static_cast<float>(kAltMarginPx) +
            static_cast<float>(kAltFontSz) * 0.85F;

        for (char ch : key.alt_label) {
            const Glyph* glyph =
                alt_atlas_->find(static_cast<std::uint32_t>(static_cast<unsigned char>(ch)));
            if (glyph == nullptr) {
                continue;
            }
            if (glyph->w > 0 && glyph->h > 0) {
                const float gx0 =
                    std::round(pen_ax + static_cast<float>(glyph->bearing_x));
                const float gy0 =
                    std::round(baseline_ay - static_cast<float>(glyph->bearing_y));
                const float gx1 = gx0 + static_cast<float>(glyph->w);
                const float gy1 = gy0 + static_cast<float>(glyph->h);
                const auto base =
                    static_cast<std::uint16_t>(averts.size() / static_cast<std::size_t>(kTFloats));
                averts.insert(averts.end(), {gx0, gy0, glyph->u0, glyph->v0});
                averts.insert(averts.end(), {gx1, gy0, glyph->u1, glyph->v0});
                averts.insert(averts.end(), {gx1, gy1, glyph->u1, glyph->v1});
                averts.insert(averts.end(), {gx0, gy1, glyph->u0, glyph->v1});
                push_quad(aidx, base);
            }
            pen_ax += static_cast<float>(glyph->advance_x);
        }
    }
    alt_text_mesh_.upload(averts, aidx, sizeof(float) * kTFloats, kTextAttribs);

    // ---- Layer pill background (top-left margin area) ----
    const std::string_view pill_lbl = layer_label(current_layer_);
    const int   pill_tw = alt_atlas_->measure(pill_lbl);
    constexpr int kPillPadH = 6;
    constexpr int kPillPadV = 2;
    const float pill_w  = static_cast<float>(pill_tw + kPillPadH * 2);
    const float pill_h  = static_cast<float>(kAltFontSz + kPillPadV * 2);
    const float pill_x  = static_cast<float>(kMarginPx);
    const float pill_y  =
        (static_cast<float>(kTopMarginPx) - pill_h) * 0.5F;  // centre in top margin

    const float px0 = std::round(pill_x);
    const float py0 = std::round(pill_y);
    const float px1 = std::round(pill_x + pill_w);
    const float py1 = std::round(pill_y + pill_h);
    const float pw  = px1 - px0;
    const float ph  = py1 - py0;

    // Use key_idx = -2 so it is never selected as cursor or shift key.
    constexpr float kPillKeyIdx = -2.0F;
    const std::vector<float> pverts = {
        px0, py0, -1.0F, -1.0F, pw, ph, kPillKeyIdx,
        px1, py0,  1.0F, -1.0F, pw, ph, kPillKeyIdx,
        px1, py1,  1.0F,  1.0F, pw, ph, kPillKeyIdx,
        px0, py1, -1.0F,  1.0F, pw, ph, kPillKeyIdx,
    };
    const std::vector<std::uint16_t> pidx = {0, 1, 2, 0, 2, 3};
    pill_bg_mesh_.upload(pverts, pidx, sizeof(float) * kKFloats, kKeyAttribs);

    // ---- Layer pill text (centred inside the pill) ----
    std::vector<float>         ptverts;
    std::vector<std::uint16_t> ptidx;

    float pen_px =
        px0 + static_cast<float>(kPillPadH);
    const float baseline_py =
        py0 + static_cast<float>(kPillPadV) + static_cast<float>(kAltFontSz) * 0.82F;

    for (char ch : pill_lbl) {
        const Glyph* glyph =
            alt_atlas_->find(static_cast<std::uint32_t>(static_cast<unsigned char>(ch)));
        if (glyph == nullptr) {
            continue;
        }
        if (glyph->w > 0 && glyph->h > 0) {
            const float gx0 =
                std::round(pen_px + static_cast<float>(glyph->bearing_x));
            const float gy0 =
                std::round(baseline_py - static_cast<float>(glyph->bearing_y));
            const float gx1 = gx0 + static_cast<float>(glyph->w);
            const float gy1 = gy0 + static_cast<float>(glyph->h);
            const auto base =
                static_cast<std::uint16_t>(ptverts.size() / static_cast<std::size_t>(kTFloats));
            ptverts.insert(ptverts.end(), {gx0, gy0, glyph->u0, glyph->v0});
            ptverts.insert(ptverts.end(), {gx1, gy0, glyph->u1, glyph->v0});
            ptverts.insert(ptverts.end(), {gx1, gy1, glyph->u1, glyph->v1});
            ptverts.insert(ptverts.end(), {gx0, gy1, glyph->u0, glyph->v1});
            push_quad(ptidx, base);
        }
        pen_px += static_cast<float>(glyph->advance_x);
    }
    pill_text_mesh_.upload(ptverts, ptidx, sizeof(float) * kTFloats, kTextAttribs);

    // ---- Cache shift key index for the current layout ----
    shift_key_idx_ = -1;
    int scan_idx   = 0;
    for (const auto& key : layout_->keys()) {
        if (key.keysym == "Shift_L") {
            shift_key_idx_ = scan_idx;
            break;
        }
        ++scan_idx;
    }
}

void Renderer::draw(const core::KeyboardState& state) {
    if (!state.visible) {
        glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    glClearColor(kBgR, kBgG, kBgB, kBgA);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float res_x = static_cast<float>(width_);
    const float res_y = static_cast<float>(height_);

    // Choose shift-key highlight colour (or -1 to disable).
    std::array<float, 4> shift_fill{};
    int active_shift_idx = -1;
    if (state.shift == core::ShiftState::OneShotArmed) {
        shift_fill       = kShiftArmedFill;
        active_shift_idx = shift_key_idx_;
    } else if (state.shift == core::ShiftState::CapsLock) {
        shift_fill       = kShiftCapsLockFill;
        active_shift_idx = shift_key_idx_;
    }

    // --- Key backgrounds ---
    key_shader_->use();
    key_shader_->set_vec2("u_resolution",          res_x, res_y);
    key_shader_->set_vec4("u_fill",                kKeyFill[0], kKeyFill[1], kKeyFill[2], kKeyFill[3]);
    key_shader_->set_float("u_radius_px",          kKeyRadiusPx);
    key_shader_->set_float("u_top_highlight_alpha",kKeyTopHighlight);
    key_shader_->set_int("u_cursor_key_idx",       state.cursor);
    key_shader_->set_vec4("u_outline_color",       1.0F, 1.0F, 1.0F, 1.0F);
    key_shader_->set_int("u_shift_key_idx",        active_shift_idx);
    key_shader_->set_vec4("u_shift_fill",          shift_fill[0], shift_fill[1], shift_fill[2], shift_fill[3]);
    key_mesh_.draw();

    // --- Layer pill background (different fill, small radius, no cursor/shift highlight) ---
    constexpr std::array kPillFill = {0.12F, 0.20F, 0.30F, 0.95F};
    key_shader_->set_vec4("u_fill",                kPillFill[0], kPillFill[1], kPillFill[2], kPillFill[3]);
    key_shader_->set_float("u_radius_px",          4.0F);
    key_shader_->set_float("u_top_highlight_alpha",0.0F);
    key_shader_->set_int("u_cursor_key_idx",       -1);
    key_shader_->set_int("u_shift_key_idx",        -1);
    pill_bg_mesh_.draw();

    // --- Key labels (main atlas, full brightness) ---
    text_shader_->use();
    text_shader_->set_vec2("u_resolution", res_x, res_y);
    text_shader_->set_vec4("u_color", kGlyphColor[0], kGlyphColor[1], kGlyphColor[2], kGlyphColor[3]);
    text_shader_->set_int("u_atlas", 0);
    atlas_->bind(0);
    text_mesh_.draw();

    // --- Alt-glyph labels (alt atlas, dimmed) ---
    constexpr float kAltDim = 0.55F;
    text_shader_->set_vec4(
        "u_color",
        kGlyphColor[0] * kAltDim,
        kGlyphColor[1] * kAltDim,
        kGlyphColor[2] * kAltDim,
        kGlyphColor[3]
    );
    alt_atlas_->bind(0);
    alt_text_mesh_.draw();

    // --- Layer pill text (alt atlas, full brightness) ---
    text_shader_->set_vec4("u_color", kGlyphColor[0], kGlyphColor[1], kGlyphColor[2], kGlyphColor[3]);
    // alt_atlas_ is already bound to unit 0
    pill_text_mesh_.draw();
}

} // namespace thumbboard::render
