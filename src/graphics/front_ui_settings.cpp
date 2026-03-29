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

constexpr std::array<SettingsRowDefinition, 4> kRows{{
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
    {SettingsItem::VoiceVolume,
     "settings-row-voice",
     "settings-code-voice",
     "settings-label-voice",
     "settings-voice-value",
     "settings-voice-slider",
     "02",
     "VOICE VOLUME",
     "VOICE VOLUME",
     "Adjust spoken dialogue volume for the visual novel and battle scenes."},
    {SettingsItem::TextSpeed,
     "settings-row-text",
     "settings-code-text",
     "settings-label-text",
     "settings-text-value",
     "settings-text-slider",
     "03",
     "TEXT SPEED",
     "TEXT SPEED",
     "Set the dialogue reveal speed in characters per second."},
    {SettingsItem::Back,
     "settings-row-back",
     "settings-code-back",
     "settings-label-back",
     nullptr,
     nullptr,
     "04",
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

bool settingsEqual(const GameSettings& lhs, const GameSettings& rhs) {
    return lhs.fullscreen == rhs.fullscreen &&
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

float clampVoice(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float clampTextSpeed(float value) {
    return std::clamp(value, kMinTextSpeed, kMaxTextSpeed);
}

}  // namespace

bool SettingsDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
    pendingCommand_.reset();
    pendingSoundRequests_.clear();
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

void SettingsDocumentController::unbind() {
    detachEventListeners(listeners_);
    pendingSoundRequests_.clear();
    document_ = nullptr;
}

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

void SettingsDocumentController::moveSelection(int delta) {
    if (delta == 0) {
        return;
    }
    setSelection(itemForIndex(itemIndex(selection_) + delta), true);
}

void SettingsDocumentController::adjustSelection(int delta) {
    if (delta == 0) {
        return;
    }

    switch (selection_) {
        case SettingsItem::DisplayMode:
            queueDisplayMode(delta > 0);
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

void SettingsDocumentController::activateSelection() {
    switch (selection_) {
        case SettingsItem::DisplayMode:
            queueDisplayMode(!workingSettings_.fullscreen);
            break;

        case SettingsItem::Back:
            queueReturn();
            break;

        case SettingsItem::VoiceVolume:
        case SettingsItem::TextSpeed:
            break;
    }
}

void SettingsDocumentController::cancel() {
    queueReturn();
}

void SettingsDocumentController::applyState(AppState& state) {
    state.settingsSelection = selection_;
    state.settings.fullscreen = workingSettings_.fullscreen;
    state.settings.voiceVolume = workingSettings_.voiceVolume;
    state.settings.textSpeed = workingSettings_.textSpeed;
}

std::optional<Command> SettingsDocumentController::consumeCommand() {
    const std::optional<Command> result = pendingCommand_;
    pendingCommand_.reset();
    return result;
}

std::vector<SoundRequest> SettingsDocumentController::consumeSoundRequests() {
    std::vector<SoundRequest> requests = std::move(pendingSoundRequests_);
    pendingSoundRequests_.clear();
    return requests;
}

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
                        if (item == SettingsItem::VoiceVolume) {
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

void SettingsDocumentController::syncControlValues() const {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("settings-display-value")) {
        element->SetInnerRML(formatDisplayMode(workingSettings_.fullscreen));
    }
    if (Rml::Element* element = document_->GetElementById("settings-voice-value")) {
        element->SetInnerRML(formatPercent(workingSettings_.voiceVolume));
    }
    if (Rml::Element* element = document_->GetElementById("settings-text-value")) {
        element->SetInnerRML(formatCharsPerSecond(workingSettings_.textSpeed));
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
            : (selection_ == SettingsItem::VoiceVolume
                ? formatPercent(workingSettings_.voiceVolume)
                : (selection_ == SettingsItem::TextSpeed
                    ? formatCharsPerSecond(workingSettings_.textSpeed)
                    : (returnScreen_ == ScreenState::MainMenu ? "MAIN MENU" : "RETURN"))));
    }
    if (Rml::Element* element = document_->GetElementById("settings-preview-chip")) {
        element->SetInnerRML(selection_ == SettingsItem::Back ? "RETURN ROUTE" : "LIVE VALUE");
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

void SettingsDocumentController::queueReturn() {
    pendingCommand_ = Command{CommandType::ReturnFromSettings};
    queueSound(kBackSfxPath, 0.92f);
}

void SettingsDocumentController::setVoiceVolume(float value) {
    const float clamped = clampVoice(value);
    if (std::fabs(workingSettings_.voiceVolume - clamped) < 0.001f) {
        return;
    }
    workingSettings_.voiceVolume = clamped;
    syncControlValues();
    updateFocusCopy();
    queueSound(kAdjustSfxPath, 0.84f);
}

void SettingsDocumentController::setTextSpeed(float value) {
    const float clamped = clampTextSpeed(value);
    if (std::fabs(workingSettings_.textSpeed - clamped) < 0.001f) {
        return;
    }
    workingSettings_.textSpeed = clamped;
    syncControlValues();
    updateFocusCopy();
    queueSound(kAdjustSfxPath, 0.84f);
}

void SettingsDocumentController::queueSound(const char* path, float volume) {
    pendingSoundRequests_.push_back(SoundRequest{path, volume});
}

}  // namespace graphics::frontui
