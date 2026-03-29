#include "front_ui_pause.h"

#include <array>
#include <functional>
#include <string>
#include <utility>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include "../game/vn/vn_system.h"

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

struct PauseButtonDefinition {
    PauseAction action;
    const char* id;
};

constexpr std::array<PauseButtonDefinition, 5> kPauseButtons{{
    {PauseAction::Continue, "pause-button-continue"},
    {PauseAction::Save, "pause-button-save"},
    {PauseAction::Load, "pause-button-load"},
    {PauseAction::Settings, "pause-button-settings"},
    {PauseAction::ExitToMainMenu, "pause-button-exit"},
}};

struct ConfirmButtonDefinition {
    ConfirmAction action;
    const char* id;
};

constexpr std::array<ConfirmButtonDefinition, 2> kConfirmButtons{{
    {ConfirmAction::Cancel, "pause-confirm-cancel"},
    {ConfirmAction::ExitToMainMenu, "pause-confirm-confirm"},
}};

}  // namespace

constexpr const char* kScrollSfxPath = "assets/ui/sfx/Multiplayer_player-ready-all.wav";
constexpr const char* kConfirmSfxPath = "assets/ui/sfx/UI_dialog-pop-in.wav";
constexpr const char* kBackSfxPath = "assets/ui/sfx/UI_screen-back.wav";

bool PauseDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
    pendingPauseAction_.reset();
    pendingConfirmAction_.reset();
    pendingDismissConfirm_ = false;
    pendingResume_ = false;
    refreshFromState(state);
    attachListeners();
    refreshDocument();

    // Entry animation and open sound happen after first frame is rendered
    if (Rml::Element* shell = document_->GetElementById("pause-shell")) {
        shell->SetClass("entered", false);
        needsEntryAnimation_ = true;
    }
    queueSound(kConfirmSfxPath, 0.9f);
    return true;
}

void PauseDocumentController::unbind() {
    detachEventListeners(listeners_);
    document_ = nullptr;
}

void PauseDocumentController::sync(const AppState& state) {
    refreshFromState(state);
    refreshDocument();
}

void PauseDocumentController::update(const AppState& state, float deltaSeconds) {
    (void)deltaSeconds;
    refreshFromState(state);

    if (needsEntryAnimation_ && document_ != nullptr) {
        if (Rml::Element* shell = document_->GetElementById("pause-shell")) {
            shell->SetClass("entered", true);
        }
        needsEntryAnimation_ = false;
    }

    refreshDocument();
}

void PauseDocumentController::moveSelection(int delta) {
    if (delta == 0) {
        return;
    }

    if (showingConfirm()) {
        confirmSelection_ = nextConfirm(delta);
    } else {
        const PauseAction prev = selection_;
        selection_ = nextAction(delta);
        if (selection_ != prev) {
            queueSound(kScrollSfxPath, 0.82f);
        }
    }
    refreshDocument();
}

void PauseDocumentController::adjustSelection(int delta) {
    if (!showingConfirm() || delta == 0) {
        return;
    }

    confirmSelection_ = nextConfirm(delta);
    refreshDocument();
}

void PauseDocumentController::activateSelection() {
    if (showingConfirm()) {
        queueConfirmAction(confirmSelection_);
    } else {
        if (selection_ == PauseAction::Continue) {
            pendingResume_ = true;
            queueSound(kBackSfxPath, 0.9f);
        } else {
            queuePauseAction(selection_);
            queueSound(kConfirmSfxPath, 0.92f);
        }
    }
}

void PauseDocumentController::cancel() {
    if (showingConfirm()) {
        pendingDismissConfirm_ = true;
    } else {
        pendingResume_ = true;
        queueSound(kBackSfxPath, 0.9f);
    }
}

