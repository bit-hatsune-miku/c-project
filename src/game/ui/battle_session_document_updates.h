#ifndef BATTLE_SESSION_DOCUMENT_UPDATES_H
#define BATTLE_SESSION_DOCUMENT_UPDATES_H

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "../core/turn_system.h"
#include "battle_session_ui_state.h"

namespace battle::app::ui {

struct BattleHudDocumentDependencies {
    std::function<std::string(std::string)> uppercaseText;
    std::function<std::string(const std::string&, const std::string&)> findCombatImagePath;
    std::function<std::string(const std::string&, Uint64, Uint64, float)> revealNarrationText;
};

namespace detail {

struct PauseOverlayCopy {
    const char* title = "";
    const char* hint = "";
};

inline void setElementText(Rml::ElementDocument* document, const std::string& id, const std::string& text) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetInnerRML(text);
    }
}

inline void setElementDisplay(Rml::ElementDocument* document, const std::string& id, bool visible) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetProperty("display", visible ? "block" : "none");
    }
}

inline void setElementProperty(Rml::ElementDocument* document,
                               const std::string& id,
                               const std::string& property,
                               const std::string& value) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetProperty(property, value);
    }
}

inline void setElementClass(Rml::ElementDocument* document,
                            const std::string& id,
                            const std::string& className,
                            bool enabled) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetClass(className, enabled);
    }
}

inline void setRangeValue(Rml::ElementDocument* document, const std::string& id, const std::string& value) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        if (auto* control = dynamic_cast<Rml::ElementFormControl*>(element)) {
            control->SetValue(value);
        }
    }
}

inline void setPortraitDecorator(Rml::ElementDocument* document,
                                 const std::string& id,
                                 const std::string& assetName,
                                 const BattleHudDocumentDependencies& dependencies) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        if (!dependencies.findCombatImagePath) {
            element->RemoveProperty("decorator");
            return;
        }
        const std::string path = dependencies.findCombatImagePath("icons", assetName);
        if (!path.empty()) {
            element->SetProperty("decorator", "image(" + path + " cover center center)");
        } else {
            element->RemoveProperty("decorator");
        }
    }
}

inline std::vector<int> getSortedTurnActorIndices(const battle::TurnState& turnState) {
    std::vector<int> sorted(turnState.actors.size());
    std::iota(sorted.begin(), sorted.end(), 0);
    std::sort(sorted.begin(), sorted.end(), [&](int lhs, int rhs) {
        constexpr float kEpsilon = 0.0001f;
        const battle::TurnActor& a = turnState.actors[static_cast<size_t>(lhs)];
        const battle::TurnActor& b = turnState.actors[static_cast<size_t>(rhs)];
        if (std::fabs(a.currentActionValue - b.currentActionValue) > kEpsilon) {
            return a.currentActionValue < b.currentActionValue;
        }
        return battle::turn::turnPriorityLess(a, b);
    });
    return sorted;
}

inline std::string hpColorForRatio(float ratio) {
    if (ratio > 0.66f) {
        return "#48bf7b";
    }
    if (ratio > 0.33f) {
        return "#e4a943";
    }
    return "#d96b56";
}

inline PauseOverlayCopy getPauseOverlayCopy(PauseOverlayMode mode) {
    switch (mode) {
        case PauseOverlayMode::Settings:
            return PauseOverlayCopy{
                "SETTINGS",
                "ADJUST BATTLE SETTINGS. CHANGES CARRY BACK TO THE MAIN APP."
            };
        case PauseOverlayMode::Menu:
        default:
            return PauseOverlayCopy{
                "PAUSED",
                "ESC TO RESUME. SETTINGS MATCH THE MAIN APP."
            };
    }
}

inline void updateToastDocument(Rml::ElementDocument* document, const HudFeedbackState& feedback) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* toast = document->GetElementById("battle-toast")) {
        toast->SetInnerRML(feedback.toastText);
        toast->SetClass("visible", !feedback.toastText.empty());
    }
}

inline void updateHintDocument(Rml::ElementDocument* document, const HudFeedbackState& feedback) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* hint = document->GetElementById("battle-hint")) {
        hint->SetInnerRML(feedback.hintText);
        hint->SetClass("visible", !feedback.hintText.empty());
    }
}

