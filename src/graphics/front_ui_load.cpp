#include "front_ui_load.h"

#include <algorithm>
#include <array>
#include <functional>
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

}  // namespace

bool LoadDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
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

void LoadDocumentController::unbind() {
    detachEventListeners(listeners_);
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

void LoadDocumentController::moveSelection(int delta) {
    if (delta == 0) {
        return;
    }

    if (showingConfirm()) {
        confirmSelection_ = confirmSelection_ == ConfirmAction::Cancel
            ? ConfirmAction::ExitToMainMenu
            : ConfirmAction::Cancel;
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
    selection_ = static_cast<std::size_t>(next);
    if (selection_ < cachedSlots_.size()) {
        slotSelection_ = selection_;
    }
    refreshDocument();
}

void LoadDocumentController::adjustSelection(int delta) {
    if (delta == 0) {
        return;
    }

    if (showingConfirm()) {
        confirmSelection_ = confirmSelection_ == ConfirmAction::Cancel
            ? ConfirmAction::ExitToMainMenu
            : ConfirmAction::Cancel;
        refreshDocument();
        return;
    }

    if (selection_ < cachedSlots_.size()) {
        selection_ = delta < 0 ? cachedSlots_.size() : cachedSlots_.size() + 1;
    } else if (deleteSelected() && delta > 0) {
        selection_ = cachedSlots_.size() + 1;
    } else if (backSelected() && delta < 0) {
        selection_ = cachedSlots_.size();
    }
    refreshDocument();
}

void LoadDocumentController::activateSelection() {
    if (showingConfirm()) {
        pendingDeleteConfirm_ = true;
    } else {
        pendingActivateSelection_ = true;
    }
}

void LoadDocumentController::cancel() {
    if (showingConfirm()) {
        pendingDismissConfirm_ = true;
    } else {
        pendingBack_ = true;
    }
}

void LoadDocumentController::applyState(AppState& state) {
    state.loadSelection = selection_;
    state.loadSlotSelection = slotSelection_;
    state.confirmSelection = confirmSelection_;

    if (pendingDismissConfirm_) {
        pendingDismissConfirm_ = false;
        pendingDeletePath_.clear();
        state.screen = ScreenState::LoadMenu;
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
            state.screen = ScreenState::LoadMenu;
        } else if (!pendingDeletePath_.empty()) {
            const bool deleted = save::deleteManualSave(pendingDeletePath_);
            refreshSlots();
            applySelectionAfterDelete();
            state.loadSelection = selection_;
            state.loadSlotSelection = slotSelection_;
            state.screen = ScreenState::LoadMenu;
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
        const save::SlotInfo* slot = selectedSlot();
        if (slot == nullptr) {
            state.noticeText = "Select a manual save first.";
            state.noticeTimer = 2.0f;
            return;
        }
        if (slot->isAutosave) {
            state.noticeText = "Autosave cannot be deleted.";
            state.noticeTimer = 2.0f;
            return;
        }

        pendingDeletePath_ = slot->path.string();
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
        state.pendingLoadPath = cachedSlots_[selection_].path.string();
    }
}

std::optional<Command> LoadDocumentController::consumeCommand() {
    return std::nullopt;
}

void LoadDocumentController::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < kVisibleSlotCount; ++i) {
        if (Rml::Element* element = document_->GetElementById(kSlotButtonIds[i])) {
            auto hoverListener = std::make_unique<CallbackEventListener>([this, i](Rml::Event&) {
                const std::size_t index = visibleWindowStart() + i;
                if (index < cachedSlots_.size()) {
                    selection_ = index;
                    slotSelection_ = index;
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
                    selection_ = index;
                    slotSelection_ = index;
                    pendingActivateSelection_ = true;
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
            selection_ = cachedSlots_.size();
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
            selection_ = cachedSlots_.size();
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
            selection_ = cachedSlots_.size() + 1;
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
            selection_ = cachedSlots_.size() + 1;
            pendingBack_ = true;
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

void LoadDocumentController::refreshSlots() {
    cachedSlots_ = save::listSlots();
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
        }
        if (!visible) {
            continue;
        }

        const save::SlotInfo& slot = cachedSlots_[slotIndex];
        if (Rml::Element* element = document_->GetElementById(kSlotKindIds[i])) {
            element->SetInnerRML(slot.isAutosave ? "AUTOSAVE" : "MANUAL");
        }
        if (Rml::Element* element = document_->GetElementById(kSlotLabelIds[i])) {
            element->SetInnerRML(escapeRmlText(slot.label));
        }
        if (Rml::Element* element = document_->GetElementById(kSlotTimeIds[i])) {
            element->SetInnerRML(escapeRmlText(save::formatTimestampForDisplay(slot.timestamp)));
        }
    }

    if (Rml::Element* element = document_->GetElementById("load-footer-delete")) {
        element->SetClass("is-selected", deleteSelected());
        element->SetClass("is-disabled", !selectedSlotCanDelete());
    }
    if (Rml::Element* element = document_->GetElementById("load-footer-back")) {
        element->SetClass("is-selected", backSelected());
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
}

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

bool LoadDocumentController::deleteSelected() const {
    return selection_ == cachedSlots_.size();
}

const save::SlotInfo* LoadDocumentController::selectedSlot() const {
    if (cachedSlots_.empty()) {
        return nullptr;
    }

    const std::size_t index = selection_ < cachedSlots_.size() ? selection_ : slotSelection_;
    if (index >= cachedSlots_.size()) {
        return nullptr;
    }
    return &cachedSlots_[index];
}

bool LoadDocumentController::selectedSlotCanDelete() const {
    const save::SlotInfo* slot = selectedSlot();
    return slot != nullptr && !slot->isAutosave;
}

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

}  // namespace graphics::frontui
