#include "gl_function_loader.h"

#include <iostream>

namespace battle::render::gl {
namespace {

Functions gFunctions;
bool gLoaded = false;

template <typename ProcType>
bool loadProc(ProcType& target, const char* name) {
    target = reinterpret_cast<ProcType>(SDL_GL_GetProcAddress(name));
    if (target != nullptr) {
        return true;
    }

    std::cerr << "Missing OpenGL entry point: " << name << "\n";
    return false;
}

} // namespace

bool ensureLoaded() {
    if (gLoaded) {
        return true;
    }

    bool success = true;
    success = loadProc(gFunctions.activeTexture, "glActiveTexture") && success;
    success = loadProc(gFunctions.attachShader, "glAttachShader") && success;
    success = loadProc(gFunctions.bindBuffer, "glBindBuffer") && success;
    success = loadProc(gFunctions.bindVertexArray, "glBindVertexArray") && success;
    success = loadProc(gFunctions.bufferData, "glBufferData") && success;
    success = loadProc(gFunctions.compileShader, "glCompileShader") && success;
    success = loadProc(gFunctions.createProgram, "glCreateProgram") && success;
    success = loadProc(gFunctions.createShader, "glCreateShader") && success;
    success = loadProc(gFunctions.deleteBuffers, "glDeleteBuffers") && success;
    success = loadProc(gFunctions.deleteProgram, "glDeleteProgram") && success;
    success = loadProc(gFunctions.deleteShader, "glDeleteShader") && success;
    success = loadProc(gFunctions.deleteVertexArrays, "glDeleteVertexArrays") && success;
    success = loadProc(gFunctions.enableVertexAttribArray, "glEnableVertexAttribArray") && success;
    success = loadProc(gFunctions.genBuffers, "glGenBuffers") && success;
    success = loadProc(gFunctions.genVertexArrays, "glGenVertexArrays") && success;
    success = loadProc(gFunctions.getProgramInfoLog, "glGetProgramInfoLog") && success;
    success = loadProc(gFunctions.getProgramiv, "glGetProgramiv") && success;
    success = loadProc(gFunctions.getShaderInfoLog, "glGetShaderInfoLog") && success;
    success = loadProc(gFunctions.getShaderiv, "glGetShaderiv") && success;
    success = loadProc(gFunctions.getUniformLocation, "glGetUniformLocation") && success;
    success = loadProc(gFunctions.linkProgram, "glLinkProgram") && success;
    success = loadProc(gFunctions.shaderSource, "glShaderSource") && success;
    success = loadProc(gFunctions.uniform1i, "glUniform1i") && success;
    success = loadProc(gFunctions.useProgram, "glUseProgram") && success;
    success = loadProc(gFunctions.vertexAttribPointer, "glVertexAttribPointer") && success;

    gLoaded = success;
    return gLoaded;
}

const Functions& get() {
    return gFunctions;
}

} // namespace battle::render::gl
