#pragma once

#include <algorithm>
#include <iostream>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

namespace battle::render {

struct GlScreenBlitter {
    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;
    int textureWidth = 0;
    int textureHeight = 0;

    ~GlScreenBlitter() {
        destroy();
    }

    bool initialize();
    void destroy();
    void ensureTextureSize(int width, int height);
    void uploadSurface(SDL_Surface* surface);
    void draw();
};

namespace detail {

inline GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<size_t>(std::max(1, logLength)), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    std::cerr << "Shader compilation failed: " << log << "\n";
    glDeleteShader(shader);
    return 0;
}

} // namespace detail

inline bool GlScreenBlitter::initialize() {
    static const char* kVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 in_position;
        layout (location = 1) in vec2 in_uv;
        out vec2 frag_uv;
        void main() {
            frag_uv = in_uv;
            gl_Position = vec4(in_position, 0.0, 1.0);
        }
    )";

    static const char* kFragmentShader = R"(
        #version 330 core
        in vec2 frag_uv;
        uniform sampler2D scene_texture;
        out vec4 out_color;
        void main() {
            out_color = texture(scene_texture, frag_uv);
        }
    )";

    vertexShader = detail::compileShader(GL_VERTEX_SHADER, kVertexShader);
    fragmentShader = detail::compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertexShader == 0 || fragmentShader == 0) {
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<size_t>(std::max(1, logLength)), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::cerr << "Program link failed: " << log << "\n";
        destroy();
        return false;
    }

    const float quadVertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

inline void GlScreenBlitter::destroy() {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (program != 0) {
        glDeleteProgram(program);
        program = 0;
    }
    if (vertexShader != 0) {
        glDeleteShader(vertexShader);
        vertexShader = 0;
    }
    if (fragmentShader != 0) {
        glDeleteShader(fragmentShader);
        fragmentShader = 0;
    }
    textureWidth = 0;
    textureHeight = 0;
}

inline void GlScreenBlitter::ensureTextureSize(int width, int height) {
    if (textureWidth == width && textureHeight == height) {
        return;
    }

    textureWidth = width;
    textureHeight = height;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

inline void GlScreenBlitter::uploadSurface(SDL_Surface* surface) {
    if (surface == nullptr) {
        return;
    }

    ensureTextureSize(surface->w, surface->h);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface->w, surface->h, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

inline void GlScreenBlitter::draw() {
    glDisable(GL_BLEND);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "scene_texture"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

} // namespace battle::render
