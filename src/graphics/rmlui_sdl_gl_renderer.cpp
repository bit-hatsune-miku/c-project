#define GL_GLEXT_PROTOTYPES

#include "rmlui_sdl_gl_renderer.h"

#if defined(VN_ENABLE_IMAGE) || defined(BATTLE_ENABLE_IMAGE)
#include <SDL2/SDL_image.h>
#endif

#include <SDL2/SDL.h>

#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>

namespace graphics {

Rml::TextureHandle RmlUiSdlGlRenderInterface::LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) {
#if defined(VN_ENABLE_IMAGE) || defined(BATTLE_ENABLE_IMAGE)
    Rml::FileInterface* fileInterface = Rml::GetFileInterface();
    if (fileInterface == nullptr) {
        return {};
    }
    Rml::FileHandle fileHandle = fileInterface->Open(source);
    if (!fileHandle) {
        return {};
    }

    fileInterface->Seek(fileHandle, 0, SEEK_END);
    const size_t bufferSize = fileInterface->Tell(fileHandle);
    fileInterface->Seek(fileHandle, 0, SEEK_SET);
    if (bufferSize == 0) {
        fileInterface->Close(fileHandle);
        return {};
    }

    using Rml::byte;
    Rml::UniquePtr<byte[]> buffer(new byte[bufferSize]);
    fileInterface->Read(buffer.get(), bufferSize, fileHandle);
    fileInterface->Close(fileHandle);

    const size_t extensionIndex = source.rfind('.');
    const Rml::String extension =
        extensionIndex == Rml::String::npos ? Rml::String() : source.substr(extensionIndex + 1);

    SDL_Surface* surface =
        IMG_LoadTyped_RW(SDL_RWFromMem(buffer.get(), static_cast<int>(bufferSize)), 1, extension.c_str());
    if (surface == nullptr) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "Could not load texture: %s", source.c_str());
        return {};
    }

    if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface* convertedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        if (convertedSurface == nullptr) {
            return {};
        }
        surface = convertedSurface;
    }

    textureDimensions = {surface->w, surface->h};

    byte* pixels = static_cast<byte*>(surface->pixels);
    const size_t pixelBytes = static_cast<size_t>(surface->w) * static_cast<size_t>(surface->h) * 4;
    for (size_t i = 0; i < pixelBytes; i += 4) {
        const byte alpha = pixels[i + 3];
        pixels[i + 0] = byte((int(pixels[i + 0]) * int(alpha)) / 255);
        pixels[i + 1] = byte((int(pixels[i + 1]) * int(alpha)) / 255);
        pixels[i + 2] = byte((int(pixels[i + 2]) * int(alpha)) / 255);
    }

    const Rml::TextureHandle textureHandle = GenerateTexture({pixels, pixelBytes}, textureDimensions);
    SDL_FreeSurface(surface);
    return textureHandle;
#else
    return RenderInterface_GL3::LoadTexture(textureDimensions, source);
#endif
}

}  // namespace graphics
