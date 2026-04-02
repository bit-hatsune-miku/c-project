#define GL_GLEXT_PROTOTYPES

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>
#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "audio/bgm_player.h"
#include "core/ability_system.h"
#include "core/battle_loader.h"
#include "core/battle_manager.h"
#include "core/battle_turn_flow.h"
#include "core/easing.h"
#include "presentation/ability_presentation.h"
#include "presentation/presentation_runtime.h"
#include "presentation/splash_art_animation.h"
#include "render/battle_camera_staging.h"
#include "render/battle_combat_begin_animation.h"
#include "render/battle_feedback.h"
#include "render/battle_party_staging.h"
#include "render/battle_world_renderer.h"
#include "render/battle_scene_renderer.h"
#include "render/camera_3d.h"
#include "render/free_view_camera_debug_log.h"
#include "render/gl_battle_scene_renderer.h"
#include "render/gl_screen_blitter.h"
#include "../graphics/front_ui_pause.h"
#include "../graphics/front_ui_settings.h"
#include "../graphics/rmlui_loading_overlay.h"
#include "../graphics/rmlui_sdl_gl_renderer.h"
#include "demo/demo_narrative_flow.h"
#include "ui/battle_session_document_updates.h"
#include "ui/battle_session_overlay_bindings.h"
#include "ui/battle_session_ui_state.h"
#include "vn/vn_script.h"
#include "vn/vn_system.h"
#include "audio/wav_one_shot.h"
#include "app_battle_session.h"
#include "../platform/path_resolution.h"
#include "../window.h"

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

constexpr float kGoalCameraPosX = -405.0f;
constexpr float kGoalCameraPosY = 45.0f;
constexpr float kGoalCameraPosZ = -175.0f;
constexpr float kGoalCameraPitch = 5.0f;
constexpr float kGoalCameraYaw = 4.0f;

constexpr float kGoalCameraFocal = 50000.0f;

constexpr float kActionIntroOffsetX = -130.0f;
constexpr float kActionIntroOffsetY = -110.0f;
constexpr float kActionIntroOffsetZ = 28.0f;
constexpr float kActionIntroDurationSeconds = 0.22f;
constexpr float kNarrationCharsPerSecond = 42.0f;
constexpr Uint64 kIdleDelayMs = 5000;
constexpr float kMinSettingsTextSpeed = 18.0f;
constexpr float kMaxSettingsTextSpeed = 90.0f;
constexpr float kBossTurnCharacterBaseY = 420.0f;
constexpr float kCharacterSpacingWorld = 400.0f;
constexpr float kBossTurnCharacterSpacingWorld = 260.0f;
constexpr Uint64 kHitFlashDurationMs = 320;
constexpr Uint64 kJudgementPopupDurationMs = 1320;
constexpr const char* kLoadingOverlayDocumentPath = "assets/rmlui/shared/loading_overlay.rml";
constexpr float kCharacterVisibilityAnimSeconds = 0.22f;
constexpr float kCharacterVisibilityOffsetPx = 70.0f;
constexpr const char* kPerfectJudgementSfxPath = "assets/ui/sfx/SongSelect_select-random.wav";
constexpr const char* kGoodJudgementSfxPath = "assets/ui/sfx/Selection_roulette-4.wav";
constexpr const char* kOkayJudgementSfxPath = "assets/ui/sfx/Selection_roulette-0.wav";
constexpr const char* kFlopJudgementSfxPath = "assets/ui/sfx/UI_notification-error.wav";

int manualUltimatePartyIndexFromKey(SDL_Keycode key) {
    switch (key) {
        case SDLK_1:
        case SDLK_KP_1:
            return 0;
        case SDLK_2:
        case SDLK_KP_2:
            return 1;
        case SDLK_3:
        case SDLK_KP_3:
            return 2;
        case SDLK_4:
        case SDLK_KP_4:
            return 3;
        default:
            return -1;
    }
}

struct CameraIntroAnimation {
    bool active = false;
    float elapsed = 0.0f;
    float duration = kActionIntroDurationSeconds;
    float startX = 0.0f;
    float startY = 0.0f;
    float startZ = 0.0f;
    float startPitch = 0.0f;
    float startYaw = 0.0f;
    float startFocal = 0.0f;
    float goalX = 0.0f;
    float goalY = 0.0f;
    float goalZ = 0.0f;
    float goalPitch = 0.0f;
    float goalYaw = 0.0f;
    float goalFocal = 0.0f;
};

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

game::audio::WavOneShotPlayer gOneShotAudio;
game::audio::WavOneShotPlayer gPresentationSfxAudio;
game::audio::BgmPlayer gBgmPlayer;
game::audio::BgmPlayer gPresentationLoopAudio;
game::audio::BgmPlayer gPauseMenuBgmPlayer;

std::vector<std::string> resolveUiMusicTrackPaths() {
    std::vector<std::string> tracks;
    const std::string classicsDirectory = platform::path::resolvePath("assets/ui/classics");
    std::error_code filesystemError;
    if (classicsDirectory.empty() ||
        !std::filesystem::exists(classicsDirectory, filesystemError) ||
        filesystemError) {
        return tracks;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(classicsDirectory, filesystemError)) {
        if (filesystemError) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (extension == ".wav") {
            tracks.push_back(entry.path().string());
        }
    }

    std::sort(tracks.begin(), tracks.end());
    return tracks;
}

bool loadRmlFontIfPresent(const std::string& path, bool fallback = false) {
    if (path.empty() || !std::filesystem::exists(path)) {
        return false;
    }

    const std::string extension = std::filesystem::path(path).extension().string();
    if (extension == ".ttc" || extension == ".otc") {
        bool anyLoaded = false;
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            if (!Rml::LoadFontFace(path, fallback, Rml::Style::FontWeight::Auto, faceIndex)) {
                if (faceIndex == 0 && !anyLoaded) {
                    return false;
                }
                break;
            }
            anyLoaded = true;
        }
        return anyLoaded;
    }

    return Rml::LoadFontFace(path, fallback);
}

std::string getPresentationCasterVoiceKey(const battle::PresentationContext& context,
                                          const battle::BattleManager& manager) {
    const battle::BattleState& battleState = manager.getBattleState();
    if (context.isBoss) {
        return battleState.boss.assets.empty() ? battleState.boss.key : battleState.boss.assets;
    }

    if (context.casterIndex < 0 || static_cast<size_t>(context.casterIndex) >= battleState.party.size()) {
        return std::string();
    }

    return battleState.party[static_cast<size_t>(context.casterIndex)].assets;
}

bool playCombatVoiceClip(const std::string& assetName,
                         const std::string& clipName,
                         float volume,
                         int repeatCount = 1) {
    if (assetName.empty() || clipName.empty() || repeatCount <= 0) {
        return false;
    }

    if (const auto clipPath = platform::path::resolveCombatVoicePath(assetName, clipName);
        clipPath.has_value()) {
        bool played = false;
        for (int index = 0; index < repeatCount; ++index) {
            if (gOneShotAudio.playWavOneShot(*clipPath, volume)) {
                played = true;
            }
        }
        return played;
    }

    return false;
}

bool playResolvedVoicePath(const std::string& path, float voiceVolume, int repeatCount = 1) {
    if (path.empty() || repeatCount <= 0) {
        return false;
    }

    const std::string resolved = platform::path::resolvePath(path);
    if (!std::filesystem::exists(resolved)) {
        return false;
    }

    bool played = false;
    for (int index = 0; index < repeatCount; ++index) {
        if (gOneShotAudio.playWavOneShot(resolved, voiceVolume)) {
            played = true;
        }
    }

    return played;
}

bool playResolvedOneShot(game::audio::WavOneShotPlayer& player,
                         const std::string& path,
                         float volume,
                         bool replaceExisting = true) {
    if (path.empty()) {
        return false;
    }

    const std::string resolved = platform::path::resolvePath(path);
    if (!std::filesystem::exists(resolved)) {
        return false;
    }

    return player.playWavOneShot(resolved, volume, replaceExisting);
}

bool playResolvedLoop(game::audio::BgmPlayer& player, const std::string& path, float volume) {
    if (path.empty()) {
        return false;
    }

    const std::string resolved = platform::path::resolvePath(path);
    if (!std::filesystem::exists(resolved)) {
        return false;
    }

    return player.play(resolved, volume);
}

void stopPresentationAudioPlayback(bool resumeBgm = false) {
    gPresentationLoopAudio.stop();
    gPresentationSfxAudio.stopAllPlayback();
    if (resumeBgm) {
        gBgmPlayer.resume();
    }
}

std::optional<std::string> resolveBossHitVoicePath(const battle::BattleState& battleState) {
    if (!battleState.boss.voiceHit.empty()) {
        const std::string resolved = platform::path::resolvePath(battleState.boss.voiceHit);
        if (std::filesystem::exists(resolved)) {
            return resolved;
        }
    }

    if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.boss.assets, "hit"); hitVoice.has_value()) {
        return *hitVoice;
    }
    if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.boss.key, "hit"); hitVoice.has_value()) {
        return *hitVoice;
    }
    return std::nullopt;
}

std::optional<std::string> resolveBossDeadVoicePath(const battle::BattleState& battleState) {
    if (const auto deadVoice = platform::path::resolveCombatVoicePath(battleState.boss.assets, "dead"); deadVoice.has_value()) {
        return *deadVoice;
    }
    if (const auto deadVoice = platform::path::resolveCombatVoicePath(battleState.boss.key, "dead"); deadVoice.has_value()) {
        return *deadVoice;
    }
    return std::nullopt;
}

std::optional<std::string> resolveBossHealedVoicePath(const battle::BattleState& battleState) {
    if (const auto healedVoice = platform::path::resolveCombatVoicePath(battleState.boss.assets, "healed");
        healedVoice.has_value()) {
        return *healedVoice;
    }
    if (const auto healedVoice = platform::path::resolveCombatVoicePath(battleState.boss.key, "healed");
        healedVoice.has_value()) {
        return *healedVoice;
    }
    return std::nullopt;
}

bool playBossHitVoice(const battle::BattleState& battleState, float voiceVolume, int repeatCount = 1) {
    if (playResolvedVoicePath(battleState.boss.voiceHit, voiceVolume, repeatCount)) {
        return true;
    }

    if (playCombatVoiceClip(battleState.boss.assets, "hit", voiceVolume, repeatCount)) {
        return true;
    }

    return playCombatVoiceClip(battleState.boss.key, "hit", voiceVolume, repeatCount);
}

bool playBossHealedVoice(const battle::BattleState& battleState, float voiceVolume, int repeatCount = 1) {
    if (const auto healedVoice = resolveBossHealedVoicePath(battleState); healedVoice.has_value()) {
        return playResolvedVoicePath(*healedVoice, voiceVolume, repeatCount);
    }
    return false;
}

using battle::app::ui::HudFeedbackState;
using battle::app::ui::HudAnimationState;
using battle::app::ui::HudHitReactionState;
using battle::app::ui::HudValueAnimationState;
using battle::app::ui::PauseOverlayMode;
using battle::app::ui::PauseSelection;
using battle::app::ui::RhythmChallengeState;
using battle::app::ui::SettingsSelection;
using battle::app::ui::TutorialOverlayState;
using battle::app::ui::TutorialScriptLibrary;
using battle::app::ui::TutorialStep;
using battle::app::ui::getActiveCharacterPartyIndex;
using battle::app::ui::getRhythmProgress;
using battle::render::GlScreenBlitter;
using battle::render::SceneEntity;
using battle::render::SoftwareSceneRenderer;
using graphics::frontui::DocumentController;
using graphics::frontui::PauseDocumentController;
using graphics::frontui::SoundRequest;
using graphics::frontui::SettingsDocumentController;

constexpr float kHudAnimationDurationSeconds = 0.5f;
constexpr Uint64 kHudUltSheenDurationMs = 520;

