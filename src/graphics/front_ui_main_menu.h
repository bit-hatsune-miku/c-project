#pragma once

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include <RmlUi/Core/EventListener.h>

#include "front_ui_document.h"

namespace graphics::frontui {

class MainMenuDocumentController final : public DocumentController {
public:
    bool bind(Rml::ElementDocument& document, const AppState& state) override;
    void unbind() override;
    void sync(const AppState& state) override;
    void update(const AppState& state, float deltaSeconds) override;
    void moveSelection(int delta) override;
    void activateSelection() override;
    void applyState(AppState& state) override;
    std::optional<Command> consumeCommand() override;
    std::optional<MainMenuAction> selectedMainMenuAction() const override;

private:
    void attachListeners();
    void applyButtonCopy() const;
    void setSelection(MainMenuAction action);
    void queueActivation(MainMenuAction action);
    void applySelectionStyles() const;
    void updateStatusCopy() const;

    Rml::ElementDocument* document_ = nullptr;
    MainMenuAction selection_ = MainMenuAction::Start;
    std::optional<Command> pendingCommand_;
    std::vector<EventListenerBinding> listeners_;
};

}  // namespace graphics::frontui
