#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../game/save/save.h"
#include "front_ui_document.h"

/**
 * Represent presentation data for a single save slot used by the load UI.
 */
 
/**
 * Attach the controller to an Rml document and initialize internal UI state from the application state.
 * @param document Rml document to bind to.
 * @param state Application state used to initialize the controller.
 * @returns `true` if the document was successfully bound, `false` otherwise.
 */
 
/**
 * Detach the controller from any bound document and remove associated listeners/resources.
 */
 
/**
 * Update the controller's cached UI state to reflect the provided application state without advancing time.
 * @param state Current application state to synchronize from.
 */
 
/**
 * Perform time-based or deferred processing and update internal pending actions.
 * @param state Current application state that may be observed during update.
 * @param deltaSeconds Time elapsed since the last update in seconds.
 */
 
/**
 * Move the current selection by the given delta within the selectable item range.
 * @param delta Signed offset to apply to the current selection (positive moves forward, negative moves backward).
 */
 
/**
 * Adjust the slot selection by the given delta (used for fine-grained slot navigation).
 * @param delta Signed offset to apply to the slot selection.
 */
 
/**
 * Execute the action associated with the currently selected UI item (e.g., open load, confirm dialogs).
 */
 
/**
 * Cancel the current operation or navigate back from the current UI screen.
 */
 
/**
 * Apply controller-driven changes back into the provided application state (e.g., chosen slot, navigation intent).
 * @param state Application state to modify.
 */
 
/**
 * Consume and return at most one pending command produced by the controller, clearing it from internal storage.
 * @returns An optional `Command` containing the next pending command, or an empty `std::optional` if none is available.
 */
 
/**
 * Consume and return all pending sound requests accumulated by user interactions, clearing the internal queue.
 * @returns A vector of `SoundRequest` items that were queued since the last consumption.
 */
 
/**
 * Register event listeners on the bound Rml document and store their bindings for later removal.
 */
 
/**
 * Refresh internal cached presentation data from the given application state.
 * @param state Application state to refresh from.
 */
 
/**
 * Rebuild or update cached slot presentation data used to render the slot list and detail panel.
 */
 
/**
 * Apply the latest cached controller data to the bound Rml document, updating visible UI elements.
 */
 
/**
 * Update the detail panel in the document to reflect the currently selected slot's presentation.
 */
 
/**
 * Compute how many items are currently selectable in the UI (including slots and navigation entries).
 * @returns The total number of selectable items.
 */
 
/**
 * Compute the start index of the visible scrolling window for the slot list.
 * @returns The index of the first visible item within the selectable range.
 */
 
/**
 * Determine whether a confirmation dialog is currently being shown.
 * @returns `true` if a confirmation UI is active, `false` otherwise.
 */
 
/**
 * Determine whether the current selection target is the UI's back/cancel entry.
 * @returns `true` if the back entry is selected, `false` otherwise.
 */
 
/**
 * Determine whether the current selection target is the delete entry.
 * @returns `true` if the delete entry is selected, `false` otherwise.
 */
 
/**
 * Return a pointer to the currently selected slot's presentation data, or `nullptr` if the selection is invalid.
 * @returns Pointer to the selected `SlotPresentation` or `nullptr` when no valid slot is selected.
 */
 
/**
 * Determine whether the currently selected slot supports deletion.
 * @returns `true` if the selected slot can be deleted (e.g., contains a save), `false` otherwise.
 */
 
/**
 * Adjust selection indices after a slot has been deleted to ensure the selection remains valid.
 */
 
/**
 * Set the current selection index and optionally synchronize the slot selection and play a selection sound.
 * @param selection New selection index to apply.
 * @param syncSlotSelection If `true`, also synchronize `slotSelection_` with the new selection.
 * @param playSound If `true`, enqueue the selection-change sound.
 */
 
/**
 * Enqueue a sound request to be played later.
 * @param path Filesystem or resource path identifying the sound to play.
 * @param volume Playback volume in the range [0.0, 1.0] (default 0.9).
 */
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
