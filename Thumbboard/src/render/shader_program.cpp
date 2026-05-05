#include "shader_program.hpp"

#include <GLES3/gl3.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace thumbboard::render {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("shader: cannot open " + path.string());
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

unsigned int ShaderProgram::compile(unsigned int stage, const std::string& src, const char* tag) {
    GLuint shader = glCreateShader(stage);
    const char* c_src = src.c_str();
    glShaderSource(shader, 1, &c_src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<std::size_t>(len), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        glDeleteShader(shader);
        std::fprintf(stderr, "thumbboard: %s shader compile failed:\n%s\n", tag, log.c_str());
        std::exit(1);
    }
    return shader;
}

ShaderProgram::ShaderProgram(
    const std::filesystem::path& vert_path, const std::filesystem::path& frag_path
) {
    const std::string vsrc = read_file(vert_path);
    const std::string fsrc = read_file(frag_path);

    GLuint vs = compile(GL_VERTEX_SHADER,   vsrc, "vertex");
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc, "fragment");

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<std::size_t>(len), '\0');
        glGetProgramInfoLog(program_, len, nullptr, log.data());
        glDeleteProgram(program_);
        std::fprintf(stderr, "thumbboard: program link failed:\n%s\n", log.c_str());
        std::exit(1);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

ShaderProgram::~ShaderProgram() {
    if (program_ != 0) {
        glDeleteProgram(program_);
    }
}

void ShaderProgram::use() const {
    glUseProgram(program_);
}

void ShaderProgram::set_vec2(std::string_view name, float x, float y) const {
    glUniform2f(glGetUniformLocation(program_, std::string(name).c_str()), x, y);
}

void ShaderProgram::set_vec4(std::string_view name, float r, float g, float b, float a) const {
    glUniform4f(glGetUniformLocation(program_, std::string(name).c_str()), r, g, b, a);
}

void ShaderProgram::set_float(std::string_view name, float v) const {
    glUniform1f(glGetUniformLocation(program_, std::string(name).c_str()), v);
}

void ShaderProgram::set_int(std::string_view name, int v) const {
    glUniform1i(glGetUniformLocation(program_, std::string(name).c_str()), v);
}

} // namespace thumbboard::render
