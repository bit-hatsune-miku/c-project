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

constexpr std::array<PauseAction, 5> kStoryPauseOrder{{
    PauseAction::Continue,
    PauseAction::Save,
    PauseAction::Load,
    PauseAction::Settings,
    PauseAction::ExitToMainMenu,
}};

constexpr std::array<PauseAction, 4> kBattlePauseOrder{{
    PauseAction::Continue,
    PauseAction::Load,
    PauseAction::Settings,
    PauseAction::ExitToMainMenu,
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

/**
 * @brief Bind the controller to an RML document and initialize UI state from the application state.
 *
 * Attaches this controller to the provided Rml document, clears existing listeners and pending UI
 * actions, synchronizes internal state from `state`, attaches event listeners, and updates the
 * bound document to reflect the current pause UI. If the document contains the pause shell element,
 * the controller prepares an entry animation and queues the pause-open sound effect.
 *
 * @param document Rml document to bind the controller to.
 * @param state Current application state used to initialize the UI.
 * @return true if the controller was successfully bound to the document.
 */
bool PauseDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
    pendingPauseAction_.reset();
    pendingConfirmAction_.reset();
    pendingDismissConfirm_ = false;
    pendingResume_ = false;
    pendingBlockedLoadNotice_ = false;
    pressedPauseAction_.reset();
    pressedConfirmAction_.reset();
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
    pressedPauseAction_.reset();
    pressedConfirmAction_.reset();
    document_ = nullptr;
}

void PauseDocumentController::sync(const AppState& state) {
    refreshFromState(state);
    refreshDocument();
}

/**
 * @brief Synchronizes the controller from the given application state and, if pending,
 *        applies the pause-screen entry animation once.
 *
 * This updates internal state from `state`, sets the RML "entered" class on the
 * `pause-shell` element the first time an entry animation is needed, clears the
 * pending-entry flag, and refreshes the document to reflect any changes.
 *
 * @param state Current application state to synchronize from.
 * @param deltaSeconds Frame time in seconds (ignored).
 */
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

/**
 * @brief Move the current menu selection by a signed offset.
 *
 * Updates the pause menu selection or the confirm-dialog selection depending
 * on which screen is active. If `delta` is zero this is a no-op. When the
 * main pause selection changes a scroll sound request is queued. The document
 * is refreshed to reflect the new selection.
 *
 * @param delta Signed offset to apply to the current selection; positive
 * values advance, negative values move backward, zero leaves selection unchanged.
 */
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

void PauseDocumentController::handleMouseMotion(const SDL_MouseMotionEvent& event) {
    if (showingConfirm()) {
        if (const std::optional<ConfirmAction> hoveredAction = hitTestConfirmButton(
                static_cast<float>(event.x), static_cast<float>(event.y));
            hoveredAction.has_value()) {
            setConfirmSelection(*hoveredAction, true);
        }
        return;
    }

    if (const std::optional<PauseAction> hoveredAction = hitTestPauseButton(
            static_cast<float>(event.x), static_cast<float>(event.y));
        hoveredAction.has_value()) {
        setPauseSelection(*hoveredAction, true);
    }
}

void PauseDocumentController::handleMouseButtonDown(const SDL_MouseButtonEvent& event) {
    if (event.button != SDL_BUTTON_LEFT) {
        return;
    }

    pressedPauseAction_.reset();
    pressedConfirmAction_.reset();

    if (showingConfirm()) {
        pressedConfirmAction_ = hitTestConfirmButton(static_cast<float>(event.x), static_cast<float>(event.y));
        if (pressedConfirmAction_.has_value()) {
            setConfirmSelection(*pressedConfirmAction_, true);
        }
        return;
    }

    pressedPauseAction_ = hitTestPauseButton(static_cast<float>(event.x), static_cast<float>(event.y));
    if (pressedPauseAction_.has_value()) {
        setPauseSelection(*pressedPauseAction_, true);
    }
}

void PauseDocumentController::handleMouseButtonUp(const SDL_MouseButtonEvent& event) {
    if (event.button != SDL_BUTTON_LEFT) {
        return;
    }

    if (showingConfirm()) {
        const std::optional<ConfirmAction> releasedAction = hitTestConfirmButton(
            static_cast<float>(event.x), static_cast<float>(event.y));
        if (pressedConfirmAction_.has_value() && releasedAction == pressedConfirmAction_) {
            setConfirmSelection(*releasedAction, false);
            queueConfirmAction(*releasedAction);
        }
        pressedConfirmAction_.reset();
        pressedPauseAction_.reset();
        return;
    }

    const std::optional<PauseAction> releasedAction = hitTestPauseButton(
        static_cast<float>(event.x), static_cast<float>(event.y));
    if (pressedPauseAction_.has_value() && releasedAction == pressedPauseAction_) {
        setPauseSelection(*releasedAction, false);
        activateSelection();
    }
    pressedPauseAction_.reset();
    pressedConfirmAction_.reset();
}

/**
 * @brief Activate the currently focused pause or confirm selection.
 *
 * If a confirm dialog is showing, queues the selected confirm action. Otherwise,
 * if the current pause selection is Continue, marks the controller to resume
 * playback and queues the back sound. For any other pause selection, queues
 * that pause action and queues the confirm/open sound.
 */
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

/**
 * @brief Requests cancellation of the current pause UI interaction.
 *
 * If a confirmation dialog is visible, schedules the dialog to be dismissed.
 * Otherwise schedules resuming gameplay and queues the back/cancel sound effect.
 */
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
        state.screen = pauseContext_ == PauseContext::Battle ? ScreenState::BattleDemo : ScreenState::Playing;
        return;
    }

    if (pendingBlockedLoadNotice_) {
        pendingBlockedLoadNotice_ = false;
        state.screen = ScreenState::PauseMenu;
        state.noticeText = "You cannot load during battle.";
        state.noticeTimer = 2.2f;
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
            state.screen = pauseContext_ == PauseContext::Battle ? ScreenState::BattleDemo : ScreenState::Playing;
            break;

        case PauseAction::Save:
            state.requestStoryManualSave = true;
            break;

        case PauseAction::Load:
            if (pauseContext_ == PauseContext::Battle) {
                pendingBlockedLoadNotice_ = true;
                break;
            }
            state.loadReturnScreen = ScreenState::PauseMenu;
            state.loadSelection = 0;
            state.loadSlotSelection = 0;
            state.pendingDeletePath.clear();
            state.pendingDeleteSelection = 0;
            state.confirmSelection = ConfirmAction::Cancel;
            state.screen = ScreenState::LoadGameMenu;
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

/**
 * @brief Attach UI event listeners for pause and confirm buttons in the bound RML document.
 *
 * Attaches mouseover and click handlers to each pause and confirm button element (if present)
 * so UI selection state is updated, actions are queued or activated, and hover sounds are enqueued.
 * Listener objects are stored in the controller's internal listener collection.
 *
 * If no document is bound (document_ is nullptr), this function does nothing.
 */
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

/**
 * @brief Update the controller's UI-related fields from the given application state.
 *
 * Copies the current screen, pause-selection, confirm-selection, and notice text (only if the notice timer is positive)
 * into the controller's corresponding members.
 *
 * @param state The source application state to read values from.
 */
void PauseDocumentController::refreshFromState(const AppState& state) {
    screen_ = state.screen;
    selection_ = state.pauseSelection;
    confirmSelection_ = state.confirmSelection;
    pauseContext_ = state.pauseContext;
    noticeText_ = state.noticeTimer > 0.0f ? state.noticeText : std::string();
}

/**
 * @brief Update the bound RML document to reflect the controller's current pause UI state.
 *
 * Updates element classes, visible text, and selection indicators for:
 * - pause buttons (sets `is-selected` on matching pause action),
 * - pause overlay (`fade-in`),
 * - confirm container (`is-visible`),
 * - toast (`pause-toast`) inner RML and `is-visible` state based on `noticeText_`,
 * - confirm dialog title and body text depending on whether the screen is an overwrite-confirm,
 * - confirm buttons and their separate label elements (selection classes and label text).
 */
void PauseDocumentController::refreshDocument() const {
    if (document_ == nullptr) {
        return;
    }

    for (const PauseButtonDefinition& button : kPauseButtons) {
        if (Rml::Element* element = document_->GetElementById(button.id)) {
            element->SetClass("is-selected", button.action == selection_);
            const bool visible =
                pauseContext_ != PauseContext::Battle ||
                button.action != PauseAction::Save;
            element->SetProperty("display", visible ? "block" : "none");
            element->SetClass(
                "is-disabled",
                pauseContext_ == PauseContext::Battle && button.action == PauseAction::Load);
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
            screen_ == ScreenState::PauseConfirmOverwriteSave
                ? "Overwrite Save?"
                : (pauseContext_ == PauseContext::Battle ? "Exit Battle?" : "Exit To Main Menu?"));
    }
    if (Rml::Element* element = document_->GetElementById("pause-confirm-body")) {
        element->SetInnerRML(
            screen_ == ScreenState::PauseConfirmOverwriteSave
                ? "A manual save already exists at this story point.<br/>Overwrite that save file?"
                : (pauseContext_ == PauseContext::Battle
                    ? "You will lose the current battle progress if you leave now."
                    : "You will lose the current chapter progress if you leave now."));
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

void PauseDocumentController::setPauseSelection(PauseAction action, bool playSound) {
    if (selection_ == action) {
        return;
    }

    selection_ = action;
    refreshDocument();
    if (playSound) {
        queueSound(kScrollSfxPath, 0.82f);
    }
}

void PauseDocumentController::setConfirmSelection(ConfirmAction action, bool playSound) {
    if (confirmSelection_ == action) {
        return;
    }

    confirmSelection_ = action;
    refreshDocument();
    if (playSound) {
        queueSound(kScrollSfxPath, 0.82f);
    }
}

std::optional<PauseAction> PauseDocumentController::hitTestPauseButton(float x, float y) const {
    if (document_ == nullptr || showingConfirm()) {
        return std::nullopt;
    }

    for (const PauseButtonDefinition& button : kPauseButtons) {
        const bool visible =
            pauseContext_ != PauseContext::Battle ||
            button.action != PauseAction::Save;
        if (!visible) {
            continue;
        }

        Rml::Element* element = document_->GetElementById(button.id);
        if (element == nullptr) {
            continue;
        }

        Rml::Vector2f point{x, y};
        if (!element->Project(point)) {
            continue;
        }
        if (element->IsPointWithinElement(point)) {
            return button.action;
        }
    }

    return std::nullopt;
}

std::optional<ConfirmAction> PauseDocumentController::hitTestConfirmButton(float x, float y) const {
    if (document_ == nullptr || !showingConfirm()) {
        return std::nullopt;
    }

    for (const ConfirmButtonDefinition& button : kConfirmButtons) {
        Rml::Element* element = document_->GetElementById(button.id);
        if (element == nullptr) {
            continue;
        }

        Rml::Vector2f point{x, y};
        if (!element->Project(point)) {
            continue;
        }
        if (element->IsPointWithinElement(point)) {
            return button.action;
        }
    }

    return std::nullopt;
}

PauseAction PauseDocumentController::nextAction(int delta) const {
    const PauseAction* order = nullptr;
    std::size_t count = 0;
    if (pauseContext_ == PauseContext::Battle) {
        order = kBattlePauseOrder.data();
        count = kBattlePauseOrder.size();
    } else {
        order = kStoryPauseOrder.data();
        count = kStoryPauseOrder.size();
    }
    const int countInt = static_cast<int>(count);
    int index = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (order[i] == selection_) {
            index = static_cast<int>(i);
            break;
        }
    }
    index = (index + delta) % countInt;
    if (index < 0) {
        index += countInt;
    }
    return order[static_cast<std::size_t>(index)];
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

/**
 * @brief Schedule a confirm action to be applied on the next call to applyState.
 *
 * @param action The confirm action to enqueue (e.g., Cancel, ExitToMainMenu).
 */
void PauseDocumentController::queueConfirmAction(ConfirmAction action) {
    pendingConfirmAction_ = action;
}

/**
 * @brief Queue a sound request to be played later by the controller.
 *
 * Appends a sound request (path and volume) to the controller's pending sound queue.
 *
 * @param path Filesystem or resource path to the sound asset.
 * @param volume Playback volume in the range [0.0, 1.0].
 */
void PauseDocumentController::queueSound(const char* path, float volume) {
    pendingSoundRequests_.push_back(SoundRequest{path, volume});
}

/**
 * @brief Collects and clears all queued sound requests.
 *
 * Moves the controller's pending sound requests into a returned vector and clears the internal queue.
 *
 * @return A vector of queued SoundRequest objects; the controller's pending queue is emptied.
 */
std::vector<SoundRequest> PauseDocumentController::consumeSoundRequests() {
    std::vector<SoundRequest> requests = std::move(pendingSoundRequests_);
    pendingSoundRequests_.clear();
    return requests;
}

/**
 * @brief Prepare application state to exit the current story and return to the main menu.
 *
 * Unpauses the engine and mutates the provided AppState to request a story exit to the main menu.
 * The following fields are set:
 * - `pauseSelection` -> `PauseAction::Continue`
 * - `confirmSelection` -> `ConfirmAction::Cancel`
 * - `settingsReturnScreen` -> `ScreenState::MainMenu`
 * - `mainSelection` -> `MainMenuAction::Start`
 * - `requestStoryExitToMainMenu` -> `true`
 *
 * @param state AppState to modify to trigger the exit-to-main-menu flow.
 */
void PauseDocumentController::exitStoryToMainMenu(AppState& state) const {
    vn::setPaused(false);
    state.pauseSelection = PauseAction::Continue;
    state.confirmSelection = ConfirmAction::Cancel;
    state.settingsReturnScreen = ScreenState::MainMenu;
    state.mainSelection = MainMenuAction::Start;
    if (pauseContext_ == PauseContext::Battle) {
        state.screen = ScreenState::MainMenu;
        state.pauseContext = PauseContext::Story;
        state.mainSelection = MainMenuAction::Battle;
        state.noticeText = "Current battle was discarded.";
        state.noticeTimer = 2.6f;
    } else {
        state.requestStoryExitToMainMenu = true;
    }
}

}  // namespace graphics::frontui
