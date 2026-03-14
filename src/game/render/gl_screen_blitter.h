#pragma once

#include <algorithm>
#include <iostream>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "gl_function_loader.h"

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
    const auto& gl = battle::render::gl::get();
    GLuint shader = gl.createShader(type);
    gl.shaderSource(shader, 1, &source, nullptr);
    gl.compileShader(shader);

    GLint status = GL_FALSE;
    gl.getShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    gl.getShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<size_t>(std::max(1, logLength)), '\0');
    gl.getShaderInfoLog(shader, logLength, nullptr, log.data());
    std::cerr << "Shader compilation failed: " << log << "\n";
    gl.deleteShader(shader);
    return 0;
}

} // namespace detail

inline bool GlScreenBlitter::initialize() {
    if (!battle::render::gl::ensureLoaded()) {
        return false;
    }

    const auto& gl = battle::render::gl::get();
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

    program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);

    GLint linkStatus = GL_FALSE;
    gl.getProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        GLint logLength = 0;
        gl.getProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<size_t>(std::max(1, logLength)), '\0');
        gl.getProgramInfoLog(program, logLength, nullptr, log.data());
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

    gl.genVertexArrays(1, &vao);
    gl.genBuffers(1, &vbo);
    gl.bindVertexArray(vao);
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.bufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    gl.bindVertexArray(0);

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
    const auto& gl = battle::render::gl::get();
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (vbo != 0) {
        gl.deleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao != 0) {
        gl.deleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (program != 0) {
        gl.deleteProgram(program);
        program = 0;
    }
    if (vertexShader != 0) {
        gl.deleteShader(vertexShader);
        vertexShader = 0;
    }
    if (fragmentShader != 0) {
        gl.deleteShader(fragmentShader);
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
    const auto& gl = battle::render::gl::get();
    glDisable(GL_BLEND);
    gl.useProgram(program);
    gl.activeTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    gl.uniform1i(gl.getUniformLocation(program, "scene_texture"), 0);
    gl.bindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl.bindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    gl.useProgram(0);
}

} // namespace battle::render
