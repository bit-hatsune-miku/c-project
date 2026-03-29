#pragma once

#include <string>

namespace Rml {
class Context;
class Element;
class ElementDocument;
}  // namespace Rml

namespace graphics {

struct RmlUiLoadingOverlayState {
    bool visible = false;
    float opacity = 0.0f;
    bool showLogo = false;
    float logoBounceOffsetY = 0.0f;
};

class RmlUiLoadingOverlay {
public:
    bool initialize(Rml::Context& context, const std::string& documentPath);
    void shutdown();
    void apply(const RmlUiLoadingOverlayState& state);

private:
    Rml::ElementDocument* document_ = nullptr;
    Rml::Element* rootElement_ = nullptr;
    Rml::Element* logoElement_ = nullptr;
};

}  // namespace graphics
