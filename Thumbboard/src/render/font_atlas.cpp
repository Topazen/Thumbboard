#include "font_atlas.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <GLES3/gl3.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace thumbboard::render {

namespace {

// Search a few well-known TTF locations. Order: env override → Inter (the
// bundled / recommended font, see ARCHITECTURE.md §2) → DejaVu Sans (almost
// always present) → Liberation Sans. We deliberately don't depend on
// FontConfig — the build target needs only one font.
std::string discover_font() {
    if (const char* env = std::getenv("THUMBBOARD_FONT"); env != nullptr && *env != '\0') {
        return env;
    }
    static const std::array<const char*, 9> candidates = {
        "/usr/share/fonts/inter/Inter-Regular.ttf",
        "/usr/share/fonts/TTF/Inter-Regular.ttf",
        "/usr/share/fonts/truetype/inter/Inter-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };
    for (const char* path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return {};
}

[[noreturn]] void die(const char* msg) {
    std::fprintf(stderr, "thumbboard: font atlas: %s\n", msg);
    std::exit(1);
}

} // namespace

FontAtlas::FontAtlas(std::string_view font_path, int pixel_size)
    : pixel_size_(pixel_size) {
    std::string path = font_path.empty() ? discover_font() : std::string(font_path);
    if (path.empty()) {
        die("no usable font found; set $THUMBBOARD_FONT");
    }

    FT_Library lib{};
    if (FT_Init_FreeType(&lib) != 0) {
        die("FT_Init_FreeType failed");
    }
    FT_Face face{};
    if (FT_New_Face(lib, path.c_str(), 0, &face) != 0) {
        std::fprintf(stderr, "thumbboard: cannot open font %s\n", path.c_str());
        std::exit(1);
    }
    FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size));

    line_height_ = static_cast<int>(face->size->metrics.height >> 6);
    ascent_      = static_cast<int>(face->size->metrics.ascender >> 6);

    // Shelf packing into a square-ish atlas. 512² is plenty for ASCII at 24 px.
    constexpr int kAtlasW = 512;
    constexpr int kAtlasH = 512;
    constexpr int kPad    = 1;        // 1-px padding to avoid bilinear bleed
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kAtlasW * kAtlasH), 0);

    int pen_x = kPad;
    int pen_y = kPad;
    int row_h = 0;

    for (std::uint32_t cp = 0x20; cp <= 0x7E; ++cp) {
        if (FT_Load_Char(face, cp, FT_LOAD_RENDER) != 0) {
            continue;
        }
        const FT_Bitmap& bmp = face->glyph->bitmap;
        const int gw = static_cast<int>(bmp.width);
        const int gh = static_cast<int>(bmp.rows);

        if (pen_x + gw + kPad > kAtlasW) {
            pen_x  = kPad;
            pen_y += row_h + kPad;
            row_h  = 0;
        }
        if (pen_y + gh + kPad > kAtlasH) {
            // Out of room — extend with future codepoints would need a bigger atlas,
            // but ASCII at 24 px fits comfortably. Treat as fatal so we notice.
            die("atlas overflow during ASCII rasterisation");
        }

        for (int row = 0; row < gh; ++row) {
            const std::uint8_t* src = bmp.buffer + (row * bmp.pitch);
            std::uint8_t* dst = pixels.data() + ((pen_y + row) * kAtlasW) + pen_x;
            std::memcpy(dst, src, static_cast<std::size_t>(gw));
        }

        Glyph g;
        g.w = gw;
        g.h = gh;
        g.u0 = static_cast<float>(pen_x)        / static_cast<float>(kAtlasW);
        g.v0 = static_cast<float>(pen_y)        / static_cast<float>(kAtlasH);
        g.u1 = static_cast<float>(pen_x + gw)   / static_cast<float>(kAtlasW);
        g.v1 = static_cast<float>(pen_y + gh)   / static_cast<float>(kAtlasH);
        g.bearing_x = face->glyph->bitmap_left;
        g.bearing_y = face->glyph->bitmap_top;
        g.advance_x = static_cast<int>(face->glyph->advance.x >> 6);
        glyphs_.emplace(cp, g);

        pen_x += gw + kPad;
        if (gh > row_h) {
            row_h = gh;
        }
    }

    FT_Done_Face(face);
    FT_Done_FreeType(lib);

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_R8, kAtlasW, kAtlasH, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data()
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    atlas_w_ = kAtlasW;
    atlas_h_ = kAtlasH;
}

FontAtlas::~FontAtlas() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
    }
}

void FontAtlas::bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture_);
}

const Glyph* FontAtlas::find(std::uint32_t codepoint) const {
    auto it = glyphs_.find(codepoint);
    return it == glyphs_.end() ? nullptr : &it->second;
}

int FontAtlas::measure(std::string_view text) const {
    int w = 0;
    for (char c : text) {
        const Glyph* g = find(static_cast<std::uint32_t>(static_cast<unsigned char>(c)));
        if (g != nullptr) {
            w += g->advance_x;
        }
    }
    return w;
}

} // namespace thumbboard::render
