#include "front_ui_settings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

namespace graphics::frontui {
namespace {

constexpr float kMinTextSpeed = 18.0f;
constexpr float kMaxTextSpeed = 90.0f;
constexpr const char* kSelectionSfxPath = "assets/ui/sfx/Keyboard_select-word.wav";
constexpr const char* kAdjustSfxPath = "assets/ui/sfx/Keyboard_select-all.wav";
constexpr const char* kBackSfxPath = "assets/ui/sfx/Menu_back-to-top.wav";

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

struct SettingsRowDefinition {
    SettingsItem item;
    const char* rowId;
    const char* codeId;
    const char* labelId;
    const char* valueId;
    const char* sliderId;
    const char* code;
    const char* label;
    const char* focusTitle;
    const char* focusBody;
};

constexpr std::array<SettingsRowDefinition, 5> kRows{{
    {SettingsItem::DisplayMode,
     "settings-row-display",
     "settings-code-display",
     "settings-label-display",
     "settings-display-value",
     nullptr,
     "01",
     "DISPLAY MODE",
     "DISPLAY MODE",
     "Toggle between windowed mode and fullscreen rendering."},
    {SettingsItem::MusicVolume,
     "settings-row-music",
     "settings-code-music",
     "settings-label-music",
     "settings-music-value",
     "settings-music-slider",
     "02",
     "MUSIC VOLUME",
     "MUSIC VOLUME",
     "Adjust menu, story, and battle background music volume without flattening the mix."},
    {SettingsItem::VoiceVolume,
     "settings-row-voice",
     "settings-code-voice",
     "settings-label-voice",
     "settings-voice-value",
     "settings-voice-slider",
     "03",
     "VOICE VOLUME",
     "VOICE VOLUME",
     "Adjust spoken dialogue volume for the visual novel and battle scenes."},
    {SettingsItem::TextSpeed,
     "settings-row-text",
     "settings-code-text",
     "settings-label-text",
     "settings-text-value",
     "settings-text-slider",
     "04",
     "TEXT SPEED",
     "TEXT SPEED",
     "Set the dialogue reveal speed in characters per second."},
    {SettingsItem::Back,
     "settings-row-back",
     "settings-code-back",
     "settings-label-back",
     nullptr,
     nullptr,
     "05",
     "BACK",
     "RETURN",
     "Leave settings and return to the previous front-ui screen."}
}};

constexpr int itemIndex(SettingsItem item) {
    return static_cast<int>(item);
}

SettingsItem itemForIndex(int index) {
    const int count = static_cast<int>(kRows.size());
    int wrapped = index % count;
    if (wrapped < 0) {
        wrapped += count;
    }
    return kRows[static_cast<std::size_t>(wrapped)].item;
}

const SettingsRowDefinition& rowFor(SettingsItem item) {
    return kRows[static_cast<std::size_t>(itemIndex(item))];
}

/**
 * @brief Determines whether two GameSettings instances represent the same settings.
 *
 * Compares fullscreen for exact equality and compares music volume, voice volume,
 * and text speed using a tolerance of 0.0001.
 *
 * @param lhs Left-hand GameSettings to compare.
 * @param rhs Right-hand GameSettings to compare.
 * @return true if fullscreen matches exactly and the other numeric fields differ by less than 0.0001, false otherwise.
 */
bool settingsEqual(const GameSettings& lhs, const GameSettings& rhs) {
    return lhs.fullscreen == rhs.fullscreen &&
           std::fabs(lhs.musicVolume - rhs.musicVolume) < 0.0001f &&
           std::fabs(lhs.voiceVolume - rhs.voiceVolume) < 0.0001f &&
           std::fabs(lhs.textSpeed - rhs.textSpeed) < 0.0001f;
}

std::string formatDisplayMode(bool fullscreen) {
    return fullscreen ? "FULLSCREEN" : "WINDOWED";
}

std::string formatPercent(float value) {
    return std::to_string(static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 100.0f))) + "%";
}

std::string formatCharsPerSecond(float value) {
    return std::to_string(static_cast<int>(std::lround(value))) + " CPS";
}

