#include "mesh.hpp"

#include <GLES3/gl3.h>

namespace thumbboard::render {

Mesh::Mesh() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
}

Mesh::~Mesh() {
    if (ibo_ != 0) glDeleteBuffers(1, &ibo_);
    if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
    if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
}

void Mesh::upload(
    std::span<const float>         vertices,
    std::span<const std::uint16_t> indices,
    std::size_t                    stride_bytes,
    std::span<const Attribute>     attribs
) {
    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size_bytes()),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size_bytes()),
        indices.data(),
        GL_STATIC_DRAW
    );

    for (const Attribute& a : attribs) {
        glEnableVertexAttribArray(a.location);
        glVertexAttribPointer(
            a.location,
            a.components,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(stride_bytes),
            // NOLINTNEXTLINE(performance-no-int-to-ptr,cppcoreguidelines-pro-type-reinterpret-cast)
            reinterpret_cast<const void*>(a.offset_bytes)
        );
    }

    index_count_ = static_cast<int>(indices.size());

    glBindVertexArray(0);
}

void Mesh::draw() const {
    if (index_count_ == 0) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
}

} // namespace thumbboard::render
