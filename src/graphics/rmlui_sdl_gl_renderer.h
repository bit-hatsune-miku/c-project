#pragma once

#include <RmlUi/Core/Core.h>

#include "RmlUi_Renderer_GL3.h"

namespace graphics {

class RmlUiSdlGlRenderInterface final : public RenderInterface_GL3 {
public:
    Rml::TextureHandle LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) override;
};

}  // namespace graphics