/**
 * @brief Clamp a voice volume value into the valid range.
 *
 * @param value Input volume value.
 * @return float Value clamped to the inclusive range [0.0, 1.0].
 */
float clampVoice(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

/**
 * @brief Clamp a music volume value to the valid range.
 *
 * @param value Desired music volume where 0.0 is silence and 1.0 is full volume.
 * @return float The value clamped to the range [0.0, 1.0].
 */
float clampMusic(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

/**
 * @brief Clamps a text display speed to the allowed characters-per-second range.
 *
 * @param value Desired text speed in characters per second.
 * @return float Value clamped to the inclusive range [kMinTextSpeed, kMaxTextSpeed].
 */
float clampTextSpeed(float value) {
    return std::clamp(value, kMinTextSpeed, kMaxTextSpeed);
}

}  /**
 * @brief Bind the controller to a document and initialize its UI state from the application state.
 *
 * Initializes internal pointers and working settings, clears pending commands and sound requests,
 * attaches event listeners, and synchronizes the document's UI to the current settings.
 *
 * @param document The Rml document to bind to; the controller will cache a pointer to this document.
 * @param state Source application state used to initialize selection, settings, and return-screen.
 * @return true if the controller was successfully bound to the document and initialized.
 */

bool SettingsDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
    pendingCommand_.reset();
    pendingSoundRequests_.clear();
    pressedRowItem_.reset();
    activeSliderItem_.reset();
    selection_ = state.settingsSelection;
    workingSettings_ = state.settings;
    returnScreen_ = state.settingsReturnScreen;

    applyRowCopy();
    attachListeners();
    applySelectionStyles();
    syncControlValues();
    updateFocusCopy();
    return true;
}

/**
 * @brief Detaches all UI event listeners, clears queued sound requests, and disassociates the controller from its document.
 *
 * After calling this, the controller will not receive DOM events, there will be no pending sound requests retained, and the internal document pointer will be set to null.
 */
void SettingsDocumentController::unbind() {
    detachEventListeners(listeners_);
    pendingSoundRequests_.clear();
    pressedRowItem_.reset();
    activeSliderItem_.reset();
    document_ = nullptr;
}

/**
 * @brief Synchronizes the controller with the given application state and refreshes the UI when needed.
 *
 * Updates the controller's selection, working settings, and return-screen to match the provided AppState.
 * If any of those fields change, refreshes the document's row copy, selection styling, control values, and focus copy.
 *
 * @param state Source application state to synchronize from.
 */
void SettingsDocumentController::sync(const AppState& state) {
    bool needsRefresh = false;

    if (selection_ != state.settingsSelection) {
        selection_ = state.settingsSelection;
        needsRefresh = true;
    }

    if (!settingsEqual(workingSettings_, state.settings)) {
        workingSettings_ = state.settings;
        needsRefresh = true;
    }

    if (returnScreen_ != state.settingsReturnScreen) {
        returnScreen_ = state.settingsReturnScreen;
        needsRefresh = true;
    }

    if (needsRefresh) {
        applyRowCopy();
        applySelectionStyles();
        syncControlValues();
        updateFocusCopy();
    }
}

void SettingsDocumentController::update(const AppState& state, float deltaSeconds) {
    (void)state;
    (void)deltaSeconds;
}

/**
 * @brief Move the current settings selection by a relative number of rows.
 *
 * Advances or retreats the active selection by the given number of positions in the row list, wrapping around if needed, and plays the selection sound when a change occurs.
 *
 * @param delta Number of positions to move: positive to move forward, negative to move backward. A value of zero has no effect.
 */
void SettingsDocumentController::moveSelection(int delta) {
    if (delta == 0) {
        return;
    }
    setSelection(itemForIndex(itemIndex(selection_) + delta), true);
}

/**
 * @brief Adjusts the currently selected setting by the given step delta.
 *
 * Positive delta increases and negative delta decreases the selected value; zero is a no-op.
 *
 * Behavior by selection:
 * - DisplayMode: requests fullscreen when delta > 0 or windowed when delta < 0.
 * - MusicVolume, VoiceVolume: increments the volume by 0.05 per step (result constrained to the valid volume range).
 * - TextSpeed: increments text speed by 6.0 CPS per step (result constrained to the valid text-speed range).
 * - Back: no effect.
 *
 * @param delta Number of adjustment steps; positive to increase, negative to decrease, zero does nothing.
 */
