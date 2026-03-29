#include "rmlui_loading_overlay.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

namespace graphics {
namespace {

std::string formatNumber(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::string formatDp(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << "dp";
    return stream.str();
}

}  // namespace

bool RmlUiLoadingOverlay::initialize(Rml::Context& context, const std::string& documentPath) {
    shutdown();

    document_ = context.LoadDocument(documentPath);
    if (document_ == nullptr) {
        return false;
    }

    document_->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    document_->PullToFront();
    rootElement_ = document_->GetElementById("loading-overlay-root");
    logoElement_ = document_->GetElementById("loading-overlay-logo-anchor");
    apply(RmlUiLoadingOverlayState{});
    return rootElement_ != nullptr && logoElement_ != nullptr;
}

void RmlUiLoadingOverlay::shutdown() {
    rootElement_ = nullptr;
    logoElement_ = nullptr;

    if (document_ != nullptr) {
        document_->Close();
        document_ = nullptr;
    }
}

void RmlUiLoadingOverlay::apply(const RmlUiLoadingOverlayState& state) {
    if (document_ == nullptr || rootElement_ == nullptr || logoElement_ == nullptr) {
        return;
    }

    document_->PullToFront();

    if (!state.visible || state.opacity <= 0.001f) {
        rootElement_->SetProperty("display", "none");
        rootElement_->SetProperty("opacity", "0.0");
        logoElement_->SetProperty("display", "none");
        logoElement_->SetProperty("transform", "translate(0dp, 0dp)");
        return;
    }

    rootElement_->SetProperty("display", "block");
    rootElement_->SetProperty("opacity", formatNumber(std::clamp(state.opacity, 0.0f, 1.0f)));

    if (state.showLogo) {
        logoElement_->SetProperty("display", "block");
        logoElement_->SetProperty(
            "transform",
            "translate(0dp, " + formatDp(state.logoBounceOffsetY) + ")");
    } else {
        logoElement_->SetProperty("display", "none");
        logoElement_->SetProperty("transform", "translate(0dp, 0dp)");
    }
}

}  // namespace graphics
