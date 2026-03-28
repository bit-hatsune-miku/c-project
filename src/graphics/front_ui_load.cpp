#include "front_ui_load.h"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <string>
#include <utility>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

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

constexpr std::size_t kVisibleSlotCount = 5;
constexpr const char* kScrollSfxPath = "assets/ui/sfx/Multiplayer_player-ready-all.wav";
constexpr const char* kBackSfxPath = "assets/ui/sfx/Menu_back-to-logo.wav";
constexpr const char* kLoadConfirmSfxPath = "assets/ui/sfx/Menu_button-daily-select.wav";

/**
 * @brief Escape characters in a string for safe insertion into RmlUi inner RML.
 *
 * Replaces the characters `&`, `<`, `>`, and `"` with their corresponding
 * XML/HTML entities (`&amp;`, `&lt;`, `&gt;`, `&quot;`) and leaves all other
 * characters unchanged.
 *
 * @param text Input text to escape.
 * @return std::string Input text with `&`, `<`, `>`, and `"` replaced by their entities.
 */
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

const std::array<const char*, kVisibleSlotCount> kSlotButtonIds{{
    "load-slot-button-0",
    "load-slot-button-1",
    "load-slot-button-2",
    "load-slot-button-3",
    "load-slot-button-4",
}};

const std::array<const char*, kVisibleSlotCount> kSlotKindIds{{
    "load-slot-kind-0",
    "load-slot-kind-1",
    "load-slot-kind-2",
    "load-slot-kind-3",
    "load-slot-kind-4",
}};

const std::array<const char*, kVisibleSlotCount> kSlotLabelIds{{
    "load-slot-label-0",
    "load-slot-label-1",
    "load-slot-label-2",
    "load-slot-label-3",
    "load-slot-label-4",
}};

const std::array<const char*, kVisibleSlotCount> kSlotTimeIds{{
    "load-slot-time-0",
    "load-slot-time-1",
    "load-slot-time-2",
    "load-slot-time-3",
    "load-slot-time-4",
}};

}  /**
 * @brief Bind the controller to an Rml document and initialize its UI state.
 *
 * Clears any existing event listeners and pending sound/command flags, sets
 * the target document, initializes controller state from `state`, rebuilds
 * the slot list, attaches new UI event listeners, and refreshes the document
 * display.
 *
 * @param document Rml document to bind the controller to.
 * @param state Application state used to initialize controller selections and notices.
 * @return true if the controller successfully bound to the document.
 */

bool LoadDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
    pendingSoundRequests_.clear();
    pendingBack_ = false;
    pendingActivateSelection_ = false;
    pendingDismissConfirm_ = false;
    pendingDeleteConfirm_ = false;
    pendingDeletePath_.clear();
    pendingDeleteSelection_ = 0;
    refreshFromState(state);
    refreshSlots();
    attachListeners();
    refreshDocument();
    return true;
}

/**
 * @brief Unbinds the controller from its document and clears transient UI state.
 *
 * Detaches any registered event listeners, clears queued sound requests, and
 * resets the internal document pointer to null.
 */
void LoadDocumentController::unbind() {
    detachEventListeners(listeners_);
    pendingSoundRequests_.clear();
    document_ = nullptr;
}

void LoadDocumentController::sync(const AppState& state) {
    refreshFromState(state);
    refreshDocument();
}

void LoadDocumentController::update(const AppState& state, float deltaSeconds) {
    (void)deltaSeconds;
    refreshFromState(state);
    refreshDocument();
}

/**
 * @brief Moves the current selection by a relative offset, handling confirm-panel toggling and wrapping.
 *
 * Adjusts the active selection by delta (positive moves forward, negative moves backward) and wraps around the available selectable entries. If the confirm dialog is visible, toggles the confirm choice instead. When the selection or confirm choice changes, a navigation sound is enqueued and the document view is refreshed.
 *
 * @param delta Relative step count to move the selection; positive moves forward, negative moves backward. A value of 0 has no effect.
 */