void SettingsDocumentController::adjustSelection(int delta) {
    if (delta == 0) {
        return;
    }

    switch (selection_) {
        case SettingsItem::DisplayMode:
            queueDisplayMode(delta > 0);
            break;

        case SettingsItem::MusicVolume:
            setMusicVolume(workingSettings_.musicVolume + 0.05f * static_cast<float>(delta));
            break;

        case SettingsItem::VoiceVolume:
            setVoiceVolume(workingSettings_.voiceVolume + 0.05f * static_cast<float>(delta));
            break;

        case SettingsItem::TextSpeed:
            setTextSpeed(workingSettings_.textSpeed + 6.0f * static_cast<float>(delta));
            break;

        case SettingsItem::Back:
            break;
    }
}

/**
 * @brief Perform the primary action for the currently selected settings item.
 *
 * If the DisplayMode row is selected, toggles the fullscreen setting.
 * If the Back row is selected, initiates returning from the settings screen.
 * If a volume or text-speed row is selected, no action is taken.
 */
void SettingsDocumentController::activateSelection() {
    switch (selection_) {
        case SettingsItem::DisplayMode:
            queueDisplayMode(!workingSettings_.fullscreen);
            break;

        case SettingsItem::Back:
            queueReturn();
            break;

        case SettingsItem::MusicVolume:
        case SettingsItem::VoiceVolume:
        case SettingsItem::TextSpeed:
            break;
    }
}

void SettingsDocumentController::handleMouseMotion(const SDL_MouseMotionEvent& event) {
    const float mouseX = static_cast<float>(event.x);
    const float mouseY = static_cast<float>(event.y);

    if (activeSliderItem_.has_value()) {
        updateSliderDrag(mouseX, mouseY, false);
        return;
    }

    if (const std::optional<SettingsItem> hoveredSlider = hitTestSlider(mouseX, mouseY);
        hoveredSlider.has_value()) {
        setSelection(*hoveredSlider, true);
        return;
    }

    if (const std::optional<SettingsItem> hoveredRow = hitTestRow(mouseX, mouseY);
        hoveredRow.has_value()) {
        setSelection(*hoveredRow, true);
    }
}

void SettingsDocumentController::handleMouseButtonDown(const SDL_MouseButtonEvent& event) {
    if (event.button != SDL_BUTTON_LEFT) {
        return;
    }

    const float mouseX = static_cast<float>(event.x);
    const float mouseY = static_cast<float>(event.y);

    pressedRowItem_.reset();
    activeSliderItem_.reset();

    if (const std::optional<SettingsItem> sliderItem = hitTestSlider(mouseX, mouseY);
        sliderItem.has_value()) {
        beginSliderDrag(*sliderItem, mouseX, mouseY);
        return;
    }

    pressedRowItem_ = hitTestRow(mouseX, mouseY);
    if (pressedRowItem_.has_value()) {
        setSelection(*pressedRowItem_, true);
    }
}

void SettingsDocumentController::handleMouseButtonUp(const SDL_MouseButtonEvent& event) {
    if (event.button != SDL_BUTTON_LEFT) {
        return;
    }

    const float mouseX = static_cast<float>(event.x);
    const float mouseY = static_cast<float>(event.y);

    if (activeSliderItem_.has_value()) {
        endSliderDrag(mouseX, mouseY);
        pressedRowItem_.reset();
        return;
    }

    const std::optional<SettingsItem> releasedRow = hitTestRow(mouseX, mouseY);
    if (pressedRowItem_.has_value() && releasedRow == pressedRowItem_) {
        setSelection(*releasedRow, false);
        if (pendingCommand_ == std::nullopt &&
            (*releasedRow == SettingsItem::DisplayMode || *releasedRow == SettingsItem::Back)) {
            activateSelection();
        }
    }
    pressedRowItem_.reset();
}