void PauseDocumentController::applyState(AppState& state) {
    state.pauseSelection = selection_;
    state.confirmSelection = confirmSelection_;

    if (pendingDismissConfirm_) {
        pendingDismissConfirm_ = false;
        state.screen = ScreenState::PauseMenu;
        return;
    }

    if (pendingResume_) {
        pendingResume_ = false;
        vn::setPaused(false);
        state.screen = ScreenState::Playing;
        return;
    }

    if (pendingConfirmAction_.has_value()) {
        const ConfirmAction action = *pendingConfirmAction_;
        pendingConfirmAction_.reset();

        if (screen_ == ScreenState::PauseConfirmOverwriteSave) {
            if (action == ConfirmAction::ExitToMainMenu) {
                state.requestStoryOverwriteSave = true;
            }
            state.screen = ScreenState::PauseMenu;
            return;
        }

        if (action == ConfirmAction::Cancel) {
            state.screen = ScreenState::PauseMenu;
        } else {
            exitStoryToMainMenu(state);
        }
        return;
    }

    if (!pendingPauseAction_.has_value()) {
        return;
    }

    const PauseAction action = *pendingPauseAction_;
    pendingPauseAction_.reset();
    switch (action) {
        case PauseAction::Continue:
            vn::setPaused(false);
            state.screen = ScreenState::Playing;
            break;

        case PauseAction::Save:
            state.requestStoryManualSave = true;
            break;

        case PauseAction::Load:
            openLoadMenu(state, ScreenState::PauseMenu);
            break;

        case PauseAction::Settings:
            state.settingsSelection = SettingsItem::DisplayMode;
            state.settingsReturnScreen = ScreenState::PauseMenu;
            state.screen = ScreenState::Settings;
            break;

        case PauseAction::ExitToMainMenu:
            state.confirmSelection = ConfirmAction::Cancel;
            state.screen = ScreenState::PauseConfirmExit;
            break;
    }
}

std::optional<Command> PauseDocumentController::consumeCommand() {
    return std::nullopt;
}

