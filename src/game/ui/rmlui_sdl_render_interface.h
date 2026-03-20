#pragma once

#include <memory>

#include <SDL2/SDL.h>

#include <RmlUi/Core/Matrix4.h>
#include <RmlUi/Core/RenderInterface.h>

namespace battle::app::ui {

class RmlUiSdlRenderInterface final : public Rml::RenderInterface {
public:
    explicit RmlUiSdlRenderInterface(SDL_Renderer* renderer);

    void PrepareRender();

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void SetTransform(const Rml::Matrix4f* transform) override;

private:
    struct GeometryView {
        Rml::Span<const Rml::Vertex> vertices;
        Rml::Span<const int> indices;
    };

    SDL_Renderer* renderer_ = nullptr;
    SDL_BlendMode blendMode_ = SDL_BLENDMODE_BLEND;
    SDL_Rect scissorRect_{0, 0, 0, 0};
    bool scissorEnabled_ = false;
    std::unique_ptr<Rml::Matrix4f> transform_;
};

} // namespace battle::app::ui