std::string resolveBattleSpritePath(const std::string& assetName) {
    const std::array<std::string, 2> candidates = {
        platform::path::resolvePath("assets/combat/sprites/" + assetName + ".png"),
        platform::path::resolvePath("assets/combat/sprites/" + assetName + ".webp")
    };

    for (const std::string& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return std::string();
}

bool isWindowResizeEvent(const SDL_Event& event) {
    return event.type == SDL_WINDOWEVENT &&
           (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            event.window.event == SDL_WINDOWEVENT_RESIZED);
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string normalizeCombatKey(std::string key) {
    if (key == "cupcake") {
        return "cupcakke";
    }
    return key;
}

size_t utf8ByteOffsetForCodepoints(const std::string& text, size_t codepointCount) {
    size_t byteOffset = 0;
    size_t visibleCodepoints = 0;
    while (byteOffset < text.size() && visibleCodepoints < codepointCount) {
        const unsigned char leadByte = static_cast<unsigned char>(text[byteOffset]);
        size_t codepointBytes = 1;
        if ((leadByte & 0x80u) == 0x00u) {
            codepointBytes = 1;
        } else if ((leadByte & 0xE0u) == 0xC0u) {
            codepointBytes = 2;
        } else if ((leadByte & 0xF0u) == 0xE0u) {
            codepointBytes = 3;
        } else if ((leadByte & 0xF8u) == 0xF0u) {
            codepointBytes = 4;
        }

        if (byteOffset + codepointBytes > text.size()) {
            return text.size();
        }

        byteOffset += codepointBytes;
        ++visibleCodepoints;
    }

    return byteOffset;
}

void showToast(HudFeedbackState& feedback, std::string message, Uint64 nowMs, Uint64 durationMs = 1800) {
    feedback.toastText = std::move(message);
    feedback.toastUntilMs = nowMs + durationMs;
}

void showHint(HudFeedbackState& feedback, std::string message, Uint64 nowMs, Uint64 durationMs = 0) {
    feedback.hintText = std::move(message);
    feedback.hintUntilMs = durationMs > 0 ? nowMs + durationMs : 0;
}

void clearHint(HudFeedbackState& feedback) {
    feedback.hintText.clear();
    feedback.hintUntilMs = 0;
}

void showJudgement(HudFeedbackState& feedback,
                   battle::CombatJudgement judgement,
                   std::string rewardText,
                   Uint64 nowMs,
                   Uint64 durationMs = kJudgementPopupDurationMs) {
    feedback.judgementText = battle::combatJudgementLabel(judgement);
    feedback.judgementRewardText = std::move(rewardText);
    feedback.judgementClassName = battle::combatJudgementClassName(judgement);
    feedback.judgementStartedMs = nowMs;
    feedback.judgementUntilMs = nowMs + durationMs;
}

void clearJudgement(HudFeedbackState& feedback) {
    feedback.judgementText.clear();
    feedback.judgementRewardText.clear();
    feedback.judgementClassName.clear();
    feedback.judgementStartedMs = 0;
    feedback.judgementUntilMs = 0;
}

void playJudgementSfx(battle::CombatJudgement judgement) {
    const char* relativePath = kPerfectJudgementSfxPath;
    switch (judgement) {
        case battle::CombatJudgement::Perfect:
            relativePath = kPerfectJudgementSfxPath;
            break;
        case battle::CombatJudgement::Good:
            relativePath = kGoodJudgementSfxPath;
            break;
        case battle::CombatJudgement::Okay:
            relativePath = kOkayJudgementSfxPath;
            break;
        case battle::CombatJudgement::Flop:
        default:
            relativePath = kFlopJudgementSfxPath;
            break;
    }
    (void)gOneShotAudio.playWavOneShot(platform::path::resolvePath(relativePath), 0.92f, false);
}

void markBossHit(HudFeedbackState& feedback, Uint64 nowMs, Uint64 durationMs = kHitFlashDurationMs) {
    feedback.bossHitUntilMs = std::max(feedback.bossHitUntilMs, nowMs + durationMs);
}

void markUnitHit(HudFeedbackState& feedback,
                 int partyIndex,
                 Uint64 nowMs,
                 Uint64 durationMs = kHitFlashDurationMs) {
    if (partyIndex < 0 || partyIndex >= static_cast<int>(feedback.unitHitUntilMs.size())) {
        return;
    }
    feedback.unitHitUntilMs[static_cast<size_t>(partyIndex)] =
        std::max(feedback.unitHitUntilMs[static_cast<size_t>(partyIndex)], nowMs + durationMs);
}

void blinkMissingOrbs(HudFeedbackState& feedback,
                      int unitIndex,
                      int firstMissingOrb,
                      int lastMissingOrb,
                      Uint64 nowMs,
                      Uint64 durationMs = 1100) {
    feedback.blinkUnitIndex = unitIndex;
    feedback.blinkMissingFrom = firstMissingOrb;
    feedback.blinkMissingTo = lastMissingOrb;
    feedback.blinkUntilMs = nowMs + durationMs;
}

void tickHudFeedback(HudFeedbackState& feedback, Uint64 nowMs) {
    if (feedback.hintUntilMs != 0 && nowMs >= feedback.hintUntilMs) {
        clearHint(feedback);
    }
    if (feedback.toastUntilMs != 0 && nowMs >= feedback.toastUntilMs) {
        feedback.toastText.clear();
        feedback.toastUntilMs = 0;
    }
    if (feedback.judgementUntilMs != 0 && nowMs >= feedback.judgementUntilMs) {
        clearJudgement(feedback);
    }
    if (feedback.blinkUntilMs != 0 && nowMs >= feedback.blinkUntilMs) {
        feedback.blinkUnitIndex = -1;
        feedback.blinkMissingFrom = 0;
        feedback.blinkMissingTo = 0;
        feedback.blinkUntilMs = 0;
    }
}

void syncHudFeedbackState(HudFeedbackState& feedback, const battle::BattleManager& manager) {
    const std::size_t partySize = manager.getBattleState().party.size();
    if (feedback.unitHitUntilMs.size() != partySize) {
        feedback.unitHitUntilMs.assign(partySize, 0);
    }
    const battle::BattleComboState& comboState = manager.getComboState();
    feedback.comboCount = comboState.comboCount;
    feedback.comboBonusFraction = comboState.damageBonusFraction;
}

void resetHudValueAnimation(HudValueAnimationState& state, float value) {
    state.initialized = true;
    state.active = false;
    state.displayedValue = value;
    state.fromValue = value;
    state.targetValue = value;
    state.trailValue = value;
    state.elapsedSeconds = 0.0f;
    state.durationSeconds = kHudAnimationDurationSeconds;
}

void retargetHudValueAnimation(HudValueAnimationState& state,
                               float targetValue,
                               float durationSeconds = kHudAnimationDurationSeconds) {
    if (!state.initialized) {
        resetHudValueAnimation(state, targetValue);
        return;
    }

    if (std::fabs(state.targetValue - targetValue) <= 0.001f &&
        (!state.active || std::fabs(state.displayedValue - targetValue) <= 0.001f)) {
        return;
    }

    state.fromValue = state.displayedValue;
    state.targetValue = targetValue;
    state.elapsedSeconds = 0.0f;
    state.durationSeconds = std::max(0.001f, durationSeconds);
    state.active = true;
    state.trailValue = targetValue < state.displayedValue ? state.displayedValue : targetValue;
}

void advanceHudValueAnimation(HudValueAnimationState& state, float deltaSeconds) {
    if (!state.initialized) {
        return;
    }

    if (!state.active) {
        state.displayedValue = state.targetValue;
        state.trailValue = state.targetValue;
        return;
    }

    state.elapsedSeconds += std::max(0.0f, deltaSeconds);
    const float t = battle::easing::clamp01(state.elapsedSeconds / state.durationSeconds);
    const float eased = battle::easing::easeOutCubic(t);
    state.displayedValue = battle::easing::lerp(state.fromValue, state.targetValue, eased);

    if (state.targetValue >= state.fromValue) {
        state.trailValue = state.displayedValue;
    }

    if (t >= 1.0f) {
        state.active = false;
        state.displayedValue = state.targetValue;
        state.fromValue = state.targetValue;
        state.trailValue = state.targetValue;
    }
}

void syncHudHitReaction(HudHitReactionState& state, Uint64 untilMs, Uint64 nowMs) {
    if (untilMs <= nowMs) {
        state.active = false;
        state.untilMs = 0;
        state.startedMs = 0;
        return;
    }

    if (!state.active || untilMs > state.untilMs) {
        state.active = true;
        state.startedMs = nowMs;
        state.untilMs = untilMs;
    }
}

void resetHudAnimationState(HudAnimationState& state, const battle::BattleManager& manager) {
    state = HudAnimationState{};
    resetHudValueAnimation(state.bossHp, static_cast<float>(manager.getBossCurrentHp()));

    const battle::BattleState& battleState = manager.getBattleState();
    state.units.assign(battleState.party.size(), battle::app::ui::HudUnitAnimationState{});
    for (int index = 0; index < static_cast<int>(battleState.party.size()); ++index) {
        resetHudValueAnimation(
            state.units[static_cast<size_t>(index)].hp,
            static_cast<float>(manager.getCharacterCurrentHp(index)));
        resetHudValueAnimation(
            state.units[static_cast<size_t>(index)].ultimate,
            static_cast<float>(manager.getCharacterUltimateCharge(index)));
    }
}

void updateHudAnimationState(HudAnimationState& state,
                             const battle::BattleManager& manager,
                             const HudFeedbackState& feedback,
                             float deltaSeconds,
                             Uint64 nowMs) {
    retargetHudValueAnimation(state.bossHp, static_cast<float>(manager.getBossCurrentHp()));
    advanceHudValueAnimation(state.bossHp, deltaSeconds);
    syncHudHitReaction(state.bossHit, feedback.bossHitUntilMs, nowMs);

    const battle::BattleState& battleState = manager.getBattleState();
    if (state.units.size() != battleState.party.size()) {
        resetHudAnimationState(state, manager);
    }
    for (int index = 0; index < static_cast<int>(state.units.size()); ++index) {
        auto& unitState = state.units[static_cast<size_t>(index)];
        const float hpTarget = static_cast<float>(manager.getCharacterCurrentHp(index));
        const float ultimateTarget = static_cast<float>(manager.getCharacterUltimateCharge(index));
        const bool hadUltimateTarget = unitState.ultimate.initialized;
        const float previousUltimateTarget = unitState.ultimate.targetValue;

        retargetHudValueAnimation(unitState.hp, hpTarget);
        retargetHudValueAnimation(unitState.ultimate, ultimateTarget);
        advanceHudValueAnimation(unitState.hp, deltaSeconds);
        advanceHudValueAnimation(unitState.ultimate, deltaSeconds);

        if (hadUltimateTarget && std::fabs(previousUltimateTarget - ultimateTarget) > 0.001f) {
            unitState.ultSheenStartedMs = nowMs;
            unitState.ultSheenUntilMs = nowMs + kHudUltSheenDurationMs;
        } else if (unitState.ultSheenUntilMs <= nowMs) {
            unitState.ultSheenStartedMs = 0;
            unitState.ultSheenUntilMs = 0;
        }

        const Uint64 hitUntilMs =
            index < static_cast<int>(feedback.unitHitUntilMs.size())
                ? feedback.unitHitUntilMs[static_cast<size_t>(index)]
                : 0;
        syncHudHitReaction(unitState.hit, hitUntilMs, nowMs);
    }
}

std::string revealNarrationText(const std::string& fullText, Uint64 startedMs, Uint64 nowMs, float charsPerSecond) {
    if (fullText.empty()) {
        return std::string();
    }

    if (nowMs <= startedMs) {
        return std::string();
    }

    const double elapsedSeconds = static_cast<double>(nowMs - startedMs) / 1000.0;
    const size_t visibleCodepoints = static_cast<size_t>(std::floor(elapsedSeconds * charsPerSecond));
    const size_t visibleBytes = utf8ByteOffsetForCodepoints(fullText, visibleCodepoints);
    if (visibleBytes >= fullText.size()) {
        return fullText;
    }
    return fullText.substr(0, visibleBytes);
}

battle::app::ui::BattleHudDocumentDependencies makeBattleHudDocumentDependencies() {
    return battle::app::ui::BattleHudDocumentDependencies{
        uppercase,
        platform::path::findCombatImagePath,
        revealNarrationText
    };
}

void startTutorial(TutorialOverlayState& tutorial,
                   TutorialStep step,
                   const vn::ScriptEntry& entry,
                   Uint64 nowMs) {
    tutorial.step = step;
    tutorial.entry = entry;
    tutorial.startedMs = nowMs;
    tutorial.audioPlayed = false;
}

void dismissTutorialOverlay(TutorialOverlayState& tutorial) {
    tutorial.step = TutorialStep::None;
    tutorial.entry = vn::ScriptEntry{};
    tutorial.startedMs = 0;
    tutorial.audioPlayed = false;
}

void completeTutorialStep(TutorialOverlayState& tutorial) {
    switch (tutorial.step) {
        case TutorialStep::Standard:
            tutorial.standardShown = true;
            break;
        case TutorialStep::Skill:
            tutorial.skillShown = true;
            break;
        case TutorialStep::Ultimate:
            tutorial.ultimateShown = true;
            break;
        case TutorialStep::None:
        default:
            break;
    }
    dismissTutorialOverlay(tutorial);
}

void skipAllTutorials(TutorialOverlayState& tutorial) {
    tutorial.standardShown = true;
    tutorial.skillShown = true;
    tutorial.ultimateShown = true;
    tutorial.dismissed = true;
    dismissTutorialOverlay(tutorial);
}

bool loadTutorialScriptLibrary(TutorialScriptLibrary& outLibrary) {
    vn::Script script;
    if (!vn::loadScript(platform::path::resolvePath("assets/vn/json/demo.json"), script)) {
        return false;
    }
    if (script.entries.size() < 3) {
        return false;
    }

    outLibrary.standard = script.entries[0];
    outLibrary.skill = script.entries[1];
    outLibrary.ultimate = script.entries[2];
    outLibrary.loaded = true;
    return true;
}

void consumeBattleActionEvents(HudFeedbackState& feedback,
                               battle::BattleManager& manager,
                               float voiceVolume,
                               Uint64 nowMs,
                               battle::render::BattleCameraStaging* cameraStaging = nullptr) {
    const battle::BattleState& battleState = manager.getBattleState();
    for (const battle::BattleActionEvent& event : manager.getRecentActionEvents()) {
        if (cameraStaging != nullptr && event.actorType == battle::ParticipantType::Boss) {
            cameraStaging->queueCharacterTurnIntro();
        }

        const std::string actorVoiceKey = event.actorType == battle::ParticipantType::Boss
            ? battleState.boss.key
            : ((event.actorPartyIndex >= 0 && event.actorPartyIndex < static_cast<int>(battleState.party.size()))
                ? battleState.party[static_cast<size_t>(event.actorPartyIndex)].assets
                : std::string());

        if (!event.abilityVoicesHandledDuringPresentation && event.action == battle::BattleAction::Skill) {
            if (const auto abilityVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "ability");
                abilityVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*abilityVoice, voiceVolume);
            } else if (const auto skillVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "skill");
                       skillVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*skillVoice, voiceVolume);
            }
        } else if (!event.abilityVoicesHandledDuringPresentation && event.action == battle::BattleAction::Ultimate) {
            if (const auto ultimateVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "ultimate");
                ultimateVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*ultimateVoice, voiceVolume);
            } else if (const auto abilityVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "ability");
                       abilityVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*abilityVoice, voiceVolume);
            }
        }

        if (event.bossHpAfter < event.bossHpBefore) {
            markBossHit(feedback, nowMs);
        }
        if (!event.hitVoicesHandledDuringPresentation && event.bossHpAfter < event.bossHpBefore) {
            if (event.bossHpBefore > 0 && event.bossHpAfter <= 0) {
                const auto deadVoice = resolveBossDeadVoicePath(battleState);
                if (deadVoice.has_value()) {
                    if (const auto hitVoice = resolveBossHitVoicePath(battleState); hitVoice.has_value()) {
                        gOneShotAudio.stopPlayback(*hitVoice);
                    }
                    (void)gOneShotAudio.playWavOneShot(*deadVoice, voiceVolume);
                } else {
                    (void)playBossHitVoice(battleState, voiceVolume);
                }
            } else {
                (void)playBossHitVoice(battleState, voiceVolume);
            }
        }

        if (event.hitVoicesHandledDuringPresentation) {
            for (size_t i = 0; i < event.targetPartyIndices.size() && i < event.targetHpBefore.size() && i < event.targetHpAfter.size(); ++i) {
                if (event.targetHpAfter[i] < event.targetHpBefore[i]) {
                    markUnitHit(feedback, event.targetPartyIndices[i], nowMs);
                }
            }
            continue;
        }

        for (size_t i = 0; i < event.targetPartyIndices.size() && i < event.targetHpBefore.size() && i < event.targetHpAfter.size(); ++i) {
            if (event.targetHpAfter[i] >= event.targetHpBefore[i]) {
                continue;
            }
            const int partyIndex = event.targetPartyIndices[i];
            if (partyIndex < 0 || partyIndex >= static_cast<int>(battleState.party.size())) {
                continue;
            }
            markUnitHit(feedback, partyIndex, nowMs);
            const std::string& assetKey = battleState.party[static_cast<size_t>(partyIndex)].assets;
            if (event.targetHpBefore[i] > 0 && event.targetHpAfter[i] <= 0) {
                if (const auto deadVoice = platform::path::resolveCombatVoicePath(assetKey, "dead"); deadVoice.has_value()) {
                    if (const auto hitVoice = platform::path::resolveCombatVoicePath(assetKey, "hit"); hitVoice.has_value()) {
                        gOneShotAudio.stopPlayback(*hitVoice);
                    }
                    (void)gOneShotAudio.playWavOneShot(*deadVoice, voiceVolume);
                } else if (const auto hitVoice = platform::path::resolveCombatVoicePath(assetKey, "hit"); hitVoice.has_value()) {
                    (void)gOneShotAudio.playWavOneShot(*hitVoice, voiceVolume);
                }
            } else if (const auto hitVoice = platform::path::resolveCombatVoicePath(assetKey, "hit"); hitVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*hitVoice, voiceVolume);
            }
        }
    }
    manager.clearRecentActionEvents();
}

void applyGoalCamera(battle::Camera3D& camera) {
    camera.posX = kGoalCameraPosX;
    camera.posY = kGoalCameraPosY;
    camera.posZ = kGoalCameraPosZ;
    camera.pitchDegrees = kGoalCameraPitch;
    camera.yawDegrees = kGoalCameraYaw;
    camera.focalLength = kGoalCameraFocal;
}

void startActionIntroCamera(battle::Camera3D& camera, CameraIntroAnimation& anim) {
    anim.active = true;
    anim.elapsed = 0.0f;
    anim.duration = kActionIntroDurationSeconds;

    anim.startX = kGoalCameraPosX + kActionIntroOffsetX;
    anim.startY = kGoalCameraPosY + kActionIntroOffsetY;
    anim.startZ = kGoalCameraPosZ + kActionIntroOffsetZ;
    anim.startPitch = kGoalCameraPitch;
    anim.startYaw = kGoalCameraYaw;
    anim.startFocal = kGoalCameraFocal;

    anim.goalX = kGoalCameraPosX;
    anim.goalY = kGoalCameraPosY;
    anim.goalZ = kGoalCameraPosZ;
    anim.goalPitch = kGoalCameraPitch;
    anim.goalYaw = kGoalCameraYaw;
    anim.goalFocal = kGoalCameraFocal;

    camera.posX = anim.startX;
    camera.posY = anim.startY;
    camera.posZ = anim.startZ;
    camera.pitchDegrees = anim.startPitch;
    camera.yawDegrees = anim.startYaw;
    camera.focalLength = anim.startFocal;
}

void updateActionIntroCamera(battle::Camera3D& camera, CameraIntroAnimation& anim, float deltaSeconds) {
    if (!anim.active) {
        return;
    }

    anim.elapsed += deltaSeconds;
    const float t = battle::easing::clamp01(anim.elapsed / std::max(0.001f, anim.duration));
    const float eased = battle::easing::easeOutCubic(t);

    camera.posX = battle::easing::lerp(anim.startX, anim.goalX, eased);
    camera.posY = battle::easing::lerp(anim.startY, anim.goalY, eased);
    camera.posZ = battle::easing::lerp(anim.startZ, anim.goalZ, eased);
    camera.pitchDegrees = battle::easing::lerp(anim.startPitch, anim.goalPitch, eased);
    camera.yawDegrees = battle::easing::lerp(anim.startYaw, anim.goalYaw, eased);
    camera.focalLength = battle::easing::lerp(anim.startFocal, anim.goalFocal, eased);

    if (t >= 1.0f) {
        anim.active = false;
        applyGoalCamera(camera);
    }
}

std::vector<std::string> resolveBattlePartyLineup(const battle::BattleDefinition& battleDefinition,
                                                  const battle::PlayerProgression& progression,
                                                  std::optional<std::vector<std::string>> initialPartyLineup) {
    auto appendUnique = [](std::vector<std::string>& out, const std::string& key) {
        if (key.empty()) {
            return;
        }
        if (std::find(out.begin(), out.end(), key) == out.end()) {
            out.push_back(key);
        }
    };

    std::vector<std::string> lineup;
    lineup.reserve(static_cast<size_t>(std::max(1, battleDefinition.partySize)));

    if (initialPartyLineup.has_value()) {
        for (const std::string& key : *initialPartyLineup) {
            appendUnique(lineup, key);
        }
    } else if (battleDefinition.isLineupFixed) {
        for (const std::string& key : battleDefinition.lockedLineup) {
            appendUnique(lineup, key);
        }
        for (const std::string& key : battleDefinition.lineup) {
            appendUnique(lineup, key);
        }
    } else {
        for (const std::string& key : battleDefinition.lockedLineup) {
            appendUnique(lineup, key);
        }
        for (const std::string& key : progression.currentPartyLineup) {
            appendUnique(lineup, key);
        }
        for (const std::string& key : battleDefinition.lineup) {
            appendUnique(lineup, key);
        }
    }

    const int safePartySize = std::max(1, battleDefinition.partySize);
    if (static_cast<int>(lineup.size()) > safePartySize) {
        lineup.resize(static_cast<size_t>(safePartySize));
    }

    return lineup;
}

int findEntityIndexByPartyIndex(const std::vector<SceneEntity>& entities, int partyIndex) {
    for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
        if (!entities[static_cast<size_t>(i)].isBoss &&
            entities[static_cast<size_t>(i)].partyIndex == partyIndex) {
            return i;
        }
    }
    return -1;
}

int findBossEntityIndex(const std::vector<SceneEntity>& entities) {
    for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
        if (entities[static_cast<size_t>(i)].isBoss) {
            return i;
        }
    }
    return -1;
}

} // namespace

namespace battle::app {

class SessionImpl {
public:
    bool initialize(Window& hostWindow,
                    GameSettings& settings,
                    const std::string& battleKey,
                    const battle::PlayerProgression& progression,
                    std::optional<std::vector<std::string>> initialPartyLineup) {
        shutdown();

        windowHost_ = &hostWindow;
        settings_ = &settings;
        window_ = hostWindow.getNativeWindow();
        glContext_ = hostWindow.getGlContext();
        if (window_ == nullptr || glContext_ == nullptr) {
            std::cerr << "[Battle] Window is not in OpenGL mode.\n";
            return false;
        }

        if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL subsystem init failed: " << SDL_GetError() << "\n";
            return false;
        }

        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        SDL_GL_MakeCurrent(window_, glContext_);
        SDL_GL_SetSwapInterval(1);
        SDL_StopTextInput();
        frameTimingEnabled_ = SDL_getenv("BIT_BATTLE_TIMING") != nullptr;
        frameTimingFrequency_ = SDL_GetPerformanceFrequency();
        timingFrameCount_ = 0;
        timingWorldMs_ = 0.0;
        timingCompatibilityMs_ = 0.0;
        timingUiMs_ = 0.0;

#ifdef BATTLE_ENABLE_IMAGE
        const int requiredImageFlags = IMG_INIT_PNG | IMG_INIT_WEBP;
        if ((IMG_Init(requiredImageFlags) & requiredImageFlags) == requiredImageFlags) {
            imageInitialized_ = true;
        } else {
            std::cerr << "SDL_image init failed: " << IMG_GetError() << "\n";
        }
#endif

