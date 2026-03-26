#include "front_ui_story.h"

#include <filesystem>
#include <functional>
#include <utility>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include "../platform/path_resolution.h"
#include "../game/vn/vn_system.h"

namespace graphics::frontui {
namespace {

class CallbackEventListener final : public Rml::EventListener {
public:
    explicit CallbackEventListener(std::function<void(Rml::Event&)> callback)
        : callback_(std::move(callback)) {}

    void ProcessEvent(Rml::Event& event) override {
        if (callback_) {
            callback_(event);
        }
    }

private:
    std::function<void(Rml::Event&)> callback_;
};

std::string escapeRmlText(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (char ch : text) {
        switch (ch) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

std::string toDocumentRelativeAssetPath(const std::string& path) {
    if (path.empty()) {
        return std::string();
    }

    namespace fs = std::filesystem;
    const fs::path documentDirectory = fs::absolute(platform::path::resolvePath("assets/rmlui/front_ui"));
    const fs::path assetPath = fs::absolute(fs::path(path));
    const fs::path relativePath = assetPath.lexically_relative(documentDirectory);
    if (relativePath.empty()) {
        return path;
    }
    return relativePath.generic_string();
}

}  // namespace

bool StoryDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    (void)state;
    document_ = &document;
    detachEventListeners(listeners_);
    pendingOpenPause_ = false;
    lastBackground_.clear();
    lastPortrait_.clear();
    lastSpeaker_.clear();
    lastText_.clear();
    attachListeners();
    syncPresentation();
    return true;
}

void StoryDocumentController::unbind() {
    detachEventListeners(listeners_);
    document_ = nullptr;
}

void StoryDocumentController::sync(const AppState& state) {
    (void)state;
}

void StoryDocumentController::update(const AppState& state, float deltaSeconds) {
    (void)state;
    (void)deltaSeconds;
    syncPresentation();
}

void StoryDocumentController::moveSelection(int delta) {
    (void)delta;
}

void StoryDocumentController::activateSelection() {
    requestAdvance();
}

void StoryDocumentController::cancel() {
    pendingOpenPause_ = true;
}

void StoryDocumentController::applyState(AppState& state) {
    if (pendingOpenPause_) {
        pendingOpenPause_ = false;
        openPauseMenu(state, PauseContext::Story);
    }
}

std::optional<Command> StoryDocumentController::consumeCommand() {
    return std::nullopt;
}

void StoryDocumentController::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* target = document_->GetElementById("story-advance-target")) {
        auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
            requestAdvance();
        });
        target->AddEventListener(Rml::EventId::Click, clickListener.get());
        listeners_.push_back(EventListenerBinding{
            target,
            Rml::EventId::Click,
            false,
            std::move(clickListener),
        });
    }
}

void StoryDocumentController::syncPresentation() {
    if (document_ == nullptr) {
        return;
    }

    const vn::PresentationState presentation = vn::getPresentationState();

    if (presentation.backgroundPath != lastBackground_) {
        if (Rml::Element* element = document_->GetElementById("story-bg-art")) {
            if (presentation.backgroundPath.empty()) {
                element->SetClass("is-hidden", true);
                element->SetAttribute("src", "");
            } else {
                element->SetClass("is-hidden", false);
                element->SetAttribute("src", toDocumentRelativeAssetPath(presentation.backgroundPath));
            }
        }
        lastBackground_ = presentation.backgroundPath;
    }

    if (presentation.iconPath != lastPortrait_) {
        if (Rml::Element* element = document_->GetElementById("story-portrait-img")) {
            const bool hasPortrait = !presentation.iconPath.empty();
            element->SetClass("is-hidden", !hasPortrait);
            element->SetAttribute("src", hasPortrait ? toDocumentRelativeAssetPath(presentation.iconPath) : "");
        }
        if (Rml::Element* element = document_->GetElementById("story-dialogue-box")) {
            element->SetClass("story-no-portrait", presentation.iconPath.empty());
        }
        lastPortrait_ = presentation.iconPath;
    }

    if (presentation.speakerName != lastSpeaker_) {
        if (Rml::Element* element = document_->GetElementById("story-speaker-name")) {
            element->SetInnerRML(presentation.speakerName.empty() ? "" : escapeRmlText(presentation.speakerName));
            element->SetClass("is-hidden", presentation.speakerName.empty());
        }
        if (Rml::Element* element = document_->GetElementById("story-dialogue-box")) {
            element->SetClass("story-no-speaker", presentation.speakerName.empty());
        }
        lastSpeaker_ = presentation.speakerName;
    }

    if (presentation.visibleTextRml != lastText_) {
        if (Rml::Element* element = document_->GetElementById("story-dialogue-text")) {
            element->SetInnerRML(presentation.visibleTextRml);
        }
        lastText_ = presentation.visibleTextRml;
    }
}

void StoryDocumentController::requestAdvance() {
    vn::onSpacePressed();
}

}  // namespace graphics::frontui
