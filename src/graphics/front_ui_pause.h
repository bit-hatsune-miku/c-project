#pragma once

#include <optional>
#include <string>
#include <vector>

#include "front_ui_document.h"

namespace graphics::frontui {

class PauseDocumentController final : public DocumentController {
public:
    bool bind(Rml::ElementDocument& document, const AppState& state) override;
    void unbind() override;
    void sync(const AppState& state) override;
    void update(const AppState& state, float deltaSeconds) override;
    void moveSelection(int delta) override;
    void adjustSelection(int delta) override;
    void activateSelection() override;
    void cancel() override;
    void applyState(AppState& state) override;
    std::optional<Command> consumeCommand() override;

private:
    void attachListeners();
    void refreshFromState(const AppState& state);
    void refreshDocument() const;
    bool showingConfirm() const;
    PauseAction nextAction(int delta) const;
    ConfirmAction nextConfirm(int delta) const;
    void queuePauseAction(PauseAction action);
    void queueConfirmAction(ConfirmAction action);
    void exitStoryToMainMenu(AppState& state) const;

    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    ScreenState screen_ = ScreenState::PauseMenu;
    PauseAction selection_ = PauseAction::Continue;
    ConfirmAction confirmSelection_ = ConfirmAction::Cancel;
    std::string noticeText_;
    std::optional<PauseAction> pendingPauseAction_;
    std::optional<ConfirmAction> pendingConfirmAction_;
    bool pendingDismissConfirm_ = false;
    bool pendingResume_ = false;
};

}  // namespace graphics::frontui