void LoadDocumentController::moveSelection(int delta) {
    if (delta == 0) {
        return;
    }

    if (showingConfirm()) {
        const ConfirmAction nextSelection = confirmSelection_ == ConfirmAction::Cancel
            ? ConfirmAction::ExitToMainMenu
            : ConfirmAction::Cancel;
        if (confirmSelection_ != nextSelection) {
            confirmSelection_ = nextSelection;
            queueSound(kScrollSfxPath, 0.82f);
        }
        refreshDocument();
        return;
    }

    const std::size_t total = totalSelectableItems();
    if (total == 0) {
        selection_ = 0;
        slotSelection_ = 0;
        return;
    }

    int next = static_cast<int>(selection_);
    next = (next + delta) % static_cast<int>(total);
    if (next < 0) {
        next += static_cast<int>(total);
    }
    setSelection(static_cast<std::size_t>(next), true, true);
    refreshDocument();
}

/**
 * @brief Adjusts the current selection by a single step for fine-grained navigation.
 *
 * If `delta` is zero this is a no-op. When the confirm panel is visible, toggles the
 * confirm choice between Cancel and ExitToMainMenu (queues a scroll sound when the
 * selection actually changes) and refreshes the document. When not confirming:
 * - If a save slot is focused, moves selection to the delete footer on negative `delta`
 *   or to the back footer on positive `delta`.
 * - If the delete footer is focused and `delta > 0`, moves to the back footer.
 * - If the back footer is focused and `delta < 0`, moves to the delete footer.
 *
 * The function always refreshes the document after applying any change.
 *
 * @param delta Positive to move forward (toward back), negative to move backward
 *              (toward delete).
 */
void LoadDocumentController::adjustSelection(int delta) {
    if (delta == 0) {
        return;
    }

    if (showingConfirm()) {
        const ConfirmAction nextSelection = confirmSelection_ == ConfirmAction::Cancel
            ? ConfirmAction::ExitToMainMenu
            : ConfirmAction::Cancel;
        if (confirmSelection_ != nextSelection) {
            confirmSelection_ = nextSelection;
            queueSound(kScrollSfxPath, 0.82f);
        }
        refreshDocument();
        return;
    }

    if (selection_ < cachedSlots_.size()) {
        setSelection(delta < 0 ? cachedSlots_.size() : cachedSlots_.size() + 1, false, true);
    } else if (deleteSelected() && delta > 0) {
        setSelection(cachedSlots_.size() + 1, false, true);
    } else if (backSelected() && delta < 0) {
        setSelection(cachedSlots_.size(), false, true);
    }
    refreshDocument();
}

/**
 * @brief Handle activation (confirm/click) of the currently focused item.
 *
 * Sets the appropriate pending action flag based on the current UI selection and may enqueue a UI sound.
 *
 * - If the confirm dialog is visible, requests confirm-delete handling.
 * - If the "Back" footer is selected, requests navigation back and queues the back SFX.
 * - If a save-slot entry is selected, requests activation of that slot (load) and queues the load-confirm SFX.
 * - Otherwise, requests either confirm-delete or activation depending on whether the confirm dialog is visible.
 */
void LoadDocumentController::activateSelection() {
    if (showingConfirm()) {
        pendingDeleteConfirm_ = true;
        return;
    }

    if (backSelected()) {
        pendingBack_ = true;
        queueSound(kBackSfxPath, 0.92f);
        return;
    }

    if (selection_ < cachedSlots_.size()) {
        pendingActivateSelection_ = true;
        queueSound(kLoadConfirmSfxPath, 0.92f);
        return;
    }

    if (showingConfirm()) {
        pendingDeleteConfirm_ = true;
    } else {
        pendingActivateSelection_ = true;
    }
}

/**
 * @brief Handle a user cancel action from the load UI.
 *
 * If the confirm-delete panel is visible, schedules dismissal of that panel.
 * Otherwise schedules a navigation back to the previous screen and queues the back sound effect.
 *
 * The method only sets internal pending flags and sound requests; actual screen changes occur when pending actions are applied to the application state.
 */
void LoadDocumentController::cancel() {
    if (showingConfirm()) {
        pendingDismissConfirm_ = true;
    } else {
        pendingBack_ = true;
        queueSound(kBackSfxPath, 0.92f);
    }
}

/**
 * @brief Apply pending controller actions to the application state.
 *
 * Copies the controller's current selection and confirm choice into `state` and then
 * consumes any pending UI actions in priority order: dismissing the confirm panel,
 * navigating back, processing a delete-confirm result (may delete a manual save and
 * set a notice), or activating the current selection. When activating a slot this
 * may set `state.pendingLoadPath`; when processing delete confirmation this will
 * call the save deletion routine and set `state.noticeText`/`state.noticeTimer`.
 *
 * @param state Mutable application state to update with the controller's effects.
 */
