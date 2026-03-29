#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../game/save/save.h"
#include "front_ui_document.h"

namespace graphics::frontui {

class LoadDocumentController final : public DocumentController {
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
    std::vector<SoundRequest> consumeSoundRequests() override;

private:
    struct SlotPresentation {
        save::SlotInfo slot;
        std::optional<save::SaveGame> saveGame;
        std::string chapterLabel;
        std::string partyLabel;
        std::string summaryText;
        std::string statusLabel;
        int idolRank = 0;
    };

    void attachListeners();
    void refreshFromState(const AppState& state);
    void refreshSlots();
    void refreshDocument() const;
    void refreshDetailPanel() const;
    std::size_t totalSelectableItems() const;
    std::size_t visibleWindowStart() const;
    bool showingConfirm() const;
    bool backSelected() const;
    bool deleteSelected() const;
    const SlotPresentation* selectedSlot() const;
    bool selectedSlotCanDelete() const;
    void applySelectionAfterDelete();
    void setSelection(std::size_t selection, bool syncSlotSelection, bool playSound);
    void queueSound(const char* path, float volume = 0.9f);

    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    std::vector<SlotPresentation> cachedSlots_;
    ScreenState screen_ = ScreenState::LoadMenu;
    ScreenState returnScreen_ = ScreenState::MainMenu;
    std::size_t selection_ = 0;
    std::size_t slotSelection_ = 0;
    ConfirmAction confirmSelection_ = ConfirmAction::Cancel;
    std::string noticeText_;
    std::size_t pendingDeleteSelection_ = 0;
    std::string pendingDeletePath_;
    std::vector<SoundRequest> pendingSoundRequests_;
    bool pendingBack_ = false;
    bool pendingActivateSelection_ = false;
    bool pendingDismissConfirm_ = false;
    bool pendingDeleteConfirm_ = false;
};

}  // namespace graphics::frontui