inline void updateTutorialDocument(Rml::ElementDocument* document,
                                   const TutorialOverlayState& tutorial,
                                   Uint64 nowMs,
                                   float narrationCharsPerSecond,
                                   const BattleHudDocumentDependencies& dependencies) {
    setElementClass(document, "battle-tutorial", "visible", tutorial.step != TutorialStep::None);
    if (tutorial.step == TutorialStep::None) {
        return;
    }

    setElementText(document, "battle-tutorial-speaker", vn::getDisplaySpeakerName(tutorial.entry));
    if (dependencies.revealNarrationText) {
        setElementText(document,
                       "battle-tutorial-text",
                       dependencies.revealNarrationText(
                           tutorial.entry.text,
                           tutorial.startedMs,
                           nowMs,
                           narrationCharsPerSecond));
    }

    if (tutorial.entry.icon.empty() || !dependencies.findCombatImagePath) {
        return;
    }

    const std::string tutorialIconKey = std::filesystem::path(tutorial.entry.icon).stem().string();
    const std::string iconPath = dependencies.findCombatImagePath("icons", tutorialIconKey);
    if (iconPath.empty()) {
        return;
    }

    if (Rml::Element* element = document->GetElementById("battle-tutorial-portrait")) {
        element->SetProperty("decorator", "image(" + iconPath + " cover center center)");
    }
}

inline void updateRhythmDocument(Rml::ElementDocument* document, const RhythmChallengeState& rhythm, Uint64 nowMs) {
    setElementClass(document, "battle-rhythm", "visible", rhythm.active);
    if (!rhythm.active || document == nullptr) {
        return;
    }

    setElementText(document, "battle-rhythm-title", "");
    setElementText(document, "battle-rhythm-body", "PRESS E WHEN THE CYAN PULSE CROSSES THE GOLD WINDOW.");
    if (Rml::Element* pulse = document->GetElementById("battle-rhythm-pulse")) {
        const float progress = getRhythmProgress(rhythm, nowMs);
        pulse->SetProperty("left", std::to_string(static_cast<int>(std::round(progress * 278.0f))) + "px");
    }
}

} // namespace detail

inline void applyPauseOverlayDocumentState(Rml::ElementDocument* document,
                                           PauseOverlayMode pauseOverlayMode,
                                           PauseSelection pauseSelection,
                                           SettingsSelection settingsSelection,
                                           const GameSettings* settings) {
    const bool showingSettings = pauseOverlayMode == PauseOverlayMode::Settings;
    const detail::PauseOverlayCopy copy = detail::getPauseOverlayCopy(pauseOverlayMode);

    detail::setElementText(document, "battle-pause-title", copy.title);
    detail::setElementText(document, "battle-pause-hint", copy.hint);
    detail::setElementClass(document, "battle-pause-shell", "settings-open", showingSettings);
    detail::setElementClass(document, "battle-pause-menu", "visible", pauseOverlayMode == PauseOverlayMode::Menu);
    detail::setElementClass(document, "battle-pause-settings", "visible", showingSettings);

    if (document == nullptr) {
        return;
    }

    if (Rml::Element* continueButton = document->GetElementById("battle-pause-continue")) {
        continueButton->SetClass("selected", pauseSelection == PauseSelection::Continue);
    }
    if (Rml::Element* settingsButton = document->GetElementById("battle-pause-settings-button")) {
        settingsButton->SetClass("selected", pauseSelection == PauseSelection::Settings);
    }
    if (Rml::Element* exitButton = document->GetElementById("battle-pause-exit")) {
        exitButton->SetClass("selected", pauseSelection == PauseSelection::ExitToMainMenu);
    }

    if (settings != nullptr) {
        detail::setElementText(document, "battle-settings-display-value", settings->fullscreen ? "Fullscreen" : "Windowed");
        detail::setElementText(document,
                               "battle-settings-voice-value",
                               std::to_string(static_cast<int>(std::lround(settings->voiceVolume * 100.0f))) + "%");
        detail::setElementText(document,
                               "battle-settings-text-value",
                               std::to_string(static_cast<int>(std::lround(settings->textSpeed))) + " cps");
        detail::setRangeValue(document,
                              "battle-settings-voice-slider",
                              std::to_string(static_cast<int>(std::lround(settings->voiceVolume * 100.0f))));
        detail::setRangeValue(document,
                              "battle-settings-text-slider",
                              std::to_string(static_cast<int>(std::lround(settings->textSpeed))));
    }

    detail::setElementClass(document, "battle-settings-row-display", "selected", settingsSelection == SettingsSelection::DisplayMode);
    detail::setElementClass(document, "battle-settings-row-voice", "selected", settingsSelection == SettingsSelection::VoiceVolume);
    detail::setElementClass(document, "battle-settings-row-text", "selected", settingsSelection == SettingsSelection::TextSpeed);
    detail::setElementClass(document, "battle-settings-row-back", "selected", settingsSelection == SettingsSelection::Back);
}

