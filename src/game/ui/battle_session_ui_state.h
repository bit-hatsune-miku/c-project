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
    Ultimate,
    Custom
};

enum class BattleHintFamily {
    Info,
    Tutorial,
    Warning,
    Major
};

enum class BattleHintResolveKind {
    None,
    Timeout,
    PresentationEnd,
    BossPhaseIntroEnd,
    BattleAction,
    TutorialStepComplete
};

enum class BattleHintPhase {
    Opening,
    Active,
    Closing
};

struct BattleHintResolveRule {
    BattleHintResolveKind kind = BattleHintResolveKind::None;
    Uint64 durationMs = 0;
    std::optional<battle::BattleAction> action;
    std::string abilityId;
    TutorialStep tutorialStep = TutorialStep::None;
};

struct BattleHintRequest {
    std::string stableKey;
    BattleHintFamily family = BattleHintFamily::Info;
    std::string kicker;
    std::string sourceTag;
    std::string badgeText;
    std::string message;
    std::string dismissLabel;
    std::string showSfxPath;
    std::string voicePath;
    bool manualDismissAllowed = false;
    bool refreshIfShown = true;
    BattleHintResolveRule resolveRule{};
};

struct BattleHintInstance {
    BattleHintRequest request;
    BattleHintPhase phase = BattleHintPhase::Opening;
    Uint64 phaseElapsedMs = 0;
    Uint64 activeElapsedMs = 0;
    float measuredWidthDp = 0.0f;
    float measuredHeightDp = 0.0f;
    bool measurementDirty = true;
    std::string visibleMessage;
};

struct BattleHintOverlayState {
    std::vector<BattleHintInstance> active;
};

struct AutoActionIndicatorState {
    bool active = false;
    bool isBoss = false;
    Uint64 startedMs = 0;
    Uint64 enterDurationMs = 0;
    Uint64 holdDurationMs = 0;
    Uint64 exitDurationMs = 0;
    std::string assetName;
    std::string labelText;
};

struct HudFeedbackState {
    BattleHintOverlayState hints;
    AutoActionIndicatorState autoActionIndicator;
    std::string toastText;
    Uint64 toastUntilMs = 0;
    int comboCount = 0;
    float comboBonusFraction = 0.0f;
    std::string judgementText;
    std::string judgementRewardText;
    std::string judgementClassName;
    Uint64 judgementStartedMs = 0;
    Uint64 judgementUntilMs = 0;
    int presentationDamageTotal = 0;
    std::string presentationDamageText;
    bool presentationDamageVisible = false;
    bool presentationDamageActive = false;
    Uint64 presentationDamageStartedMs = 0;
    Uint64 presentationDamageHoldUntilMs = 0;
    Uint64 presentationDamageFadeUntilMs = 0;
    int blinkUnitIndex = -1;
    int blinkMissingFrom = 0;
    int blinkMissingTo = 0;
    Uint64 blinkUntilMs = 0;
    Uint64 bossHitUntilMs = 0;
    std::vector<Uint64> unitHitUntilMs;
};

inline constexpr std::size_t kBattleHintMaxVisible = 6;
inline constexpr Uint64 kBattleHintOpenDurationMs = 220;
inline constexpr Uint64 kBattleHintCloseDurationMs = 180;
inline constexpr float kBattleHintMinWidthDp = 460.0f;
inline constexpr float kBattleHintMaxWidthDp = 790.0f;
inline constexpr float kBattleHintStackGapDp = 2.0f;
inline constexpr Uint64 kAutoActionIndicatorEnterDurationMs = 320;
inline constexpr Uint64 kAutoActionIndicatorHoldDurationMs = 980;
inline constexpr Uint64 kAutoActionIndicatorExitDurationMs = 240;

inline bool battleHintHasAutoTimeout(const BattleHintInstance& hint) {
    return hint.request.resolveRule.kind == BattleHintResolveKind::Timeout &&
        hint.request.resolveRule.durationMs > 0;
}

inline bool battleHintIsLive(const BattleHintInstance& hint) {
    return hint.phase != BattleHintPhase::Closing;
}

inline float battleHintTimeoutProgress(const BattleHintInstance& hint) {
    if (!battleHintHasAutoTimeout(hint)) {
        return 1.0f;
    }

    const Uint64 durationMs = std::max<Uint64>(hint.request.resolveRule.durationMs, 1);
    const double remainingFraction =
        1.0 - (static_cast<double>(hint.activeElapsedMs) / static_cast<double>(durationMs));
    return std::clamp(static_cast<float>(remainingFraction), 0.0f, 1.0f);
}

