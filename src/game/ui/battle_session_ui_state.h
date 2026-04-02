#ifndef BATTLE_SESSION_UI_STATE_H
#define BATTLE_SESSION_UI_STATE_H

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "../../GameMenu/menu_shared.h"
#include "../core/battle_manager.h"
#include "../vn/vn_script.h"

namespace battle::app::ui {

struct HudFeedbackState {
    std::string hintText;
    Uint64 hintUntilMs = 0;
    std::string toastText;
    Uint64 toastUntilMs = 0;
    int comboCount = 0;
    float comboBonusFraction = 0.0f;
    std::string judgementText;
    std::string judgementRewardText;
    std::string judgementClassName;
    Uint64 judgementStartedMs = 0;
    Uint64 judgementUntilMs = 0;
    int blinkUnitIndex = -1;
    int blinkMissingFrom = 0;
    int blinkMissingTo = 0;
    Uint64 blinkUntilMs = 0;
    Uint64 bossHitUntilMs = 0;
    std::vector<Uint64> unitHitUntilMs;
};

struct HudValueAnimationState {
    bool initialized = false;
    bool active = false;
    float displayedValue = 0.0f;
    float fromValue = 0.0f;
    float targetValue = 0.0f;
    float trailValue = 0.0f;
    float elapsedSeconds = 0.0f;
    float durationSeconds = 0.5f;
};

struct HudHitReactionState {
    bool active = false;
    Uint64 startedMs = 0;
    Uint64 untilMs = 0;
};

struct HudUnitAnimationState {
    HudValueAnimationState hp;
    HudValueAnimationState ultimate;
    HudHitReactionState hit;
    Uint64 ultSheenStartedMs = 0;
    Uint64 ultSheenUntilMs = 0;
};

struct HudAnimationState {
    HudValueAnimationState bossHp;
    HudHitReactionState bossHit;
    std::vector<HudUnitAnimationState> units;
};

enum class TutorialStep {
    None,
    Standard,
    Skill,
    Ultimate
};

struct TutorialOverlayState {
    TutorialStep step = TutorialStep::None;
    vn::ScriptEntry entry;
    Uint64 startedMs = 0;
    bool audioPlayed = false;
    bool standardShown = false;
    bool skillShown = false;
    bool ultimateShown = false;
    bool dismissed = false;
};

struct TutorialScriptLibrary {
    vn::ScriptEntry standard;
    vn::ScriptEntry skill;
    vn::ScriptEntry ultimate;
    bool loaded = false;
};

enum class PauseSelection {
    Continue,
    Settings,
    ExitToMainMenu
};

enum class PauseOverlayMode {
    Menu,
    Settings
};

enum class SettingsSelection {
    DisplayMode,
    VoiceVolume,
    TextSpeed,
    Back
};

struct RhythmChallengeState {
    bool active = false;
    int partyIndex = -1;
    std::string actorTitle;
    std::string abilityName;
    Uint64 startedMs = 0;
    Uint64 durationMs = 1350;
    float targetCenter = (206.0f + (62.0f * 0.5f)) / 278.0f;
    float targetWindow = 62.0f / 278.0f;
};

inline std::optional<int> getActiveCharacterPartyIndex(const battle::BattleManager& manager) {
    const battle::TurnState& turnState = manager.getTurnState();
    const int activeActorIndex = manager.getPreviewNextActorIndex();
    if (activeActorIndex < 0 || activeActorIndex >= static_cast<int>(turnState.actors.size())) {
        return std::nullopt;
    }

    const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(activeActorIndex)];
    if (actor.type != battle::ParticipantType::Character || actor.partyIndex < 0) {
        return std::nullopt;
    }

    return actor.partyIndex;
}

inline float getRhythmProgress(const RhythmChallengeState& rhythm, Uint64 nowMs) {
    if (!rhythm.active || rhythm.durationMs == 0) {
        return 0.0f;
    }

    const double elapsed = static_cast<double>(nowMs - rhythm.startedMs);
    const double duration = static_cast<double>(rhythm.durationMs);
    return std::clamp(static_cast<float>(elapsed / duration), 0.0f, 1.0f);
}

} // namespace battle::app::ui

#endif
