#pragma once

#include <string>
#include <vector>

#include "../game/credits/credits_data.h"
#include "front_ui_document.h"

namespace graphics::frontui {

class CreditsDocumentController final : public DocumentController {
public:
    bool bind(Rml::ElementDocument& document, const AppState& state) override;
    void unbind() override;
    void sync(const AppState& state) override;
    void update(const AppState& state, float deltaSeconds) override;
    void moveSelection(int delta) override;
    void activateSelection() override;
    void handleKeyDown(const SDL_KeyboardEvent& event) override;
    void cancel() override;
    void applyState(AppState& state) override;
    std::optional<Command> consumeCommand() override;

private:
    void attachListeners();
    void populateContent();
    void updateScroll(float deltaSeconds);
    void initializeScrollMetrics();
    void setScrollTop(float top);
    void syncPrompt() const;
    void requestReturn();

    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    game::credits::CreditsData creditsData_;
    bool creditsLoaded_ = false;
    bool pendingReturn_ = false;
    bool scrollMetricsReady_ = false;
    bool rollFinished_ = false;
    float currentTop_ = 0.0f;
    float startTop_ = 0.0f;
    float endTop_ = 0.0f;
};

}  // namespace graphics::frontui