void LoadDocumentController::applyState(AppState& state) {
    state.loadSelection = selection_;
    state.loadSlotSelection = slotSelection_;
    state.confirmSelection = confirmSelection_;

    if (pendingDismissConfirm_) {
        pendingDismissConfirm_ = false;
        pendingDeletePath_.clear();
        state.screen = ScreenState::LoadGameMenu;
        return;
    }

    if (pendingBack_) {
        pendingBack_ = false;
        state.screen = returnScreen_;
        return;
    }

    if (pendingDeleteConfirm_) {
        pendingDeleteConfirm_ = false;
        if (confirmSelection_ == ConfirmAction::Cancel) {
            pendingDeletePath_.clear();
            state.screen = ScreenState::LoadGameMenu;
        } else if (!pendingDeletePath_.empty()) {
            const bool deleted = save::deleteManualSave(pendingDeletePath_);
            refreshSlots();
            applySelectionAfterDelete();
            state.loadSelection = selection_;
            state.loadSlotSelection = slotSelection_;
            state.screen = ScreenState::LoadGameMenu;
            state.noticeText = deleted ? "Save deleted." : "Delete failed.";
            state.noticeTimer = 2.0f;
            pendingDeletePath_.clear();
        }
        return;
    }

    if (!pendingActivateSelection_) {
        return;
    }

    pendingActivateSelection_ = false;
    if (deleteSelected()) {
        const SlotPresentation* slot = selectedSlot();
        if (slot == nullptr) {
            state.noticeText = "Select a manual save first.";
            state.noticeTimer = 2.0f;
            return;
        }
        if (slot->slot.isAutosave) {
            state.noticeText = "Autosave cannot be deleted.";
            state.noticeTimer = 2.0f;
            return;
        }

        pendingDeletePath_ = slot->slot.path.string();
        pendingDeleteSelection_ = slotSelection_;
        confirmSelection_ = ConfirmAction::Cancel;
        state.confirmSelection = confirmSelection_;
        state.screen = ScreenState::LoadConfirmDelete;
        return;
    }

    if (backSelected()) {
        state.screen = returnScreen_;
        return;
    }

    if (selection_ < cachedSlots_.size()) {
        slotSelection_ = selection_;
        state.loadSlotSelection = slotSelection_;
        state.pendingLoadPath = cachedSlots_[selection_].slot.path.string();
    }
}

/**
 * @brief Report any pending high-level command produced by this controller (none).
 *
 * This implementation never produces a command; the controller does not emit
 * actionable commands through this API.
 *
 * @return std::nullopt Indicates there is no pending command.
 */
std::optional<Command> LoadDocumentController::consumeCommand() {
    return std::nullopt;
}

/**
 * @brief Drains and returns all pending sound requests queued by the controller.
 *
 * The controller's internal pending sound queue is cleared as a result of this call.
 *
 * @return std::vector<SoundRequest> Vector containing the pending sound requests that were queued; the controller's pending list is emptied.
 */
std::vector<SoundRequest> LoadDocumentController::consumeSoundRequests() {
    std::vector<SoundRequest> requests = std::move(pendingSoundRequests_);
    pendingSoundRequests_.clear();
    return requests;
}

/**
 * @brief Attaches UI event listeners for the load screen controls.
 *
 * Registers mouseover and click handlers on visible slot buttons, the delete and back footers,
 * and the confirm dialog buttons so UI interactions update controller selection state,
 * set pending actions (activate, back, or delete-confirm), and enqueue appropriate sound requests.
 *
 * Mouseover on slot buttons syncs the hovered slot into the current selection and refreshes the UI;
 * click on a slot selects it and marks activation pending. Footer mouseover/select and click
 * update selection or set pending back/delete actions. Confirm-button hover changes the
 * confirmation choice and queues a scroll SFX; confirm click sets the pending deletion confirmation.
 *
 * Listeners are stored in the controller's listener bindings so they can be detached later.
 */