        Rml::String glInitMessage;
        if (!RmlGL3::Initialize(&glInitMessage)) {
            std::cerr << "RmlGL3 initialization failed: " << glInitMessage << "\n";
            shutdown();
            return false;
        }
        rmlGlInitialized_ = true;

        if (!screenBlitter_.initialize()) {
            std::cerr << "Failed to initialize screen blitter\n";
            shutdown();
            return false;
        }
        if (!presentationOverlayBlitter_.initialize()) {
            std::cerr << "Failed to initialize presentation overlay blitter\n";
            shutdown();
            return false;
        }
        if (!glSceneRenderer_.initialize()) {
            std::cerr << "Failed to initialize GL battle scene renderer\n";
            shutdown();
            return false;
        }

        systemInterface_.SetWindow(window_);
        renderInterface_ = std::make_unique<graphics::RmlUiSdlGlRenderInterface>();
        if (!(*renderInterface_)) {
            std::cerr << "RmlUi GL3 render interface construction failed\n";
            shutdown();
            return false;
        }

        Rml::SetSystemInterface(&systemInterface_);
        Rml::SetRenderInterface(renderInterface_.get());
        if (!Rml::Initialise()) {
            std::cerr << "RmlUi core initialization failed\n";
            shutdown();
            return false;
        }
        rmlInitialized_ = true;

        if (!loadFonts()) {
            std::cerr << "[Battle] No usable fonts were loaded.\n";
            shutdown();
            return false;
        }

        const std::string resolvedBattleKey = battleKey.empty() ? "tutorial_vs_lyoo" : battleKey;
        if (!battle::loader::loadBattleDefinition(resolvedBattleKey, battleDefinition_)) {
            std::cerr << "[Battle] Failed to resolve battle definition: " << resolvedBattleKey << "\n";
            shutdown();
            return false;
        }
        if (!battle::render::loadStageDefinition(battleDefinition_.stageKey, stageDefinition_)) {
            std::cerr << "[Battle] Failed to resolve stage definition, using built-in fallback.\n";
            stageDefinition_ = battle::render::StageDefinition{};
        }

        activePartyLineup_ = resolveBattlePartyLineup(battleDefinition_, progression, std::move(initialPartyLineup));
        if (activePartyLineup_.empty()) {
            std::cerr << "[Battle] Battle requires at least one party member.\n";
            shutdown();
            return false;
        }

        if (!manager_.initialize(battleDefinition_, activePartyLineup_)) {
            std::cerr << "[Battle] Initialization failed.\n";
            shutdown();
            return false;
        }

        tutorialEnabled_ = false;
        narrativeEnabled_ = (battleDefinition_.type == "tutorial");
        narrativeInitialized_ = false;

#ifdef BATTLE_ENABLE_TTF
        if (!narrativeEnabled_ && TTF_WasInit() == 0) {
            if (TTF_Init() != 0) {
                std::cerr << "SDL_ttf init failed: " << TTF_GetError() << "\n";
                shutdown();
                return false;
            }
            ttfInitializedHere_ = true;
        }
#endif

        battle::ability::setPresentationInteractionRunner([this](const battle::PresentationContext& context) {
            return runPresentationInteraction(context);
        });

        const battle::BattleState& state = manager_.getBattleState();
        battleBgmBaseVolume_ = std::clamp(state.boss.bgmVolume, 0.0f, 1.0f);
        if (!state.boss.bgm.empty()) {
            if (const auto bgmPath = platform::path::resolveCombatBgmPath(state.boss.bgm); bgmPath.has_value()) {
                gBgmPlayer.play(*bgmPath, battleBgmBaseVolume_ * currentMusicMasterVolume());
            }
        }
        worldAssets_.clear();
        for (const battle::CharacterDefinition& character : state.party) {
            worldAssets_.push_back(character.assets);
        }
        worldAssets_.push_back(state.boss.assets);
        if (std::find(worldAssets_.begin(), worldAssets_.end(), "miku") == worldAssets_.end()) {
            worldAssets_.push_back("miku");
        }

        updateViewportFromWindow();
        if (!sceneRenderer_.initialize(sceneWidth(), sceneHeight(), worldAssets_, resolveBattleSpritePath)) {
            shutdown();
            return false;
        }
        if (!presentationOverlayRenderer_.initialize(sceneWidth(), sceneHeight(), worldAssets_, resolveBattleSpritePath)) {
            shutdown();
            return false;
        }
        (void)glSceneRenderer_.ensureWorldAssets(worldAssets_);
        (void)glSceneRenderer_.ensureStageAssets(stageDefinition_);

        if (narrativeEnabled_) {
            if (!vn::initialize(presentationOverlayRenderer_.renderer, sceneWidth(), sceneHeight())) {
                std::cerr << "[Battle] Failed to initialize VN system for narrative overlay.\n";
                shutdown();
                return false;
            }
            if (settings_ != nullptr) {
                vn::setMusicVolume(settings_->musicVolume);
                vn::setVoiceVolume(settings_->voiceVolume);
                vn::setTypewriterSpeed(settings_->textSpeed);
            }
            if (!narrative_.initialize()) {
                std::cerr << "[Battle] Failed to initialize battle narrative flow.\n";
                shutdown();
                return false;
            }
            narrativeInitialized_ = true;
        }

        initializeWorldEntities();
        const battle::flow::PreviewActorContext preview = battle::flow::inspectPreviewActor(manager_);
        const bool bossActing = !(preview.valid && preview.type == battle::ParticipantType::Character);
        const int actingPartyIndex =
            (!bossActing && preview.partyIndex >= 0) ? preview.partyIndex : -1;
        updateSceneEntities(0.0f, bossActing, actingPartyIndex);
        feedback_.reset(manager_);
        syncHudFeedbackState(hudFeedback_, manager_);
        resetHudAnimationState(hudAnimationState_, manager_);

        SDL_Texture* mikuSprite = nullptr;
        if (const auto mikuIt = sceneRenderer_.textureByAsset.find("miku"); mikuIt != sceneRenderer_.textureByAsset.end()) {
            mikuSprite = mikuIt->second;
        }
        SDL_Texture* bossSprite = nullptr;
        if (const auto bossIt = sceneRenderer_.textureByAsset.find(state.boss.assets); bossIt != sceneRenderer_.textureByAsset.end()) {
            bossSprite = bossIt->second;
        }
        combatBeginAnimation_.reset(mikuSprite, bossSprite);
        activeUltimateTurnSplash_.reset();
        previewUltimateSplashPartyIndex_ = -1;
        discardNextUpdateDelta_ = false;
        activeOverlay_ = nullptr;

        renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
        context_ = Rml::CreateContext("battle-app", Rml::Vector2i(windowWidth_, windowHeight_));
        if (context_ == nullptr) {
            std::cerr << "Failed to create RmlUi context\n";
            shutdown();
            return false;
        }

        const std::string documentPath = platform::path::resolvePath("assets/rmlui/battle_hud.rml");
        document_ = context_->LoadDocument(documentPath);
        if (document_ == nullptr) {
            std::cerr << "Failed to load RmlUi document: " << documentPath << "\n";
            shutdown();
            return false;
        }
        document_->Show();
        applyReferenceSceneMetrics();
        pauseDocument_ = nullptr;
        pauseMenuController_.reset();
        settingsDocument_ = nullptr;
        settingsMenuController_.reset();
        pauseMenuTracks_ = resolveUiMusicTrackPaths();
        pauseMenuTrackCursor_ = 0;
        battleBgmWasPlayingBeforePause_ = false;
        battleBgmWasPausedBeforePause_ = false;
        pauseMenuUiState_ = AppState{};
        pauseMenuUiState_.screen = ScreenState::PauseMenu;
        pauseMenuUiState_.pauseContext = PauseContext::Battle;
        pauseMenuUiState_.pauseSelection = PauseAction::Continue;
        pauseMenuUiState_.confirmSelection = ConfirmAction::Cancel;
        pauseMenuUiState_.settingsReturnScreen = ScreenState::PauseMenu;
        settingsMenuUiState_ = AppState{};
        settingsMenuUiState_.screen = ScreenState::Settings;
        settingsMenuUiState_.settingsReturnScreen = ScreenState::PauseMenu;
        settingsMenuUiState_.settingsSelection = SettingsItem::DisplayMode;
        settingsMenuUiState_.pauseContext = PauseContext::Battle;

        if (!loadingOverlay_.initialize(*context_, platform::path::resolvePath(kLoadingOverlayDocumentPath))) {
            std::cerr << "[Battle] Failed to load loading overlay document.\n";
            shutdown();
            return false;
        }
        loadingOverlayState_ = graphics::RmlUiLoadingOverlayState{};

        const battle::app::ui::AttachListenerFn attachOverlayListener =
            [this](const std::string& id, Rml::EventId eventId, std::function<void(Rml::Event&)> callback) {
                attachListener(id, eventId, std::move(callback));
            };
        battle::app::ui::bindRhythmOverlayControls(attachOverlayListener, [this]() {
            if (!rhythmChallenge_.active) {
                return;
            }
            const Uint64 hitTime = SDL_GetTicks64();
            const float progress = getRhythmProgress(rhythmChallenge_, hitTime);
            const float halfWindow = rhythmChallenge_.targetWindow * 0.5f;
            finalizeSkillChallenge(progress >= rhythmChallenge_.targetCenter - halfWindow &&
                                   progress <= rhythmChallenge_.targetCenter + halfWindow);
        });
        battle::app::ui::bindPauseOverlayControls(attachOverlayListener, {
            [this]() {
                pauseOverlayMode_ = PauseOverlayMode::Menu;
                paused_ = false;
                pauseSelection_ = PauseSelection::Continue;
            },
            [this]() {
                pauseOverlayMode_ = PauseOverlayMode::Settings;
            },
            [this]() {
                exitToMainMenuRequested_ = true;
                finished_ = true;
            },
            [this]() {
                toggleDisplayMode();
            },
            [this]() {
                pauseOverlayMode_ = PauseOverlayMode::Menu;
            },
            [this](PauseSelection selection) {
                pauseSelection_ = selection;
            },
            [this](SettingsSelection selection) {
                settingsSelection_ = selection;
            },
            [this](float value) {
                settingsSelection_ = SettingsSelection::VoiceVolume;
                if (settings_ == nullptr) {
                    return;
                }
                settings_->voiceVolume = std::clamp(value / 100.0f, 0.0f, 1.0f);
            },
            [this](float value) {
                settingsSelection_ = SettingsSelection::TextSpeed;
                if (settings_ == nullptr) {
                    return;
                }
                settings_->textSpeed = std::clamp(value, kMinSettingsTextSpeed, kMaxSettingsTextSpeed);
            }
        });

        battle::Camera3D goalCamera = battle::render::makeDefaultBattleCamera();
        battle::render::applyStageCameraOverride(stageDefinition_.camera, goalCamera);
        cameraStaging_.setGoalCamera(goalCamera);
        camera_.screenCenterX = sceneWidth() / 2;
        camera_.screenCenterY = sceneHeight() / 2;
        cameraStaging_.reset(camera_);
        syncHudDocument(SDL_GetTicks64());

        initialized_ = true;
        return true;
    }

    void shutdown() {
        initialized_ = false;
        resetIdleVoicelineState();
        battle::ability::setPresentationInteractionRunner(nullptr);
        freeViewCameraDebugLog_.clear();
        closeSettingsMenuDocument();
        closePauseMenuDocument();
        stopPauseMenuMusic();
        if (document_ != nullptr) {
            document_->Close();
            document_ = nullptr;
        }
        loadingOverlay_.shutdown();
        if (rmlInitialized_) {
            Rml::Shutdown();
            rmlInitialized_ = false;
        }
        if (rmlGlInitialized_) {
            RmlGL3::Shutdown();
            rmlGlInitialized_ = false;
        }
        uiListeners_.clear();
        renderInterface_.reset();
        screenBlitter_.destroy();
        presentationOverlayBlitter_.destroy();
        glSceneRenderer_.destroy();
        presentationOverlayRenderer_.destroy();
        sceneRenderer_.destroy();
        feedback_.shutdown();
        narrative_.shutdown();
        vn::stopVoicePlayback();
        vn::shutdown();
#ifdef BATTLE_ENABLE_TTF
        if (ttfInitializedHere_ && TTF_WasInit() != 0) {
            TTF_Quit();
            ttfInitializedHere_ = false;
        }
#endif
        stopPresentationAudioPlayback(false);
        gPresentationSfxAudio.shutdown();
        gBgmPlayer.stop();
        gOneShotAudio.shutdown();
        gOneShotAudio.cleanupFinishedPlayback();
#ifdef BATTLE_ENABLE_IMAGE
        if (imageInitialized_) {
            IMG_Quit();
            imageInitialized_ = false;
        }
#endif
        context_ = nullptr;
        window_ = nullptr;
        glContext_ = nullptr;
        windowHost_ = nullptr;
        finished_ = false;
        exitToMainMenuRequested_ = false;
        paused_ = false;
        freeViewEnabled_ = false;
        pauseOverlayMode_ = PauseOverlayMode::Menu;
        pauseSelection_ = PauseSelection::Continue;
        settingsSelection_ = SettingsSelection::DisplayMode;
        settings_ = nullptr;
        tutorialEnabled_ = false;
        narrativeEnabled_ = false;
        narrativeInitialized_ = false;
        tutorialOverlay_ = TutorialOverlayState{};
        tutorialLibrary_ = TutorialScriptLibrary{};
        hudFeedback_ = HudFeedbackState{};
        hudAnimationState_ = HudAnimationState{};
        rhythmChallenge_ = RhythmChallengeState{};
        bufferedManualUltimatePartyIndices_.clear();
        presentationPlaybackActive_ = false;
        presentationCasterIsBoss_ = false;
        presentationCasterPartyIndex_ = -1;
        activePresentation_ = nullptr;
        worldAssets_.clear();
        entities_.clear();
        lastTurnToken_.clear();
        presentationAudioSequenceId_.clear();
        presentationAudioCueIndex_ = 0;
        activeUltimateTurnSplash_.reset();
        previewUltimateSplashPartyIndex_ = -1;
        activeOverlay_ = nullptr;
        frameAccumulator_ = 0.0f;
        discardNextUpdateDelta_ = false;
        battleDefinition_ = battle::BattleDefinition{};
        stageDefinition_ = battle::render::StageDefinition{};
        activePartyLineup_.clear();
        loadingOverlayState_ = graphics::RmlUiLoadingOverlayState{};
        drawableWidth_ = kWindowWidth;
        drawableHeight_ = kWindowHeight;
        frameTimingEnabled_ = false;
        frameTimingFrequency_ = 0;
        timingFrameCount_ = 0;
        timingWorldMs_ = 0.0;
        timingCompatibilityMs_ = 0.0;
        timingUiMs_ = 0.0;
    }

    void handleEvent(const SDL_Event& event) {
        if (!initialized_ || context_ == nullptr || window_ == nullptr) {
            return;
        }

        if (isWindowResizeEvent(event)) {
            updateViewportFromWindow();
            applyReferenceSceneMetrics();
            if (!refreshSceneRenderers()) {
                finished_ = true;
            }
        }

        if (event.type == SDL_KEYDOWN && event.key.repeat != 0) {
            return;
        }

        if (combatBeginAnimation_.isActive()) {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                finished_ = true;
            }
            return;
        }