void SettingsDocumentController::cancel() {
    queueReturn();
}

/**
 * @brief Writes the controller's current selection and working settings back into the provided AppState.
 *
 * @param state Application state to receive the controller's selection, fullscreen flag, music volume, voice volume, and text speed.
 */
void SettingsDocumentController::applyState(AppState& state) {
    state.settingsSelection = selection_;
    state.settings.fullscreen = workingSettings_.fullscreen;
    state.settings.musicVolume = workingSettings_.musicVolume;
    state.settings.voiceVolume = workingSettings_.voiceVolume;
    state.settings.textSpeed = workingSettings_.textSpeed;
}

/**
 * @brief Retrieves and clears the currently queued command.
 *
 * Consumes the controller's pending command: returns the stored command and resets internal pending state.
 *
 * @return std::optional<Command> The pending `Command` if one was queued, `std::nullopt` otherwise.
 */
std::optional<Command> SettingsDocumentController::consumeCommand() {
    const std::optional<Command> result = pendingCommand_;
    pendingCommand_.reset();
    return result;
}

/**
 * @brief Retrieve and clear queued sound requests.
 *
 * Returns the list of sound requests that were pending in the controller and clears the controller's internal queue.
 *
 * @return std::vector<SoundRequest> The previously queued SoundRequest objects; the controller's pending list is empty after this call.
 */
std::vector<SoundRequest> SettingsDocumentController::consumeSoundRequests() {
    std::vector<SoundRequest> requests = std::move(pendingSoundRequests_);
    pendingSoundRequests_.clear();
    return requests;
}

std::optional<SettingsItem> SettingsDocumentController::hitTestRow(float x, float y) const {
    if (document_ == nullptr) {
        return std::nullopt;
    }

    for (const SettingsRowDefinition& row : kRows) {
        Rml::Element* element = document_->GetElementById(row.rowId);
        if (element == nullptr) {
            continue;
        }

        Rml::Vector2f point{x, y};
        if (!element->Project(point)) {
            continue;
        }
        if (element->IsPointWithinElement(point)) {
            return row.item;
        }
    }

    return std::nullopt;
}

std::optional<SettingsItem> SettingsDocumentController::hitTestSlider(float x, float y) const {
    if (document_ == nullptr) {
        return std::nullopt;
    }

    for (const SettingsRowDefinition& row : kRows) {
        if (row.sliderId == nullptr) {
            continue;
        }

        Rml::Element* slider = document_->GetElementById(row.sliderId);
        if (slider == nullptr) {
            continue;
        }

        Rml::Vector2f point{x, y};
        if (!slider->Project(point)) {
            continue;
        }
        if (slider->IsPointWithinElement(point)) {
            return row.item;
        }
    }

    return std::nullopt;
}

void SettingsDocumentController::beginSliderDrag(SettingsItem item, float x, float y) {
    activeSliderItem_ = item;
    pressedRowItem_ = item;
    setSelection(item, true);
    (void)applySliderValueAtPoint(item, x, y, true, true);
}

void SettingsDocumentController::updateSliderDrag(float x, float y, bool playSound) {
    if (!activeSliderItem_.has_value()) {
        return;
    }

    setSelection(*activeSliderItem_, false);
    (void)applySliderValueAtPoint(*activeSliderItem_, x, y, false, playSound);
}

void SettingsDocumentController::endSliderDrag(float x, float y) {
    if (!activeSliderItem_.has_value()) {
        return;
    }

    (void)applySliderValueAtPoint(*activeSliderItem_, x, y, false, false);
    activeSliderItem_.reset();
}

