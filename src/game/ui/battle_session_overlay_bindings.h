#ifndef BATTLE_SESSION_OVERLAY_BINDINGS_H
#define BATTLE_SESSION_OVERLAY_BINDINGS_H

#include <functional>
#include <string>

#include <RmlUi/Core/Event.h>

#include "battle_session_ui_state.h"

namespace battle::app::ui {

using AttachListenerFn = std::function<void(const std::string&, Rml::EventId, std::function<void(Rml::Event&)>)>;

struct PauseOverlayBindingCallbacks {
    std::function<void()> onContinue;
    std::function<void()> onOpenSettings;
    std::function<void()> onExitToMainMenu;
    std::function<void()> onToggleDisplayMode;
    std::function<void()> onReturnToPauseMenu;
    std::function<void(PauseSelection)> onPauseSelectionHover;
    std::function<void(SettingsSelection)> onSettingsSelectionHover;
    std::function<void(float)> onVoiceSliderChange;
    std::function<void(float)> onTextSliderChange;
};

inline void bindRhythmOverlayControls(const AttachListenerFn& attachListener,
                                      const std::function<void()>& onRhythmClick) {
    attachListener("battle-rhythm", Rml::EventId::Click, [onRhythmClick](Rml::Event&) {
        if (onRhythmClick) {
            onRhythmClick();
        }
    });
}

inline void bindPauseOverlayControls(const AttachListenerFn& attachListener,
                                     const PauseOverlayBindingCallbacks& callbacks) {
    attachListener("battle-pause-continue", Rml::EventId::Click, [onContinue = callbacks.onContinue](Rml::Event&) {
        if (onContinue) {
            onContinue();
        }
    });
    attachListener("battle-pause-settings-button", Rml::EventId::Click, [onOpenSettings = callbacks.onOpenSettings](Rml::Event&) {
        if (onOpenSettings) {
            onOpenSettings();
        }
    });
    attachListener("battle-pause-exit", Rml::EventId::Click, [onExitToMainMenu = callbacks.onExitToMainMenu](Rml::Event&) {
        if (onExitToMainMenu) {
            onExitToMainMenu();
        }
    });
    attachListener("battle-settings-row-display", Rml::EventId::Click, [onToggleDisplayMode = callbacks.onToggleDisplayMode](Rml::Event&) {
        if (onToggleDisplayMode) {
            onToggleDisplayMode();
        }
    });
    attachListener("battle-settings-row-back", Rml::EventId::Click, [onReturnToPauseMenu = callbacks.onReturnToPauseMenu](Rml::Event&) {
        if (onReturnToPauseMenu) {
            onReturnToPauseMenu();
        }
    });

    attachListener("battle-pause-continue", Rml::EventId::Mouseover, [onPauseSelectionHover = callbacks.onPauseSelectionHover](Rml::Event&) {
        if (onPauseSelectionHover) {
            onPauseSelectionHover(PauseSelection::Continue);
        }
    });
    attachListener("battle-pause-settings-button", Rml::EventId::Mouseover, [onPauseSelectionHover = callbacks.onPauseSelectionHover](Rml::Event&) {
        if (onPauseSelectionHover) {
            onPauseSelectionHover(PauseSelection::Settings);
        }
    });
    attachListener("battle-pause-exit", Rml::EventId::Mouseover, [onPauseSelectionHover = callbacks.onPauseSelectionHover](Rml::Event&) {
        if (onPauseSelectionHover) {
            onPauseSelectionHover(PauseSelection::ExitToMainMenu);
        }
    });

    attachListener("battle-settings-row-display", Rml::EventId::Mouseover, [onSettingsSelectionHover = callbacks.onSettingsSelectionHover](Rml::Event&) {
        if (onSettingsSelectionHover) {
            onSettingsSelectionHover(SettingsSelection::DisplayMode);
        }
    });
    attachListener("battle-settings-row-voice", Rml::EventId::Mouseover, [onSettingsSelectionHover = callbacks.onSettingsSelectionHover](Rml::Event&) {
        if (onSettingsSelectionHover) {
            onSettingsSelectionHover(SettingsSelection::VoiceVolume);
        }
    });
    attachListener("battle-settings-row-text", Rml::EventId::Mouseover, [onSettingsSelectionHover = callbacks.onSettingsSelectionHover](Rml::Event&) {
        if (onSettingsSelectionHover) {
            onSettingsSelectionHover(SettingsSelection::TextSpeed);
        }
    });
    attachListener("battle-settings-row-back", Rml::EventId::Mouseover, [onSettingsSelectionHover = callbacks.onSettingsSelectionHover](Rml::Event&) {
        if (onSettingsSelectionHover) {
            onSettingsSelectionHover(SettingsSelection::Back);
        }
    });
    attachListener("battle-settings-voice-slider", Rml::EventId::Mouseover, [onSettingsSelectionHover = callbacks.onSettingsSelectionHover](Rml::Event&) {
        if (onSettingsSelectionHover) {
            onSettingsSelectionHover(SettingsSelection::VoiceVolume);
        }
    });
    attachListener("battle-settings-text-slider", Rml::EventId::Mouseover, [onSettingsSelectionHover = callbacks.onSettingsSelectionHover](Rml::Event&) {
        if (onSettingsSelectionHover) {
            onSettingsSelectionHover(SettingsSelection::TextSpeed);
        }
    });

    attachListener("battle-settings-voice-slider", Rml::EventId::Change, [onVoiceSliderChange = callbacks.onVoiceSliderChange](Rml::Event& event) {
        if (onVoiceSliderChange) {
            onVoiceSliderChange(event.GetParameter<float>("value", 0.0f));
        }
    });
    attachListener("battle-settings-text-slider", Rml::EventId::Change, [onTextSliderChange = callbacks.onTextSliderChange](Rml::Event& event) {
        if (onTextSliderChange) {
            onTextSliderChange(event.GetParameter<float>("value", 0.0f));
        }
    });
}

} // namespace battle::app::ui

#endif
