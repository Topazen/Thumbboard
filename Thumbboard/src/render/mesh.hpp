#pragma once

#include <cstdint>
#include <span>

namespace thumbboard::render {

class Mesh {
public:
    struct Attribute {
        unsigned int location;
        int          components;       // 1..4
        std::size_t  offset_bytes;
    };

    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void upload(
        std::span<const float>          vertices,
        std::span<const std::uint16_t>  indices,
        std::size_t                     stride_bytes,
        std::span<const Attribute>      attribs
    );

    void draw() const;

private:
    unsigned int vao_         = 0;
    unsigned int vbo_         = 0;
    unsigned int ibo_         = 0;
    int          index_count_ = 0;
};

} // namespace thumbboard::render
