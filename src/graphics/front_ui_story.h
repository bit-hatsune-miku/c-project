#pragma once

#include <string>
#include <vector>

#include "front_ui_document.h"

namespace vn {
struct PresentationState;
}

namespace graphics::frontui {

class StoryDocumentController final : public DocumentController {
public:
    bool bind(Rml::ElementDocument& document, const AppState& state) override;
    void unbind() override;
    void sync(const AppState& state) override;
    void update(const AppState& state, float deltaSeconds) override;
    void moveSelection(int delta) override;
    void activateSelection() override;
    void handleMouseButtonUp(const SDL_MouseButtonEvent& event) override;
    void cancel() override;
    void applyState(AppState& state) override;
    std::optional<Command> consumeCommand() override;

private:
    void attachListeners();
    void syncPresentation();
    void syncDialogueScroll(const vn::PresentationState& presentation);
    void requestAdvance();

    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    std::string lastBackground_;
    std::string lastBackgroundColor_;
    std::string fadingOutBackground_;
    std::string lastPortrait_;
    std::string lastSpeaker_;
    std::string lastAccentColor_;
    std::string lastText_;
    std::size_t lastVisibleCharacters_ = 0;
    std::size_t lastTotalVisibleCharacters_ = 0;
    float backgroundFadeElapsed_ = 0.0f;
    bool backgroundFadeActive_ = false;
    bool pendingOpenPause_ = false;
};

}  // namespace graphics::frontui
