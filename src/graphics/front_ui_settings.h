#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <RmlUi/Core/EventListener.h>

#include "front_ui_document.h"

namespace graphics::frontui {

class SettingsDocumentController final : public DocumentController {
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
    void applySelectionStyles() const;
    void applyRowCopy() const;
    void updateFocusCopy() const;
    void syncControlValues() const;
    void setSelection(SettingsItem selection);
    void queueDisplayMode(bool fullscreen);
    void queueReturn();
    void setVoiceVolume(float value);
    void setTextSpeed(float value);

    Rml::ElementDocument* document_ = nullptr;
    SettingsItem selection_ = SettingsItem::DisplayMode;
    GameSettings workingSettings_{};
    std::optional<Command> pendingCommand_;
    std::vector<EventListenerBinding> listeners_;
};

}  // namespace graphics::frontui
