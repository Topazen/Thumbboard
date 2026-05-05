#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace thumbboard::render {

class ShaderProgram {
public:
    ShaderProgram(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path);
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    void use() const;

    void set_vec2(std::string_view name, float x, float y) const;
    void set_vec4(std::string_view name, float r, float g, float b, float a) const;
    void set_float(std::string_view name, float v) const;
    void set_int(std::string_view name, int v) const;

    unsigned int id() const { return program_; }

private:
    static unsigned int compile(unsigned int stage, const std::string& src, const char* tag);

    unsigned int program_ = 0;
};

} // namespace thumbboard::render