void LoadDocumentController::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < kVisibleSlotCount; ++i) {
        if (Rml::Element* element = document_->GetElementById(kSlotButtonIds[i])) {
            auto hoverListener = std::make_unique<CallbackEventListener>([this, i](Rml::Event&) {
                const std::size_t index = visibleWindowStart() + i;
                if (index < cachedSlots_.size()) {
                    setSelection(index, true, true);
                    refreshDocument();
                }
            });
            element->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Mouseover,
                false,
                std::move(hoverListener),
            });

            auto clickListener = std::make_unique<CallbackEventListener>([this, i](Rml::Event&) {
                const std::size_t index = visibleWindowStart() + i;
                if (index < cachedSlots_.size()) {
                    setSelection(index, true, false);
                    pendingActivateSelection_ = true;
                    queueSound(kLoadConfirmSfxPath, 0.92f);
                }
            });
            element->AddEventListener(Rml::EventId::Click, clickListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Click,
                false,
                std::move(clickListener),
            });
        }
    }

    if (Rml::Element* element = document_->GetElementById("load-footer-delete")) {
        auto hoverListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
            setSelection(cachedSlots_.size(), false, true);
            refreshDocument();
        });
        element->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
        listeners_.push_back(EventListenerBinding{
            element,
            Rml::EventId::Mouseover,
            false,
            std::move(hoverListener),
        });

        auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
            setSelection(cachedSlots_.size(), false, false);
            pendingActivateSelection_ = true;
        });
        element->AddEventListener(Rml::EventId::Click, clickListener.get());
        listeners_.push_back(EventListenerBinding{
            element,
            Rml::EventId::Click,
            false,
            std::move(clickListener),
        });
    }

    if (Rml::Element* element = document_->GetElementById("load-footer-back")) {
        auto hoverListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
            setSelection(cachedSlots_.size() + 1, false, true);
            refreshDocument();
        });
        element->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
        listeners_.push_back(EventListenerBinding{
            element,
            Rml::EventId::Mouseover,
            false,
            std::move(hoverListener),
        });

        auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
            setSelection(cachedSlots_.size() + 1, false, false);
            pendingBack_ = true;
            queueSound(kBackSfxPath, 0.92f);
        });
        element->AddEventListener(Rml::EventId::Click, clickListener.get());
        listeners_.push_back(EventListenerBinding{
            element,
            Rml::EventId::Click,
            false,
            std::move(clickListener),
        });
    }

    for (const ConfirmAction action : {ConfirmAction::Cancel, ConfirmAction::ExitToMainMenu}) {
        const char* id = action == ConfirmAction::Cancel ? "load-confirm-cancel" : "load-confirm-delete";
        if (Rml::Element* element = document_->GetElementById(id)) {
            auto hoverListener = std::make_unique<CallbackEventListener>([this, action](Rml::Event&) {
                confirmSelection_ = action;
                queueSound(kScrollSfxPath, 0.82f);
                refreshDocument();
            });
            element->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Mouseover,
                false,
                std::move(hoverListener),
            });

            auto clickListener = std::make_unique<CallbackEventListener>([this, action](Rml::Event&) {
                confirmSelection_ = action;
                pendingDeleteConfirm_ = true;
            });
            element->AddEventListener(Rml::EventId::Click, clickListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Click,
                false,
                std::move(clickListener),
            });
        }
    }
}

/**
 * @brief Update controller state from the given application state and clamp selection indices.
 *
 * Copies screen, return-screen, selection, slot-selection, and confirm-selection values
 * from the provided AppState. Sets the controller notice text only if state.noticeTimer > 0.
 * If there are no selectable items, forces both selection indices to 0. Otherwise clamps
 * selection to be at most totalSelectableItems() - 1 and, when cached slots exist, clamps
 * slotSelection to the last cached slot index.
 *
 * @param state Current application state to synchronize from.
 */
void LoadDocumentController::refreshFromState(const AppState& state) {
    screen_ = state.screen;
    returnScreen_ = state.loadReturnScreen;
    selection_ = state.loadSelection;
    slotSelection_ = state.loadSlotSelection;
    confirmSelection_ = state.confirmSelection;
    noticeText_ = state.noticeTimer > 0.0f ? state.noticeText : std::string();

    if (totalSelectableItems() == 0) {
        selection_ = 0;
        slotSelection_ = 0;
        return;
    }

    if (selection_ >= totalSelectableItems()) {
        selection_ = totalSelectableItems() - 1;
    }
    if (!cachedSlots_.empty() && slotSelection_ >= cachedSlots_.size()) {
        slotSelection_ = cachedSlots_.size() - 1;
    }
}