inline void updateBattleHudDocument(Rml::ElementDocument* document,
                                    const battle::BattleManager& manager,
                                    const HudFeedbackState& feedback,
                                    const TutorialOverlayState& tutorial,
                                    const RhythmChallengeState& rhythm,
                                    bool paused,
                                    PauseOverlayMode pauseOverlayMode,
                                    PauseSelection pauseSelection,
                                    SettingsSelection settingsSelection,
                                    const GameSettings* settings,
                                    float narrationCharsPerSecond,
                                    Uint64 nowMs,
                                    const BattleHudDocumentDependencies& dependencies) {
    if (document == nullptr) {
        return;
    }

    const battle::BattleState& battleState = manager.getBattleState();
    const battle::TurnState& turnState = manager.getTurnState();
    const int activeActorIndex = manager.getPreviewNextActorIndex();

    const std::string bossDisplayName = !battleState.boss.title.empty() ? battleState.boss.title : battleState.boss.key;
    detail::setElementText(document,
                           "boss-name",
                           dependencies.uppercaseText ? dependencies.uppercaseText(bossDisplayName) : bossDisplayName);
    const int bossCurrentHp = manager.getBossCurrentHp();
    const int bossMaxHp = std::max(1, manager.getBossMaxHp());
    const int bossPercent = static_cast<int>(std::round((100.0f * bossCurrentHp) / bossMaxHp));
    detail::setElementText(document,
                           "boss-readout",
                           std::to_string(bossCurrentHp) + " / " + std::to_string(bossMaxHp) + " HP");
    detail::setElementText(document, "boss-percent", std::to_string(bossPercent) + "%");
    if (Rml::Element* bossFill = document->GetElementById("boss-fill")) {
        bossFill->SetProperty("width", std::to_string(bossPercent) + "%");
    }
    detail::setElementClass(document, "boss-band", "hit", nowMs < feedback.bossHitUntilMs);

    const std::vector<int> sortedActorIndices = detail::getSortedTurnActorIndices(turnState);
    for (int slot = 0; slot < 5; ++slot) {
        const std::string slotIndex = std::to_string(slot + 1);
        const bool hasActor = slot < static_cast<int>(sortedActorIndices.size());
        detail::setElementDisplay(document, "turn-card-" + slotIndex, hasActor);
        if (!hasActor) {
            continue;
        }

        const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(sortedActorIndices[static_cast<size_t>(slot)])];
        detail::setPortraitDecorator(
            document,
            "turn-portrait-" + slotIndex,
            actor.assetId.empty() ? actor.key : actor.assetId,
            dependencies);
        detail::setElementText(document,
                               "turn-value-" + slotIndex,
                               std::to_string(static_cast<int>(std::round(actor.currentActionValue))));
        if (Rml::Element* card = document->GetElementById("turn-card-" + slotIndex)) {
            card->SetClass("active", sortedActorIndices[static_cast<size_t>(slot)] == activeActorIndex);
            card->SetClass("boss", actor.type == battle::ParticipantType::Boss);
        }
    }

    for (int i = 0; i < 4; ++i) {
        const std::string index = std::to_string(i + 1);
        const bool hasCharacter = i < static_cast<int>(battleState.party.size());
        detail::setElementDisplay(document, "unit-card-" + index, hasCharacter);
        if (!hasCharacter) {
            continue;
        }

        const battle::CharacterDefinition& character = battleState.party[static_cast<size_t>(i)];
        const int currentHp = manager.getCharacterCurrentHp(i);
        const int maxHp = std::max(1, manager.getCharacterMaxHp(i));
        const float ratio = static_cast<float>(std::clamp(currentHp, 0, maxHp)) / static_cast<float>(maxHp);
        const int ultimateCharge = manager.getCharacterUltimateCharge(i);
        const int ultimateRequired = std::max(1, manager.getCharacterUltimateRequired(i));
        const float ultimateRatio =
            static_cast<float>(std::clamp(ultimateCharge, 0, ultimateRequired)) / static_cast<float>(ultimateRequired);
        const int shield = manager.getCharacterShield(i);
        const float shieldRatio =
            currentHp > 0 ? std::min(1.0f, static_cast<float>(shield) / static_cast<float>(currentHp)) : 0.0f;

        detail::setElementText(document,
                               "unit-hp-text-" + index,
                               std::to_string(currentHp) + " / " + std::to_string(maxHp));
        detail::setPortraitDecorator(document, "unit-icon-" + index, character.assets, dependencies);
        detail::setElementText(document,
                               "unit-ult-text-" + index,
                               std::to_string(ultimateCharge) + " / " + std::to_string(ultimateRequired));
        detail::setElementProperty(document,
                                   "unit-ult-fill-" + index,
                                   "height",
                                   std::to_string(static_cast<int>(std::round(ultimateRatio * 100.0f))) + "%");
        if (Rml::Element* hpFill = document->GetElementById("unit-hp-fill-" + index)) {
            hpFill->SetProperty("width", std::to_string(static_cast<int>(std::round(ratio * 100.0f))) + "%");
            hpFill->SetProperty("background-color", detail::hpColorForRatio(ratio));
        }
        detail::setElementDisplay(document, "unit-shield-badge-" + index, shield > 0);
        detail::setElementText(document, "unit-shield-value-" + index, std::to_string(shield));
        detail::setElementDisplay(document, "unit-shield-top-" + index, shield > 0);
        detail::setElementDisplay(document, "unit-shield-bottom-" + index, shield > 0);
        detail::setElementDisplay(document, "unit-shield-left-" + index, shield > 0);
        detail::setElementDisplay(document, "unit-shield-right-" + index, shield > 0 && shieldRatio >= 0.999f);
        const std::string shieldWidth =
            std::to_string(static_cast<int>(std::round(shieldRatio * 100.0f))) + "%";
        detail::setElementProperty(document, "unit-shield-top-" + index, "width", shieldWidth);
        detail::setElementProperty(document, "unit-shield-bottom-" + index, "width", shieldWidth);
        if (Rml::Element* card = document->GetElementById("unit-card-" + index)) {
            const bool isFocused =
                activeActorIndex >= 0 && activeActorIndex < static_cast<int>(turnState.actors.size()) &&
                turnState.actors[static_cast<size_t>(activeActorIndex)].type == battle::ParticipantType::Character &&
                turnState.actors[static_cast<size_t>(activeActorIndex)].partyIndex == i;
            card->SetClass("active", isFocused);
            card->SetClass("ghost", currentHp <= 0);
            card->SetClass("hit", nowMs < feedback.unitHitUntilMs[static_cast<size_t>(i)]);
            card->SetClass(
                "ult-ready",
                currentHp > 0 && ultimateCharge >= ultimateRequired
            );
        }
    }

    detail::updateHintDocument(document, feedback);
    detail::updateToastDocument(document, feedback);
    detail::updateTutorialDocument(document, tutorial, nowMs, narrationCharsPerSecond, dependencies);
    detail::updateRhythmDocument(document, rhythm, nowMs);

    detail::setElementClass(document, "battle-pause", "visible", paused);
    if (paused) {
        applyPauseOverlayDocumentState(document, pauseOverlayMode, pauseSelection, settingsSelection, settings);
    }
}

} // namespace battle::app::ui

#endif