void PauseDocumentController::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    for (const PauseButtonDefinition& button : kPauseButtons) {
        if (Rml::Element* element = document_->GetElementById(button.id)) {
            auto hoverListener = std::make_unique<CallbackEventListener>([this, action = button.action](Rml::Event&) {
                selection_ = action;
                refreshDocument();
            });
            element->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Mouseover,
                false,
                std::move(hoverListener),
            });

            auto clickListener = std::make_unique<CallbackEventListener>([this, action = button.action](Rml::Event&) {
                selection_ = action;
                activateSelection();
            });
            element->AddEventListener(Rml::EventId::Click, clickListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Click,
                false,
                std::move(clickListener),
            });
            // play scroll sfx on hover
            auto hoverSfxListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
                queueSound(kScrollSfxPath, 0.82f);
            });
            element->AddEventListener(Rml::EventId::Mouseover, hoverSfxListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Mouseover,
                false,
                std::move(hoverSfxListener),
            });
        }
    }

    for (const ConfirmButtonDefinition& button : kConfirmButtons) {
        if (Rml::Element* element = document_->GetElementById(button.id)) {
            auto hoverListener = std::make_unique<CallbackEventListener>([this, action = button.action](Rml::Event&) {
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

            auto clickListener = std::make_unique<CallbackEventListener>([this, action = button.action](Rml::Event&) {
                confirmSelection_ = action;
                queueConfirmAction(action);
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

    // confirm buttons: play sfx on hover as well
    for (const ConfirmButtonDefinition& button : kConfirmButtons) {
        if (Rml::Element* element = document_->GetElementById(button.id)) {
            auto hoverSfxListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
                queueSound(kScrollSfxPath, 0.82f);
            });
            element->AddEventListener(Rml::EventId::Mouseover, hoverSfxListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Mouseover,
                false,
                std::move(hoverSfxListener),
            });
        }
    }
}

void PauseDocumentController::refreshFromState(const AppState& state) {
    screen_ = state.screen;
    selection_ = state.pauseSelection;
    confirmSelection_ = state.confirmSelection;
    noticeText_ = state.noticeTimer > 0.0f ? state.noticeText : std::string();
}

void PauseDocumentController::refreshDocument() const {
    if (document_ == nullptr) {
        return;
    }

    for (const PauseButtonDefinition& button : kPauseButtons) {
        if (Rml::Element* element = document_->GetElementById(button.id)) {
            element->SetClass("is-selected", button.action == selection_);
        }
    }

    if (Rml::Element* overlay = document_->GetElementById("pause-overlay")) {
        overlay->SetClass("fade-in", true);
    }

    const bool confirmVisible = showingConfirm();
    if (Rml::Element* element = document_->GetElementById("pause-confirm")) {
        element->SetClass("is-visible", confirmVisible);
    }

    if (Rml::Element* toast = document_->GetElementById("pause-toast")) {
        if (!noticeText_.empty()) {
            toast->SetInnerRML(escapeRmlText(noticeText_));
        }
        toast->SetClass("is-visible", !noticeText_.empty());
    }

    if (Rml::Element* element = document_->GetElementById("pause-confirm-title")) {
        element->SetInnerRML(
            screen_ == ScreenState::PauseConfirmOverwriteSave ? "Overwrite Save?" : "Exit To Main Menu?");
    }
    if (Rml::Element* element = document_->GetElementById("pause-confirm-body")) {
        element->SetInnerRML(
            screen_ == ScreenState::PauseConfirmOverwriteSave
                ? "A manual save already exists at this story point.<br/>Overwrite that save file?"
                : "You will lose the current chapter progress if you leave now.");
    }
    if (Rml::Element* element = document_->GetElementById("pause-confirm-cancel")) {
        element->SetClass("is-selected", confirmSelection_ == ConfirmAction::Cancel);
    }
    if (Rml::Element* label = document_->GetElementById("pause-confirm-cancel-label")) {
        label->SetInnerRML(screen_ == ScreenState::PauseConfirmOverwriteSave ? "Cancel" : "Stay");
    }
    if (Rml::Element* element = document_->GetElementById("pause-confirm-confirm")) {
        element->SetClass("is-selected", confirmSelection_ == ConfirmAction::ExitToMainMenu);
    }
    if (Rml::Element* label = document_->GetElementById("pause-confirm-confirm-label")) {
        label->SetInnerRML(
            screen_ == ScreenState::PauseConfirmOverwriteSave ? "Overwrite Save" : "Exit To Menu");
    }
}

bool PauseDocumentController::showingConfirm() const {
    return screen_ == ScreenState::PauseConfirmExit ||
           screen_ == ScreenState::PauseConfirmOverwriteSave;
}

PauseAction PauseDocumentController::nextAction(int delta) const {
    const int count = static_cast<int>(kPauseButtons.size());
    int index = static_cast<int>(selection_);
    index = (index + delta) % count;
    if (index < 0) {
        index += count;
    }
    return kPauseButtons[static_cast<std::size_t>(index)].action;
}

ConfirmAction PauseDocumentController::nextConfirm(int delta) const {
    if (delta == 0) {
        return confirmSelection_;
    }
    return confirmSelection_ == ConfirmAction::Cancel
        ? ConfirmAction::ExitToMainMenu
        : ConfirmAction::Cancel;
}

void PauseDocumentController::queuePauseAction(PauseAction action) {
    pendingPauseAction_ = action;
}

void PauseDocumentController::queueConfirmAction(ConfirmAction action) {
    pendingConfirmAction_ = action;
}

void PauseDocumentController::queueSound(const char* path, float volume) {
    pendingSoundRequests_.push_back(SoundRequest{path, volume});
}

std::vector<SoundRequest> PauseDocumentController::consumeSoundRequests() {
    std::vector<SoundRequest> requests = std::move(pendingSoundRequests_);
    pendingSoundRequests_.clear();
    return requests;
}

void PauseDocumentController::exitStoryToMainMenu(AppState& state) const {
    vn::setPaused(false);
    state.pauseSelection = PauseAction::Continue;
    state.confirmSelection = ConfirmAction::Cancel;
    state.settingsReturnScreen = ScreenState::MainMenu;
    state.mainSelection = MainMenuAction::Start;
    state.requestStoryExitToMainMenu = true;
}

}  // namespace graphics::frontui