/**
 * @brief Rebuilds the in-memory list of slot presentations from available save slots.
 *
 * Queries available save slots and repopulates `cachedSlots_` with a SlotPresentation
 * for each slot, attempting to load each save to fill human-readable labels,
 * summary text, party information, progression-derived fields, and status.
 *
 * After rebuilding the list this function updates selection state:
 * - If no slots are present, forces `selection_ = 1` and `slotSelection_ = 0`.
 * - Otherwise clamps `slotSelection_` to the last slot index when out of range and
 *   clamps `selection_` to `totalSelectableItems() - 1` when it exceeds the total.
 */
void LoadDocumentController::refreshSlots() {
    cachedSlots_.clear();
    for (const save::SlotInfo& slot : save::listSlots()) {
        SlotPresentation presentation;
        presentation.slot = slot;
        presentation.saveGame = save::load(slot.path);
        if (presentation.saveGame.has_value()) {
            const save::SaveGame& saveGame = *presentation.saveGame;
            presentation.chapterLabel = saveGame.chapter.empty() ? "STORY" : saveGame.chapter;
            presentation.idolRank = static_cast<int>(saveGame.progression.clearedBattleKeys.size());

            if (!saveGame.progression.currentPartyLineup.empty()) {
                std::ostringstream partyStream;
                for (std::size_t i = 0; i < saveGame.progression.currentPartyLineup.size(); ++i) {
                    if (i != 0) {
                        partyStream << " / ";
                    }
                    partyStream << saveGame.progression.currentPartyLineup[i];
                }
                presentation.partyLabel = partyStream.str();
            } else {
                presentation.partyLabel = "Starter Lineup";
            }

            presentation.summaryText = "Resume " + saveGame.label + " and continue from chapter " + presentation.chapterLabel + ".";
            presentation.statusLabel = slot.isAutosave ? "Autosave Ready" : "Manual Save Ready";
        } else {
            presentation.chapterLabel = "UNKNOWN";
            presentation.partyLabel = "Unavailable";
            presentation.summaryText = "This slot could not be parsed. Loading may fail.";
            presentation.statusLabel = "Corrupted";
        }

        cachedSlots_.push_back(std::move(presentation));
    }

    if (cachedSlots_.empty()) {
        selection_ = 1;
        slotSelection_ = 0;
        return;
    }
    if (slotSelection_ >= cachedSlots_.size()) {
        slotSelection_ = cachedSlots_.size() - 1;
    }
    if (selection_ >= totalSelectableItems()) {
        selection_ = totalSelectableItems() - 1;
    }
}

/**
 * @brief Update the bound UI document to reflect the controller's current state.
 *
 * Updates visibility, selection, labels, timestamps, footer states, notice text,
 * and confirm-button selection for the load screen, and refreshes the detail panel.
 * If no document is bound, the method performs no action.
 */
void LoadDocumentController::refreshDocument() const {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("load-confirm")) {
        element->SetClass("is-visible", showingConfirm());
    }

    if (Rml::Element* element = document_->GetElementById("load-empty-state")) {
        element->SetClass("is-visible", cachedSlots_.empty());
    }

    const std::size_t start = visibleWindowStart();
    for (std::size_t i = 0; i < kVisibleSlotCount; ++i) {
        const std::size_t slotIndex = start + i;
        const bool visible = slotIndex < cachedSlots_.size();
        if (Rml::Element* element = document_->GetElementById(kSlotButtonIds[i])) {
            element->SetClass("is-hidden", !visible);
            element->SetClass("is-selected", visible && selection_ == slotIndex);
            element->SetProperty("display", visible ? "block" : "none");
        }
        if (!visible) {
            continue;
        }

        const SlotPresentation& slot = cachedSlots_[slotIndex];
        if (Rml::Element* element = document_->GetElementById(kSlotKindIds[i])) {
            element->SetInnerRML(slot.slot.isAutosave ? "AUTOSAVE" : "MANUAL");
        }
        if (Rml::Element* element = document_->GetElementById(kSlotLabelIds[i])) {
            element->SetInnerRML(escapeRmlText(slot.slot.label));
        }
        if (Rml::Element* element = document_->GetElementById(kSlotTimeIds[i])) {
            element->SetInnerRML(escapeRmlText(save::formatTimestampForDisplay(slot.slot.timestamp)));
        }
    }

    const bool hasSlotData = !cachedSlots_.empty();

    if (Rml::Element* element = document_->GetElementById("load-footer-delete")) {
        element->SetClass("is-hidden", !hasSlotData);
        element->SetClass("is-selected", deleteSelected());
        element->SetClass("is-disabled", !selectedSlotCanDelete());
        element->SetProperty("display", hasSlotData ? "block" : "none");
    }
    if (Rml::Element* element = document_->GetElementById("load-footer-back")) {
        element->SetClass("is-selected", backSelected());
    }
    if (Rml::Element* element = document_->GetElementById("load-footer-back-label")) {
        element->SetInnerRML(returnScreen_ == ScreenState::MainMenu ? "RETURN TO MAIN MENU" : "BACK");
    }
    if (Rml::Element* element = document_->GetElementById("load-footer-back-value")) {
        element->SetInnerRML(returnScreen_ == ScreenState::MainMenu ? "RETURN" : "BACK");
    }
    if (Rml::Element* element = document_->GetElementById("load-notice")) {
        element->SetInnerRML(noticeText_.empty() ? "" : escapeRmlText(noticeText_));
        element->SetClass("is-visible", !noticeText_.empty());
    }
    if (Rml::Element* element = document_->GetElementById("load-confirm-cancel")) {
        element->SetClass("is-selected", confirmSelection_ == ConfirmAction::Cancel);
    }
    if (Rml::Element* element = document_->GetElementById("load-confirm-delete")) {
        element->SetClass("is-selected", confirmSelection_ == ConfirmAction::ExitToMainMenu);
    }

    refreshDetailPanel();
}

