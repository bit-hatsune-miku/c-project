#include "front_ui_story.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <utility>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/SystemInterface.h>

#include "../platform/path_resolution.h"
#include "../game/vn/vn_system.h"

namespace graphics::frontui {
namespace {

constexpr float kBackgroundFadeDurationSeconds = 0.30f;

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

std::string toRmlAssetSource(const std::string& path) {
    if (path.empty()) {
        return std::string();
    }

    return platform::path::resolvePath(path);
}

void releaseDocumentTexture(Rml::ElementDocument& document, const std::string& source) {
    if (source.empty()) {
        return;
    }

    Rml::String resolvedSource = source;
    if (Rml::SystemInterface* systemInterface = Rml::GetSystemInterface()) {
        systemInterface->JoinPath(resolvedSource, document.GetSourceURL(), source);
    }

    (void)Rml::ReleaseTexture(resolvedSource);
}

}  // namespace

bool StoryDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    (void)state;
    document_ = &document;
    detachEventListeners(listeners_);
    pendingOpenPause_ = false;
    lastBackground_.clear();
    fadingOutBackground_.clear();
    lastPortrait_.clear();
    lastSpeaker_.clear();
    lastText_.clear();
    lastVisibleCharacters_ = 0;
    lastTotalVisibleCharacters_ = 0;
    backgroundFadeElapsed_ = 0.0f;
    backgroundFadeActive_ = false;
    attachListeners();
    syncPresentation();
    return true;
}

void StoryDocumentController::unbind() {
    if (document_ != nullptr) {
        releaseDocumentTexture(*document_, toRmlAssetSource(lastBackground_));
        releaseDocumentTexture(*document_, toRmlAssetSource(fadingOutBackground_));
        releaseDocumentTexture(*document_, toRmlAssetSource(lastPortrait_));
    }
    detachEventListeners(listeners_);
    document_ = nullptr;
    lastBackground_.clear();
    fadingOutBackground_.clear();
    lastPortrait_.clear();
}

void StoryDocumentController::sync(const AppState& state) {
    (void)state;
}