bool SettingsDocumentController::applySliderValueAtPoint(SettingsItem item,
                                                         float x,
                                                         float y,
                                                         bool requireInside,
                                                         bool playSound) {
    if (document_ == nullptr) {
        return false;
    }

    const char* sliderId = rowFor(item).sliderId;
    if (sliderId == nullptr) {
        return false;
    }

    Rml::Element* slider = document_->GetElementById(sliderId);
    if (slider == nullptr) {
        return false;
    }

    Rml::Vector2f point{x, y};
    if (!slider->Project(point)) {
        return false;
    }
    if (requireInside && !slider->IsPointWithinElement(point)) {
        return false;
    }

    const float sliderLeft = slider->GetAbsoluteLeft();
    const float sliderWidth = std::max(1.0f, slider->GetClientWidth());
    const float fraction = std::clamp((point.x - sliderLeft) / sliderWidth, 0.0f, 1.0f);

    switch (item) {
        case SettingsItem::MusicVolume:
            setMusicVolume(fraction, playSound);
            return true;

        case SettingsItem::VoiceVolume:
            setVoiceVolume(fraction, playSound);
            return true;

        case SettingsItem::TextSpeed:
            setTextSpeed(kMinTextSpeed + (kMaxTextSpeed - kMinTextSpeed) * fraction, playSound);
            return true;

        case SettingsItem::DisplayMode:
        case SettingsItem::Back:
            break;
    }

    return false;
}

/**
 * @brief Attach UI event listeners for each settings row and its sliders.
 *
 * @details If no document is bound, this is a no-op. For every row defined in
 * `kRows` it:
 * - Adds a mouseover listener that selects the row and plays the selection sound.
 * - For the DisplayMode row, adds a click listener that queues a display-mode toggle.
 * - For the Back row, adds a click listener that queues a return command.
 * - For rows with an associated slider element, adds a mouseover listener that
 *   selects the row and a change listener that reads the slider's form-control
 *   value, parses it as a float, and applies it to the appropriate setting:
 *   MusicVolume (value interpreted as percentage and divided by 100), VoiceVolume
 *   (percentage divided by 100), or TextSpeed (raw value).
 *
 * Successfully created listeners are stored in `listeners_` so they can be
 * detached later. Slider value parse failures are silently ignored.
 */
void SettingsDocumentController::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    for (const SettingsRowDefinition& row : kRows) {
        if (Rml::Element* element = document_->GetElementById(row.rowId)) {
            auto hoverListener = std::make_unique<CallbackEventListener>([this, item = row.item](Rml::Event&) {
                setSelection(item, true);
            });
            element->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Mouseover,
                false,
                std::move(hoverListener),
            });

            if (row.item == SettingsItem::DisplayMode) {
                auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
                    queueDisplayMode(!workingSettings_.fullscreen);
                });
                element->AddEventListener(Rml::EventId::Click, clickListener.get());
                listeners_.push_back(EventListenerBinding{
                    element,
                    Rml::EventId::Click,
                    false,
                    std::move(clickListener),
                });
            } else if (row.item == SettingsItem::Back) {
                auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
                    queueReturn();
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

        if (row.sliderId == nullptr) {
            continue;
        }

        if (Rml::Element* slider = document_->GetElementById(row.sliderId)) {
            auto hoverListener = std::make_unique<CallbackEventListener>([this, item = row.item](Rml::Event&) {
                setSelection(item, true);
            });
            slider->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
            listeners_.push_back(EventListenerBinding{
                slider,
                Rml::EventId::Mouseover,
                false,
                std::move(hoverListener),
            });

            auto changeListener = std::make_unique<CallbackEventListener>([this, item = row.item](Rml::Event& event) {
                if (auto* control = dynamic_cast<Rml::ElementFormControl*>(event.GetTargetElement())) {
                    const std::string value = control->GetValue();
                    try {
                        const float parsed = std::stof(value);
                        if (item == SettingsItem::MusicVolume) {
                            setMusicVolume(parsed / 100.0f);
                        } else if (item == SettingsItem::VoiceVolume) {
                            setVoiceVolume(parsed / 100.0f);
                        } else if (item == SettingsItem::TextSpeed) {
                            setTextSpeed(parsed);
                        }
                    } catch (...) {
                    }
                }
            });
            slider->AddEventListener(Rml::EventId::Change, changeListener.get());
            listeners_.push_back(EventListenerBinding{
                slider,
                Rml::EventId::Change,
                false,
                std::move(changeListener),
            });
        }
    }
}