        if (activeUltimateTurnSplash_ != nullptr) {
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    finished_ = true;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    activeUltimateTurnSplash_->skip();
                } else {
                    (void)handleManualUltimateHotkey(event.key.keysym.sym, SDL_GetTicks64());
                }
            }
            return;
        }

        if (paused_) {
            SDL_Event mutablePausedEvent = makeScaledRmlInputEvent(event);
            RmlSDL::InputEventHandler(context_, window_, mutablePausedEvent);
            handlePauseEvent(event);
            return;
        }

        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            enterPauseMenu();
            return;
        }

        SDL_Event mutableEvent = makeScaledRmlInputEvent(event);
        RmlSDL::InputEventHandler(context_, window_, mutableEvent);

        if (event.type == SDL_KEYDOWN) {
            const Uint64 nowMs = SDL_GetTicks64();
            if (event.key.keysym.sym == SDLK_SPACE) {
                handleSpaceKeyIdle(nowMs);
            }
            if (event.key.keysym.sym == SDLK_f) {
                freeViewEnabled_ = !freeViewEnabled_;
                if (!freeViewEnabled_) {
                    cameraStaging_.snapToGoalCamera(camera_);
                    freeViewCameraDebugLog_.clear();
                }
            } else if (event.key.keysym.sym == SDLK_SPACE && isDialogueInProgress()) {
                narrative_.onDialogueSpacePressed();
                return;
            } else if (event.key.keysym.sym == SDLK_SPACE && tutorialOverlay_.step != TutorialStep::None) {
                const float textSpeed = settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond;
                const std::string revealed = revealNarrationText(
                    tutorialOverlay_.entry.text,
                    tutorialOverlay_.startedMs,
                    nowMs,
                    textSpeed
                );
                if (revealed.size() < tutorialOverlay_.entry.text.size()) {
                    tutorialOverlay_.startedMs = 0;
                } else {
                    gOneShotAudio.shutdown();
                    completeTutorialStep(tutorialOverlay_);
                }
                return;
            } else if (event.key.keysym.sym == SDLK_BACKSPACE && tutorialOverlay_.step != TutorialStep::None) {
                gOneShotAudio.shutdown();
                skipAllTutorials(tutorialOverlay_);
            } else if (rhythmChallenge_.active) {
                if (handleManualUltimateHotkey(event.key.keysym.sym, nowMs)) {
                    return;
                }
                if (event.key.keysym.sym == SDLK_e) {
                    const Uint64 hitTime = nowMs;
                    const float progress = getRhythmProgress(rhythmChallenge_, hitTime);
                    const float halfWindow = rhythmChallenge_.targetWindow * 0.5f;
                    finalizeSkillChallenge(progress >= rhythmChallenge_.targetCenter - halfWindow &&
                                           progress <= rhythmChallenge_.targetCenter + halfWindow);
                }
            } else if (!freeViewEnabled_) {
                if (handleManualUltimateHotkey(event.key.keysym.sym, nowMs)) {
                    return;
                }
                if (event.key.keysym.sym == SDLK_SPACE) {
                    if (!isBattleSpaceEnabled() || manager_.isBattleOver()) {
                        return;
                    }

                    const battle::flow::PreviewActorContext preview = battle::flow::inspectPreviewActor(manager_);
                    maybeStartUltimateTurnSplash(
                        preview,
                        isDialogueInProgress() || tutorialOverlay_.step != TutorialStep::None);
                    if (activeUltimateTurnSplash_ != nullptr) {
                        return;
                    }

                    const battle::flow::PlayerTurnExecution turnExecution =
                        battle::flow::executeDefaultPlayerTurn(manager_);
                    if (!turnExecution.executed) {
                        showToast(hudFeedback_, "WAIT FOR AN ALLY TURN.", nowMs);
                        return;
                    }

                    const bool allowAutoTurns =
                        !narrativeEnabled_ ||
                        !narrativeInitialized_ ||
                        narrative_.onPlayerTurnExecuted(turnExecution);
                    if (allowAutoTurns) {
                        manager_.processAutomaticTurns();
                    }
                    consumeBattleActionEvents(
                        hudFeedback_,
                        manager_,
                        settings_ != nullptr ? settings_->voiceVolume : 1.0f,
                        nowMs,
                        &cameraStaging_);
                    return;
                    // Legacy replacement of the multi-action controls:
                    // attemptAction(battle::BattleAction::Ultimate);
                    // attemptAction(battle::BattleAction::Standard);
                    // attemptAction(battle::BattleAction::Skill);
                }
            }
        } else if (event.type == SDL_MOUSEWHEEL) {
            if (freeViewEnabled_ && !cameraStaging_.isIntroActive()) {
                camera_.focalLength += event.wheel.y * 500.0f;
                camera_.focalLength = std::clamp(camera_.focalLength, 1000.0f, 50000.0f);
            }
        }
    }

    void update(float deltaSeconds) {
        if (!initialized_ || finished_) {
            return;
        }

        const Uint64 nowMs = SDL_GetTicks64();
        tickHudFeedback(hudFeedback_, nowMs);
        gOneShotAudio.cleanupFinishedPlayback();
        gPresentationSfxAudio.cleanupFinishedPlayback();

        if (discardNextUpdateDelta_) {
            discardNextUpdateDelta_ = false;
            deltaSeconds = 0.0f;
        }

        if (combatBeginAnimation_.isActive()) {
            combatBeginAnimation_.update(deltaSeconds);
            return;
        }

        if (paused_) {
            if (narrativeEnabled_ && narrativeInitialized_) {
                vn::setPaused(true);
            }
            updatePauseMenuUi(deltaSeconds);
            updateSettingsMenuUi(deltaSeconds);
            updateHudAnimationState(hudAnimationState_, manager_, hudFeedback_, deltaSeconds, nowMs);
            syncHudDocument(nowMs);
            return;
        }

        if (narrativeEnabled_ && narrativeInitialized_) {
            vn::setPaused(false);
            syncNarrativeSettings();
            vn::update(deltaSeconds);
            narrative_.maybeStartBossDefeatedDialogue(manager_);
            narrative_.handleAutomaticProgression(manager_);
        } else {
            manager_.processAutomaticTurns();
        }

        consumeBattleActionEvents(
            hudFeedback_,
            manager_,
            settings_ != nullptr ? settings_->voiceVolume : 1.0f,
            nowMs,
            &cameraStaging_);
        feedback_.syncFromManager(manager_, presentationPlaybackActive_);
        feedback_.update(deltaSeconds);
        maybeStartTutorial(nowMs);
        if (tutorialEnabled_ && tutorialOverlay_.step != TutorialStep::None &&
            !tutorialOverlay_.audioPlayed && !tutorialOverlay_.entry.voice.empty()) {
            if (gOneShotAudio.playWavOneShot(platform::path::resolvePath(tutorialOverlay_.entry.voice),
                                             settings_ != nullptr ? settings_->voiceVolume : 1.0f)) {
                tutorialOverlay_.audioPlayed = true;
            }
        }

        if (rhythmChallenge_.active && nowMs >= rhythmChallenge_.startedMs + rhythmChallenge_.durationMs) {
            finalizeSkillChallenge(false);
        }

        frameAccumulator_ += deltaSeconds;

        const bool dialogueActive = isDialogueInProgress();
        int previewActorIndex = manager_.getPreviewNextActorIndex();
        const battle::flow::PreviewActorContext preview = battle::flow::inspectPreviewActor(manager_);
        maybeStartUltimateTurnSplash(preview, dialogueActive || tutorialOverlay_.step != TutorialStep::None);
        if (activeUltimateTurnSplash_ != nullptr) {
            activeUltimateTurnSplash_->update(deltaSeconds);
            if (activeUltimateTurnSplash_->isComplete()) {
                activeUltimateTurnSplash_.reset();
            }
        }

        std::string turnToken = "none";
        bool nextIsCharacter = false;
        const battle::TurnActor* previewActor = nullptr;
        const battle::BattleState& state = manager_.getBattleState();
        if (previewActorIndex >= 0) {
            const battle::TurnState& turnState = manager_.getTurnState();
            if (previewActorIndex < static_cast<int>(turnState.actors.size())) {
                const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(previewActorIndex)];
                previewActor = &actor;
                turnToken = (actor.type == battle::ParticipantType::Boss ? "B:" : "C:") +
                            actor.key + ":" + std::to_string(actor.partyIndex) + ":" +
                            (actor.isExtraTurn ? "E" : "N");
                nextIsCharacter = actor.type == battle::ParticipantType::Character;
            }
        }

        if (turnToken != lastTurnToken_) {
            lastTurnToken_ = turnToken;
            onActiveActorChanged(previewActor, state, nowMs);
        }

        cameraStaging_.notifyTurnPreview(
            manager_.getTurnState(),
            previewActorIndex,
            dialogueActive || tutorialOverlay_.step != TutorialStep::None,
            freeViewEnabled_,
            camera_);

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float camMovementSpeed = 15.0f;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_W]) camera_.posY += camMovementSpeed;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_S]) camera_.posY -= camMovementSpeed;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_A]) camera_.posX -= camMovementSpeed;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_D]) camera_.posX += camMovementSpeed;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_E]) camera_.posZ += camMovementSpeed;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_Q]) camera_.posZ -= camMovementSpeed;

        const float rotationSpeed = 2.0f;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_LEFT]) camera_.yawDegrees -= rotationSpeed;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_RIGHT]) camera_.yawDegrees += rotationSpeed;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_UP]) camera_.pitchDegrees -= rotationSpeed;
        if (freeViewEnabled_ && !cameraStaging_.isIntroActive() && keys[SDL_SCANCODE_DOWN]) camera_.pitchDegrees += rotationSpeed;
        camera_.pitchDegrees = battle::clampFreeViewPitchDegrees(camera_.pitchDegrees);

        cameraStaging_.update(camera_, deltaSeconds, freeViewEnabled_);
        freeViewCameraDebugLog_.update(freeViewEnabled_ && !cameraStaging_.isIntroActive(), camera_);

        const bool bossActing = !(preview.valid && preview.type == battle::ParticipantType::Character);
        const int actingPartyIndex =
            (!bossActing && preview.partyIndex >= 0) ? preview.partyIndex : -1;
        updateSceneEntities(deltaSeconds, bossActing, actingPartyIndex);
        maybeHandleIdleVoiceline(nowMs);

        updateHudAnimationState(hudAnimationState_, manager_, hudFeedback_, deltaSeconds, nowMs);
        syncHudDocument(nowMs);

        if (manager_.isBattleOver() && !dialogueActive && !presentationPlaybackActive_ &&
            tutorialOverlay_.step == TutorialStep::None &&
            !rhythmChallenge_.active && activeUltimateTurnSplash_ == nullptr &&
            isBossDeathFadeComplete() && !isBattleFinishBlocked()) {
            finished_ = true;
        }
    }

    void render() {
        if (!initialized_ || sceneRenderer_.renderer == nullptr || sceneRenderer_.surface == nullptr) {
            return;
        }

        if (combatBeginAnimation_.isActive()) {
            combatBeginAnimation_.render(sceneRenderer_.renderer, sceneRenderer_.width, sceneRenderer_.height);
            screenBlitter_.uploadSurface(sceneRenderer_.surface);

            SDL_GL_MakeCurrent(window_, glContext_);
            glViewport(0, 0, drawableWidth_, drawableHeight_);
            glClearColor(0.035f, 0.043f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            screenBlitter_.draw();
            return;
        }

        int focusedIndex = -1;
        if (presentationPlaybackActive_) {
            int presentationFocusedPartyIndex = -1;
            if (activePresentation_ != nullptr) {
                presentationFocusedPartyIndex = activePresentation_->getFocusedPartyIndex();
            }

            if (presentationFocusedPartyIndex >= 0) {
                focusedIndex = findEntityIndexByPartyIndex(entities_, presentationFocusedPartyIndex);
            } else {
                focusedIndex = presentationCasterIsBoss_
                    ? findBossEntityIndex(entities_)
                    : findEntityIndexByPartyIndex(entities_, presentationCasterPartyIndex_);
            }
        }
        if (focusedIndex < 0) {
            const std::optional<int> activePartyIndex = getActiveCharacterPartyIndex(manager_);
            focusedIndex = activePartyIndex.has_value()
                ? findEntityIndexByPartyIndex(entities_, *activePartyIndex)
                : findBossEntityIndex(entities_);
        }
        if (presentationPlaybackActive_) {
            focusedIndex = std::max(focusedIndex, 0);
        }

        camera_.screenCenterX = sceneWidth() / 2;
        camera_.screenCenterY = sceneHeight() / 2;

        const battle::BattleSessionCore::BattleFrameSnapshot snapshot = buildFrameSnapshot(focusedIndex);
        SDL_GL_MakeCurrent(window_, glContext_);
        glViewport(0, 0, drawableWidth_, drawableHeight_);
        glClearColor(snapshot.blackoutWorld ? 0.0f : 0.035f,
                     snapshot.blackoutWorld ? 0.0f : 0.043f,
                     snapshot.blackoutWorld ? 0.0f : 0.07f,
                     1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        double compatibilityMs = 0.0;
        const Uint64 worldStartCounter = frameTimingEnabled_ ? SDL_GetPerformanceCounter() : 0;

        const bool nativePresentation =
            activePresentation_ != nullptr &&
            glSceneRenderer_.rendersPresentationNatively(activePresentation_);

        glSceneRenderer_.renderStageBase(snapshot, sceneWidth(), sceneHeight());

        if (!nativePresentation && activePresentation_ != nullptr) {
            SDL_SetRenderDrawBlendMode(sceneRenderer_.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(sceneRenderer_.renderer, 0, 0, 0, 0);
            SDL_RenderClear(sceneRenderer_.renderer);
            activePresentation_->renderBelowWorld(
                sceneRenderer_.renderer,
                sceneWidth(),
                sceneHeight(),
                snapshot.camera);
            const Uint64 compatibilityStartCounter = frameTimingEnabled_ ? SDL_GetPerformanceCounter() : 0;
            drawCompatibilitySurface(sceneRenderer_, screenBlitter_, true);
            if (frameTimingEnabled_) {
                compatibilityMs += elapsedTimingMs(compatibilityStartCounter, SDL_GetPerformanceCounter());
            }
        } else {
            (void)glSceneRenderer_.renderNativeBelowWorld(snapshot, sceneWidth(), sceneHeight());
        }

        glSceneRenderer_.renderWorld(snapshot, sceneWidth(), sceneHeight());

        if (snapshot.blackoutWorld) {
            glSceneRenderer_.renderFullscreenFade(
                sceneWidth(),
                sceneHeight(),
                SDL_Color{0, 0, 0, 255}
            );
        }

        const bool drewNativeMidWorld =
            nativePresentation && glSceneRenderer_.renderNativeMidWorld(snapshot, sceneWidth(), sceneHeight());
        const bool hasFeedbackPopups = feedback_.hasVisiblePopups();
        const bool renderFeedbackInOverlay =
            hasFeedbackPopups &&
            activePresentation_ != nullptr &&
            activePresentation_->shouldRenderAboveHud();
        if (!drewNativeMidWorld || (hasFeedbackPopups && !renderFeedbackInOverlay)) {
            SDL_SetRenderDrawBlendMode(sceneRenderer_.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(sceneRenderer_.renderer, 0, 0, 0, 0);
            SDL_RenderClear(sceneRenderer_.renderer);

            bool hasSceneCompatibilityPass = false;
            if (!drewNativeMidWorld &&
                activePresentation_ != nullptr &&
                !activePresentation_->shouldRenderAboveHud()) {
                activePresentation_->render(sceneRenderer_.renderer, sceneWidth(), sceneHeight(), snapshot.camera);
                hasSceneCompatibilityPass = true;
            }
            if (hasFeedbackPopups && !renderFeedbackInOverlay) {
                feedback_.render(sceneRenderer_.renderer, snapshot.camera, snapshot.feedbackAnchors);
                hasSceneCompatibilityPass = true;
            }
            if (hasSceneCompatibilityPass) {
                const Uint64 compatibilityStartCounter = frameTimingEnabled_ ? SDL_GetPerformanceCounter() : 0;
                drawCompatibilitySurface(sceneRenderer_, screenBlitter_, true);
                if (frameTimingEnabled_) {
                    compatibilityMs += elapsedTimingMs(compatibilityStartCounter, SDL_GetPerformanceCounter());
                }
            }
        }
        const double worldMs = frameTimingEnabled_
            ? elapsedTimingMs(worldStartCounter, SDL_GetPerformanceCounter()) - compatibilityMs
            : 0.0;

        loadingOverlay_.apply(loadingOverlayState_);
        const Uint64 uiStartCounter = frameTimingEnabled_ ? SDL_GetPerformanceCounter() : 0;
        context_->Update();
        renderInterface_->BeginFrame();
        context_->Render();
        renderInterface_->EndFrame();
        const double uiMs = frameTimingEnabled_
            ? elapsedTimingMs(uiStartCounter, SDL_GetPerformanceCounter())
            : 0.0;

        const bool drewNativeAboveHud =
            nativePresentation && glSceneRenderer_.renderNativeAboveHud(snapshot, sceneWidth(), sceneHeight());

        const bool hasSceneOverlayPass =
            (activePresentation_ != nullptr && activePresentation_->shouldRenderAboveHud() && !drewNativeAboveHud) ||
            renderFeedbackInOverlay;
        if (hasSceneOverlayPass && sceneRenderer_.renderer != nullptr && sceneRenderer_.surface != nullptr) {
            SDL_SetRenderDrawBlendMode(sceneRenderer_.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(sceneRenderer_.renderer, 0, 0, 0, 0);
            SDL_RenderClear(sceneRenderer_.renderer);

            if (activePresentation_ != nullptr && activePresentation_->shouldRenderAboveHud() && !drewNativeAboveHud) {
                activePresentation_->render(
                    sceneRenderer_.renderer,
                    sceneWidth(),
                    sceneHeight(),
                    snapshot.camera);
            }
            if (renderFeedbackInOverlay) {
                feedback_.render(sceneRenderer_.renderer, snapshot.camera, snapshot.feedbackAnchors);
            }
            const Uint64 compatibilityStartCounter = frameTimingEnabled_ ? SDL_GetPerformanceCounter() : 0;
            drawCompatibilitySurface(sceneRenderer_, screenBlitter_, true);
            if (frameTimingEnabled_) {
                compatibilityMs += elapsedTimingMs(compatibilityStartCounter, SDL_GetPerformanceCounter());
            }
        }

        const bool hasPresentationOverlayPass =
            activeUltimateTurnSplash_ != nullptr ||
            static_cast<bool>(activeOverlay_) ||
            isDialogueInProgress();
        if (hasPresentationOverlayPass &&
            presentationOverlayRenderer_.renderer != nullptr &&
            presentationOverlayRenderer_.surface != nullptr) {
            SDL_SetRenderDrawBlendMode(presentationOverlayRenderer_.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(presentationOverlayRenderer_.renderer, 0, 0, 0, 0);
            SDL_RenderClear(presentationOverlayRenderer_.renderer);

            if (activeUltimateTurnSplash_ != nullptr) {
                activeUltimateTurnSplash_->renderOverlay(
                    presentationOverlayRenderer_.renderer,
                    sceneWidth(),
                    sceneHeight());
            }
            if (activeOverlay_) {
                activeOverlay_(presentationOverlayRenderer_.renderer, sceneWidth(), sceneHeight());
            }
            if (isDialogueInProgress()) {
                vn::render();
            }

            const Uint64 compatibilityStartCounter = frameTimingEnabled_ ? SDL_GetPerformanceCounter() : 0;
            drawCompatibilitySurface(presentationOverlayRenderer_, presentationOverlayBlitter_, true);
            if (frameTimingEnabled_) {
                compatibilityMs += elapsedTimingMs(compatibilityStartCounter, SDL_GetPerformanceCounter());
            }
        }

        accumulateFrameTiming(worldMs, compatibilityMs, uiMs);
    }

    bool isFinished() const {
        return finished_;
    }

    bool exitedToMainMenu() const {
        return exitToMainMenuRequested_;
    }

    void setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) {
        loadingOverlayState_ = state;
        loadingOverlay_.apply(loadingOverlayState_);
    }

    BattleOutcome outcome() const {
        if (!initialized_) {
            return BattleOutcome::None;
        }

        switch (manager_.outcome()) {
            case battle::BattleResolvedOutcome::Victory:
                return BattleOutcome::Victory;
            case battle::BattleResolvedOutcome::Defeat:
                return BattleOutcome::Defeat;
            case battle::BattleResolvedOutcome::None:
            default:
                return BattleOutcome::None;
        }
    }

    const std::vector<std::string>& currentPartyLineup() const {
        return activePartyLineup_;
    }

private:
    bool isBattleFinishBlocked() const {
        if (manager_.getBossCurrentHp() > 0) {
            return false;
        }

        const auto bossDeadVoicePath = resolveBossDeadVoicePath(manager_.getBattleState());
        return bossDeadVoicePath.has_value() && gOneShotAudio.isPlaying(*bossDeadVoicePath);
    }

    bool isDialogueInProgress() const {
        return narrativeEnabled_ && narrativeInitialized_ && narrative_.isDialogueInProgress();
    }

    bool isBattleSpaceEnabled() const {
        if (!narrativeEnabled_) {
            return true;
        }
        if (!narrativeInitialized_) {
            return false;
        }
        return narrative_.isSpaceEnabledForBattle();
    }

    void syncNarrativeSettings() const {
        if (!narrativeEnabled_ || !narrativeInitialized_ || settings_ == nullptr) {
            return;
        }

        vn::setMusicVolume(settings_->musicVolume);
        vn::setVoiceVolume(settings_->voiceVolume);
        vn::setTypewriterSpeed(settings_->textSpeed);
    }

    float currentMusicMasterVolume() const {
        return settings_ != nullptr ? std::clamp(settings_->musicVolume, 0.0f, 1.0f) : 1.0f;
    }

    float currentVoiceVolume() const {
        return settings_ != nullptr ? settings_->voiceVolume : 1.0f;
    }

    void handlePresentationAudioCommands(
        const battle::PresentationContext& context,
        const std::vector<battle::PresentationAudioCommand>& commands) {
        (void)context;

        for (const battle::PresentationAudioCommand& command : commands) {
            switch (command.type) {
                case battle::PresentationAudioCommandType::PlayOneShot:
                    (void)playResolvedOneShot(
                        gPresentationSfxAudio,
                        command.id,
                        command.volume,
                        true
                    );
                    break;
                case battle::PresentationAudioCommandType::PlayOneShotAllowOverlap:
                    (void)playResolvedOneShot(
                        gPresentationSfxAudio,
                        command.id,
                        command.volume,
                        false
                    );
                    break;
                case battle::PresentationAudioCommandType::StartLoop:
                    presentationLoopBaseVolume_ = std::clamp(command.volume, 0.0f, 1.0f);
                    (void)playResolvedLoop(
                        gPresentationLoopAudio,
                        command.id,
                        presentationLoopBaseVolume_ * currentMusicMasterVolume()
                    );
                    break;
                case battle::PresentationAudioCommandType::StopLoop:
                    presentationLoopBaseVolume_ = 1.0f;
                    gPresentationLoopAudio.stop();
                    break;
                case battle::PresentationAudioCommandType::StopAllSfx:
                    presentationLoopBaseVolume_ = 1.0f;
                    stopPresentationAudioPlayback(false);
                    break;
                case battle::PresentationAudioCommandType::PauseBgm:
                    gBgmPlayer.pause();
                    break;
                case battle::PresentationAudioCommandType::ResumeBgm:
                    gBgmPlayer.resume();
                    break;
            }
        }
    }

    void handlePresentationAbilityAudio(const battle::PresentationContext& context, int cueCount) {
        if (cueCount <= 0) {
            return;
        }

        const float voiceVolume = currentVoiceVolume();
        const battle::BattleState& battleState = manager_.getBattleState();
        if (context.isBoss && context.presentationId == "qr_code_attack") {
            for (int i = 0; i < cueCount; ++i) {
                (void)playCombatVoiceClip(battleState.boss.key, "ability", voiceVolume);
            }
            manager_.markPresentationAbilityAudioPlayed();
            return;
        }

        if (context.isBoss) {
            const std::string audioSequenceId = battleState.boss.key + "|" + context.presentationId;
            if (presentationAudioSequenceId_ != audioSequenceId) {
                presentationAudioSequenceId_ = audioSequenceId;
                presentationAudioCueIndex_ = 0;
            }

            for (int i = 0; i < cueCount; ++i) {
                ++presentationAudioCueIndex_;
                if (playCombatVoiceClip(battleState.boss.key, "ability", voiceVolume)) {
                    continue;
                }
                (void)playCombatVoiceClip(battleState.boss.assets, "ability", voiceVolume);
            }

            manager_.markPresentationAbilityAudioPlayed();
            return;
        }

        const std::string casterVoiceKey = getPresentationCasterVoiceKey(context, manager_);
        const std::string audioSequenceId = casterVoiceKey + "|" + context.presentationId;
        if (presentationAudioSequenceId_ != audioSequenceId) {
            presentationAudioSequenceId_ = audioSequenceId;
            presentationAudioCueIndex_ = 0;
        }

        for (int i = 0; i < cueCount; ++i) {
            ++presentationAudioCueIndex_;

            std::string clipName = "ability";
            if (casterVoiceKey == "cupcakke" && context.presentationId == "drum_attack") {
                clipName = (presentationAudioCueIndex_ == 1) ? "ability" : "ability2";
            } else if (casterVoiceKey == "cupcakke" && context.presentationId == "niagara_falls_ultimate") {
                clipName = (presentationAudioCueIndex_ == 1) ? "ultimate" : "ability2";
            } else if (context.isUltimate) {
                clipName = "ultimate";
            }

            if (playCombatVoiceClip(casterVoiceKey, clipName, voiceVolume)) {
                continue;
            }

            if ((clipName == "ultimate" || clipName == "ability2") &&
                playCombatVoiceClip(casterVoiceKey, "ability", voiceVolume)) {
                continue;
            }
        }

        manager_.markPresentationAbilityAudioPlayed();
    }

    void handlePresentationHealAudio(const battle::PresentationContext& context, int hitEvents) {
        if (context.isBoss || hitEvents <= 0) {
            return;
        }

        const float voiceVolume = currentVoiceVolume();
        const battle::BattleState& battleState = manager_.getBattleState();
        for (size_t i = 0; i < battleState.party.size(); ++i) {
            if (manager_.getCharacterCurrentHp(static_cast<int>(i)) <= 0) {
                continue;
            }
            (void)playCombatVoiceClip(battleState.party[i].assets, "healed", voiceVolume);
        }
    }

    void handlePresentationHitAudio(const battle::PresentationContext& context,
                                    int hitEvents,
                                    int targetPartyIndex,
                                    int bossHpBefore,
                                    const std::vector<int>& partyHpBefore) {
        if (hitEvents <= 0) {
            return;
        }

        const float voiceVolume = currentVoiceVolume();
        const battle::BattleState& battleState = manager_.getBattleState();
        if (context.isBoss) {
            auto handlePartyHitAudio = [&](int partyIndex, int repeatCount) {
                if (partyIndex < 0 || partyIndex >= static_cast<int>(battleState.party.size()) ||
                    static_cast<size_t>(partyIndex) >= partyHpBefore.size()) {
                    return;
                }

                const int nowHp = manager_.getCharacterCurrentHp(partyIndex);
                const int prevHp = partyHpBefore[static_cast<size_t>(partyIndex)];
                const std::string& assetKey = battleState.party[static_cast<size_t>(partyIndex)].assets;
                const bool diedNow = prevHp > 0 && nowHp <= 0;
                const auto deadVoice = platform::path::resolveCombatVoicePath(assetKey, "dead");
                if (diedNow && deadVoice.has_value()) {
                    if (const auto hitVoice = platform::path::resolveCombatVoicePath(assetKey, "hit"); hitVoice.has_value()) {
                        gOneShotAudio.stopPlayback(*hitVoice);
                    }
                    (void)gOneShotAudio.playWavOneShot(*deadVoice, voiceVolume);
                    return;
                }

                if (nowHp <= 0 && !diedNow) {
                    return;
                }

                if (const auto hitVoice = platform::path::resolveCombatVoicePath(assetKey, "hit"); hitVoice.has_value()) {
                    for (int hit = 0; hit < repeatCount; ++hit) {
                        (void)gOneShotAudio.playWavOneShot(*hitVoice, voiceVolume);
                    }
                }
            };

            if (targetPartyIndex >= 0 && targetPartyIndex < static_cast<int>(battleState.party.size())) {
                markUnitHit(hudFeedback_, targetPartyIndex, SDL_GetTicks64());
                handlePartyHitAudio(targetPartyIndex, hitEvents);
            } else {
                for (size_t i = 0; i < battleState.party.size(); ++i) {
                    markUnitHit(hudFeedback_, static_cast<int>(i), SDL_GetTicks64());
                    handlePartyHitAudio(static_cast<int>(i), hitEvents);
                }
            }

            manager_.markPresentationHitAudioPlayed();
            return;
        }

        const std::string casterVoiceKey = getPresentationCasterVoiceKey(context, manager_);
        if (context.presentationId == "drum_attack" && casterVoiceKey == "cupcakke") {
            (void)playCombatVoiceClip(casterVoiceKey, "ability2", voiceVolume, hitEvents);
            manager_.markPresentationHitAudioPlayed();
            return;
        }

        const int nowBossHp = manager_.getBossCurrentHp();
        if (manager_.playerDamageHealsBoss() && nowBossHp > bossHpBefore) {
            (void)playBossHealedVoice(battleState, voiceVolume, hitEvents);
            manager_.markPresentationHitAudioPlayed();
            return;
        }
        if (nowBossHp < bossHpBefore) {
            markBossHit(hudFeedback_, SDL_GetTicks64());
        }
        const bool bossDiedNow = bossHpBefore > 0 && nowBossHp <= 0;
        const auto bossDeadVoice = resolveBossDeadVoicePath(battleState);
        if (bossDiedNow && bossDeadVoice.has_value()) {
            if (const auto bossHitVoice = resolveBossHitVoicePath(battleState); bossHitVoice.has_value()) {
                gOneShotAudio.stopPlayback(*bossHitVoice);
            }
            (void)gOneShotAudio.playWavOneShot(*bossDeadVoice, voiceVolume);
            manager_.markPresentationHitAudioPlayed();
            return;
        }

        (void)playBossHitVoice(battleState, voiceVolume, hitEvents);
        manager_.markPresentationHitAudioPlayed();
    }

    std::string resolvePresentationRewardText(const battle::PresentationContext& context,
                                              const battle::AbilityDefinition* abilityDef,
                                              const battle::PresentationFeedbackEvent& feedback) const {
        if (!feedback.rewardText.empty()) {
            return feedback.rewardText;
        }
        if (abilityDef == nullptr) {
            return std::string();
        }

        auto formatSignedPercent = [](int percent, const std::string& label) {
            const std::string prefix = percent >= 0 ? "+" : "";
            return prefix + std::to_string(percent) + "% " + label;
        };

        const float clampedMultiplier = std::max(0.0f, feedback.multiplier);
        switch (abilityDef->type) {
            case battle::AbilityType::Attack:
            case battle::AbilityType::Debuff: {
                if (context.isBoss) {
                    const int reductionPercent = static_cast<int>(std::lround(
                        (1.0f - std::clamp(feedback.multiplier, 0.0f, 1.0f)) * 100.0f
                    ));
                    return "-" + std::to_string(std::max(0, reductionPercent)) + "% DMG TAKEN";
                }
                const int damagePercent = static_cast<int>(std::lround((clampedMultiplier - 1.0f) * 100.0f));
                return formatSignedPercent(damagePercent, "DMG");
            }
            case battle::AbilityType::Heal: {
                const int healPercent = static_cast<int>(std::lround((clampedMultiplier - 1.0f) * 100.0f));
                return formatSignedPercent(healPercent, "HEAL");
            }
            case battle::AbilityType::Shield: {
                const int shieldPercent = static_cast<int>(std::lround(clampedMultiplier * 100.0f));
                return formatSignedPercent(shieldPercent, "SHIELD");
            }
            case battle::AbilityType::Buff: {
                if (abilityDef->atkBuff != 0) {
                    const int atkPercent = static_cast<int>(std::lround(
                        static_cast<float>(abilityDef->atkBuff) * feedback.multiplier
                    ));
                    return formatSignedPercent(atkPercent, "ATK");
                }
                if (abilityDef->speedBuff != 0) {
                    const int spdPercent = static_cast<int>(std::lround(
                        static_cast<float>(abilityDef->speedBuff) * feedback.multiplier
                    ));
                    return formatSignedPercent(spdPercent, "SPD");
                }
                return std::string();
            }
        }

        return std::string();
    }

    void applyPresentationFeedbackEvent(const battle::PresentationContext& context,
                                        const battle::AbilityDefinition* abilityDef,
                                        const battle::PresentationFeedbackEvent& feedback) {
        if (!feedback.valid()) {
            return;
        }

        const battle::CombatJudgement judgement = battle::classifyCombatJudgement(feedback.signal);
        const std::string rewardText = resolvePresentationRewardText(context, abilityDef, feedback);
        manager_.applyPresentationFeedback(context.isBoss, feedback);
        showJudgement(hudFeedback_, judgement, rewardText, SDL_GetTicks64());
        playJudgementSfx(judgement);
    }

    float runPresentationInteraction(const battle::PresentationContext& context) {
        if (!initialized_ || finished_) {
            return 1.0f;
        }
        if (context.presentationId.empty()) {
            return 1.0f;
        }

        const battle::AbilityDefinition* abilityDef = manager_.findAbilityDefinition(context.abilityId);
        const Uint64 hintStartedMs = SDL_GetTicks64();
        if (abilityDef != nullptr && !abilityDef->instructionHint.empty()) {
            showHint(hudFeedback_, abilityDef->instructionHint, hintStartedMs);
        } else {
            clearHint(hudFeedback_);
        }
        syncHudDocument(hintStartedMs);

        updateSceneEntities(0.0f, context.isBoss, context.isBoss ? -1 : context.casterIndex);
        cameraStaging_.snapToGoalCamera(camera_);

        auto resolveTextureForEntity = [&](auto predicate,
                                          const auto& resolveTextureFn) -> SDL_Texture* {
            for (const SceneEntity& entity : entities_) {
                if (!predicate(entity)) {
                    continue;
                }
                if (SDL_Texture* texture = resolveTextureFn(entity.assetName)) {
                    return texture;
                }
            }
            return nullptr;
        };

        SDL_Texture* casterSpriteTexture = resolveTextureForEntity([&context](const SceneEntity& entity) {
            return context.isBoss ? entity.isBoss
                                  : (!entity.isBoss && entity.partyIndex == context.casterIndex);
        }, [this](const std::string& assetName) {
            return resolveSceneTextureByAsset(assetName);
        });
        SDL_Texture* targetSpriteTexture = resolveTextureForEntity([&context](const SceneEntity& entity) {
            if (context.isBoss) {
                if (context.targetIndex >= 0) {
                    return !entity.isBoss && entity.partyIndex == context.targetIndex;
                }
                return !entity.isBoss;
            }
            return entity.isBoss;
        }, [this](const std::string& assetName) {
            return resolveSceneTextureByAsset(assetName);
        });
        SDL_Texture* overlayCasterSpriteTexture = resolveTextureForEntity([&context](const SceneEntity& entity) {
            return context.isBoss ? entity.isBoss
                                  : (!entity.isBoss && entity.partyIndex == context.casterIndex);
        }, [this](const std::string& assetName) {
            return resolveOverlayTextureByAsset(assetName);
        });
        SDL_Texture* overlayTargetSpriteTexture = resolveTextureForEntity([&context](const SceneEntity& entity) {
            if (context.isBoss) {
                if (context.targetIndex >= 0) {
                    return !entity.isBoss && entity.partyIndex == context.targetIndex;
                }
                return !entity.isBoss;
            }
            return entity.isBoss;
        }, [this](const std::string& assetName) {
            return resolveOverlayTextureByAsset(assetName);
        });

        battle::presentation_runtime::PlaybackStateRefs stateRefs;
        stateRefs.playbackActive = &presentationPlaybackActive_;
        stateRefs.casterIsBoss = &presentationCasterIsBoss_;
        stateRefs.casterPartyIndex = &presentationCasterPartyIndex_;
        stateRefs.activePresentation = &activePresentation_;

        battle::presentation_runtime::PlaybackCallbacks callbacks;
        callbacks.splashSpriteTexture = overlayCasterSpriteTexture != nullptr
            ? overlayCasterSpriteTexture
            : casterSpriteTexture;
        callbacks.casterSpriteTexture = casterSpriteTexture;
        callbacks.targetSpriteTexture = targetSpriteTexture;
        callbacks.overlayCasterSpriteTexture = overlayCasterSpriteTexture;
        callbacks.overlayTargetSpriteTexture = overlayTargetSpriteTexture;
        callbacks.onUnhandledKeyDown = [this](SDL_Keycode key) {
            (void)handleManualUltimateHotkey(key, SDL_GetTicks64());
        };
        callbacks.onWindowResized = [this](int width, int height) {
            if (windowHost_ != nullptr && window_ != nullptr) {
                SDL_Event resizeEvent{};
                resizeEvent.type = SDL_WINDOWEVENT;
                resizeEvent.window.windowID = SDL_GetWindowID(window_);
                resizeEvent.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
                resizeEvent.window.data1 = width;
                resizeEvent.window.data2 = height;
                windowHost_->handleEvent(resizeEvent);
            }
            windowWidth_ = width;
            windowHeight_ = height;
            if (window_ != nullptr) {
                SDL_GL_GetDrawableSize(window_, &drawableWidth_, &drawableHeight_);
            }
            applyReferenceSceneMetrics();
            if (!refreshSceneRenderers()) {
                finished_ = true;
            }
        };
        callbacks.onAudioCommands = [this, &context](const std::vector<battle::PresentationAudioCommand>& commands) {
            handlePresentationAudioCommands(context, commands);
        };
        callbacks.onAbilityAudioCues = [this, &context](int cueCount) {
            handlePresentationAbilityAudio(context, cueCount);
        };
        callbacks.onHitEvents = [this, &context, abilityDef](int hitEvents, int damageLabelHitCount) {
            if (hitEvents <= 0) {
                return;
            }

            if (abilityDef == nullptr) {
                feedback_.queuePresentationHitShakes(context.isBoss, hitEvents, manager_);
                return;
            }

            if (abilityDef->type == battle::AbilityType::Heal) {
                const float healMultiplier = activePresentation_ != nullptr
                    ? std::max(0.0f, activePresentation_->getInputMultiplier())
                    : 1.0f;
                const int totalHeal = std::max(0, static_cast<int>(std::lround(
                    static_cast<float>(abilityDef->flatHeal) * healMultiplier
                )));
                const int perHitHeal = totalHeal / std::max(1, hitEvents);
                manager_.applyPresentationHealing(
                    context.isBoss,
                    perHitHeal,
                    hitEvents,
                    abilityDef->reviveDeadAllies
                );
                feedback_.queuePresentationHealFeedback(context.isBoss, hitEvents, perHitHeal, manager_);
                handlePresentationHealAudio(context, hitEvents);
                return;
            }

            int baseAtk = 0;
            if (context.isBoss) {
                baseAtk = manager_.getBattleState().boss.atk;
            } else {
                const battle::BattleState& state = manager_.getBattleState();
                if (context.casterIndex >= 0 &&
                    static_cast<size_t>(context.casterIndex) < state.party.size()) {
                    baseAtk = state.party[static_cast<size_t>(context.casterIndex)].atk;
                }
            }

            const int bossHpBefore = manager_.getBossCurrentHp();
            std::vector<int> partyHpBefore;
            const battle::BattleState& state = manager_.getBattleState();
            partyHpBefore.reserve(state.party.size());
            for (size_t i = 0; i < state.party.size(); ++i) {
                partyHpBefore.push_back(manager_.getCharacterCurrentHp(static_cast<int>(i)));
            }

            const float hitDamageMultiplier = activePresentation_ != nullptr
                ? std::max(0.0f, activePresentation_->consumeHitDamageMultiplier())
                : 1.0f;
            const float totalDamageRaw = baseAtk * abilityDef->multiplier * hitDamageMultiplier;
            const int totalDamage = std::max(1, static_cast<int>(totalDamageRaw));
            const int perHitDamage = std::max(1, totalDamage / std::max(1, damageLabelHitCount));
            const int presentationTargetPartyIndex = context.isBoss ? context.targetIndex : -1;

            manager_.applyPresentationHitDamage(
                context.isBoss,
                perHitDamage,
                hitEvents,
                presentationTargetPartyIndex
            );
            handlePresentationHitAudio(
                context,
                hitEvents,
                presentationTargetPartyIndex,
                bossHpBefore,
                partyHpBefore
            );
            feedback_.queuePresentationHitFeedback(
                context.isBoss,
                hitEvents,
                perHitDamage,
                manager_,
                presentationTargetPartyIndex
            );
        };
        bool consumedPresentationFeedback = false;
        callbacks.onPostUpdate = [this, &context, abilityDef, &consumedPresentationFeedback](float deltaSeconds) {
            const bool useCenteredPartyLayout =
                activePresentation_ != nullptr &&
                activePresentation_->shouldUseCenteredPartyLayout();
            const bool bossActingLayout = context.isBoss || useCenteredPartyLayout;
            const int actingPartyIndex = bossActingLayout ? -1 : context.casterIndex;
            const Uint64 nowMs = SDL_GetTicks64();

            if (activePresentation_ != nullptr) {
                const std::vector<battle::PresentationFeedbackEvent> feedbackEvents =
                    activePresentation_->consumeFeedbackEvents();
                for (const battle::PresentationFeedbackEvent& feedbackEvent : feedbackEvents) {
                    applyPresentationFeedbackEvent(context, abilityDef, feedbackEvent);
                    consumedPresentationFeedback = true;
                }
            }

            updateSceneEntities(deltaSeconds, bossActingLayout, actingPartyIndex);
            feedback_.syncFromManager(manager_, presentationPlaybackActive_);
            feedback_.update(deltaSeconds);
            tickHudFeedback(hudFeedback_, nowMs);
            updateHudAnimationState(hudAnimationState_, manager_, hudFeedback_, deltaSeconds, nowMs);
            syncHudDocument(nowMs);
        };
        callbacks.onBossPresentationFrame = [this]() {
            computeCharacterPositions(true, -1);

            if (activePresentation_ == nullptr) {
                return;
            }

            const int focusedPartyIndex = activePresentation_->getFocusedPartyIndex();
            if (focusedPartyIndex < 0) {
                return;
            }

            for (const SceneEntity& entity : entities_) {
                if (entity.isBoss || entity.partyIndex != focusedPartyIndex) {
                    continue;
                }

                activePresentation_->setTargetWorldPosition(entity.worldX, entity.worldY, entity.worldZ);
                break;
            }
        };
        callbacks.setRenderOverlay = [this](std::function<void(SDL_Renderer*, int, int)> fn) {
            activeOverlay_ = std::move(fn);
        };
        callbacks.clearRenderOverlay = [this]() {
            activeOverlay_ = nullptr;
        };
        callbacks.renderAndPresentFrame = [this]() {
            render();
            if (windowHost_ != nullptr) {
                windowHost_->present();
            }
        };

        const battle::presentation_runtime::PlaybackResult result =
            battle::presentation_runtime::runAbilityPresentation(
                sceneRenderer_.renderer,
                finished_,
                camera_,
                entities_,
                context,
                stateRefs,
                callbacks
            );

        activeOverlay_ = nullptr;

        if (!context.isBoss &&
            context.presentationId == "rang_wo_men_shuo_zhong_wen" &&
            result.correctToneCount > 0) {
            manager_.addLuotianyiCorrectTones(result.correctToneCount);
        }

        presentationAudioSequenceId_.clear();
        presentationAudioCueIndex_ = 0;
        if (context.presentationId == "boss_attack_lyoo_plot_twist") {
            stopPresentationAudioPlayback(true);
        }
        if (!consumedPresentationFeedback && result.feedbackSignal.valid()) {
            battle::PresentationFeedbackEvent finalFeedback;
            finalFeedback.signal = result.feedbackSignal;
            finalFeedback.multiplier = result.multiplier;
            applyPresentationFeedbackEvent(context, abilityDef, finalFeedback);
        }
        if (context.isBoss || context.interactionType != battle::InteractionType::None) {
            clearHint(hudFeedback_);
        }
        syncHudDocument(SDL_GetTicks64());

        discardNextUpdateDelta_ = true;
        return result.multiplier;
    }

    bool loadFonts() {
        bool anyLoaded = false;
        anyLoaded |= loadRmlFontIfPresent(platform::path::resolvePath("assets/fonts/SpaceMono-Regular.ttf"));
        anyLoaded |= loadRmlFontIfPresent(platform::path::resolvePath("assets/fonts/SpaceMono-Bold.ttf"));
        anyLoaded |= loadRmlFontIfPresent(platform::path::findFontPath(), true);
        anyLoaded |= loadRmlFontIfPresent(platform::path::resolvePath("assets/fonts/NotoSansCJK-Regular.ttc"), true);
        return anyLoaded;
    }

    int sceneWidth() const {
        return kWindowWidth;
    }

    int sceneHeight() const {
        return kWindowHeight;
    }

    void syncBattleHudStageTransform() {
        if (document_ == nullptr) {
            return;
        }

        Rml::Element* stage = document_->GetElementById("safe-stage");
        Rml::Element* bossBand = document_->GetElementById("boss-band");
        Rml::Element* bossMeterShell = document_->GetElementById("boss-meter-shell");
        Rml::Element* bossHitFlash = document_->GetElementById("boss-hit-flash");
        Rml::Element* turnRail = document_->GetElementById("turn-rail");
        Rml::Element* partyRack = document_->GetElementById("party-rack");
        if (stage == nullptr) {
            return;
        }

        const float scaleX = windowWidth_ > 0
            ? static_cast<float>(windowWidth_) / static_cast<float>(sceneWidth())
            : 1.0f;
        const float scaleY = windowHeight_ > 0
            ? static_cast<float>(windowHeight_) / static_cast<float>(sceneHeight())
            : 1.0f;
        const float scale = std::max(0.01f, std::min(scaleX, scaleY));
        const float scaledWidth = static_cast<float>(sceneWidth()) * scale;
        const float scaledHeight = static_cast<float>(sceneHeight()) * scale;
        const float offsetX = (static_cast<float>(windowWidth_) - scaledWidth) * 0.5f;
        const float offsetY = (static_cast<float>(windowHeight_) - scaledHeight) * 0.5f;
        const float hudScale = std::max(0.01f, scale);

        auto formatPx = [](float value) {
            return std::to_string(static_cast<int>(std::lround(value))) + "px";
        };

        auto formatScale = [](float value) {
            std::ostringstream stream;
            stream.setf(std::ios::fixed);
            stream.precision(6);
            stream << "scale(" << value << ")";
            return stream.str();
        };

        const std::string safeStageTransform = formatScale(scale);
        const std::string hudScaleTransform = formatScale(hudScale);

        stage->SetProperty("position", "absolute");
        stage->SetProperty("left", formatPx(offsetX));
        stage->SetProperty("top", formatPx(offsetY));
        stage->SetProperty("width", std::to_string(sceneWidth()) + "px");
        stage->SetProperty("height", std::to_string(sceneHeight()) + "px");
        stage->SetProperty("transform-origin", "0px 0px");
        stage->SetProperty("transform", safeStageTransform);

        auto setScaledTransform = [](Rml::Element* element,
                                     const char* origin,
                                     const std::string& transformValue) {
            if (element == nullptr) {
                return;
            }
            element->SetProperty("transform-origin", origin);
            element->SetProperty("transform", transformValue);
        };

        if (turnRail != nullptr) {
            turnRail->SetProperty("left", formatPx(12.0f * hudScale));
            turnRail->SetProperty("top", formatPx(14.0f * hudScale));
            setScaledTransform(turnRail, "0px 0px", hudScaleTransform);
        }

        if (partyRack != nullptr) {
            partyRack->SetProperty("left", formatPx(16.0f * hudScale));
            partyRack->SetProperty("bottom", formatPx(12.0f * hudScale));
            const std::size_t partyCount = manager_.getBattleState().party.size();
            const float rackWidth = 186.0f +
                std::max(0.0f, static_cast<float>(partyCount > 0 ? partyCount - 1 : 0)) * 198.0f;
            partyRack->SetProperty("width", formatPx(rackWidth));
            setScaledTransform(partyRack, "0px 100%", hudScaleTransform);
        }

        if (bossBand != nullptr) {
            const float bossLeft = 110.0f * hudScale;
            const float bossRight = 24.0f * hudScale;
            const float physicalWidth =
                std::max(0.0f, static_cast<float>(windowWidth_) - bossLeft - bossRight);
            const float unscaledWidth = physicalWidth / hudScale;

            bossBand->SetProperty("left", formatPx(bossLeft));
            bossBand->SetProperty("top", formatPx(8.0f * hudScale));
            bossBand->SetProperty("right", "auto");
            bossBand->SetProperty("width", formatPx(unscaledWidth));
            bossBand->SetProperty("height", "80px");
            setScaledTransform(bossBand, "0px 0px", hudScaleTransform);
        }

        if (bossMeterShell != nullptr) {
            bossMeterShell->RemoveProperty("transform-origin");
            bossMeterShell->SetProperty("transform", "skewX(-18deg)");
        }

        if (bossHitFlash != nullptr) {
            bossHitFlash->RemoveProperty("transform-origin");
            bossHitFlash->RemoveProperty("transform");
        }
    }

    void applyReferenceSceneMetrics() {
        if (renderInterface_ != nullptr) {
            renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
        }
        if (context_ != nullptr) {
            context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
        }
        syncBattleHudStageTransform();
        camera_.screenCenterX = sceneWidth() / 2;
        camera_.screenCenterY = sceneHeight() / 2;
    }

    SDL_Event makeScaledRmlInputEvent(const SDL_Event& event) const {
        return event;
    }

    void updateViewportFromWindow() {
        if (windowHost_ == nullptr) {
            return;
        }
        windowWidth_ = windowHost_->getWidth();
        windowHeight_ = windowHost_->getHeight();
        drawableWidth_ = windowHost_->getDrawableWidth();
        drawableHeight_ = windowHost_->getDrawableHeight();
    }

    void syncHudDocument(Uint64 nowMs) {
        syncHudFeedbackState(hudFeedback_, manager_);
        battle::app::ui::updateBattleHudDocument(document_,
                                                 manager_,
                                                 hudAnimationState_,
                                                 hudFeedback_,
                                                 tutorialOverlay_,
                                                 rhythmChallenge_,
                                                 false,
                                                 pauseOverlayMode_,
                                                 pauseSelection_,
                                                 settingsSelection_,
                                                 settings_,
                                                 settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond,
                                                 nowMs,
                                                 makeBattleHudDocumentDependencies());
    }

    battle::BattleSessionCore::BattleFrameSnapshot buildFrameSnapshot(int focusedIndex) const {
        battle::BattleSessionCore::BattleFrameSnapshot snapshot;
        snapshot.camera = camera_;
        snapshot.stage = &stageDefinition_;
        snapshot.entities = entities_;
        snapshot.focusedEntityIndex = focusedIndex;
        snapshot.frameAccumulator = frameAccumulator_;
        snapshot.blackoutWorld =
            activePresentation_ != nullptr && activePresentation_->shouldBlackoutWorld();
        snapshot.renderFloor =
            activePresentation_ == nullptr || activePresentation_->shouldRenderFloor();
        snapshot.presentationPlaybackActive = presentationPlaybackActive_;
        snapshot.presentationCasterIsBoss = presentationCasterIsBoss_;
        snapshot.presentationCasterPartyIndex = presentationCasterPartyIndex_;
        snapshot.activePresentation = activePresentation_;
        snapshot.activeUltimateTurnSplash = activeUltimateTurnSplash_.get();
        snapshot.feedbackAnchors.reserve(entities_.size());
        snapshot.shakeOffsetsX.reserve(entities_.size());
        for (const SceneEntity& entity : entities_) {
            snapshot.feedbackAnchors.push_back(battle::render::FeedbackEntityAnchor{
                entity.isBoss,
                entity.partyIndex,
                entity.worldX,
                entity.worldY,
                entity.worldZ,
                entity.visible
            });
            snapshot.shakeOffsetsX.push_back(feedback_.getShakeOffsetX(entity.isBoss, entity.partyIndex));
        }
        return snapshot;
    }

    void drawCompatibilitySurface(SoftwareSceneRenderer& renderer,
                                  GlScreenBlitter& blitter,
                                  bool enableBlend) {
        if (renderer.renderer == nullptr || renderer.surface == nullptr) {
            return;
        }
        blitter.uploadSurface(renderer.surface);
        blitter.draw(enableBlend);
    }

    double elapsedTimingMs(Uint64 startCounter, Uint64 endCounter) const {
        if (!frameTimingEnabled_ || frameTimingFrequency_ == 0 || endCounter < startCounter) {
            return 0.0;
        }
        return (static_cast<double>(endCounter - startCounter) * 1000.0) /
               static_cast<double>(frameTimingFrequency_);
    }

    void accumulateFrameTiming(double worldMs, double compatibilityMs, double uiMs) {
        if (!frameTimingEnabled_) {
            return;
        }

        timingWorldMs_ += worldMs;
        timingCompatibilityMs_ += compatibilityMs;
        timingUiMs_ += uiMs;
        ++timingFrameCount_;
        if (timingFrameCount_ < 120) {
            return;
        }

        const double frames = static_cast<double>(timingFrameCount_);
        std::cout << "[BattleTiming] avg world=" << (timingWorldMs_ / frames)
                  << "ms compat=" << (timingCompatibilityMs_ / frames)
                  << "ms ui=" << (timingUiMs_ / frames)
                  << "ms\n";
        timingWorldMs_ = 0.0;
        timingCompatibilityMs_ = 0.0;
        timingUiMs_ = 0.0;
        timingFrameCount_ = 0;
    }

    SDL_Texture* resolveSceneTextureByAsset(const std::string& assetName) const {
        if (const auto it = sceneRenderer_.textureByAsset.find(assetName); it != sceneRenderer_.textureByAsset.end()) {
            return it->second;
        }
        return nullptr;
    }

    SDL_Texture* resolveOverlayTextureByAsset(const std::string& assetName) const {
        if (const auto it = presentationOverlayRenderer_.textureByAsset.find(assetName);
            it != presentationOverlayRenderer_.textureByAsset.end()) {
            return it->second;
        }
        return nullptr;
    }

    void refreshCombatBeginAnimationTextures() {
        if (!combatBeginAnimation_.isActive()) {
            return;
        }

        const battle::BattleState& state = manager_.getBattleState();
        combatBeginAnimation_.reset(
            resolveSceneTextureByAsset("miku"),
            resolveSceneTextureByAsset(state.boss.assets)
        );
    }

    bool refreshSceneRenderers() {
        if (!sceneRenderer_.initialize(sceneWidth(), sceneHeight(), worldAssets_, resolveBattleSpritePath)) {
            return false;
        }
        if (!presentationOverlayRenderer_.initialize(sceneWidth(), sceneHeight(), worldAssets_, resolveBattleSpritePath)) {
            return false;
        }
        (void)glSceneRenderer_.ensureWorldAssets(worldAssets_);
        if (narrativeEnabled_) {
            if (!vn::initialize(presentationOverlayRenderer_.renderer, sceneWidth(), sceneHeight())) {
                return false;
            }
            vn::setViewportSize(sceneWidth(), sceneHeight());
            syncNarrativeSettings();
        }

        refreshCombatBeginAnimationTextures();

        const battle::flow::PreviewActorContext preview = battle::flow::inspectPreviewActor(manager_);
        const bool bossActing = !(preview.valid && preview.type == battle::ParticipantType::Character);
        const int actingPartyIndex =
            (!bossActing && preview.partyIndex >= 0) ? preview.partyIndex : -1;
        updateSceneEntities(0.0f, bossActing, actingPartyIndex);
        return true;
    }

    void initializeWorldEntities() {
        const battle::BattleState& state = manager_.getBattleState();

        entities_.clear();
        entities_.reserve(state.party.size() + 1);
        for (size_t partyIndex = 0; partyIndex < state.party.size(); ++partyIndex) {
            const battle::CharacterDefinition& character = state.party[partyIndex];
            entities_.push_back(SceneEntity{
                character.key,
                character.assets,
                false,
                battle::render::kDuelCharacterSlotX,
                battle::render::kDuelCharacterBaseY,
                0.0f,
                battle::render::colorFromKey(character.key, false),
                static_cast<int>(partyIndex),
                true,
                true,
                1.0f,
                0.0f
            });
        }

        entities_.push_back(SceneEntity{
            state.boss.key,
            state.boss.assets,
            true,
            battle::render::kDuelBossSlotX,
            battle::render::kDuelCharacterBaseY + battle::render::kBossCharacterDistanceWorld,
            0.0f,
            battle::render::colorFromKey(state.boss.key, true),
            -1,
            manager_.getBossCurrentHp() > 0,
            manager_.getBossCurrentHp() > 0,
            manager_.getBossCurrentHp() > 0 ? 1.0f : 0.0f,
            0.0f
        });
    }

    void computeCharacterPositions(bool bossActing, int actingPartyIndex) {
        const battle::BattleState& state = manager_.getBattleState();
        std::vector<bool> livingPartyMembers(state.party.size(), false);
        for (size_t i = 0; i < state.party.size(); ++i) {
            livingPartyMembers[i] = manager_.isCharacterAlive(static_cast<int>(i));
        }

        battle::render::computeDefaultPartyCharacterPositions(
            entities_,
            livingPartyMembers,
            bossActing,
            actingPartyIndex
        );

        const bool bossAlive = manager_.getBossCurrentHp() > 0;
        for (SceneEntity& entity : entities_) {
            if (!entity.isBoss) {
                continue;
            }
            entity.lineupVisible = bossAlive;
            break;
        }
    }

    void updateCharacterVisibilityTransitions(float deltaSeconds) {
        const float step = kCharacterVisibilityAnimSeconds <= 0.0f
            ? 1.0f
            : deltaSeconds / kCharacterVisibilityAnimSeconds;

        for (SceneEntity& entity : entities_) {
            if (entity.lineupVisible) {
                entity.spriteAlpha = std::min(1.0f, entity.spriteAlpha + step);
                const float eased = battle::easing::easeOutCubic(battle::easing::clamp01(entity.spriteAlpha));
                entity.spriteOffsetYPx = -kCharacterVisibilityOffsetPx * (1.0f - eased);
                entity.visible = true;
                continue;
            }

            entity.spriteAlpha = std::max(0.0f, entity.spriteAlpha - step);
            const float fadeT = battle::easing::clamp01(1.0f - entity.spriteAlpha);
            entity.spriteOffsetYPx = -kCharacterVisibilityOffsetPx * battle::easing::easeOutCubic(fadeT);
            entity.visible = entity.spriteAlpha > 0.001f;
            if (!entity.visible) {
                entity.spriteOffsetYPx = 0.0f;
            }
        }
    }

    void updateSceneEntities(float deltaSeconds, bool bossActing, int actingPartyIndex) {
        computeCharacterPositions(bossActing, actingPartyIndex);
        updateCharacterVisibilityTransitions(deltaSeconds);
    }

    void maybeStartUltimateTurnSplash(const battle::flow::PreviewActorContext& preview, bool dialogueActive) {
        if (dialogueActive || presentationPlaybackActive_ || activeUltimateTurnSplash_) {
            return;
        }

        const bool isCharacterUltimatePreview =
            preview.valid &&
            preview.type == battle::ParticipantType::Character &&
            preview.isExtraTurn &&
            preview.extraTurnAction == battle::BattleAction::Ultimate &&
            !preview.autoExecute &&
            preview.partyIndex >= 0;

        if (!isCharacterUltimatePreview) {
            previewUltimateSplashPartyIndex_ = -1;
            return;
        }

        if (preview.partyIndex == previewUltimateSplashPartyIndex_) {
            return;
        }

        const battle::BattleState& battleState = manager_.getBattleState();
        if (static_cast<size_t>(preview.partyIndex) >= battleState.party.size()) {
            return;
        }

        const battle::CharacterDefinition& character = battleState.party[static_cast<size_t>(preview.partyIndex)];
        const battle::AbilityDefinition* abilityDef = manager_.findAbilityDefinition(character.ultimate);

        battle::SplashArtConfig cfg;
        cfg.abilityName = abilityDef != nullptr ? abilityDef->name : character.ultimate;
        cfg.sprite = resolveOverlayTextureByAsset(character.assets);
        if (cfg.sprite == nullptr) {
            cfg.sprite = resolveSceneTextureByAsset(character.assets);
        }

        activeUltimateTurnSplash_ = std::make_unique<battle::SplashArtAnimation>(cfg);
        activeUltimateTurnSplash_->start();
        previewUltimateSplashPartyIndex_ = preview.partyIndex;
    }

    bool isBossDeathFadeComplete() const {
        if (manager_.getBossCurrentHp() > 0) {
            return true;
        }

        for (const SceneEntity& entity : entities_) {
            if (!entity.isBoss) {
                continue;
            }
            return !entity.visible || entity.spriteAlpha <= 0.001f;
        }

        return true;
    }

    void attachListener(const std::string& id, Rml::EventId eventId, std::function<void(Rml::Event&)> callback) {
        if (document_ == nullptr) {
            return;
        }
        if (Rml::Element* element = document_->GetElementById(id)) {
            auto listener = std::make_unique<CallbackEventListener>(std::move(callback));
            element->AddEventListener(eventId, listener.get());
            uiListeners_.push_back(std::move(listener));
        }
    }

    PauseAction toFrontUiPauseAction(PauseSelection selection) const {
        switch (selection) {
            case PauseSelection::Continue:
                return PauseAction::Continue;
            case PauseSelection::Settings:
                return PauseAction::Settings;
            case PauseSelection::ExitToMainMenu:
                return PauseAction::ExitToMainMenu;
        }
        return PauseAction::Continue;
    }

    PauseSelection fromFrontUiPauseAction(PauseAction action) const {
        switch (action) {
            case PauseAction::Continue:
                return PauseSelection::Continue;
            case PauseAction::Settings:
                return PauseSelection::Settings;
            case PauseAction::ExitToMainMenu:
                return PauseSelection::ExitToMainMenu;
            case PauseAction::Save:
            case PauseAction::Load:
                break;
        }
        return PauseSelection::Continue;
    }

    void syncPauseMenuUiState() {
        pauseMenuUiState_.pauseContext = PauseContext::Battle;
        pauseMenuUiState_.settingsReturnScreen = ScreenState::PauseMenu;
        if (settings_ != nullptr) {
            pauseMenuUiState_.settings = *settings_;
        }
    }

    void syncSettingsMenuUiState() {
        settingsMenuUiState_.screen = ScreenState::Settings;
        settingsMenuUiState_.settingsReturnScreen = ScreenState::PauseMenu;
        settingsMenuUiState_.pauseContext = PauseContext::Battle;
        if (settings_ != nullptr) {
            settingsMenuUiState_.settings = *settings_;
        }
    }

    void playControllerSoundRequests(DocumentController* controller) {
        if (controller == nullptr) {
            return;
        }

        for (const SoundRequest& request : controller->consumeSoundRequests()) {
            (void)playResolvedOneShot(
                gPresentationSfxAudio,
                request.relativePath,
                std::clamp(request.volume, 0.0f, 1.0f),
                false
            );
        }
    }

    void applyLiveSettingsFromUiState(const AppState& state) {
        if (settings_ == nullptr) {
            return;
        }

        *settings_ = state.settings;
        const float musicMasterVolume = currentMusicMasterVolume();
        gPauseMenuBgmPlayer.setVolume(musicMasterVolume);
        gBgmPlayer.setVolume(battleBgmBaseVolume_ * musicMasterVolume);
        gPresentationLoopAudio.setVolume(presentationLoopBaseVolume_ * musicMasterVolume);
        syncNarrativeSettings();
    }

    bool openPauseMenuDocument() {
        if (context_ == nullptr) {
            return false;
        }
        if (pauseDocument_ != nullptr && pauseMenuController_ != nullptr) {
            syncPauseMenuUiState();
            pauseMenuController_->sync(pauseMenuUiState_);
            return true;
        }

        const std::string documentPath = platform::path::resolvePath("assets/rmlui/front_ui/pause_sleek.rml");
        pauseDocument_ = context_->LoadDocument(documentPath);
        if (pauseDocument_ == nullptr) {
            std::cerr << "[Battle] Failed to load pause menu document: " << documentPath << "\n";
            return false;
        }

        pauseMenuController_ = std::make_unique<PauseDocumentController>();
        syncPauseMenuUiState();
        if (!pauseMenuController_->bind(*pauseDocument_, pauseMenuUiState_)) {
            pauseMenuController_.reset();
            pauseDocument_->Close();
            pauseDocument_ = nullptr;
            return false;
        }

        pauseDocument_->Show();
        playControllerSoundRequests(pauseMenuController_.get());
        return true;
    }

    void closePauseMenuDocument() {
        if (pauseMenuController_ != nullptr) {
            pauseMenuController_->unbind();
            pauseMenuController_.reset();
        }
        if (pauseDocument_ != nullptr) {
            pauseDocument_->Close();
            pauseDocument_ = nullptr;
        }
    }

    bool openSettingsMenuDocument() {
        if (context_ == nullptr) {
            return false;
        }
        if (settingsDocument_ != nullptr && settingsMenuController_ != nullptr) {
            syncSettingsMenuUiState();
            settingsMenuController_->sync(settingsMenuUiState_);
            return true;
        }

        const std::string documentPath = platform::path::resolvePath("assets/rmlui/front_ui/settings.rml");
        settingsDocument_ = context_->LoadDocument(documentPath);
        if (settingsDocument_ == nullptr) {
            std::cerr << "[Battle] Failed to load settings menu document: " << documentPath << "\n";
            return false;
        }

        settingsMenuController_ = std::make_unique<SettingsDocumentController>();
        syncSettingsMenuUiState();
        if (!settingsMenuController_->bind(*settingsDocument_, settingsMenuUiState_)) {
            settingsMenuController_.reset();
            settingsDocument_->Close();
            settingsDocument_ = nullptr;
            return false;
        }

        settingsDocument_->Show();
        playControllerSoundRequests(settingsMenuController_.get());
        return true;
    }

    void closeSettingsMenuDocument() {
        if (settingsMenuController_ != nullptr) {
            settingsMenuController_->unbind();
            settingsMenuController_.reset();
        }
        if (settingsDocument_ != nullptr) {
            settingsDocument_->Close();
            settingsDocument_ = nullptr;
        }
    }

    std::string choosePauseMenuTrack() {
        if (pauseMenuTracks_.empty()) {
            pauseMenuTracks_ = resolveUiMusicTrackPaths();
        }
        if (pauseMenuTracks_.empty()) {
            return std::string();
        }

        const std::size_t index = pauseMenuTrackCursor_ % pauseMenuTracks_.size();
        ++pauseMenuTrackCursor_;
        return pauseMenuTracks_[index];
    }

    void startPauseMenuMusic() {
        stopPauseMenuMusic();

        battleBgmWasPlayingBeforePause_ = gBgmPlayer.isPlaying();
        battleBgmWasPausedBeforePause_ = gBgmPlayer.isPaused();
        if (battleBgmWasPlayingBeforePause_ && !battleBgmWasPausedBeforePause_) {
            gBgmPlayer.pause();
        }

        const std::string trackPath = choosePauseMenuTrack();
        if (!trackPath.empty()) {
            const float musicVolume = settings_ != nullptr ? settings_->musicVolume : 1.0f;
            (void)gPauseMenuBgmPlayer.play(trackPath, musicVolume);
        }
    }

    void stopPauseMenuMusic() {
        gPauseMenuBgmPlayer.stop();
    }

    void leavePauseMenu(bool resumeBattleMusic) {
        closeSettingsMenuDocument();
        closePauseMenuDocument();
        stopPauseMenuMusic();
        if (resumeBattleMusic && battleBgmWasPlayingBeforePause_ && !battleBgmWasPausedBeforePause_) {
            gBgmPlayer.resume();
        }
        battleBgmWasPlayingBeforePause_ = false;
        battleBgmWasPausedBeforePause_ = false;
        battleBgmBaseVolume_ = 1.0f;
        presentationLoopBaseVolume_ = 1.0f;
        paused_ = false;
        pauseOverlayMode_ = PauseOverlayMode::Menu;
        pauseSelection_ = PauseSelection::Continue;
    }

    void reopenPauseMenuFromSettings() {
        closeSettingsMenuDocument();
        pauseOverlayMode_ = PauseOverlayMode::Menu;
        pauseMenuUiState_.screen = ScreenState::PauseMenu;
        pauseMenuUiState_.confirmSelection = ConfirmAction::Cancel;
        if (!openPauseMenuDocument()) {
            paused_ = false;
        }
    }

    void enterPauseMenu() {
        paused_ = true;
        pauseOverlayMode_ = PauseOverlayMode::Menu;
        pauseSelection_ = PauseSelection::Continue;
        settingsSelection_ = SettingsSelection::DisplayMode;
        pauseMenuUiState_.screen = ScreenState::PauseMenu;
        pauseMenuUiState_.pauseSelection = PauseAction::Continue;
        pauseMenuUiState_.confirmSelection = ConfirmAction::Cancel;
        settingsMenuUiState_.screen = ScreenState::Settings;
        settingsMenuUiState_.settingsSelection = SettingsItem::DisplayMode;
        startPauseMenuMusic();
        if (!openPauseMenuDocument()) {
            stopPauseMenuMusic();
            battleBgmWasPlayingBeforePause_ = false;
            battleBgmWasPausedBeforePause_ = false;
            paused_ = false;
        }
    }

    void applyPauseMenuControllerState() {
        if (pauseMenuController_ == nullptr) {
            return;
        }

        pauseMenuController_->applyState(pauseMenuUiState_);
        playControllerSoundRequests(pauseMenuController_.get());
        applyLiveSettingsFromUiState(pauseMenuUiState_);
        pauseSelection_ = fromFrontUiPauseAction(pauseMenuUiState_.pauseSelection);

        switch (pauseMenuUiState_.screen) {
            case ScreenState::BattleDemo:
                leavePauseMenu(true);
                return;
            case ScreenState::Settings:
                closePauseMenuDocument();
                pauseOverlayMode_ = PauseOverlayMode::Settings;
                settingsMenuUiState_.screen = ScreenState::Settings;
                settingsMenuUiState_.settingsReturnScreen = ScreenState::PauseMenu;
                settingsMenuUiState_.settingsSelection = SettingsItem::DisplayMode;
                if (!openSettingsMenuDocument()) {
                    leavePauseMenu(true);
                }
                pauseMenuUiState_.screen = ScreenState::PauseMenu;
                return;
            case ScreenState::MainMenu:
                closeSettingsMenuDocument();
                closePauseMenuDocument();
                stopPauseMenuMusic();
                battleBgmWasPlayingBeforePause_ = false;
                battleBgmWasPausedBeforePause_ = false;
                paused_ = false;
                exitToMainMenuRequested_ = true;
                finished_ = true;
                return;
            default:
                break;
        }
    }

    void updatePauseMenuUi(float deltaSeconds) {
        if (pauseOverlayMode_ != PauseOverlayMode::Menu || pauseMenuController_ == nullptr) {
            return;
        }

        syncPauseMenuUiState();
        pauseMenuController_->update(pauseMenuUiState_, deltaSeconds);
        applyPauseMenuControllerState();
    }

    void applySettingsMenuControllerState() {
        if (settingsMenuController_ == nullptr) {
            return;
        }

        settingsMenuController_->applyState(settingsMenuUiState_);
        playControllerSoundRequests(settingsMenuController_.get());
        applyLiveSettingsFromUiState(settingsMenuUiState_);

        if (const std::optional<graphics::frontui::Command> command = settingsMenuController_->consumeCommand();
            command.has_value()) {
            switch (command->type) {
                case graphics::frontui::CommandType::ApplyDisplayMode:
                    applyDisplayMode(command->displayModeFullscreen);
                    syncSettingsMenuUiState();
                    settingsMenuController_->sync(settingsMenuUiState_);
                    break;
                case graphics::frontui::CommandType::ReturnFromSettings:
                    reopenPauseMenuFromSettings();
                    break;
                case graphics::frontui::CommandType::ActivateMainMenuAction:
                    break;
            }
        }
    }

    void updateSettingsMenuUi(float deltaSeconds) {
        if (pauseOverlayMode_ != PauseOverlayMode::Settings || settingsMenuController_ == nullptr) {
            return;
        }

        syncSettingsMenuUiState();
        settingsMenuController_->update(settingsMenuUiState_, deltaSeconds);
        applySettingsMenuControllerState();
    }

    void handlePauseEvent(const SDL_Event& event) {
        if (pauseOverlayMode_ == PauseOverlayMode::Menu) {
            if (event.type == SDL_KEYDOWN && pauseMenuController_ != nullptr) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    pauseMenuController_->cancel();
                } else if (event.key.keysym.sym == SDLK_F11) {
                    toggleDisplayMode();
                } else if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
                    pauseMenuController_->moveSelection(-1);
                } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
                    pauseMenuController_->moveSelection(1);
                } else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) {
                    pauseMenuController_->adjustSelection(-1);
                } else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) {
                    pauseMenuController_->adjustSelection(1);
                } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                           event.key.keysym.sym == SDLK_SPACE) {
                    pauseMenuController_->activateSelection();
                }
            }
            applyPauseMenuControllerState();
            return;
        }

        if (settingsMenuController_ == nullptr) {
            return;
        }

        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                settingsMenuController_->cancel();
            } else if (event.key.keysym.sym == SDLK_F11) {
                applyDisplayMode(settings_ == nullptr ? true : !settings_->fullscreen);
                syncSettingsMenuUiState();
                settingsMenuController_->sync(settingsMenuUiState_);
            } else if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
                settingsMenuController_->moveSelection(-1);
            } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
                settingsMenuController_->moveSelection(1);
            } else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) {
                settingsMenuController_->adjustSelection(-1);
            } else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) {
                settingsMenuController_->adjustSelection(1);
            } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                       event.key.keysym.sym == SDLK_SPACE) {
                settingsMenuController_->activateSelection();
            }
        }
        applySettingsMenuControllerState();
    }

    void toggleDisplayMode() {
        applyDisplayMode(settings_ == nullptr ? true : !settings_->fullscreen);
    }

    void applyDisplayMode(bool fullscreen) {
        if (settings_ == nullptr || windowHost_ == nullptr) {
            return;
        }
        if (windowHost_->setFullscreen(fullscreen)) {
            settings_->fullscreen = fullscreen;
        }
        updateViewportFromWindow();
        applyReferenceSceneMetrics();
        (void)refreshSceneRenderers();
    }

    void beginRhythmChallenge(int partyIndex) {
        const battle::BattleState& state = manager_.getBattleState();
        if (partyIndex < 0 || partyIndex >= static_cast<int>(state.party.size())) {
            return;
        }
        const battle::CharacterDefinition& character = state.party[static_cast<size_t>(partyIndex)];
        rhythmChallenge_.active = true;
        rhythmChallenge_.partyIndex = partyIndex;
        rhythmChallenge_.actorTitle = character.title;
        rhythmChallenge_.abilityName = "Rhythm Skill";
        rhythmChallenge_.startedMs = SDL_GetTicks64();
    }

    void handleManualUltimateRequestResult(int partyIndex,
                                           battle::ManualUltimateRequestResult result,
                                           Uint64 nowMs,
                                           bool buffered) {
        switch (result) {
            case battle::ManualUltimateRequestResult::Queued:
                return;
            case battle::ManualUltimateRequestResult::MeterNotReady: {
                const int charge = manager_.getCharacterUltimateCharge(partyIndex);
                const int required = manager_.getCharacterUltimateRequired(partyIndex);
                const int missing = std::max(0, required - charge);
                const std::string message =
                    "ALLY " + std::to_string(partyIndex + 1) +
                    " ULTIMATE NEEDS " + std::to_string(missing) +
                    " MORE " + (missing == 1 ? "ABILITY." : "ABILITIES.");
                if (buffered) {
                    showHint(hudFeedback_, message, nowMs, 1800);
                } else {
                    showToast(hudFeedback_, message, nowMs);
                }
                blinkMissingOrbs(hudFeedback_, partyIndex, charge, required - 1, nowMs);
                return;
            }
            case battle::ManualUltimateRequestResult::AlreadyQueued:
                if (buffered) {
                    showHint(hudFeedback_, "ULTIMATE ALREADY QUEUED.", nowMs, 1400);
                } else {
                    showToast(hudFeedback_, "ULTIMATE ALREADY QUEUED.", nowMs);
                }
                return;
            case battle::ManualUltimateRequestResult::Unavailable:
                if (buffered) {
                    showHint(hudFeedback_, "ULTIMATE NOT AVAILABLE.", nowMs, 1400);
                } else {
                    showToast(hudFeedback_, "ULTIMATE NOT AVAILABLE.", nowMs);
                }
                return;
        }
    }

    void flushBufferedManualUltimateRequests(Uint64 nowMs) {
        std::vector<int> pendingRequests;
        pendingRequests.swap(bufferedManualUltimatePartyIndices_);
        for (int partyIndex : pendingRequests) {
            handleManualUltimateRequestResult(
                partyIndex,
                manager_.requestManualUltimateTurn(partyIndex),
                nowMs,
                true
            );
        }
    }

    bool handleManualUltimateHotkey(SDL_Keycode key, Uint64 nowMs) {
        const int partyIndex = manualUltimatePartyIndexFromKey(key);
        if (partyIndex < 0) {
            return false;
        }

        if (freeViewEnabled_ ||
            tutorialOverlay_.step != TutorialStep::None ||
            !isBattleSpaceEnabled() ||
            isDialogueInProgress() ||
            manager_.isBattleOver()) {
            return true;
        }

        if (rhythmChallenge_.active) {
            if (std::find(bufferedManualUltimatePartyIndices_.begin(),
                          bufferedManualUltimatePartyIndices_.end(),
                          partyIndex) == bufferedManualUltimatePartyIndices_.end()) {
                bufferedManualUltimatePartyIndices_.push_back(partyIndex);
                showHint(hudFeedback_, "ULTIMATE WILL QUEUE AFTER THIS SKILL.", nowMs, 1200);
            }
            return true;
        }

        handleManualUltimateRequestResult(
            partyIndex,
            manager_.requestManualUltimateTurn(partyIndex),
            nowMs,
            false
        );
        return true;
    }

    void maybeStartTutorial(Uint64 nowMs) {
        if (!tutorialEnabled_ || !tutorialLibrary_.loaded ||
            tutorialOverlay_.dismissed || rhythmChallenge_.active || tutorialOverlay_.step != TutorialStep::None) {
            return;
        }
        if (!tutorialOverlay_.standardShown && manager_.isPlayerActionReady(battle::BattleAction::Standard)) {
            startTutorial(tutorialOverlay_, TutorialStep::Standard, tutorialLibrary_.standard, nowMs);
            return;
        }
        if (!tutorialOverlay_.skillShown && manager_.isPlayerActionReady(battle::BattleAction::Skill)) {
            startTutorial(tutorialOverlay_, TutorialStep::Skill, tutorialLibrary_.skill, nowMs);
            return;
        }
        bool hasReadyManualUltimate = false;
        const battle::BattleState& state = manager_.getBattleState();
        for (int i = 0; i < static_cast<int>(state.party.size()); ++i) {
            if (manager_.previewManualUltimateTurnRequest(i) == battle::ManualUltimateRequestResult::Queued) {
                hasReadyManualUltimate = true;
                break;
            }
        }
        if (!tutorialOverlay_.ultimateShown && hasReadyManualUltimate) {
            startTutorial(tutorialOverlay_, TutorialStep::Ultimate, tutorialLibrary_.ultimate, nowMs);
        }
    }

    void completeTutorialForAction(battle::BattleAction action) {
        if (action == battle::BattleAction::Standard && tutorialOverlay_.step == TutorialStep::Standard) {
            completeTutorialStep(tutorialOverlay_);
        } else if (action == battle::BattleAction::Skill && tutorialOverlay_.step == TutorialStep::Skill) {
            completeTutorialStep(tutorialOverlay_);
        } else if (action == battle::BattleAction::Ultimate && tutorialOverlay_.step == TutorialStep::Ultimate) {
            completeTutorialStep(tutorialOverlay_);
        }
    }

    void finalizeSkillChallenge(bool onBeat) {
        if (!rhythmChallenge_.active) {
            return;
        }
        const Uint64 nowMs = SDL_GetTicks64();
        rhythmChallenge_ = RhythmChallengeState{};
        if (!manager_.executePlayerAction(battle::BattleAction::Skill)) {
            bufferedManualUltimatePartyIndices_.clear();
            showToast(hudFeedback_, "ACTION NOT AVAILABLE.", nowMs);
            return;
        }
        showToast(hudFeedback_, onBeat ? "ON-BEAT INPUT." : "LATE INPUT.", nowMs, 1100);
        completeTutorialForAction(battle::BattleAction::Skill);
        flushBufferedManualUltimateRequests(nowMs);
        manager_.processAutomaticTurns();
        consumeBattleActionEvents(
            hudFeedback_,
            manager_,
            settings_ != nullptr ? settings_->voiceVolume : 1.0f,
            nowMs,
            &cameraStaging_);
    }

    void attemptAction(battle::BattleAction action) {
        const Uint64 nowMs = SDL_GetTicks64();
        if (paused_) {
            return;
        }
        if (rhythmChallenge_.active) {
            showToast(hudFeedback_, "FINISH THE RHYTHM INPUT.", nowMs, 1200);
            return;
        }
        const std::optional<int> activePartyIndex = getActiveCharacterPartyIndex(manager_);
        if (!activePartyIndex.has_value()) {
            showToast(hudFeedback_, "WAIT FOR AN ALLY TURN.", nowMs);
            return;
        }
        if (action == battle::BattleAction::Ultimate && !manager_.isPlayerActionReady(action)) {
            const int charge = manager_.getCharacterUltimateCharge(*activePartyIndex);
            const int required = manager_.getCharacterUltimateRequired(*activePartyIndex);
            const int missing = std::max(0, required - charge);
            showToast(
                hudFeedback_,
                "ULTIMATE NEEDS " + std::to_string(missing) + " MORE ORB" + (missing == 1 ? "" : "S") + ".",
                nowMs
            );
            blinkMissingOrbs(hudFeedback_, *activePartyIndex, charge, required - 1, nowMs);
            return;
        }
        if (action == battle::BattleAction::Skill && !manager_.isPlayerActionReady(action)) {
            showToast(hudFeedback_, "SKILL NOT AVAILABLE.", nowMs);
            return;
        }
        if (action == battle::BattleAction::Skill) {
            beginRhythmChallenge(*activePartyIndex);
            return;
        }
        if (!manager_.executePlayerAction(action)) {
            showToast(hudFeedback_, "ACTION NOT AVAILABLE.", nowMs);
            return;
        }
        completeTutorialForAction(action);
        manager_.processAutomaticTurns();
        consumeBattleActionEvents(
            hudFeedback_,
            manager_,
            settings_ != nullptr ? settings_->voiceVolume : 1.0f,
            nowMs,
            &cameraStaging_);
    }

    void onActiveActorChanged(const battle::TurnActor* actor, const battle::BattleState& state, Uint64 nowMs) {
        if (actor == nullptr || actor->type != battle::ParticipantType::Character ||
            actor->partyIndex < 0 || actor->partyIndex >= static_cast<int>(state.party.size())) {
            resetIdleVoicelineState();
            return;
        }

        const battle::CharacterDefinition& character = state.party[static_cast<size_t>(actor->partyIndex)];
        scheduleIdleVoiceline(character.assets, actor->partyIndex, nowMs);
    }

    void scheduleIdleVoiceline(const std::string& assetName, int partyIndex, Uint64 nowMs) {
        stopIdleVoicelinePlayback();
        idleActorPartyIndex_ = partyIndex;
        idleVoiceClipPath_ = findIdleVoicePath(assetName);
        idlePlayingPath_.clear();
        idleNextPlayMs_ = idleVoiceClipPath_.has_value() ? nowMs + kIdleDelayMs : 0;
    }

    void resetIdleVoicelineState() {
        stopIdleVoicelinePlayback();
        idleActorPartyIndex_.reset();
        idleVoiceClipPath_.reset();
        idleNextPlayMs_ = 0;
    }

    void stopIdleVoicelinePlayback() {
        if (!idlePlayingPath_.empty()) {
            gOneShotAudio.stopPlayback(idlePlayingPath_);
            idlePlayingPath_.clear();
        }
    }

    void handleSpaceKeyIdle(Uint64 nowMs) {
        if (!idleActorPartyIndex_.has_value()) {
            return;
        }
        stopIdleVoicelinePlayback();
        if (idleVoiceClipPath_.has_value()) {
            idleNextPlayMs_ = nowMs + kIdleDelayMs;
        } else {
            idleNextPlayMs_ = 0;
        }
    }

    void maybeHandleIdleVoiceline(Uint64 nowMs) {
        if (!idleActorPartyIndex_.has_value() || !idleVoiceClipPath_.has_value()) {
            return;
        }

        const std::optional<int> activeIndex = getActiveCharacterPartyIndex(manager_);
        if (!activeIndex.has_value() || activeIndex != idleActorPartyIndex_) {
            resetIdleVoicelineState();
            return;
        }

        if (!idlePlayingPath_.empty()) {
            if (!gOneShotAudio.isPlaying(idlePlayingPath_)) {
                idlePlayingPath_.clear();
                idleNextPlayMs_ = nowMs + kIdleDelayMs;
            }
            return;
        }

        if (idleNextPlayMs_ == 0 || nowMs < idleNextPlayMs_) {
            return;
        }

        const float voiceVolume = settings_ != nullptr ? settings_->voiceVolume : 1.0f;
        if (gOneShotAudio.playWavOneShot(*idleVoiceClipPath_, voiceVolume)) {
            idlePlayingPath_ = *idleVoiceClipPath_;
            idleNextPlayMs_ = 0;
        } else {
            idleNextPlayMs_ = nowMs + kIdleDelayMs;
        }
    }

    std::optional<std::string> findIdleVoicePath(const std::string& assetName) {
        const auto it = idleVoicePathCache_.find(assetName);
        if (it != idleVoicePathCache_.end()) {
            return it->second;
        }
        const std::optional<std::string> resolved = platform::path::resolveCombatVoicePath(assetName, "idle");
        idleVoicePathCache_.emplace(assetName, resolved);
        return resolved;
    }

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool initialized_ = false;
    bool finished_ = false;
    bool exitToMainMenuRequested_ = false;
    bool paused_ = false;
    bool rmlInitialized_ = false;
    bool rmlGlInitialized_ = false;
    bool imageInitialized_ = false;
    bool ttfInitializedHere_ = false;
    int windowWidth_ = kWindowWidth;
    int windowHeight_ = kWindowHeight;
    int drawableWidth_ = kWindowWidth;
    int drawableHeight_ = kWindowHeight;
    GameSettings* settings_ = nullptr;
    PauseOverlayMode pauseOverlayMode_ = PauseOverlayMode::Menu;
    PauseSelection pauseSelection_ = PauseSelection::Continue;
    SettingsSelection settingsSelection_ = SettingsSelection::DisplayMode;
    battle::BattleDefinition battleDefinition_{};
    battle::render::StageDefinition stageDefinition_{};
    std::vector<std::string> activePartyLineup_;
    battle::BattleManager manager_;
    battle::demo::DemoNarrativeFlow narrative_;
    TutorialScriptLibrary tutorialLibrary_;
    HudFeedbackState hudFeedback_;
    HudAnimationState hudAnimationState_;
    TutorialOverlayState tutorialOverlay_;
    RhythmChallengeState rhythmChallenge_;
    std::vector<int> bufferedManualUltimatePartyIndices_;
    std::vector<std::unique_ptr<Rml::EventListener>> uiListeners_;
    std::vector<std::string> worldAssets_;
    std::vector<SceneEntity> entities_;
    SoftwareSceneRenderer sceneRenderer_;
    SoftwareSceneRenderer presentationOverlayRenderer_;
    battle::render::GlBattleSceneRenderer glSceneRenderer_;
    GlScreenBlitter screenBlitter_;
    GlScreenBlitter presentationOverlayBlitter_;
    SystemInterface_SDL systemInterface_;
    std::unique_ptr<graphics::RmlUiSdlGlRenderInterface> renderInterface_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    Rml::ElementDocument* pauseDocument_ = nullptr;
    std::unique_ptr<PauseDocumentController> pauseMenuController_;
    Rml::ElementDocument* settingsDocument_ = nullptr;
    std::unique_ptr<SettingsDocumentController> settingsMenuController_;
    graphics::RmlUiLoadingOverlay loadingOverlay_;
    graphics::RmlUiLoadingOverlayState loadingOverlayState_{};
    AppState pauseMenuUiState_{};
    AppState settingsMenuUiState_{};
    std::vector<std::string> pauseMenuTracks_;
    std::size_t pauseMenuTrackCursor_ = 0;
    bool battleBgmWasPlayingBeforePause_ = false;
    bool battleBgmWasPausedBeforePause_ = false;
    float battleBgmBaseVolume_ = 1.0f;
    float presentationLoopBaseVolume_ = 1.0f;
    battle::Camera3D camera_;
    battle::render::BattleCameraStaging cameraStaging_;
    battle::render::BattleCombatBeginAnimation combatBeginAnimation_;
    battle::render::BattleFeedbackSystem feedback_;
    CameraIntroAnimation cameraIntro_;
    battle::render::FreeViewCameraDebugLog freeViewCameraDebugLog_;
    bool freeViewEnabled_ = false;
    bool tutorialEnabled_ = false;
    bool narrativeEnabled_ = false;
    bool narrativeInitialized_ = false;
    bool presentationPlaybackActive_ = false;
    bool presentationCasterIsBoss_ = false;
    int presentationCasterPartyIndex_ = -1;
    battle::AbilityPresentation* activePresentation_ = nullptr;
    std::unique_ptr<battle::SplashArtAnimation> activeUltimateTurnSplash_;
    int previewUltimateSplashPartyIndex_ = -1;
    std::function<void(SDL_Renderer*, int, int)> activeOverlay_;
    float frameAccumulator_ = 0.0f;
    bool discardNextUpdateDelta_ = false;
    std::string presentationAudioSequenceId_;
    int presentationAudioCueIndex_ = 0;
    std::optional<int> idleActorPartyIndex_;
    std::optional<std::string> idleVoiceClipPath_;
    std::string idlePlayingPath_;
    Uint64 idleNextPlayMs_ = 0;
    std::unordered_map<std::string, std::optional<std::string>> idleVoicePathCache_;
    std::string lastTurnToken_;
    bool frameTimingEnabled_ = false;
    Uint64 frameTimingFrequency_ = 0;
    int timingFrameCount_ = 0;
    double timingWorldMs_ = 0.0;
    double timingCompatibilityMs_ = 0.0;
    double timingUiMs_ = 0.0;
};

Session::Session() : impl_(std::make_unique<SessionImpl>()) {}
Session::~Session() = default;
bool Session::initialize(Window& window,
                         GameSettings& settings,
                         const std::string& battleKey,
                         const battle::PlayerProgression& progression,
                         std::optional<std::vector<std::string>> initialPartyLineup) {
    return impl_->initialize(window, settings, battleKey, progression, std::move(initialPartyLineup));
}
void Session::shutdown() { impl_->shutdown(); }
void Session::handleEvent(const SDL_Event& event) { impl_->handleEvent(event); }
void Session::update(float deltaSeconds) { impl_->update(deltaSeconds); }
void Session::render() { impl_->render(); }
void Session::setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) { impl_->setLoadingOverlay(state); }
bool Session::isFinished() const { return impl_->isFinished(); }
bool Session::exitedToMainMenu() const { return impl_->exitedToMainMenu(); }
BattleOutcome Session::outcome() const { return impl_->outcome(); }
const std::vector<std::string>& Session::currentPartyLineup() const { return impl_->currentPartyLineup(); }

} // namespace battle::app
