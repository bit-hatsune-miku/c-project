#include "rmlui_sdl_render_interface.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Types.h>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle::app::ui {
namespace {

void setRenderClipRect(SDL_Renderer* renderer, const SDL_Rect* rect) {
    SDL_RenderSetClipRect(renderer, rect);
}

SDL_Surface* createSurfaceFromRmlBuffer(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) {
    return SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<Rml::byte*>(source.data()),
        dimensions.x,
        dimensions.y,
        32,
        dimensions.x * 4,
        SDL_PIXELFORMAT_RGBA32
    );
}

} // namespace

RmlUiSdlRenderInterface::RmlUiSdlRenderInterface(SDL_Renderer* renderer)
    : renderer_(renderer) {
    blendMode_ = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD
    );
}

void RmlUiSdlRenderInterface::PrepareRender() {
    if (renderer_ == nullptr) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer_, blendMode_);
    setRenderClipRect(renderer_, scissorEnabled_ ? &scissorRect_ : nullptr);
}

Rml::CompiledGeometryHandle RmlUiSdlRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                     Rml::Span<const int> indices) {
    return reinterpret_cast<Rml::CompiledGeometryHandle>(new GeometryView{vertices, indices});
}

void RmlUiSdlRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                             Rml::Vector2f translation,
                                             Rml::TextureHandle texture) {
    if (renderer_ == nullptr || geometry == 0) {
        return;
    }

    const GeometryView* view = reinterpret_cast<GeometryView*>(geometry);
    const size_t numVertices = view->vertices.size();
    if (numVertices == 0) {
        return;
    }

    std::unique_ptr<SDL_Vertex[]> sdlVertices(new SDL_Vertex[numVertices]);

    for (size_t i = 0; i < numVertices; ++i) {
        const Rml::Vertex& src = view->vertices[i];
        Rml::Vector4f position(src.position.x + translation.x, src.position.y + translation.y, 0.0f, 1.0f);
        if (transform_ != nullptr) {
            position = (*transform_) * position;
            if (position.w != 0.0f && position.w != 1.0f) {
                position.x /= position.w;
                position.y /= position.w;
            }
        }

        sdlVertices[i].position = SDL_FPoint{position.x, position.y};
        sdlVertices[i].tex_coord = SDL_FPoint{src.tex_coord.x, src.tex_coord.y};
        sdlVertices[i].color = SDL_Color{
            src.colour.red,
            src.colour.green,
            src.colour.blue,
            src.colour.alpha
        };
    }

    SDL_RenderGeometry(
        renderer_,
        reinterpret_cast<SDL_Texture*>(texture),
        sdlVertices.get(),
        static_cast<int>(numVertices),
        view->indices.data(),
        static_cast<int>(view->indices.size())
    );
}

void RmlUiSdlRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
    delete reinterpret_cast<GeometryView*>(geometry);
}

Rml::TextureHandle RmlUiSdlRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
#ifdef BATTLE_ENABLE_IMAGE
    Rml::FileInterface* fileInterface = Rml::GetFileInterface();
    Rml::FileHandle fileHandle = fileInterface->Open(source);
    if (!fileHandle) {
        return {};
    }

    fileInterface->Seek(fileHandle, 0, SEEK_END);
    const size_t bufferSize = fileInterface->Tell(fileHandle);
    fileInterface->Seek(fileHandle, 0, SEEK_SET);

    using Rml::byte;
    Rml::UniquePtr<byte[]> buffer(new byte[bufferSize]);
    fileInterface->Read(buffer.get(), bufferSize, fileHandle);
    fileInterface->Close(fileHandle);

    const size_t extIndex = source.rfind('.');
    const Rml::String extension = (extIndex == Rml::String::npos ? Rml::String() : source.substr(extIndex + 1));

    SDL_Surface* surface = IMG_LoadTyped_RW(SDL_RWFromMem(buffer.get(), static_cast<int>(bufferSize)), 1, extension.c_str());
    if (surface == nullptr) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "Could not load texture: %s", source.c_str());
        return {};
    }

    if (surface->format->format != SDL_PIXELFORMAT_RGBA32 && surface->format->format != SDL_PIXELFORMAT_BGRA32) {
        SDL_Surface* convertedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        if (convertedSurface == nullptr) {
            return {};
        }
        surface = convertedSurface;
    }

    texture_dimensions = {surface->w, surface->h};

    byte* pixels = static_cast<byte*>(surface->pixels);
    const size_t pixelBytes = static_cast<size_t>(surface->w) * static_cast<size_t>(surface->h) * 4;
    for (size_t i = 0; i < pixelBytes; i += 4) {
        const byte alpha = pixels[i + 3];
        pixels[i + 0] = byte((int(pixels[i + 0]) * int(alpha)) / 255);
        pixels[i + 1] = byte((int(pixels[i + 1]) * int(alpha)) / 255);
        pixels[i + 2] = byte((int(pixels[i + 2]) * int(alpha)) / 255);
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr) {
        return {};
    }

    SDL_SetTextureBlendMode(texture, blendMode_);
    return reinterpret_cast<Rml::TextureHandle>(texture);
#else
    (void)texture_dimensions;
    (void)source;
    Rml::Log::Message(Rml::Log::LT_ERROR, "SDL_image support is required to load RmlUi textures.");
    return {};
#endif
}

Rml::TextureHandle RmlUiSdlRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                                            Rml::Vector2i source_dimensions) {
    if (renderer_ == nullptr || source.data() == nullptr ||
        source.size() != static_cast<size_t>(source_dimensions.x * source_dimensions.y * 4)) {
        return {};
    }

    SDL_Surface* surface = createSurfaceFromRmlBuffer(source, source_dimensions);
    if (surface == nullptr) {
        return {};
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr) {
        return {};
    }

    SDL_SetTextureBlendMode(texture, blendMode_);
    return reinterpret_cast<Rml::TextureHandle>(texture);
}

void RmlUiSdlRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
    SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(texture));
}

void RmlUiSdlRenderInterface::EnableScissorRegion(bool enable) {
    scissorEnabled_ = enable;
    setRenderClipRect(renderer_, scissorEnabled_ ? &scissorRect_ : nullptr);
}

void RmlUiSdlRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
    scissorRect_.x = region.Left();
    scissorRect_.y = region.Top();
    scissorRect_.w = region.Width();
    scissorRect_.h = region.Height();

    if (scissorEnabled_) {
        setRenderClipRect(renderer_, &scissorRect_);
    }
}

void RmlUiSdlRenderInterface::SetTransform(const Rml::Matrix4f* transform) {
    if (transform == nullptr) {
        transform_.reset();
        return;
    }

    transform_ = std::make_unique<Rml::Matrix4f>(*transform);
}

} // namespace battle::app::ui
