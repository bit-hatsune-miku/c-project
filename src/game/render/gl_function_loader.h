#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

namespace battle::render::gl {

struct Functions {
    PFNGLACTIVETEXTUREPROC activeTexture = nullptr;
    PFNGLATTACHSHADERPROC attachShader = nullptr;
    PFNGLBINDBUFFERPROC bindBuffer = nullptr;
    PFNGLBINDVERTEXARRAYPROC bindVertexArray = nullptr;
    PFNGLBUFFERDATAPROC bufferData = nullptr;
    PFNGLCOMPILESHADERPROC compileShader = nullptr;
    PFNGLCREATEPROGRAMPROC createProgram = nullptr;
    PFNGLCREATESHADERPROC createShader = nullptr;
    PFNGLDELETEBUFFERSPROC deleteBuffers = nullptr;
    PFNGLDELETEPROGRAMPROC deleteProgram = nullptr;
    PFNGLDELETESHADERPROC deleteShader = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC deleteVertexArrays = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC enableVertexAttribArray = nullptr;
    PFNGLGENBUFFERSPROC genBuffers = nullptr;
    PFNGLGENVERTEXARRAYSPROC genVertexArrays = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC getProgramInfoLog = nullptr;
    PFNGLGETPROGRAMIVPROC getProgramiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC getShaderInfoLog = nullptr;
    PFNGLGETSHADERIVPROC getShaderiv = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC getUniformLocation = nullptr;
    PFNGLLINKPROGRAMPROC linkProgram = nullptr;
    PFNGLSHADERSOURCEPROC shaderSource = nullptr;
    PFNGLUNIFORM1IPROC uniform1i = nullptr;
    PFNGLUSEPROGRAMPROC useProgram = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC vertexAttribPointer = nullptr;
};

bool ensureLoaded();
const Functions& get();

} // namespace battle::render::gl