/**
 * @brief Computes the number of selectable entries in the load screen.
 *
 * @return std::size_t Total number of selectable items: the count of cached save slots plus two footer entries (delete and back).
 */
std::size_t LoadDocumentController::totalSelectableItems() const {
    return cachedSlots_.size() + 2;
}

std::size_t LoadDocumentController::visibleWindowStart() const {
    if (cachedSlots_.size() <= kVisibleSlotCount) {
        return 0;
    }

    const std::size_t visibleSelection = selection_ < cachedSlots_.size() ? selection_ : slotSelection_;
    if (visibleSelection < kVisibleSlotCount) {
        return 0;
    }

    const std::size_t maxStart = cachedSlots_.size() - kVisibleSlotCount;
    return std::min(visibleSelection - kVisibleSlotCount + 1, maxStart);
}

bool LoadDocumentController::showingConfirm() const {
    return screen_ == ScreenState::LoadConfirmDelete;
}

bool LoadDocumentController::backSelected() const {
    return selection_ == cachedSlots_.size() + 1;
}

/**
 * @brief Checks whether the current selection points to the delete footer.
 *
 * @return `true` if the delete footer is selected, `false` otherwise.
 */
bool LoadDocumentController::deleteSelected() const {
    return selection_ == cachedSlots_.size();
}

/**
 * @brief Update the right-hand detail panel to reflect the currently selected save slot.
 *
 * Populates UI elements with the selected slot's metadata or with placeholder values when no slot
 * is selected. Text fields that can contain user or file-derived content are escaped for safe RML
 * insertion. The following element IDs are written when present:
 * - load-detail-kind, load-detail-code, load-detail-name, load-detail-sub, load-detail-body,
 *   load-detail-scene, load-detail-rank, load-detail-party, load-detail-status,
 *   load-detail-back-hint.
 */
void LoadDocumentController::refreshDetailPanel() const {
    const SlotPresentation* slot = selectedSlot();

    const std::string kind = slot == nullptr ? "NO SAVE" : (slot->slot.isAutosave ? "AUTOSAVE" : "MANUAL SAVE");
    const std::string code = slot == nullptr ? "--" : (selection_ < 9 ? "0" : "") + std::to_string(selection_ + 1);
    const std::string title = slot == nullptr ? "No Save Selected" : slot->slot.label;
    const std::string subtitle = slot == nullptr
        ? "Choose a save slot"
        : save::formatTimestampForDisplay(slot->slot.timestamp) + " / " + slot->chapterLabel;
    const std::string body = slot == nullptr
        ? "Select a slot on the left to preview its stored chapter and progression context."
        : slot->summaryText;
    const std::string scene = slot == nullptr ? "--" : slot->chapterLabel;
    const std::string rank = slot == nullptr ? "--" : std::to_string(slot->idolRank);
    const std::string party = slot == nullptr ? "--" : slot->partyLabel;
    const std::string status = slot == nullptr ? "Idle" : slot->statusLabel;
    const std::string returnHint = returnScreen_ == ScreenState::MainMenu
        ? "ESC or the footer button returns to the play menu without a screen fade."
        : "ESC or the footer button returns to the previous screen.";

    if (Rml::Element* element = document_->GetElementById("load-detail-kind")) {
        element->SetInnerRML(kind);
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-code")) {
        element->SetInnerRML(code);
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-name")) {
        element->SetInnerRML(escapeRmlText(title));
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-sub")) {
        element->SetInnerRML(escapeRmlText(subtitle));
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-body")) {
        element->SetInnerRML(escapeRmlText(body));
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-scene")) {
        element->SetInnerRML(escapeRmlText(scene));
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-rank")) {
        element->SetInnerRML(rank);
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-party")) {
        element->SetInnerRML(escapeRmlText(party));
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-status")) {
        element->SetInnerRML(escapeRmlText(status));
    }
    if (Rml::Element* element = document_->GetElementById("load-detail-back-hint")) {
        element->SetInnerRML(escapeRmlText(returnHint));
    }
}