struct TutorialOverlayState {
    TutorialStep step = TutorialStep::None;
    vn::ScriptEntry entry;
    Uint64 startedMs = 0;
    bool audioPlayed = false;
    bool manualDismissAllowed = false;
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

enum class BattleResultOverlayOutcome {
    None,
    Victory,
    Defeat
};

enum class BattleResultOverlayAction {
    Continue,
    RestartStory
};

struct BattleResultOverlayState {
    bool active = false;
    bool inputEnabled = false;
    bool acknowledged = false;
    bool revealSfxPlayed = false;
    bool applausePlayed = false;
    bool hideButton = false;
    bool whiteout = false;
    bool reverseReveal = false;
    int nextLetterSfxIndex = 0;
    int forcedVisibleLetterCount = -1;
    float reverseAnimatingLetterElapsedSeconds = 0.0f;
    float dimOpacityOverride = -1.0f;
    BattleResultOverlayOutcome outcome = BattleResultOverlayOutcome::None;
    BattleResultOverlayAction action = BattleResultOverlayAction::Continue;
    Uint64 startedMs = 0;
    float presentationElapsedSeconds = 0.0f;
    std::string word;
    std::string plainWordText;
    float plainWordOpacity = 1.0f;
    std::string kickerText;
    std::string buttonLabel;
    std::string buttonSubcopy;
};

struct BattleVsIntroOverlayState {
    bool pendingStart = false;
    bool active = false;
    bool lineDropSfxPlayed = false;
    bool lineTiltSfxPlayed = false;
    bool leftCardSfxPlayed = false;
    bool rightCardSfxPlayed = false;
    bool vsVHitSfxPlayed = false;
    bool vsSHitSfxPlayed = false;
    bool matchStartSfxPlayed = false;
    bool bgmStarted = false;
    int nextLeftLetterSfxIndex = 0;
    int nextRightLetterSfxIndex = 0;
    float presentationElapsedSeconds = 0.0f;
    std::string leftName;
    std::string rightName;
    std::string leftAsset;
    std::string rightAsset;
};

inline constexpr float kBattleResultDimFadeDurationSeconds = 0.28f;
inline constexpr float kBattleResultKickerDelaySeconds = 0.06f;
inline constexpr float kBattleResultKickerDurationSeconds = 0.22f;
inline constexpr float kBattleResultIntroDelaySeconds = 0.18f;
inline constexpr float kBattleResultLetterIntervalSeconds = 0.068f;
inline constexpr float kBattleResultLetterDurationSeconds = 0.52f;
inline constexpr float kBattleResultWhiteoutIntroDelaySeconds = 2.00f;
inline constexpr float kBattleResultWhiteoutLetterIntervalSeconds = 0.09f;
inline constexpr float kBattleResultWhiteoutLetterDurationSeconds = 0.72f;
inline constexpr float kBattleResultWhiteoutDimFadeDurationSeconds = 0.55f;
inline constexpr float kBattleResultSettleDelaySeconds = 0.04f;
inline constexpr float kBattleResultSettleDurationSeconds = 0.32f;
inline constexpr float kBattleResultButtonDelaySeconds = 0.08f;
inline constexpr float kBattleResultButtonDurationSeconds = 0.26f;
inline constexpr float kBattleVsIntroLineDropSeconds = 0.020f;
inline constexpr float kBattleVsIntroLineTiltSeconds = 0.320f;
inline constexpr float kBattleVsIntroLeftCardSeconds = 0.520f;
inline constexpr float kBattleVsIntroRightCardSeconds = 0.920f;
inline constexpr float kBattleVsIntroVoidFadeSeconds = 1.420f;
inline constexpr float kBattleVsIntroLeftNameSeconds = 1.240f;
inline constexpr float kBattleVsIntroVsVSeconds = 1.580f;
inline constexpr float kBattleVsIntroVsSSeconds = 1.720f;
inline constexpr float kBattleVsIntroRightNameSeconds = 1.900f;
inline constexpr float kBattleVsIntroExitSeconds = 3.520f;
inline constexpr float kBattleVsIntroRevealSeconds = 3.620f;
inline constexpr float kBattleVsIntroNameLetterIntervalSeconds = 0.034f;
inline constexpr float kBattleVsIntroNameLetterDurationSeconds = 0.42f;
inline constexpr float kBattleVsIntroVsLetterDurationSeconds = 0.44f;
inline constexpr float kBattleVsIntroMaxStepSeconds = 0.05f;
inline constexpr float kBattleVsIntroCompletionSeconds = 4.100f;

inline std::size_t battleResultVisibleGlyphCount(const std::string& text) {
    return static_cast<std::size_t>(std::count_if(text.begin(), text.end(), [](char glyph) {
        return glyph != ' ';
    }));
}

inline float battleResultElapsedSeconds(const BattleResultOverlayState& overlay) {
    return std::max(overlay.presentationElapsedSeconds, 0.0f);
}

inline float battleResultIntroDelaySeconds(const BattleResultOverlayState& overlay) {
    return overlay.whiteout ? kBattleResultWhiteoutIntroDelaySeconds : kBattleResultIntroDelaySeconds;
}

inline float battleResultLetterIntervalSeconds(const BattleResultOverlayState& overlay) {
    return overlay.whiteout ? kBattleResultWhiteoutLetterIntervalSeconds : kBattleResultLetterIntervalSeconds;
}

inline float battleResultLetterDurationSeconds(const BattleResultOverlayState& overlay) {
    return overlay.whiteout ? kBattleResultWhiteoutLetterDurationSeconds : kBattleResultLetterDurationSeconds;
}

inline float battleResultRevealImpactSeconds(const BattleResultOverlayState& overlay) {
    const float visibleLetters =
        static_cast<float>(std::max<std::size_t>(battleResultVisibleGlyphCount(overlay.word), 1));
    return battleResultIntroDelaySeconds(overlay) +
        ((visibleLetters - 1.0f) * battleResultLetterIntervalSeconds(overlay)) +
        battleResultLetterDurationSeconds(overlay);
}

inline float battleResultSettleStartSeconds(const BattleResultOverlayState& overlay) {
    return battleResultRevealImpactSeconds(overlay) + kBattleResultSettleDelaySeconds;
}

inline float battleResultButtonRevealStartSeconds(const BattleResultOverlayState& overlay) {
    return battleResultSettleStartSeconds(overlay) +
        kBattleResultSettleDurationSeconds +
        kBattleResultButtonDelaySeconds;
}

inline float battleResultButtonInteractiveSeconds(const BattleResultOverlayState& overlay) {
    return battleResultButtonRevealStartSeconds(overlay) + kBattleResultButtonDurationSeconds;
}

inline int battleResultVisibleLetterCount(const BattleResultOverlayState& overlay) {
    const int totalLetters = static_cast<int>(battleResultVisibleGlyphCount(overlay.word));
    if (totalLetters <= 0) {
        return 0;
    }

    if (overlay.forcedVisibleLetterCount >= 0) {
        return std::clamp(overlay.forcedVisibleLetterCount, 0, totalLetters);
    }

    const float elapsedSeconds = battleResultElapsedSeconds(overlay);
    if (overlay.reverseReveal) {
        const int hiddenLetters =
            static_cast<int>(std::floor(elapsedSeconds / battleResultLetterIntervalSeconds(overlay)));
        return std::clamp(totalLetters - hiddenLetters, 1, totalLetters);
    }

    const float revealElapsedSeconds = elapsedSeconds - battleResultIntroDelaySeconds(overlay);
    if (revealElapsedSeconds < 0.0f) {
        return 0;
    }

    const int visibleLetters = static_cast<int>(
        std::floor(revealElapsedSeconds / battleResultLetterIntervalSeconds(overlay))) + 1;
    return std::clamp(visibleLetters, 0, totalLetters);
}

inline float battleResultReverseCompletionSeconds(const BattleResultOverlayState& overlay) {
    const int totalLetters = static_cast<int>(battleResultVisibleGlyphCount(overlay.word));
    return std::max(0, totalLetters - 1) * battleResultLetterIntervalSeconds(overlay);
}

inline float battleVsIntroElapsedSeconds(const BattleVsIntroOverlayState& overlay) {
    return std::max(overlay.presentationElapsedSeconds, 0.0f);
}

inline std::size_t battleVsIntroVisibleGlyphCount(const std::string& text) {
    std::size_t count = 0;
    for (unsigned char byte : text) {
        if ((byte & 0xC0u) == 0x80u || byte == static_cast<unsigned char>(' ')) {
            continue;
        }
        ++count;
    }
    return count;
}

inline int battleVsIntroVisibleLetterCount(const std::string& text,
                                           float elapsedSeconds,
                                           float revealStartSeconds) {
    const int totalLetters = static_cast<int>(battleVsIntroVisibleGlyphCount(text));
    if (totalLetters <= 0) {
        return 0;
    }

    const float revealElapsedSeconds = elapsedSeconds - revealStartSeconds;
    if (revealElapsedSeconds < 0.0f) {
        return 0;
    }

    const int visibleLetters =
        static_cast<int>(std::floor(revealElapsedSeconds / kBattleVsIntroNameLetterIntervalSeconds)) + 1;
    return std::clamp(visibleLetters, 0, totalLetters);
}

struct BattleInputPromptState {
    bool visible = false;
    battle::InputPromptType type = battle::InputPromptType::None;
    std::string abilityId;
    bool showPrimaryKey = false;
    std::string primaryLabel = "SPACE";
    std::vector<std::string> followUpKeys;
    Uint64 startedMs = 0;
};

struct UltimateGuideOverlayState {
    bool active = false;
    int targetPartyIndex = -1;
};

inline bool battleInputPromptHasPrimary(const BattleInputPromptState& prompt) {
    return prompt.visible && prompt.showPrimaryKey && !prompt.primaryLabel.empty();
}

inline bool battleInputPromptHasFollowUp(const BattleInputPromptState& prompt) {
    return prompt.visible && !prompt.followUpKeys.empty();
}

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