void StoryDocumentController::update(const AppState& state, float deltaSeconds) {
    (void)state;
    if (document_ != nullptr && backgroundFadeActive_) {
        backgroundFadeElapsed_ = std::min(kBackgroundFadeDurationSeconds, backgroundFadeElapsed_ + deltaSeconds);
        const float t = std::clamp(backgroundFadeElapsed_ / kBackgroundFadeDurationSeconds, 0.0f, 1.0f);
        const bool hasPrevious = !fadingOutBackground_.empty();
        const bool hasCurrent = !lastBackground_.empty();

        if (Rml::Element* element = document_->GetElementById("story-bg-art-prev")) {
            if (hasPrevious) {
                element->SetClass("is-hidden", false);
                if (hasCurrent) {
                    element->SetProperty("opacity", "1.0");
                } else {
                    element->SetProperty("opacity", std::to_string(1.0f - t));
                }
            } else {
                element->SetClass("is-hidden", true);
                element->SetProperty("opacity", "0.0");
            }
        }

        if (Rml::Element* element = document_->GetElementById("story-bg-art")) {
            if (hasCurrent) {
                element->SetClass("is-hidden", false);
                element->SetProperty("opacity", std::to_string(t));
            } else {
                element->SetClass("is-hidden", true);
                element->SetProperty("opacity", "0.0");
            }
        }

        if (t >= 1.0f) {
            if (!fadingOutBackground_.empty()) {
                releaseDocumentTexture(*document_, toRmlAssetSource(fadingOutBackground_));
                fadingOutBackground_.clear();
            }
            backgroundFadeActive_ = false;

            if (Rml::Element* element = document_->GetElementById("story-bg-art-prev")) {
                element->SetClass("is-hidden", true);
                element->SetAttribute("src", "");
                element->SetProperty("opacity", "0.0");
            }
            if (Rml::Element* element = document_->GetElementById("story-bg-art")) {
                if (lastBackground_.empty()) {
                    element->SetClass("is-hidden", true);
                    element->SetAttribute("src", "");
                    element->SetProperty("opacity", "0.0");
                } else {
                    element->SetClass("is-hidden", false);
                    element->SetProperty("opacity", "1.0");
                }
            }
        }
    }
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
        if (!fadingOutBackground_.empty() && fadingOutBackground_ != lastBackground_) {
            releaseDocumentTexture(*document_, toRmlAssetSource(fadingOutBackground_));
        }
        fadingOutBackground_ = lastBackground_;
        const std::string previousBackgroundSource = toRmlAssetSource(fadingOutBackground_);
        const std::string currentBackgroundSource = toRmlAssetSource(presentation.backgroundPath);

        if (Rml::Element* element = document_->GetElementById("story-bg-art-prev")) {
            if (fadingOutBackground_.empty()) {
                element->SetClass("is-hidden", true);
                element->SetAttribute("src", "");
                element->SetProperty("opacity", "0.0");
            } else {
                element->SetClass("is-hidden", false);
                element->SetAttribute("src", previousBackgroundSource);
                element->SetProperty("opacity", "1.0");
            }
        }

        if (Rml::Element* element = document_->GetElementById("story-bg-art")) {
            if (presentation.backgroundPath.empty()) {
                element->SetClass("is-hidden", true);
                element->SetAttribute("src", "");
                element->SetProperty("opacity", "0.0");
            } else {
                element->SetClass("is-hidden", false);
                element->SetAttribute("src", currentBackgroundSource);
                element->SetProperty("opacity", fadingOutBackground_.empty() ? "0.0" : "0.0");
            }
        }

        lastBackground_ = presentation.backgroundPath;
        backgroundFadeElapsed_ = 0.0f;
        backgroundFadeActive_ = !fadingOutBackground_.empty() || !lastBackground_.empty();

        if (!backgroundFadeActive_ && !previousBackgroundSource.empty()) {
            releaseDocumentTexture(*document_, previousBackgroundSource);
        }
    }

    if (presentation.iconPath != lastPortrait_) {
        const std::string previousPortraitSource = toRmlAssetSource(lastPortrait_);
        if (Rml::Element* element = document_->GetElementById("story-portrait-img")) {
            const bool hasPortrait = !presentation.iconPath.empty();
            element->SetClass("is-hidden", !hasPortrait);
            element->SetAttribute("src", hasPortrait ? toRmlAssetSource(presentation.iconPath) : "");
        }
        if (Rml::Element* element = document_->GetElementById("story-dialogue-box")) {
            element->SetClass("story-no-portrait", presentation.iconPath.empty());
        }
        releaseDocumentTexture(*document_, previousPortraitSource);
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

    syncDialogueScroll(presentation);
}

void StoryDocumentController::requestAdvance() {
    vn::onSpacePressed();
}

void StoryDocumentController::syncDialogueScroll(const vn::PresentationState& presentation) {
    if (document_ == nullptr) {
        return;
    }

    Rml::Element* container = document_->GetElementById("story-dialogue-copy");
    if (container == nullptr) {
        return;
    }

    const bool startedNewLine =
        presentation.visibleCharacters < lastVisibleCharacters_ ||
        (presentation.visibleCharacters == 0 && lastVisibleCharacters_ != 0) ||
        (presentation.totalVisibleCharacters != lastTotalVisibleCharacters_ && presentation.visibleCharacters <= 1);

    if (startedNewLine) {
        container->SetScrollTop(0.0f);
    }

    if (presentation.visibleCharacters != lastVisibleCharacters_) {
        const float clientHeight = container->GetClientHeight();
        const float scrollHeight = container->GetScrollHeight();
        const float maxScrollTop = std::max(0.0f, scrollHeight - clientHeight);

        if (maxScrollTop > 0.0f) {
            const float currentScrollTop = container->GetScrollTop();
            const float distanceFromBottom = maxScrollTop - currentScrollTop;
            const float dpRatio =
                (container->GetContext() != nullptr) ? container->GetContext()->GetDensityIndependentPixelRatio() : 1.0f;
            constexpr float kSnapThresholdDp = 30.0f;
            const float snapThreshold = kSnapThresholdDp * dpRatio;

            // Only follow the typewriter when the reader is already near the bottom.
            // If they scrolled up to reread, leave the viewport where they put it.
            if (distanceFromBottom <= snapThreshold) {
                container->SetScrollTop(maxScrollTop);
            }
        }
    }

    lastVisibleCharacters_ = presentation.visibleCharacters;
    lastTotalVisibleCharacters_ = presentation.totalVisibleCharacters;
}

}  // namespace graphics::frontui