/**
 * @brief Update DOM row elements to reflect the current selection by toggling the "is-selected" CSS class.
 *
 * Iterates the configured settings rows and sets the "is-selected" class on the DOM element for a row when
 * that row matches the controller's current selection; clears the class for other rows. If the document is
 * not bound, the function is a no-op.
 */
void SettingsDocumentController::applySelectionStyles() const {
    if (document_ == nullptr) {
        return;
    }

    for (const SettingsRowDefinition& row : kRows) {
        if (Rml::Element* element = document_->GetElementById(row.rowId)) {
            element->SetClass("is-selected", row.item == selection_);
        }
    }
}

/**
 * @brief Updates the document's row labels and codes to reflect current row copy and return-screen state.
 *
 * Updates each row's code and label elements in the bound Rml document. For the Back row, the label
 * is set to "RETURN TO MAIN MENU" when the return screen is MainMenu, otherwise "BACK". Also updates
 * the "settings-back-value" element to "RETURN" when returning to the main menu, otherwise "BACK".
 */
void SettingsDocumentController::applyRowCopy() const {
    if (document_ == nullptr) {
        return;
    }

    for (const SettingsRowDefinition& row : kRows) {
        if (Rml::Element* element = document_->GetElementById(row.codeId)) {
            element->SetInnerRML(row.code);
        }
        if (Rml::Element* element = document_->GetElementById(row.labelId)) {
            if (row.item == SettingsItem::Back) {
                element->SetInnerRML(returnScreen_ == ScreenState::MainMenu ? "RETURN TO MAIN MENU" : "BACK");
            } else {
                element->SetInnerRML(row.label);
            }
        }
    }

    if (Rml::Element* element = document_->GetElementById("settings-back-value")) {
        element->SetInnerRML(returnScreen_ == ScreenState::MainMenu ? "RETURN" : "BACK");
    }
}

/**
 * @brief Update the focus pane copy (code, title, and descriptive body) to reflect the current selection.
 *
 * If no document is bound, this is a no-op. For the Back selection when the return screen is MainMenu,
 * the title and body are replaced with a special "RETURN TO MAIN MENU" title and a specific explanatory body;
 * otherwise the selected row's configured focus title and body are used.
 */
void SettingsDocumentController::updateFocusCopy() const {
    if (document_ == nullptr) {
        return;
    }

    const SettingsRowDefinition& row = rowFor(selection_);
    if (Rml::Element* element = document_->GetElementById("settings-focus-code")) {
        element->SetInnerRML(row.code);
    }
    if (Rml::Element* element = document_->GetElementById("settings-focus-title")) {
        if (selection_ == SettingsItem::Back && returnScreen_ == ScreenState::MainMenu) {
            element->SetInnerRML("RETURN TO MAIN MENU");
        } else {
            element->SetInnerRML(row.focusTitle);
        }
    }
    if (Rml::Element* element = document_->GetElementById("settings-focus-copy")) {
        if (selection_ == SettingsItem::Back && returnScreen_ == ScreenState::MainMenu) {
            element->SetInnerRML("Leave settings and bring the play menu back into place without a screen fade.");
        } else {
            element->SetInnerRML(row.focusBody);
        }
    }
}

/**
 * @brief Update all visible settings controls and preview/context text to match the controller's working settings and selection.
 *
 * Updates display-mode, music/voice percent strings, text-speed CPS, slider element values, preview main value and chip,
 * preview fill widths (music/voice/text normalized), and contextual labels that reflect the current workingSettings_,
 * the active selection, and the returnScreen state.
 *
 * If no document is bound, the method does nothing.
 */