/**
 * Get the currently selected slot presentation.
 *
 * Chooses `selection_` if it indexes into `cachedSlots_`, otherwise uses `slotSelection_`.
 *
 * @return Pointer to the selected SlotPresentation, or `nullptr` if `cachedSlots_` is empty or the chosen index is out of range.
 */
const LoadDocumentController::SlotPresentation* LoadDocumentController::selectedSlot() const {
    if (cachedSlots_.empty()) {
        return nullptr;
    }

    const std::size_t index = selection_ < cachedSlots_.size() ? selection_ : slotSelection_;
    if (index >= cachedSlots_.size()) {
        return nullptr;
    }
    return &cachedSlots_[index];
}

/**
 * @brief Determines whether the currently selected save slot may be deleted.
 *
 * A slot is deletable only when a slot is selected and that slot is not an autosave.
 *
 * @return `true` if a slot is selected and it is not an autosave, `false` otherwise.
 */
bool LoadDocumentController::selectedSlotCanDelete() const {
    const SlotPresentation* slot = selectedSlot();
    return slot != nullptr && !slot->slot.isAutosave;
}

/**
 * @brief Restore or adjust the current selection indices after a deletion.
 *
 * If there are no cached slots, both selection indices are set to 0.
 * If a previously recorded deletion selection (`pendingDeleteSelection_`) is
 * still within range, both `selection_` and `slotSelection_` are restored to
 * that value. Otherwise both selections are set to the last available slot
 * index.
 */
void LoadDocumentController::applySelectionAfterDelete() {
    if (cachedSlots_.empty()) {
        selection_ = 0;
        slotSelection_ = 0;
        return;
    }

    if (pendingDeleteSelection_ < cachedSlots_.size()) {
        selection_ = pendingDeleteSelection_;
        slotSelection_ = pendingDeleteSelection_;
    } else {
        selection_ = cachedSlots_.size() - 1;
        slotSelection_ = cachedSlots_.size() - 1;
    }
}

/**
 * @brief Update the controller's selection index, wrapping it into the valid range.
 *
 * Wraps the provided `selection` modulo the number of selectable items and updates
 * internal selection state. If `syncSlotSelection` is true and the resulting
 * selection refers to a slot entry, the slot-focused index is synchronized to
 * the new selection. When the selection actually changes and `playSound` is
 * true, a scroll sound request is queued.
 *
 * @param selection Desired selection index (will be wrapped into the valid range).
 * @param syncSlotSelection If true, set `slotSelection_` to the selection when it points to a slot.
 * @param playSound If true, queue the scroll sound when the selection changes.
 */
void LoadDocumentController::setSelection(std::size_t selection, bool syncSlotSelection, bool playSound) {
    const std::size_t total = totalSelectableItems();
    if (total == 0) {
        selection_ = 0;
        slotSelection_ = 0;
        return;
    }

    selection %= total;
    const bool changed = selection_ != selection;
    selection_ = selection;
    if (syncSlotSelection && selection_ < cachedSlots_.size()) {
        slotSelection_ = selection_;
    }

    if (changed && playSound) {
        queueSound(kScrollSfxPath, 0.82f);
    }
}

/**
 * @brief Enqueues a sound request to be played later.
 *
 * Adds a sound request (asset path + volume) to the controller's pending sound queue.
 *
 * @param path Path to the sound asset.
 * @param volume Playback volume multiplier (1.0 = full volume).
 */
void LoadDocumentController::queueSound(const char* path, float volume) {
    pendingSoundRequests_.push_back(SoundRequest{path, volume});
}

}  // namespace graphics::frontui
