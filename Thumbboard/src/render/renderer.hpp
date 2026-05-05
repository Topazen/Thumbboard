#pragma once

#include <memory>

#include "../core/keyboard_state.hpp"
#include "font_atlas.hpp"
#include "mesh.hpp"
#include "shader_program.hpp"

namespace thumbboard::core {
class Layout;
} // namespace thumbboard::core

namespace thumbboard::render {

class Renderer {
public:
    explicit Renderer(const core::Layout& layout,
                      core::Layer         initial_layer = core::Layer::Alpha);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    void resize(int width, int height);

    // Switch to a different layer layout; rebuilds GPU meshes immediately.
    void set_layout(const core::Layout& layout, core::Layer layer);

    void draw(const core::KeyboardState& state);

private:
    void rebuild_meshes();
    [[nodiscard]] static std::string_view layer_label(core::Layer layer);

    const core::Layout* layout_;
    core::Layer         current_layer_ = core::Layer::Alpha;
    int                 width_         = 0;
    int                 height_        = 0;
    int                 shift_key_idx_ = -1;

    std::unique_ptr<ShaderProgram> key_shader_;
    std::unique_ptr<ShaderProgram> text_shader_;
    std::unique_ptr<FontAtlas>     atlas_;
    std::unique_ptr<FontAtlas>     alt_atlas_;

    Mesh key_mesh_;
    Mesh text_mesh_;
    Mesh alt_text_mesh_;
    Mesh pill_bg_mesh_;
    Mesh pill_text_mesh_;
};

} // namespace thumbboard::render