void SettingsDocumentController::syncControlValues() const {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("settings-display-value")) {
        element->SetInnerRML(formatDisplayMode(workingSettings_.fullscreen));
    }
    if (Rml::Element* element = document_->GetElementById("settings-music-value")) {
        element->SetInnerRML(formatPercent(workingSettings_.musicVolume));
    }
    if (Rml::Element* element = document_->GetElementById("settings-voice-value")) {
        element->SetInnerRML(formatPercent(workingSettings_.voiceVolume));
    }
    if (Rml::Element* element = document_->GetElementById("settings-text-value")) {
        element->SetInnerRML(formatCharsPerSecond(workingSettings_.textSpeed));
    }
    if (auto* control = dynamic_cast<Rml::ElementFormControl*>(document_->GetElementById("settings-music-slider"))) {
        control->SetValue(std::to_string(static_cast<int>(std::lround(clampMusic(workingSettings_.musicVolume) * 100.0f))));
    }
    if (auto* control = dynamic_cast<Rml::ElementFormControl*>(document_->GetElementById("settings-voice-slider"))) {
        control->SetValue(std::to_string(static_cast<int>(std::lround(clampVoice(workingSettings_.voiceVolume) * 100.0f))));
    }
    if (auto* control = dynamic_cast<Rml::ElementFormControl*>(document_->GetElementById("settings-text-slider"))) {
        control->SetValue(std::to_string(static_cast<int>(std::lround(clampTextSpeed(workingSettings_.textSpeed)))));
    }
    if (Rml::Element* element = document_->GetElementById("settings-preview-main-value")) {
        element->SetInnerRML(selection_ == SettingsItem::DisplayMode
            ? formatDisplayMode(workingSettings_.fullscreen)
            : (selection_ == SettingsItem::MusicVolume
                ? formatPercent(workingSettings_.musicVolume)
                : (selection_ == SettingsItem::VoiceVolume
                ? formatPercent(workingSettings_.voiceVolume)
                : (selection_ == SettingsItem::TextSpeed
                    ? formatCharsPerSecond(workingSettings_.textSpeed)
                    : (returnScreen_ == ScreenState::MainMenu ? "MAIN MENU" : "RETURN")))));
    }
    if (Rml::Element* element = document_->GetElementById("settings-preview-chip")) {
        element->SetInnerRML(selection_ == SettingsItem::Back ? "RETURN ROUTE" : "LIVE VALUE");
    }
    if (Rml::Element* element = document_->GetElementById("settings-preview-music-fill")) {
        element->SetProperty("width", formatPercent(workingSettings_.musicVolume));
    }
    if (Rml::Element* element = document_->GetElementById("settings-preview-voice-fill")) {
        element->SetProperty("width", formatPercent(workingSettings_.voiceVolume));
    }
    if (Rml::Element* element = document_->GetElementById("settings-preview-text-fill")) {
        const float normalized = (clampTextSpeed(workingSettings_.textSpeed) - kMinTextSpeed) /
                                 (kMaxTextSpeed - kMinTextSpeed);
        element->SetProperty("width", formatPercent(normalized));
    }
    if (Rml::Element* element = document_->GetElementById("settings-context-display")) {
        element->SetInnerRML(formatDisplayMode(workingSettings_.fullscreen));
    }
    if (Rml::Element* element = document_->GetElementById("settings-context-music")) {
        element->SetInnerRML(formatPercent(workingSettings_.musicVolume));
    }
    if (Rml::Element* element = document_->GetElementById("settings-context-voice")) {
        element->SetInnerRML(formatPercent(workingSettings_.voiceVolume));
    }
    if (Rml::Element* element = document_->GetElementById("settings-context-speed")) {
        element->SetInnerRML(formatCharsPerSecond(workingSettings_.textSpeed));
    }
    if (Rml::Element* element = document_->GetElementById("settings-context-return")) {
        element->SetInnerRML(returnScreen_ == ScreenState::MainMenu ? "Main Menu" : "Previous Screen");
    }
}

/**
 * @brief Change the currently highlighted settings row and refresh related UI.
 *
 * Updates internal selection state, reapplies selection styling, updates the focus panel
 * copy, and synchronizes control values. Optionally queues the selection sound.
 *
 * @param selection The settings item to select (which row becomes highlighted).
 * @param playSound If `true`, enqueue the selection sound effect.
 */
void SettingsDocumentController::setSelection(SettingsItem selection, bool playSound) {
    if (selection_ == selection) {
        return;
    }

    selection_ = selection;
    applySelectionStyles();
    updateFocusCopy();
    syncControlValues();
    if (playSound) {
        queueSound(kSelectionSfxPath, 0.88f);
    }
}

