#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace thumbboard::render {

struct Glyph {
    float u0 = 0.0F, v0 = 0.0F;
    float u1 = 0.0F, v1 = 0.0F;
    int   w  = 0,    h  = 0;          // bitmap size in pixels
    int   bearing_x = 0, bearing_y = 0;
    int   advance_x = 0;              // pixels, integer (FreeType units >> 6)
};

class FontAtlas {
public:
    // Rasterizes printable ASCII (0x20..0x7E) at `pixel_size` into a single
    // GL_R8 texture. `font_path` empty → discover via search of well-known
    // system fonts (Inter / DejaVu Sans).
    FontAtlas(std::string_view font_path, int pixel_size);
    ~FontAtlas();

    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    void bind(unsigned int unit = 0) const;

    const Glyph* find(std::uint32_t codepoint) const;

    int          pixel_size()  const { return pixel_size_; }
    int          line_height() const { return line_height_; }
    int          ascent()      const { return ascent_; }
    int          atlas_w()     const { return atlas_w_; }
    int          atlas_h()     const { return atlas_h_; }
    unsigned int texture()     const { return texture_; }

    // Total advance for an ASCII string (no shaping; M2 only).
    int measure(std::string_view text) const;

private:
    std::unordered_map<std::uint32_t, Glyph> glyphs_;
    unsigned int texture_     = 0;
    int          atlas_w_     = 0;
    int          atlas_h_     = 0;
    int          pixel_size_  = 0;
    int          line_height_ = 0;
    int          ascent_      = 0;
};

} // namespace thumbboard::render