/**
 * @brief Request a change to the display mode and update UI state.
 *
 * If the requested fullscreen state differs from the current working setting,
 * update the working setting, enqueue a pending ApplyDisplayMode command,
 * refresh bound UI elements, and queue the adjust sound effect.
 *
 * @param fullscreen Desired fullscreen state; no action is taken if it equals the current state.
 */
void SettingsDocumentController::queueDisplayMode(bool fullscreen) {
    if (workingSettings_.fullscreen == fullscreen) {
        return;
    }
    workingSettings_.fullscreen = fullscreen;
    pendingCommand_ = Command{CommandType::ApplyDisplayMode, MainMenuAction::Start, fullscreen};
    syncControlValues();
    updateFocusCopy();
    queueSound(kAdjustSfxPath, 0.9f);
}

/**
 * @brief Request a return from the settings screen.
 *
 * Marks that the settings UI should be exited (a return command will be consumed
 * by the caller) and enqueues the back navigation sound effect.
 */
void SettingsDocumentController::queueReturn() {
    pendingCommand_ = Command{CommandType::ReturnFromSettings};
    queueSound(kBackSfxPath, 0.92f);
}

/**
 * @brief Set the working music volume, updating UI state and playing an adjustment sound.
 *
 * Clamps the provided value to the valid `[0, 1]` range, updates the controller's working
 * settings when the change is significant, refreshes displayed controls/focus text, and
 * queues the adjust sound effect.
 *
 * @param value Desired music volume (0.0 to 1.0); values outside this range will be clamped.
 */
void SettingsDocumentController::setMusicVolume(float value, bool playSound) {
    const float clamped = clampMusic(value);
    if (std::fabs(workingSettings_.musicVolume - clamped) < 0.001f) {
        return;
    }
    workingSettings_.musicVolume = clamped;
    syncControlValues();
    updateFocusCopy();
    if (playSound) {
        queueSound(kAdjustSfxPath, 0.84f);
    }
}

/**
 * @brief Update the working voice volume, refresh visible controls and focus copy, and queue an adjustment sound.
 *
 * Does nothing if the new clamped volume differs from the current value by less than 0.001.
 *
 * @param value Desired voice volume; this value is clamped to the valid voice-volume range before being applied.
 */
void SettingsDocumentController::setVoiceVolume(float value, bool playSound) {
    const float clamped = clampVoice(value);
    if (std::fabs(workingSettings_.voiceVolume - clamped) < 0.001f) {
        return;
    }
    workingSettings_.voiceVolume = clamped;
    syncControlValues();
    updateFocusCopy();
    if (playSound) {
        queueSound(kAdjustSfxPath, 0.84f);
    }
}

/**
 * @brief Set the text rendering speed used by the settings controller.
 *
 * Clamps the provided speed to the allowed range and updates the controller's
 * working settings if the value changes by at least 0.001. When applied, the
 * UI is synchronized, focus copy is refreshed, and an adjustment sound is queued.
 *
 * @param value Desired text speed in characters per second; will be clamped to [kMinTextSpeed, kMaxTextSpeed].
 */
void SettingsDocumentController::setTextSpeed(float value, bool playSound) {
    const float clamped = clampTextSpeed(value);
    if (std::fabs(workingSettings_.textSpeed - clamped) < 0.001f) {
        return;
    }
    workingSettings_.textSpeed = clamped;
    syncControlValues();
    updateFocusCopy();
    if (playSound) {
        queueSound(kAdjustSfxPath, 0.84f);
    }
}

/**
 * @brief Enqueue a sound request to be played by the consumer of pending sound requests.
 *
 * Adds a SoundRequest with the given sound file path and volume to the controller's internal
 * pending sound queue; these requests are later retrieved via consumeSoundRequests().
 *
 * @param path Filesystem or resource path to the sound effect.
 * @param volume Playback volume multiplier where `1.0` represents the original volume (values
 *               below or above 1.0 scale the playback volume accordingly).
 */
void SettingsDocumentController::queueSound(const char* path, float volume) {
    pendingSoundRequests_.push_back(SoundRequest{path, volume});
}

}  // namespace graphics::frontui
