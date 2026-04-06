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
#include "audio/battle_voice_arbiter.h"
#include "audio/battle_bgm_controller.h"
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
constexpr Uint64 kPresentationDamageHoldDurationMs = 2500;
constexpr Uint64 kPresentationDamageFadeDurationMs = 320;
constexpr const char* kLoadingOverlayDocumentPath = "assets/rmlui/shared/loading_overlay.rml";
constexpr float kCharacterVisibilityAnimSeconds = 0.22f;
constexpr float kCharacterVisibilityOffsetPx = 70.0f;
constexpr const char* kPerfectJudgementSfxPath = "assets/ui/sfx/SongSelect_select-random.wav";
constexpr const char* kGoodJudgementSfxPath = "assets/ui/sfx/Selection_roulette-4.wav";
constexpr const char* kOkayJudgementSfxPath = "assets/ui/sfx/Selection_roulette-0.wav";
constexpr const char* kFlopJudgementSfxPath = "assets/ui/sfx/UI_notification-error.wav";
constexpr std::array<const char*, 5> kBattleResultLetterSfxPaths{
    "assets/ui/sfx/Selection_roulette-0.wav",
    "assets/ui/sfx/Selection_roulette-1.wav",
    "assets/ui/sfx/Selection_roulette-2.wav",
    "assets/ui/sfx/Selection_roulette-3.wav",
    "assets/ui/sfx/Selection_roulette-4.wav"
};
constexpr const char* kBattleResultVictoryRevealSfxPath = "assets/ui/sfx/Selection_roulette-result.wav";
constexpr const char* kBattleResultDefeatRevealSfxPath = "assets/ui/sfx/Results_rank-impact-fail.wav";
constexpr const char* kBattleResultVictoryApplauseSfxPath = "assets/ui/sfx/Results_applause-s.wav";
constexpr const char* kBattleResultButtonHoverSfxPath = "assets/ui/sfx/UI_button-hover.wav";
constexpr const char* kBattleResultButtonSelectSfxPath = "assets/ui/sfx/UI_button-select.wav";
constexpr float kBattleResultMaxPresentationStepSeconds = 0.05f;
constexpr const char* kBattleVsIntroLineDropSfxPath = "assets/ui/sfx/Matchmaking_enqueue.wav";
constexpr const char* kBattleVsIntroLineTiltSfxPath = "assets/ui/sfx/Matchmaking_match-found.wav";
constexpr const char* kBattleVsIntroLeftCardSfxPath = "assets/ui/sfx/Menu_button-default-select.wav";
constexpr const char* kBattleVsIntroRightCardSfxPath = "assets/ui/sfx/Menu_button-daily-select.wav";
constexpr std::array<const char*, 6> kBattleVsIntroLetterSfxPaths{
    "assets/ui/sfx/Cloud_appear-0.wav",
    "assets/ui/sfx/Cloud_appear-1.wav",
    "assets/ui/sfx/Cloud_appear-2.wav",
    "assets/ui/sfx/Cloud_appear-3.wav",
    "assets/ui/sfx/Cloud_appear-4.wav",
    "assets/ui/sfx/Cloud_appear-5.wav"
};
constexpr const char* kBattleVsIntroVsHitSfxPath = "assets/ui/sfx/Matchmaking_stage-segment.wav";
constexpr const char* kBattleVsIntroMatchStartSfxPathA = "assets/ui/sfx/Gameplay_restart.wav";
constexpr const char* kBattleVsIntroMatchStartSfxPathB = "assets/ui/sfx/Ranked_vs-swoosh.wav";
constexpr float kBattleVsIntroBgmFadeInSeconds = 0.60f;
constexpr float kBossPhaseIntroDurationSeconds = 0.90f;
constexpr float kBossPhaseIntroStartOffsetX = -170.0f;
constexpr float kBossPhaseIntroStartOffsetY = 40.0f;
constexpr float kBossPhaseIntroStartOffsetZ = -125.0f;
constexpr float kBossPhaseIntroEndOffsetX = -180.0f;
constexpr float kBossPhaseIntroEndOffsetY = 80.0f;
constexpr float kBossPhaseIntroEndOffsetZ = -129.0f;
constexpr float kBossPhaseIntroLookOffsetY = -10.0f;
constexpr float kBossPhaseIntroLookOffsetZ = -95.0f;
constexpr float kPi = 3.14159265359f;

/**
 * @brief Convert an angle from radians to degrees.
 *
 * @param radians Angle in radians.
 * @return float Angle in degrees.
 */
float radiansToDegrees(float radians) {
    return radians * (180.0f / kPi);
}

/**
 * @brief Maps number-row and numeric-pad keys 1–4 to party indices.
 *
 * @param key SDL keycode to map.
 * @return int Party index 0–3 for keys `1`/`KP_1`…`4`/`KP_4`, or `-1` if the key does not correspond to a party index.
 */
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
game::audio::BgmPlayer gBattleBgmCrossfadePlayer;
game::audio::BattleBgmController gBattleBgmController;
game::audio::BgmPlayer gPresentationLoopAudio;
game::audio::BgmPlayer gPauseMenuBgmPlayer;

std::vector<std::string> resolveUiMusicTrackPaths() {
    std::map<std::string, std::filesystem::path> bestByStem;
    const std::string classicsDirectory = platform::path::resolvePath("assets/ui/classics");
    std::error_code filesystemError;
    if (classicsDirectory.empty() ||
        !std::filesystem::exists(classicsDirectory, filesystemError) ||
        filesystemError) {
        return {};
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
        if (extension != ".opus" && extension != ".wav") {
            continue;
        }

        const std::string stemKey = entry.path().stem().string();
        auto [it, inserted] = bestByStem.emplace(stemKey, entry.path());
        if (!inserted && extension == ".opus") {
            it->second = entry.path();
        }
    }

    std::vector<std::string> tracks;
    tracks.reserve(bestByStem.size());
    for (const auto& [_, path] : bestByStem) {
        tracks.push_back(path.string());
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

    return manager.resolveCharacterVoiceAssetId(context.casterIndex);
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

    const std::optional<std::string> resolved = platform::path::resolveAudioPath(path);
    if (!resolved.has_value()) {
        return false;
    }

    bool played = false;
    for (int index = 0; index < repeatCount; ++index) {
        if (gOneShotAudio.playWavOneShot(*resolved, voiceVolume)) {
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

    const std::optional<std::string> resolved = platform::path::resolveAudioPath(path);
    if (!resolved.has_value()) {
        return false;
    }

    return player.playWavOneShot(*resolved, volume, replaceExisting);
}

bool playResolvedLoop(game::audio::BgmPlayer& player, const std::string& path, float volume) {
    if (path.empty()) {
        return false;
    }

    const std::optional<std::string> resolved = platform::path::resolveAudioPath(path);
    if (!resolved.has_value()) {
        return false;
    }

    return player.play(*resolved, volume);
}

/**
 * @brief Stops all presentation audio playback and related SFX.
 *
 * Stops the active presentation loop audio and cancels all presentation one-shot SFX.
 *
 * @param resumeBgm If true, resumes the battle BGM controller after stopping presentation audio.
 */
void stopPresentationAudioPlayback(bool resumeBgm = false) {
    gPresentationLoopAudio.stop();
    gPresentationSfxAudio.stopAllPlayback();
    if (resumeBgm) {
        gBattleBgmController.resume();
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

std::optional<std::string> resolveCombatVoiceClipPath(
    const std::string& assetName,
    std::initializer_list<const char*> clipNames) {
    if (assetName.empty()) {
        return std::nullopt;
    }

    for (const char* clipName : clipNames) {
        if (clipName == nullptr || *clipName == '\0') {
            continue;
        }
        if (const auto clipPath = platform::path::resolveCombatVoicePath(assetName, clipName);
            clipPath.has_value()) {
            return *clipPath;
        }
    }

    return std::nullopt;
}

std::string normalizeVoiceLookupToken(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return normalized;
}

std::string combatVoiceSpeakerKey(const battle::CharacterDefinition& character) {
    if (!character.voiceSpeakerId.empty()) {
        return character.voiceSpeakerId;
    }
    if (!character.voiceAssetId.empty()) {
        return character.voiceAssetId;
    }
    if (!character.assets.empty()) {
        return character.assets;
    }
    return character.key;
}

std::string combatVoiceAssetId(const battle::CharacterDefinition& character) {
    if (!character.voiceAssetId.empty()) {
        return character.voiceAssetId;
    }
    if (!character.assets.empty()) {
        return character.assets;
    }
    return character.key;
}

std::string combatVoiceSpeakerKey(const battle::BossDefinition& boss) {
    if (!boss.voiceSpeakerId.empty()) {
        return boss.voiceSpeakerId;
    }
    if (!boss.assets.empty()) {
        return boss.assets;
    }
    return boss.key;
}

std::string inferScriptSpeakerKey(const vn::ScriptEntry& entry, const battle::BattleState& battleState) {
    if (!entry.voiceSpeakerId.empty()) {
        return entry.voiceSpeakerId;
    }

    const auto matchesCharacter = [&](const battle::CharacterDefinition& character,
                                      const std::string& normalizedToken) {
        return normalizedToken == normalizeVoiceLookupToken(character.assets) ||
            normalizedToken == normalizeVoiceLookupToken(character.voiceAssetId) ||
            normalizedToken == normalizeVoiceLookupToken(character.key) ||
            normalizedToken == normalizeVoiceLookupToken(character.title);
    };

    if (!entry.icon.empty()) {
        const std::string stem = std::filesystem::path(entry.icon).stem().string();
        const std::string normalizedStem = normalizeVoiceLookupToken(stem);
        for (const battle::CharacterDefinition& character : battleState.party) {
            if (matchesCharacter(character, normalizedStem)) {
                return combatVoiceSpeakerKey(character);
            }
        }
        if (normalizedStem == normalizeVoiceLookupToken(battleState.boss.assets) ||
            normalizedStem == normalizeVoiceLookupToken(battleState.boss.key) ||
            normalizedStem == normalizeVoiceLookupToken(battleState.boss.title)) {
            return combatVoiceSpeakerKey(battleState.boss);
        }
    }

    const std::string normalizedSpeaker = normalizeVoiceLookupToken(entry.speaker);
    for (const battle::CharacterDefinition& character : battleState.party) {
        if (matchesCharacter(character, normalizedSpeaker)) {
            return combatVoiceSpeakerKey(character);
        }
    }

    if (normalizedSpeaker == normalizeVoiceLookupToken(battleState.boss.assets) ||
        normalizedSpeaker == normalizeVoiceLookupToken(battleState.boss.key) ||
        normalizedSpeaker == normalizeVoiceLookupToken(battleState.boss.title)) {
        return combatVoiceSpeakerKey(battleState.boss);
    }

    return std::string();
}

using battle::app::ui::HudFeedbackState;
using battle::app::ui::HudAnimationState;
using battle::app::ui::HudHitReactionState;
using battle::app::ui::HudValueAnimationState;
using battle::app::ui::BattleInputPromptState;
using battle::app::ui::BattleHintFamily;
using battle::app::ui::BattleHintInstance;
using battle::app::ui::BattleHintPhase;
using battle::app::ui::BattleHintRequest;
using battle::app::ui::BattleHintResolveKind;
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
constexpr const char* kBattleHintTutorialSfxPath = "assets/ui/sfx/UI_overlay-pop-in.wav";
constexpr const char* kBattleHintWarningSfxPath = "assets/ui/sfx/UI_generic-error.wav";
constexpr const char* kBattleHintMajorSfxPath = "assets/ui/sfx/UI_overlay-big-pop-in.wav";
constexpr const char* kBattleHintKeyPresentationInstruction = "presentation_instruction";
constexpr const char* kBattleHintKeyManualUltimateStatus = "manual_ultimate_status";
constexpr const char* kBattleHintKeyTutorialOverlay = "tutorial_overlay";
constexpr const char* kBattleHintKeyPauseBlocked = "pause_blocked";
constexpr const char* kBattleHintKeyWaitForAllyTurn = "wait_for_ally_turn";

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

const char* defaultBattleHintSfxPath(BattleHintFamily family) {
    switch (family) {
        case BattleHintFamily::Warning:
            return kBattleHintWarningSfxPath;
        case BattleHintFamily::Major:
            return kBattleHintMajorSfxPath;
        case BattleHintFamily::Tutorial:
        case BattleHintFamily::Info:
        default:
            return kBattleHintTutorialSfxPath;
    }
}

float battleHintSfxVolume(BattleHintFamily family) {
    return family == BattleHintFamily::Major ? 0.62f : 0.54f;
}

void playBattleHintSfx(const BattleHintRequest& request) {
    if (request.showSfxPath.empty()) {
        return;
    }
    (void)gOneShotAudio.playWavOneShot(
        platform::path::resolvePath(request.showSfxPath),
        battleHintSfxVolume(request.family),
        false);
}

BattleHintInstance* findBattleHint(HudFeedbackState& feedback, const std::string& stableKey) {
    if (stableKey.empty()) {
        return nullptr;
    }

    for (BattleHintInstance& hint : feedback.hints.active) {
        if (hint.request.stableKey == stableKey) {
            return &hint;
        }
    }
    return nullptr;
}

void beginClosingBattleHint(BattleHintInstance& hint) {
    if (hint.phase == BattleHintPhase::Closing) {
        return;
    }

    hint.phase = BattleHintPhase::Closing;
    hint.phaseElapsedMs = 0;
}

void dismissBattleHintByKey(HudFeedbackState& feedback,
                            const std::string& stableKey,
                            bool immediate = false) {
    if (stableKey.empty()) {
        return;
    }

    for (auto it = feedback.hints.active.begin(); it != feedback.hints.active.end(); ++it) {
        if (it->request.stableKey != stableKey) {
            continue;
        }

        if (immediate) {
            feedback.hints.active.erase(it);
        } else {
            beginClosingBattleHint(*it);
        }
        return;
    }
}

void dismissBattleHintsByResolveKind(HudFeedbackState& feedback,
                                     BattleHintResolveKind kind,
                                     bool immediate = false) {
    for (auto it = feedback.hints.active.begin(); it != feedback.hints.active.end();) {
        if (it->request.resolveRule.kind != kind) {
            ++it;
            continue;
        }

        if (immediate) {
            it = feedback.hints.active.erase(it);
        } else {
            beginClosingBattleHint(*it);
            ++it;
        }
    }
}

void dismissBattleHintsForTutorialStep(HudFeedbackState& feedback,
                                       TutorialStep step,
                                       bool immediate = false) {
    for (auto it = feedback.hints.active.begin(); it != feedback.hints.active.end();) {
        if (it->request.resolveRule.kind != BattleHintResolveKind::TutorialStepComplete ||
            it->request.resolveRule.tutorialStep != step) {
            ++it;
            continue;
        }

        if (immediate) {
            it = feedback.hints.active.erase(it);
        } else {
            beginClosingBattleHint(*it);
            ++it;
        }
    }
}

bool battleHintMatchesActionEvent(const BattleHintInstance& hint, const battle::BattleActionEvent& event) {
    if (hint.request.resolveRule.kind != BattleHintResolveKind::BattleAction) {
        return false;
    }
    if (hint.request.resolveRule.action.has_value() && *hint.request.resolveRule.action != event.action) {
        return false;
    }
    if (!hint.request.resolveRule.abilityId.empty() &&
        hint.request.resolveRule.abilityId != event.abilityId) {
        return false;
    }
    return true;
}

void resolveBattleHintsFromActionEvent(HudFeedbackState& feedback,
                                       const battle::BattleActionEvent& event,
                                       bool immediate = false) {
    for (auto it = feedback.hints.active.begin(); it != feedback.hints.active.end();) {
        if (!battleHintMatchesActionEvent(*it, event)) {
            ++it;
            continue;
        }

        if (immediate) {
            it = feedback.hints.active.erase(it);
        } else {
            beginClosingBattleHint(*it);
            ++it;
        }
    }
}

void upsertBattleHint(HudFeedbackState& feedback, BattleHintRequest request) {
    if (request.stableKey.empty()) {
        return;
    }

    if (BattleHintInstance* existing = findBattleHint(feedback, request.stableKey)) {
        if (!request.refreshIfShown) {
            return;
        }

        existing->request = std::move(request);
        existing->visibleMessage = existing->request.message;
        existing->phase = BattleHintPhase::Opening;
        existing->phaseElapsedMs = 0;
        existing->activeElapsedMs = 0;
        existing->measurementDirty = true;
        return;
    }

    BattleHintInstance instance;
    instance.request = std::move(request);
    instance.visibleMessage = instance.request.message;
    feedback.hints.active.insert(feedback.hints.active.begin(), std::move(instance));
    if (feedback.hints.active.size() > battle::app::ui::kBattleHintMaxVisible) {
        feedback.hints.active.resize(battle::app::ui::kBattleHintMaxVisible);
    }
}

bool dismissNewestManualBattleHint(HudFeedbackState& feedback) {
    for (BattleHintInstance& hint : feedback.hints.active) {
        if (!hint.request.manualDismissAllowed || hint.phase == BattleHintPhase::Closing) {
            continue;
        }
        beginClosingBattleHint(hint);
        return true;
    }
    return false;
}

void tickBattleHints(HudFeedbackState& feedback,
                     Uint64 elapsedMs,
                     bool suppressTimeoutProgress) {
    for (BattleHintInstance& hint : feedback.hints.active) {
        if (hint.phase == BattleHintPhase::Opening) {
            hint.phaseElapsedMs += elapsedMs;
            if (hint.phaseElapsedMs >= battle::app::ui::kBattleHintOpenDurationMs) {
                hint.phase = BattleHintPhase::Active;
                hint.phaseElapsedMs = 0;
            }
            continue;
        }

        if (hint.phase == BattleHintPhase::Active &&
            hint.request.resolveRule.kind == BattleHintResolveKind::Timeout &&
            hint.request.resolveRule.durationMs > 0 &&
            !suppressTimeoutProgress) {
            hint.activeElapsedMs += elapsedMs;
            if (hint.activeElapsedMs >= hint.request.resolveRule.durationMs) {
                beginClosingBattleHint(hint);
                continue;
            }
        }

        if (hint.phase == BattleHintPhase::Closing) {
            hint.phaseElapsedMs += elapsedMs;
        }
    }

    feedback.hints.active.erase(
        std::remove_if(feedback.hints.active.begin(),
                       feedback.hints.active.end(),
                       [](const BattleHintInstance& hint) {
                           return hint.phase == BattleHintPhase::Closing &&
                               hint.phaseElapsedMs >= battle::app::ui::kBattleHintCloseDurationMs;
                       }),
        feedback.hints.active.end());
}

/**
 * @brief Display a temporary judgement popup on the HUD by updating feedback state.
 *
 * Updates the provided HUD feedback state with the judgement label, reward text,
 * CSS class name, and start/expiration timestamps to show a transient judgement popup.
 *
 * @param feedback HUD feedback state to update.
 * @param judgement The combat judgement value used to select label and styling.
 * @param rewardText Text describing the reward to display beneath the judgement.
 * @param nowMs Current time in milliseconds used as the popup start time.
 * @param durationMs Duration in milliseconds the popup should remain visible.
 */
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

/**
 * @brief Clears any active judgement popup state from the HUD feedback.
 *
 * Resets the judgement text, reward text, CSS class name, and associated start/expiry
 * timestamps so no judgement is considered active.
 *
 * @param feedback HUD feedback state to clear the judgement fields on.
 */
void clearJudgement(HudFeedbackState& feedback) {
    feedback.judgementText.clear();
    feedback.judgementRewardText.clear();
    feedback.judgementClassName.clear();
    feedback.judgementStartedMs = 0;
    feedback.judgementUntilMs = 0;
}

void clearPresentationDamage(HudFeedbackState& feedback) {
    feedback.presentationDamageTotal = 0;
    feedback.presentationDamageText.clear();
    feedback.presentationDamageVisible = false;
    feedback.presentationDamageActive = false;
    feedback.presentationDamageStartedMs = 0;
    feedback.presentationDamageHoldUntilMs = 0;
    feedback.presentationDamageFadeUntilMs = 0;
}

void beginPresentationDamageTracking(HudFeedbackState& feedback) {
    clearPresentationDamage(feedback);
    feedback.presentationDamageActive = true;
}

void registerPresentationDamage(HudFeedbackState& feedback, int amount, Uint64 nowMs) {
    if (amount <= 0 || !feedback.presentationDamageActive) {
        return;
    }

    feedback.presentationDamageTotal += amount;
    feedback.presentationDamageText = std::to_string(std::max(0, feedback.presentationDamageTotal));
    feedback.presentationDamageVisible = feedback.presentationDamageTotal > 0;
    feedback.presentationDamageStartedMs = nowMs;
    feedback.presentationDamageHoldUntilMs = 0;
    feedback.presentationDamageFadeUntilMs = 0;
}

void finishPresentationDamageTracking(HudFeedbackState& feedback,
                                      Uint64 nowMs,
                                      Uint64 holdDurationMs = kPresentationDamageHoldDurationMs,
                                      Uint64 fadeDurationMs = kPresentationDamageFadeDurationMs) {
    feedback.presentationDamageActive = false;
    if (!feedback.presentationDamageVisible || feedback.presentationDamageTotal <= 0) {
        clearPresentationDamage(feedback);
        return;
    }

    feedback.presentationDamageHoldUntilMs = nowMs + holdDurationMs;
    feedback.presentationDamageFadeUntilMs = feedback.presentationDamageHoldUntilMs + fadeDurationMs;
}

/**
 * @brief Plays the sound effect corresponding to a combat judgement.
 *
 * Triggers a one-shot audio playback for the provided `judgement` value.
 *
 * @param judgement The combat judgement whose associated sound effect will be played
 *                  (e.g., `Perfect`, `Good`, `Okay`, `Flop`).
 */
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

/**
 * @brief Ensure the boss hit flash remains active until at least now + duration.
 *
 * Updates the `bossHitUntilMs` field of the provided HUD feedback state to the greater
 * of its current value and `nowMs + durationMs`, preventing shorter overrides.
 *
 * @param feedback HUD feedback state whose boss-hit expiry will be updated.
 * @param nowMs Current time in milliseconds.
 * @param durationMs Duration in milliseconds to ensure the boss hit flash remains visible.
 */
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

/**
 * @brief Advances HUD feedback timers and clears any feedback fields whose expiration has passed.
 *
 * Evaluates hint, toast, judgement, and blink expiry times against `nowMs` and resets the
 * corresponding fields in `feedback` when they have expired.
 *
 * @param feedback HUD feedback state to update; mutated in-place to clear expired entries.
 * @param nowMs Current timestamp in milliseconds used to compare against each expiry field.
 */
void tickHudFeedback(HudFeedbackState& feedback, Uint64 nowMs) {
    if (feedback.toastUntilMs != 0 && nowMs >= feedback.toastUntilMs) {
        feedback.toastText.clear();
        feedback.toastUntilMs = 0;
    }
    if (feedback.autoActionIndicator.active) {
        const Uint64 totalDurationMs =
            feedback.autoActionIndicator.enterDurationMs +
            feedback.autoActionIndicator.holdDurationMs +
            feedback.autoActionIndicator.exitDurationMs;
        if (totalDurationMs == 0 ||
            (feedback.autoActionIndicator.startedMs != 0 &&
             nowMs >= feedback.autoActionIndicator.startedMs + totalDurationMs)) {
            feedback.autoActionIndicator = battle::app::ui::AutoActionIndicatorState{};
        }
    }
    if (feedback.judgementUntilMs != 0 && nowMs >= feedback.judgementUntilMs) {
        clearJudgement(feedback);
    }
    if (!feedback.presentationDamageActive &&
        feedback.presentationDamageFadeUntilMs != 0 &&
        nowMs >= feedback.presentationDamageFadeUntilMs) {
        clearPresentationDamage(feedback);
    }
    if (feedback.blinkUntilMs != 0 && nowMs >= feedback.blinkUntilMs) {
        feedback.blinkUnitIndex = -1;
        feedback.blinkMissingFrom = 0;
        feedback.blinkMissingTo = 0;
        feedback.blinkUntilMs = 0;
    }
}

/**
 * @brief Synchronizes HUD feedback state with the current battle manager state.
 *
 * Ensures per-unit hit timers match the manager's party size and copies the
 * current combo count and damage bonus fraction from the manager into the HUD
 * feedback state.
 *
 * @param feedback Mutable HUD feedback state to update (per-unit hit timers and combo fields will be modified).
 * @param manager Const reference to the battle manager providing authoritative party and combo state.
 */
void syncHudFeedbackState(HudFeedbackState& feedback, const battle::BattleManager& manager) {
    const std::size_t partySize = manager.getBattleState().party.size();
    if (feedback.unitHitUntilMs.size() != partySize) {
        feedback.unitHitUntilMs.assign(partySize, 0);
    }
    const battle::BattleComboState& comboState = manager.getComboState();
    feedback.comboCount = comboState.comboCount;
    feedback.comboBonusFraction = comboState.damageBonusFraction;
}

/**
 * @brief Initialize a HUD value animation state to a specific value and default timing.
 *
 * Sets the state's displayed, from, target, and trail values to the provided value,
 * marks the animation as initialized and inactive, resets elapsed time, and applies
 * the default animation duration (kHudAnimationDurationSeconds).
 *
 * @param state Animation state to initialize.
 * @param value Value to set as the current/displayed/from/target/trail value.
 */
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

void completeTutorialStep(TutorialOverlayState& tutorial, HudFeedbackState* feedback = nullptr) {
    const TutorialStep completedStep = tutorial.step;
    switch (completedStep) {
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
    if (feedback != nullptr && completedStep != TutorialStep::None) {
        dismissBattleHintsForTutorialStep(*feedback, completedStep);
    }
}

void skipAllTutorials(TutorialOverlayState& tutorial, HudFeedbackState* feedback = nullptr) {
    tutorial.standardShown = true;
    tutorial.skillShown = true;
    tutorial.ultimateShown = true;
    tutorial.dismissed = true;
    dismissTutorialOverlay(tutorial);
    if (feedback != nullptr) {
        dismissBattleHintByKey(*feedback, kBattleHintKeyTutorialOverlay);
    }
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
                ? combatVoiceAssetId(battleState.party[static_cast<size_t>(event.actorPartyIndex)])
                : std::string());
        resolveBattleHintsFromActionEvent(feedback, event);

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
            const std::string assetKey = combatVoiceAssetId(battleState.party[static_cast<size_t>(partyIndex)]);
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

/**
 * @brief Advances a camera intro interpolation and updates the provided camera to its interpolated pose.
 *
 * Advances the animation timer by `deltaSeconds`, eases the progress with an out-cubic curve,
 * interpolates position, orientation (pitch/yaw), and focal length between the animation's
 * start and goal values, and deactivates the animation when the goal is reached.
 *
 * @param camera Camera instance to update with the interpolated transform.
 * @param anim   Animation state containing start/goal transforms, elapsed time, duration, and active flag.
 * @param deltaSeconds Time step, in seconds, to advance the animation.
 */
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
        camera.posX = anim.goalX;
        camera.posY = anim.goalY;
        camera.posZ = anim.goalZ;
        camera.pitchDegrees = anim.goalPitch;
        camera.yawDegrees = anim.goalYaw;
        camera.focalLength = anim.goalFocal;
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

/**
 * @brief Finds the first boss entity in a list of scene entities.
 *
 * @param entities Sequence of scene entities to search.
 * @return int Index of the first entity whose `isBoss` flag is true, or `-1` if none is found.
 */
int findBossEntityIndex(const std::vector<SceneEntity>& entities) {
    for (int i = 0; i < static_cast<int>(entities.size()); ++i) {
        if (entities[static_cast<size_t>(i)].isBoss) {
            return i;
        }
    }
    return -1;
}

std::string autoActionPhaseLabel(int phaseIndex) {
    return "Phase " + std::to_string(std::max(1, phaseIndex + 1));
}

/**
 * @brief Initializes and starts an interpolated camera intro from a start to a goal state.
 *
 * Activates `anim`, sets its elapsed time to zero, clamps the animation duration to at least 0.01s,
 * stores the start and goal camera parameters inside `anim`, and sets `camera` to the `startCamera`.
 *
 * @param startCamera Source camera state for the intro.
 * @param goalCamera Destination camera state for the intro.
 * @param durationSeconds Desired interpolation duration in seconds (clamped to >= 0.01).
 * @param camera Camera instance that will be set to the start state and subsequently driven by the animation.
 * @param anim Animation state that will be initialized and activated for the interpolation.
 */
void startCameraIntroBetween(const battle::Camera3D& startCamera,
                             const battle::Camera3D& goalCamera,
                             float durationSeconds,
                             battle::Camera3D& camera,
                             CameraIntroAnimation& anim) {
    anim.active = true;
    anim.elapsed = 0.0f;
    anim.duration = std::max(0.01f, durationSeconds);
    anim.startX = startCamera.posX;
    anim.startY = startCamera.posY;
    anim.startZ = startCamera.posZ;
    anim.startPitch = startCamera.pitchDegrees;
    anim.startYaw = startCamera.yawDegrees;
    anim.startFocal = startCamera.focalLength;
    anim.goalX = goalCamera.posX;
    anim.goalY = goalCamera.posY;
    anim.goalZ = goalCamera.posZ;
    anim.goalPitch = goalCamera.pitchDegrees;
    anim.goalYaw = goalCamera.yawDegrees;
    anim.goalFocal = goalCamera.focalLength;
    camera = startCamera;
}

/**
 * @brief Creates a camera positioned for the start of a boss-phase intro.
 *
 * Positions the camera relative to the boss entity's world coordinates using
 * the configured boss-phase intro start offsets and sets a fixed focal length.
 *
 * @param bossEntity Scene entity representing the boss; its world position is used to compute the camera origin.
 * @return battle::Camera3D Camera configured at the intro start position with the preset focal length.
 */
battle::Camera3D makeBossPhaseIntroStartCamera(const SceneEntity& bossEntity) {
    battle::Camera3D camera;
    camera.posX = bossEntity.worldX + kBossPhaseIntroStartOffsetX;
    camera.posY = bossEntity.worldY + kBossPhaseIntroStartOffsetY;
    camera.posZ = bossEntity.worldZ + kBossPhaseIntroStartOffsetZ;
    camera.focalLength = 38000.0f;
    return camera;
}

/**
 * @brief Constructs the camera positioned for the end of a boss-phase intro.
 *
 * Positions the camera relative to the given boss entity using the configured end-phase offsets and sets the focal length used for the intro shot.
 *
 * @param bossEntity Scene entity representing the boss; its worldX/worldY/worldZ are used as the reference origin for the camera offset.
 * @return battle::Camera3D Camera positioned and configured for the boss-phase intro end shot.
 */
battle::Camera3D makeBossPhaseIntroEndCamera(const SceneEntity& bossEntity) {
    battle::Camera3D camera;
    camera.posX = bossEntity.worldX + kBossPhaseIntroEndOffsetX;
    camera.posY = bossEntity.worldY + kBossPhaseIntroEndOffsetY;
    camera.posZ = bossEntity.worldZ + kBossPhaseIntroEndOffsetZ;
    camera.focalLength = 36000.0f;
    return camera;
}

/**
 * @brief Orients the camera to look toward the boss entry point used for phase intros.
 *
 * Adjusts the camera's yaw and pitch so it aims at the boss's world position offset by the
 * configured intro look offsets.
 *
 * @param camera Camera object whose `yawDegrees` and `pitchDegrees` will be updated.
 * @param bossEntity Scene entity representing the boss; its world position is the target origin for the aim.
 */
void aimCameraAtBossIntroTarget(battle::Camera3D& camera, const SceneEntity& bossEntity) {
    const float lookX = bossEntity.worldX;
    const float lookY = bossEntity.worldY + kBossPhaseIntroLookOffsetY;
    const float lookZ = bossEntity.worldZ + kBossPhaseIntroLookOffsetZ;
    const float dx = lookX - camera.posX;
    const float dy = lookY - camera.posY;
    const float dz = lookZ - camera.posZ;
    const float horizontalDist = std::sqrt((dx * dx) + (dy * dy));
    camera.yawDegrees = radiansToDegrees(std::atan2(-dx, dy));
    camera.pitchDegrees = radiansToDegrees(std::atan2(dz, std::max(1.0f, horizontalDist)));
}

} // namespace

namespace battle::app {

using ui::BattleResultOverlayAction;
using ui::BattleResultOverlayOutcome;
using ui::BattleResultOverlayState;
using ui::BattleVsIntroOverlayState;

class SessionImpl {
public:
    /**
     * @brief Initialize the battle session runtime and all required subsystems and resources.
     *
     * Initializes rendering, UI, audio, scene, battle manager, overlays, and related state using the
     * provided window and settings. On failure the function ensures partial state is cleaned up.
     *
     * @param hostWindow Native window and GL context provider used for rendering and input.
     * @param settings Live game settings used to initialize audio and UI parameters; may be null in some tests.
     * @param battleKey Key identifying which battle definition to load; when empty a default tutorial key is used.
     * @param progression Player progression data used to resolve available characters, unlocked content, and lineup defaults.
     * @param initialPartyLineup Optional explicit party lineup to use instead of deriving one from progression and the battle definition.
     * @param resultPresentation Configuration that controls end-of-battle result overlay presentation behavior.
     * @return true if initialization completed successfully and the session is ready to run, false on error.
     */
    bool initialize(Window& hostWindow,
                    GameSettings& settings,
                    const std::string& battleKey,
                    const battle::PlayerProgression& progression,
                    std::optional<std::vector<std::string>> initialPartyLineup,
                    BattleResultPresentationConfig resultPresentation) {
        shutdown();

        windowHost_ = &hostWindow;
        settings_ = &settings;
        resultPresentationConfig_ = resultPresentation;
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

        if (!manager_.initialize(battleDefinition_, activePartyLineup_, progression)) {
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
        gBattleBgmController.attach(gBgmPlayer, gBattleBgmCrossfadePlayer);
        gBattleBgmController.setMasterVolume(currentMusicMasterVolume());

        const battle::BattleState& state = manager_.getBattleState();
        battleBgmBaseVolume_ = std::clamp(manager_.getCurrentBossBgmVolume(), 0.0f, 1.0f);
        initialBattleBgmPath_.clear();
        const std::string initialBgmName = manager_.getCurrentBossBgm();
        if (!initialBgmName.empty()) {
            if (const auto bgmPath = platform::path::resolveCombatBgmPath(initialBgmName); bgmPath.has_value()) {
                initialBattleBgmPath_ = *bgmPath;
            }
        }
        worldAssets_.clear();
        for (size_t i = 0; i < state.party.size(); ++i) {
            worldAssets_.push_back(manager_.resolveCharacterAssetId(static_cast<int>(i)));
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
            narrative_.setDialogueLinePresenter([this](const vn::ScriptEntry& line) {
                presentNarrativeLine(line);
            });
            if (!narrative_.initialize(true)) {
                std::cerr << "[Battle] Failed to initialize battle narrative flow.\n";
                shutdown();
                return false;
            }
            narrativeInitialized_ = true;
        } else {
            narrative_.setDialogueLinePresenter({});
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
        activeUltimateTurnSplash_.reset();
        previewUltimateSplashPartyIndex_ = -1;
        discardNextUpdateDelta_ = false;
        activeOverlay_ = nullptr;
        resultOverlay_ = BattleResultOverlayState{};
        vsIntroOverlay_ = BattleVsIntroOverlayState{};
        vsIntroOverlay_.pendingStart = true;
        vsIntroOverlay_.leftName = "HATSUNE MIKU";
        vsIntroOverlay_.rightName = uppercase(
            !state.boss.title.empty() ? state.boss.title : state.boss.key);
        vsIntroOverlay_.leftAsset = "miku";
        vsIntroOverlay_.rightAsset = state.boss.assets.empty() ? state.boss.key : state.boss.assets;

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
        battle::app::ui::bindBattleResultOverlayControls(attachOverlayListener, {
            [this]() {
                confirmBattleResultOverlay();
            },
            [this]() {
                handleBattleResultButtonHover();
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

    /**
     * @brief Shut down the session and release all runtime resources.
     *
     * Performs a full teardown of the session: stops and detaches audio controllers, stops voice and presentation playback, shuts down RmlUi and related GL integrations, destroys renderers and blitters, closes UI documents and overlays, stops TTF/SDL image subsystems if initialized here, clears listeners and cached assets, resets all runtime state fields to their default/empty values, and disables any active interactive modes (pause, free view, presentation, tutorial, etc.).
     *
     * This function is idempotent with respect to partially-initialized subsystems and does not return a value; callers should assume the session is no longer usable after it returns.
     */
    void shutdown() {
        initialized_ = false;
        resetIdleVoicelineState();
        voiceArbiter_.stopAll([this](const game::audio::BattleVoicePlayback& playback) {
            stopBattleVoicePlayback(playback);
        });
        tutorialVoiceHandle_.reset();
        activeVnVoiceSpeakerKey_.clear();
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
        narrative_.setDialogueLinePresenter({});
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
        gBattleBgmController.stop();
        gBattleBgmController.detach();
        gOneShotAudio.shutdown();
        gOneShotAudio.cleanupFinishedPlayback();
        voiceArbiter_.clear();
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
        battleInputPrompt_ = BattleInputPromptState{};
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
        bossPhaseIntro_ = CameraIntroAnimation{};
        activeOverlay_ = nullptr;
        resultOverlay_ = BattleResultOverlayState{};
        vsIntroOverlay_ = BattleVsIntroOverlayState{};
        resultPresentationConfig_ = BattleResultPresentationConfig{};
        frameAccumulator_ = 0.0f;
        discardNextUpdateDelta_ = false;
        battleDefinition_ = battle::BattleDefinition{};
        stageDefinition_ = battle::render::StageDefinition{};
        activePartyLineup_.clear();
        initialBattleBgmPath_.clear();
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

    /**
     * @brief Process a single SDL event for the battle session, routing it to UI, overlays,
     *        input handlers, and gameplay controls.
     *
     * This method:
     * - Forwards scaled input to the RmlSDL UI context when appropriate.
     * - Handles window resize events by updating viewport and scene renderers.
     * - Routes events to pause/settings/result overlays when those are active.
     * - Blocks pausing and certain inputs during cutscenes, VS intro, combat-begin animations,
     *   and ultimate-turn splash, showing a toast when pause is attempted during those.
     * - Toggles free-view camera, adjusts free-view focal length with the mouse wheel,
     *   and snaps camera back when free-view is turned off.
     * - Handles gameplay inputs: space for player turns (starting ultimate splash, executing
     *   default turn, and processing automatic turns), E for rhythm on-beat checks,
     *   backspace for tutorial skipping, and numeric/manual ultimate hotkeys.
     * - Updates tutorial/narration progression and idle-voiceline bookkeeping.
     *
     * Side effects include modifying HUD feedback, camera staging, presentation/overlay state,
     * battle manager actions, and starting/stopping audio as necessary.
     *
     * @param event The SDL event to process.
     */
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

        if (paused_) {
            SDL_Event mutablePausedEvent = makeScaledRmlInputEvent(event);
            RmlSDL::InputEventHandler(context_, window_, mutablePausedEvent);
            handlePauseEvent(event);
            return;
        }

        if (isBattleVsIntroBlocking() || combatBeginAnimation_.isActive()) {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                showPauseBlockedHint();
            }
            return;
        }

        if (activeUltimateTurnSplash_ != nullptr) {
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    showPauseBlockedHint();
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    activeUltimateTurnSplash_->skip();
                } else {
                    (void)handleManualUltimateHotkey(event.key.keysym.sym, SDL_GetTicks64());
                }
            }
            return;
        }

        if (resultOverlay_.active) {
            SDL_Event mutableResultEvent = makeScaledRmlInputEvent(event);
            RmlSDL::InputEventHandler(context_, window_, mutableResultEvent);
            handleBattleResultEvent(event);
            return;
        }

        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE &&
            (isDialogueInProgress() || tutorialOverlay_.step != TutorialStep::None)) {
            showPauseBlockedHint();
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
                syncFinishedBattleVoiceState();
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
                    if (tutorialVoiceHandle_.has_value()) {
                        gOneShotAudio.stopPlayback(*tutorialVoiceHandle_);
                        tutorialVoiceHandle_.reset();
                    }
                    completeTutorialStep(tutorialOverlay_, &hudFeedback_);
                }
                return;
            } else if (event.key.keysym.sym == SDLK_BACKSPACE && tutorialOverlay_.step != TutorialStep::None) {
                if (tutorialVoiceHandle_.has_value()) {
                    gOneShotAudio.stopPlayback(*tutorialVoiceHandle_);
                    tutorialVoiceHandle_.reset();
                }
                skipAllTutorials(tutorialOverlay_, &hudFeedback_);
            } else if (event.key.keysym.sym == SDLK_BACKSPACE && dismissNewestManualBattleHint(hudFeedback_)) {
                return;
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
                        showWaitForAllyTurnHint();
                        return;
                    }

                    const bool allowAutoTurns =
                        !narrativeEnabled_ ||
                        !narrativeInitialized_ ||
                        narrative_.onPlayerTurnExecuted(turnExecution);
                    if (allowAutoTurns) {
                        (void)processNextAutomaticTurnWithIndicator(nowMs);
                    }
                    consumeBattleActionEvents(nowMs, &cameraStaging_);
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

    /**
     * @brief Advance the session's runtime state by a frame interval.
     *
     * Advances timers and audio controllers, processes automatic turns, narration and
     * presentation progression, HUD and feedback updates, tutorial/rhythm logic,
     * camera staging/free-view movement, scene entity updates, and transitions into
     * pause, intro, boss-phase, presentation, or battle-result overlay states as
     * dictated by current runtime conditions.
     *
     * @param deltaSeconds Frame time step in seconds.
     */
    void update(float deltaSeconds) {
        if (!initialized_ || finished_) {
            return;
        }

        const Uint64 nowMs = SDL_GetTicks64();
        tickHudFeedback(hudFeedback_, nowMs);
        gOneShotAudio.cleanupFinishedPlayback();
        gPresentationSfxAudio.cleanupFinishedPlayback();
        syncFinishedBattleVoiceState();
        gBattleBgmController.update(deltaSeconds);

        if (discardNextUpdateDelta_) {
            discardNextUpdateDelta_ = false;
            deltaSeconds = 0.0f;
        }

        advanceBattleHintTimers(deltaSeconds);
        if (tutorialOverlay_.step != TutorialStep::None) {
            refreshTutorialHintCopy(nowMs);
        }

        if (isBattleVsIntroBlocking()) {
            updateBattleVsIntro(deltaSeconds);
            refreshBattleInputPromptPreview(nowMs);
            const flow::PreviewActorContext preview = flow::inspectPreviewActor(manager_);
            const bool bossActing = !(preview.valid && preview.type == battle::ParticipantType::Character);
            const int actingPartyIndex =
                (!bossActing && preview.partyIndex >= 0) ? preview.partyIndex : -1;
            updateSceneEntities(deltaSeconds, bossActing, actingPartyIndex);
            syncHudDocument(nowMs);
            return;
        }
        if (paused_) {
            if (narrativeEnabled_ && narrativeInitialized_) {
                vn::setPaused(true);
            }
            updatePauseMenuUi(deltaSeconds);
            updateSettingsMenuUi(deltaSeconds);
            refreshBattleInputPromptPreview(nowMs);
            updateHudAnimationState(hudAnimationState_, manager_, hudFeedback_, deltaSeconds, nowMs);
            syncHudDocument(nowMs);
            return;
        }

        if (resultOverlay_.active) {
            updateBattleResultOverlay(deltaSeconds);
            refreshBattleInputPromptPreview(nowMs);
            updateHudAnimationState(hudAnimationState_, manager_, hudFeedback_, deltaSeconds, nowMs);
            syncHudDocument(nowMs);
            return;
        }

        while (const std::optional<battle::BossPhaseTransition> transition = manager_.consumeBossPhaseTransition()) {
            startBossPhaseIntro(*transition);
            startBossPhaseAutoActionIndicator(*transition, nowMs);
            battleBgmBaseVolume_ = std::clamp(manager_.getCurrentBossBgmVolume(), 0.0f, 1.0f);
            const std::string bgmName = manager_.getCurrentBossBgm();
            if (bgmName.empty()) {
                continue;
            }
            if (const auto bgmPath = platform::path::resolveCombatBgmPath(bgmName); bgmPath.has_value()) {
                gBattleBgmController.requestTrack(*bgmPath, battleBgmBaseVolume_);
            }
            const battle::BattleState& battleState = manager_.getBattleState();
            int phaseIndex = transition->toPhaseIndex;
            if (phaseIndex >= 0 && phaseIndex < static_cast<int>(battleState.boss.phases.size())) {
                const std::string& phaseVoice = battleState.boss.phases[phaseIndex].phaseChangeVoice;
                if (!phaseVoice.empty()) {
                    (void)playBossPhaseTransitionVoice(phaseVoice, currentVoiceVolume());
                }
            }
        }

        if (bossPhaseIntro_.active) {
            updateBossPhaseIntro(deltaSeconds);
            refreshBattleInputPromptPreview(nowMs);
            const flow::PreviewActorContext preview = flow::inspectPreviewActor(manager_);
            const bool bossActing = !(preview.valid && preview.type == battle::ParticipantType::Character);
            const int actingPartyIndex =
                (!bossActing && preview.partyIndex >= 0) ? preview.partyIndex : -1;
            updateSceneEntities(deltaSeconds, bossActing, actingPartyIndex);
            feedback_.syncFromManager(manager_, presentationPlaybackActive_);
            feedback_.update(deltaSeconds);
            updateHudAnimationState(hudAnimationState_, manager_, hudFeedback_, deltaSeconds, nowMs);
            syncHudDocument(nowMs);
            return;
        }

        if (combatBeginAnimation_.isActive()) {
            combatBeginAnimation_.update(deltaSeconds);
            return;
        }

        primePreviewAutoActionIndicator(nowMs);

        if (narrativeEnabled_ && narrativeInitialized_) {
            vn::setPaused(false);
            syncNarrativeSettings();
            vn::update(deltaSeconds);
            syncFinishedBattleVoiceState();
            narrative_.maybeStartBossDefeatedDialogue(manager_);
            narrative_.handleAutomaticProgression(
                manager_,
                [this, nowMs](battle::BattleManager&) {
                    return processNextAutomaticTurnWithIndicator(nowMs);
                });
        } else {
            (void)processNextAutomaticTurnWithIndicator(nowMs);
        }

        consumeBattleActionEvents(nowMs, &cameraStaging_);
        feedback_.syncFromManager(manager_, presentationPlaybackActive_);
        feedback_.update(deltaSeconds);
        maybeStartTutorial(nowMs);
        if (tutorialEnabled_ && tutorialOverlay_.step != TutorialStep::None &&
            !tutorialOverlay_.audioPlayed && !tutorialOverlay_.entry.voice.empty()) {
            const std::string speakerKey = resolveScriptVoiceSpeakerKey(tutorialOverlay_.entry);
            std::optional<game::audio::WavOneShotPlayer::PlaybackHandle> startedHandle;
            if (requestBattleVoicePath(speakerKey,
                                       game::audio::BattleVoiceKind::Dialogue,
                                       tutorialOverlay_.entry.voice,
                                       currentVoiceVolume(),
                                       &startedHandle)) {
                tutorialVoiceHandle_ = startedHandle;
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

        refreshBattleInputPromptPreview(nowMs);
        updateHudAnimationState(hudAnimationState_, manager_, hudFeedback_, deltaSeconds, nowMs);
        syncHudDocument(nowMs);

        if (manager_.isBattleOver() && !dialogueActive && !presentationPlaybackActive_ &&
            tutorialOverlay_.step == TutorialStep::None &&
            !rhythmChallenge_.active && activeUltimateTurnSplash_ == nullptr &&
            isBossDeathFadeComplete() && !isBattleFinishBlocked()) {
            enterBattleResultOverlay(outcome(), nowMs);
        }
    }

    /**
     * @brief Renders a single battle frame including world, presentation layers, HUD/feedback, and overlays.
     *
     * Performs a full-frame composition using the OpenGL scene renderer and SDL compatibility surfaces:
     * it selects the focused entity for the snapshot, applies stage and world rendering, integrates presentation
     * content either natively or via compatibility blits, renders HUD/feedback and presentation overlays (ultimate
     * splash, VN/dialogue), updates the RmlUi frame, and records optional frame timing metrics.
     */
    void render() {
        if (!initialized_ || sceneRenderer_.renderer == nullptr || sceneRenderer_.surface == nullptr) {
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

    /**
     * @brief Accesses the currently active party lineup.
     *
     * @return const std::vector<std::string>& Reference to the active party lineup where each element is a party member asset key in lineup order.
     */
    const std::vector<std::string>& currentPartyLineup() const {
        return activePartyLineup_;
    }

    /**
     * @brief Builds a post-battle summary populated from the battle manager's telemetry.
     *
     * Populates an internal cache with overall telemetry (total action value consumed) and per-character
     * analytics (character key, title, and total damage when available) and returns that cached summary.
     *
     * @return const battle::postbattle::Summary& Reference to the internal cached summary populated from
     * the manager's telemetry. The reference remains valid until the next call to this function or until
     * the owning object is destroyed.
     */
    const battle::postbattle::Summary& postBattleSummary() {
        postBattleSummaryCache_ = battle::postbattle::Summary{};
        const battle::BattleTelemetry& telemetry = manager_.getBattleTelemetry();
        postBattleSummaryCache_.totalActionValueConsumed = telemetry.totalActionValueConsumed;

        const battle::BattleState& battleState = manager_.getBattleState();
        postBattleSummaryCache_.characters.reserve(battleState.party.size());
        for (const battle::CharacterDefinition& character : battleState.party) {
            battle::postbattle::CharacterAnalytics analytics;
            analytics.key = character.key;
            analytics.title = character.title;
            if (const auto damageIt = telemetry.characterDamageByKey.find(character.key);
                damageIt != telemetry.characterDamageByKey.end()) {
                analytics.totalDamage = damageIt->second;
            }
            postBattleSummaryCache_.characters.push_back(std::move(analytics));
        }

        return postBattleSummaryCache_;
    }

private:
    /**
     * Determines whether transitioning to the battle result overlay should be blocked
     * because the boss-death voice clip is still playing.
     *
     * @return `true` if the boss has zero HP and a configured boss-dead voice clip is
     * currently playing, `false` otherwise.
     */
    bool isBattleFinishBlocked() const {
        if (manager_.getBossCurrentHp() > 0) {
            return false;
        }

        const auto bossDeadVoicePath = resolveBossDeadVoicePath(manager_.getBattleState());
        return bossDeadVoicePath.has_value() && gOneShotAudio.isPlaying(*bossDeadVoicePath);
    }

    /**
     * @brief Indicates whether the VS intro overlay is pending or active and therefore blocks normal battle progression.
     *
     * @return `true` if the VS intro overlay is pending to start or currently active, `false` otherwise.
     */
    bool isBattleVsIntroBlocking() const {
        return vsIntroOverlay_.pendingStart || vsIntroOverlay_.active;
    }

    /**
     * @brief Initialize and activate the VS-intro overlay state.
     *
     * Prepares the VS intro sequence by clearing the pending start flag, enabling
     * the overlay, resetting all per-sound playback markers and letter indices,
     * and zeroing the presentation elapsed timer.
     */
    void startBattleVsIntro() {
        vsIntroOverlay_.pendingStart = false;
        vsIntroOverlay_.active = true;
        vsIntroOverlay_.lineDropSfxPlayed = false;
        vsIntroOverlay_.lineTiltSfxPlayed = false;
        vsIntroOverlay_.leftCardSfxPlayed = false;
        vsIntroOverlay_.rightCardSfxPlayed = false;
        vsIntroOverlay_.vsVHitSfxPlayed = false;
        vsIntroOverlay_.vsSHitSfxPlayed = false;
        vsIntroOverlay_.matchStartSfxPlayed = false;
        vsIntroOverlay_.bgmStarted = false;
        vsIntroOverlay_.nextLeftLetterSfxIndex = 0;
        vsIntroOverlay_.nextRightLetterSfxIndex = 0;
        vsIntroOverlay_.presentationElapsedSeconds = 0.0f;
    }

    /**
     * @brief Advances the VS-intro overlay timeline and triggers its timed effects.
     *
     * Progresses the VS-intro overlay by up to a bounded step, plays staged one-shot SFX and background
     * music at their configured reveal times, emits per-letter SFX as names are revealed, and completes
     * the intro by starting any pending narrative intro and deactivating the overlay.
     */
    void updateBattleVsIntro(float deltaSeconds) {
        if (vsIntroOverlay_.pendingStart) {
            if (!loadingOverlayState_.visible) {
                startBattleVsIntro();
            }
            return;
        }

        if (!vsIntroOverlay_.active) {
            return;
        }

        vsIntroOverlay_.presentationElapsedSeconds +=
            std::clamp(deltaSeconds, 0.0f, ui::kBattleVsIntroMaxStepSeconds);

        const float elapsedSeconds = ui::battleVsIntroElapsedSeconds(vsIntroOverlay_);
        if (!vsIntroOverlay_.lineDropSfxPlayed && elapsedSeconds >= ui::kBattleVsIntroLineDropSeconds) {
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroLineDropSfxPath, 0.80f, false);
            vsIntroOverlay_.lineDropSfxPlayed = true;
        }
        if (!vsIntroOverlay_.lineTiltSfxPlayed && elapsedSeconds >= ui::kBattleVsIntroLineTiltSeconds) {
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroLineTiltSfxPath, 0.86f, false);
            vsIntroOverlay_.lineTiltSfxPlayed = true;
        }
        if (!vsIntroOverlay_.leftCardSfxPlayed && elapsedSeconds >= ui::kBattleVsIntroLeftCardSeconds) {
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroLeftCardSfxPath, 0.82f, false);
            vsIntroOverlay_.leftCardSfxPlayed = true;
        }
        if (!vsIntroOverlay_.rightCardSfxPlayed && elapsedSeconds >= ui::kBattleVsIntroRightCardSeconds) {
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroRightCardSfxPath, 0.82f, false);
            vsIntroOverlay_.rightCardSfxPlayed = true;
        }

        const int visibleLeftLetters = ui::battleVsIntroVisibleLetterCount(
            vsIntroOverlay_.leftName,
            elapsedSeconds,
            ui::kBattleVsIntroLeftNameSeconds);
        while (vsIntroOverlay_.nextLeftLetterSfxIndex < visibleLeftLetters) {
            const std::size_t sfxIndex =
                static_cast<std::size_t>(vsIntroOverlay_.nextLeftLetterSfxIndex) % kBattleVsIntroLetterSfxPaths.size();
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroLetterSfxPaths[sfxIndex], 0.46f, false);
            ++vsIntroOverlay_.nextLeftLetterSfxIndex;
        }

        if (!vsIntroOverlay_.vsVHitSfxPlayed && elapsedSeconds >= ui::kBattleVsIntroVsVSeconds) {
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroVsHitSfxPath, 0.86f, false);
            vsIntroOverlay_.vsVHitSfxPlayed = true;
        }

        if (!vsIntroOverlay_.vsSHitSfxPlayed && elapsedSeconds >= ui::kBattleVsIntroVsSSeconds) {
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroVsHitSfxPath, 0.88f, false);
            vsIntroOverlay_.vsSHitSfxPlayed = true;
        }

        const int visibleRightLetters = ui::battleVsIntroVisibleLetterCount(
            vsIntroOverlay_.rightName,
            elapsedSeconds,
            ui::kBattleVsIntroRightNameSeconds);
        while (vsIntroOverlay_.nextRightLetterSfxIndex < visibleRightLetters) {
            const std::size_t sfxIndex =
                static_cast<std::size_t>(vsIntroOverlay_.nextRightLetterSfxIndex) % kBattleVsIntroLetterSfxPaths.size();
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroLetterSfxPaths[sfxIndex], 0.46f, false);
            ++vsIntroOverlay_.nextRightLetterSfxIndex;
        }

        if (!vsIntroOverlay_.matchStartSfxPlayed && elapsedSeconds >= ui::kBattleVsIntroExitSeconds) {
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroMatchStartSfxPathA, 0.85f, false);
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleVsIntroMatchStartSfxPathB, 0.92f, false);
            vsIntroOverlay_.matchStartSfxPlayed = true;
        }

        if (!vsIntroOverlay_.bgmStarted && elapsedSeconds >= ui::kBattleVsIntroRevealSeconds) {
            if (!initialBattleBgmPath_.empty()) {
                gBattleBgmController.playWithFadeIn(
                    initialBattleBgmPath_,
                    battleBgmBaseVolume_,
                    kBattleVsIntroBgmFadeInSeconds);
            }
            vsIntroOverlay_.bgmStarted = true;
        }

        if (elapsedSeconds >= ui::kBattleVsIntroCompletionSeconds) {
            if (narrativeEnabled_ && narrativeInitialized_) {
                narrative_.startIntroDialogueIfPending();
            }
            vsIntroOverlay_.active = false;
            vsIntroOverlay_.pendingStart = false;
        }
    }

    /**
     * @brief Reports whether a narrative dialogue sequence is currently active.
     *
     * @return `true` if narrative support is enabled, initialized, and a dialogue is in progress; `false` otherwise.
     */
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

    /**
     * @brief Gets the effective master music volume used for background music.
     *
     * The returned value is clamped to the range 0.0 to 1.0. If no settings are available, this returns 1.0.
     *
     * @return float Effective master music volume in the range 0.0 to 1.0.
     */
    float currentMusicMasterVolume() const {
        return settings_ != nullptr ? std::clamp(settings_->musicVolume, 0.0f, 1.0f) : 1.0f;
    }

    /**
     * @brief Starts the boss-phase intro camera sequence for a boss phase transition.
     *
     * If no boss entity is present the function is a no-op.
     *
     * @param transition Describes the boss phase change and the target phase to focus.
     */
    void startBossPhaseIntro(const battle::BossPhaseTransition& transition) {
        const int bossEntityIndex = findBossEntityIndex(entities_);
        if (bossEntityIndex < 0 || static_cast<size_t>(bossEntityIndex) >= entities_.size()) {
            return;
        }

        const SceneEntity& bossEntity = entities_[static_cast<size_t>(bossEntityIndex)];
        battle::Camera3D startCamera = makeBossPhaseIntroStartCamera(bossEntity);
        battle::Camera3D endCamera = makeBossPhaseIntroEndCamera(bossEntity);
        aimCameraAtBossIntroTarget(startCamera, bossEntity);
        aimCameraAtBossIntroTarget(endCamera, bossEntity);
        startCameraIntroBetween(
            startCamera,
            endCamera,
            kBossPhaseIntroDurationSeconds,
            camera_,
            bossPhaseIntro_);
    }

    /**
     * @brief Advances the boss-phase intro sequence and updates the camera.
     *
     * Advances the boss phase intro animation by the given frame delta, updates the active
     * camera transform toward the intro goal, and deactivates the intro when the animation completes.
     *
     * @param deltaSeconds Time elapsed since the previous update, in seconds.
     * @note This function mutates the internal camera state and the `bossPhaseIntro_` active/elapsed state. */
    void updateBossPhaseIntro(float deltaSeconds) {
        if (!bossPhaseIntro_.active) {
            return;
        }

        bossPhaseIntro_.elapsed += deltaSeconds;
        const float t = battle::easing::clamp01(
            bossPhaseIntro_.elapsed / std::max(0.001f, bossPhaseIntro_.duration));
        const float eased = battle::easing::easeOutQuint(t);
        camera_.posX = battle::easing::lerp(bossPhaseIntro_.startX, bossPhaseIntro_.goalX, eased);
        camera_.posY = battle::easing::lerp(bossPhaseIntro_.startY, bossPhaseIntro_.goalY, eased);
        camera_.posZ = battle::easing::lerp(bossPhaseIntro_.startZ, bossPhaseIntro_.goalZ, eased);
        camera_.pitchDegrees =
            battle::easing::lerp(bossPhaseIntro_.startPitch, bossPhaseIntro_.goalPitch, eased);
        camera_.yawDegrees =
            battle::easing::lerp(bossPhaseIntro_.startYaw, bossPhaseIntro_.goalYaw, eased);
        camera_.focalLength =
            battle::easing::lerp(bossPhaseIntro_.startFocal, bossPhaseIntro_.goalFocal, eased);
        if (t >= 1.0f) {
            bossPhaseIntro_.active = false;
            camera_.posX = bossPhaseIntro_.goalX;
            camera_.posY = bossPhaseIntro_.goalY;
            camera_.posZ = bossPhaseIntro_.goalZ;
            camera_.pitchDegrees = bossPhaseIntro_.goalPitch;
            camera_.yawDegrees = bossPhaseIntro_.goalYaw;
            camera_.focalLength = bossPhaseIntro_.goalFocal;
        }
    }

    /**
     * @brief Get the current master voice volume used for playback.
     *
     * @return float The voice volume (configured value from settings); `1.0f` if settings are unavailable.
     */
    float currentVoiceVolume() const {
        return settings_ != nullptr ? settings_->voiceVolume : 1.0f;
    }

    std::string characterVoiceSpeakerKey(int partyIndex) const {
        const battle::BattleState& battleState = manager_.getBattleState();
        if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= battleState.party.size()) {
            return std::string();
        }
        return combatVoiceSpeakerKey(battleState.party[static_cast<size_t>(partyIndex)]);
    }

    std::string bossVoiceSpeakerKey() const {
        return combatVoiceSpeakerKey(manager_.getBattleState().boss);
    }

    std::string presentationCasterVoiceSpeakerKey(const battle::PresentationContext& context) const {
        return context.isBoss ? bossVoiceSpeakerKey() : characterVoiceSpeakerKey(context.casterIndex);
    }

    std::string resolveScriptVoiceSpeakerKey(const vn::ScriptEntry& entry) const {
        return inferScriptSpeakerKey(entry, manager_.getBattleState());
    }

    void startAutoActionIndicator(const std::string& assetName,
                                  const std::string& labelText,
                                  bool isBoss,
                                  Uint64 nowMs) {
        if (labelText.empty()) {
            hudFeedback_.autoActionIndicator = ui::AutoActionIndicatorState{};
            return;
        }

        hudFeedback_.autoActionIndicator.active = true;
        hudFeedback_.autoActionIndicator.isBoss = isBoss;
        hudFeedback_.autoActionIndicator.startedMs = nowMs;
        hudFeedback_.autoActionIndicator.enterDurationMs = ui::kAutoActionIndicatorEnterDurationMs;
        hudFeedback_.autoActionIndicator.holdDurationMs = ui::kAutoActionIndicatorHoldDurationMs;
        hudFeedback_.autoActionIndicator.exitDurationMs = ui::kAutoActionIndicatorExitDurationMs;
        hudFeedback_.autoActionIndicator.assetName = assetName;
        hudFeedback_.autoActionIndicator.labelText = labelText;
    }

    std::string resolvePreviewAutoActionIndicatorLabel() const {
        const battle::flow::PreviewAbilityContext previewAbility = battle::flow::inspectPreviewAbility(manager_);
        if (!previewAbility.valid ||
            previewAbility.actorType != battle::ParticipantType::Character ||
            previewAbility.abilityId.empty()) {
            return std::string();
        }

        const battle::AbilityDefinition* abilityDef = manager_.findAbilityDefinition(previewAbility.abilityId);
        if (abilityDef == nullptr) {
            return previewAbility.abilityId;
        }
        return !abilityDef->name.empty() ? abilityDef->name : abilityDef->id;
    }

    bool maybeStartPreviewAutoActionIndicator(Uint64 nowMs) {
        const battle::flow::PreviewActorContext preview = battle::flow::inspectPreviewActor(manager_);
        if (!preview.valid ||
            preview.type != battle::ParticipantType::Character ||
            !preview.isExtraTurn ||
            !preview.autoExecute ||
            preview.extraTurnAction == battle::BattleAction::Ultimate) {
            return false;
        }

        const battle::BattleState& battleState = manager_.getBattleState();
        if (preview.partyIndex < 0 || static_cast<size_t>(preview.partyIndex) >= battleState.party.size()) {
            return false;
        }
        if (!manager_.isCharacterAlive(preview.partyIndex)) {
            return false;
        }

        const std::string labelText = resolvePreviewAutoActionIndicatorLabel();
        if (labelText.empty()) {
            return false;
        }

        const std::string assetId = manager_.resolveCharacterAssetId(preview.partyIndex, preview.abilityKitOverride);
        startAutoActionIndicator(assetId.empty() ? battleState.party[static_cast<size_t>(preview.partyIndex)].key : assetId,
                                 labelText,
                                 false,
                                 nowMs);
        return true;
    }

    void startBossPhaseAutoActionIndicator(const battle::BossPhaseTransition& transition, Uint64 nowMs) {
        const battle::BossDefinition& boss = manager_.getBattleState().boss;
        startAutoActionIndicator(boss.assets.empty() ? boss.key : boss.assets,
                                 autoActionPhaseLabel(transition.toPhaseIndex),
                                 true,
                                 nowMs);
    }

    bool processNextAutomaticTurnWithIndicator(Uint64 nowMs) {
        (void)maybeStartPreviewAutoActionIndicator(nowMs);
        return manager_.processNextAutomaticTurn();
    }

    void primePreviewAutoActionIndicator(Uint64 nowMs) {
        if (paused_ ||
            bossPhaseIntro_.active ||
            resultOverlay_.active ||
            presentationPlaybackActive_ ||
            activeUltimateTurnSplash_ != nullptr ||
            isDialogueInProgress()) {
            return;
        }

        (void)maybeStartPreviewAutoActionIndicator(nowMs);
    }

    bool shouldCancelIdleForVoiceKind(game::audio::BattleVoiceKind kind) const {
        switch (kind) {
            case game::audio::BattleVoiceKind::Dialogue:
            case game::audio::BattleVoiceKind::UltimateActivation:
            case game::audio::BattleVoiceKind::Ultimate:
            case game::audio::BattleVoiceKind::PhaseTransition:
            case game::audio::BattleVoiceKind::Ability:
            case game::audio::BattleVoiceKind::Revived:
            case game::audio::BattleVoiceKind::Dead:
                return true;
            case game::audio::BattleVoiceKind::Hit:
            case game::audio::BattleVoiceKind::Healed:
            case game::audio::BattleVoiceKind::Shielded:
            case game::audio::BattleVoiceKind::Idle:
            default:
                return false;
        }
    }

    void syncFinishedBattleVoiceState() {
        if (!activeVnVoiceSpeakerKey_.empty() && !vn::isVoicePlaying()) {
            activeVnVoiceSpeakerKey_.clear();
        }
        if (tutorialVoiceHandle_.has_value() && !gOneShotAudio.isPlaying(*tutorialVoiceHandle_)) {
            tutorialVoiceHandle_.reset();
        }
    }

    bool isBattleVoicePlaybackActive(const game::audio::BattleVoicePlayback& playback) const {
        switch (playback.channel) {
            case game::audio::BattleVoiceChannel::OneShot:
                return gOneShotAudio.isPlaying(
                    game::audio::WavOneShotPlayer::PlaybackHandle{playback.handleId});
            case game::audio::BattleVoiceChannel::Vn:
                return !activeVnVoiceSpeakerKey_.empty() &&
                    activeVnVoiceSpeakerKey_ == playback.speakerKey &&
                    vn::isVoicePlaying();
            default:
                return false;
        }
    }

    void stopBattleVoicePlayback(const game::audio::BattleVoicePlayback& playback) {
        switch (playback.channel) {
            case game::audio::BattleVoiceChannel::OneShot:
                gOneShotAudio.stopPlayback(game::audio::WavOneShotPlayer::PlaybackHandle{playback.handleId});
                break;
            case game::audio::BattleVoiceChannel::Vn:
                vn::stopVoicePlayback();
                if (activeVnVoiceSpeakerKey_ == playback.speakerKey) {
                    activeVnVoiceSpeakerKey_.clear();
                }
                break;
            default:
                break;
        }
    }

    bool requestBattleVoice(const std::string& speakerKey,
                            game::audio::BattleVoiceKind kind,
                            game::audio::BattleVoiceChannel channel,
                            const std::optional<std::string>& resolvedPath,
                            float volume,
                            std::optional<game::audio::WavOneShotPlayer::PlaybackHandle>* startedHandle = nullptr) {
        syncFinishedBattleVoiceState();
        if (shouldCancelIdleForVoiceKind(kind)) {
            stopIdleVoicelinePlayback();
        }

        std::optional<game::audio::WavOneShotPlayer::PlaybackHandle> localHandle;
        const bool played = voiceArbiter_.request(
            {speakerKey, kind, channel},
            {
                [this](const game::audio::BattleVoicePlayback& playback) {
                    return isBattleVoicePlaybackActive(playback);
                },
                [this](const game::audio::BattleVoicePlayback& playback) {
                    stopBattleVoicePlayback(playback);
                },
                [&]() -> std::optional<std::uint64_t> {
                    if (channel != game::audio::BattleVoiceChannel::OneShot ||
                        !resolvedPath.has_value() ||
                        resolvedPath->empty() ||
                        !std::filesystem::exists(*resolvedPath)) {
                        return std::nullopt;
                    }

                    localHandle = gOneShotAudio.playTrackedWavOneShot(*resolvedPath, volume, false);
                    if (!localHandle.has_value()) {
                        return std::nullopt;
                    }
                    return localHandle->id;
                }
            });

        if (startedHandle != nullptr) {
            *startedHandle = localHandle;
        }
        return played;
    }

    bool requestBattleVoicePath(const std::string& speakerKey,
                                game::audio::BattleVoiceKind kind,
                                const std::string& path,
                                float volume,
                                std::optional<game::audio::WavOneShotPlayer::PlaybackHandle>* startedHandle = nullptr) {
        if (path.empty()) {
            return false;
        }
        const std::string resolved = platform::path::resolvePath(path);
        if (!std::filesystem::exists(resolved)) {
            return false;
        }
        return requestBattleVoice(
            speakerKey,
            kind,
            game::audio::BattleVoiceChannel::OneShot,
            resolved,
            volume,
            startedHandle);
    }

    bool requestCombatVoiceClip(const std::string& speakerKey,
                                const std::string& assetName,
                                game::audio::BattleVoiceKind kind,
                                float volume,
                                std::initializer_list<const char*> clipNames,
                                std::optional<game::audio::WavOneShotPlayer::PlaybackHandle>* startedHandle = nullptr) {
        const std::optional<std::string> resolvedPath = resolveCombatVoiceClipPath(assetName, clipNames);
        if (!resolvedPath.has_value()) {
            return false;
        }
        return requestBattleVoice(
            speakerKey,
            kind,
            game::audio::BattleVoiceChannel::OneShot,
            *resolvedPath,
            volume,
            startedHandle);
    }

    bool playBossHitVoice(float voiceVolume) {
        const battle::BattleState& battleState = manager_.getBattleState();
        const std::string speakerKey = bossVoiceSpeakerKey();
        if (requestBattleVoicePath(speakerKey, game::audio::BattleVoiceKind::Hit, battleState.boss.voiceHit, voiceVolume)) {
            return true;
        }
        if (requestCombatVoiceClip(speakerKey, battleState.boss.assets, game::audio::BattleVoiceKind::Hit, voiceVolume, {"hit"})) {
            return true;
        }
        return requestCombatVoiceClip(speakerKey, battleState.boss.key, game::audio::BattleVoiceKind::Hit, voiceVolume, {"hit"});
    }

    bool playBossPhaseTransitionVoice(const std::string& voicePath, float voiceVolume) {
        if (voicePath.empty()) {
            return false;
        }
        return requestBattleVoicePath(
            bossVoiceSpeakerKey(),
            game::audio::BattleVoiceKind::PhaseTransition,
            voicePath,
            voiceVolume);
    }

    bool playBossHealedVoice(float voiceVolume) {
        const battle::BattleState& battleState = manager_.getBattleState();
        const std::string speakerKey = bossVoiceSpeakerKey();
        if (const auto healedVoice = resolveBossHealedVoicePath(battleState); healedVoice.has_value()) {
            return requestBattleVoice(speakerKey,
                                      game::audio::BattleVoiceKind::Healed,
                                      game::audio::BattleVoiceChannel::OneShot,
                                      *healedVoice,
                                      voiceVolume);
        }
        return false;
    }

    bool playPartyVoiceClip(int partyIndex,
                            game::audio::BattleVoiceKind kind,
                            float volume,
                            std::initializer_list<const char*> clipNames) {
        const battle::BattleState& battleState = manager_.getBattleState();
        if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= battleState.party.size()) {
            return false;
        }
        const battle::CharacterDefinition& character = battleState.party[static_cast<size_t>(partyIndex)];
        return requestCombatVoiceClip(combatVoiceSpeakerKey(character),
                                      combatVoiceAssetId(character),
                                      kind,
                                      volume,
                                      clipNames);
    }

    bool lockPartySpeakerState(int partyIndex, game::audio::BattleVoiceKind kind) {
        const battle::BattleState& battleState = manager_.getBattleState();
        if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= battleState.party.size()) {
            return false;
        }
        return requestBattleVoice(
            combatVoiceSpeakerKey(battleState.party[static_cast<size_t>(partyIndex)]),
            kind,
            game::audio::BattleVoiceChannel::OneShot,
            std::nullopt,
            currentVoiceVolume());
    }

    bool lockBossSpeakerState(game::audio::BattleVoiceKind kind) {
        return requestBattleVoice(
            bossVoiceSpeakerKey(),
            kind,
            game::audio::BattleVoiceChannel::OneShot,
            std::nullopt,
            currentVoiceVolume());
    }

    void presentNarrativeLine(const vn::ScriptEntry& line) {
        const std::string speakerName = vn::getDisplaySpeakerName(line);
        const std::string iconPath = line.icon.empty() ? std::string{} : platform::path::resolvePath(line.icon);
        const std::string backgroundPath = line.background.empty()
            ? std::string{}
            : (vn::isHexColorString(line.background) ? line.background : platform::path::resolvePath(line.background));
        const std::string voicePath = line.voice.empty() ? std::string{} : platform::path::resolvePath(line.voice);
        const std::string bgmPath = line.bgm.empty() ? std::string{} : platform::path::resolvePath(line.bgm);
        const std::string fontPath = line.fontPath.empty() ? std::string{} : platform::path::resolvePath(line.fontPath);
        const std::string speakerKey = resolveScriptVoiceSpeakerKey(line);

        bool allowVoice = false;
        if (!voicePath.empty() && std::filesystem::exists(voicePath)) {
            syncFinishedBattleVoiceState();
            if (shouldCancelIdleForVoiceKind(game::audio::BattleVoiceKind::Dialogue)) {
                stopIdleVoicelinePlayback();
            }
            allowVoice = voiceArbiter_.request(
                {speakerKey, game::audio::BattleVoiceKind::Dialogue, game::audio::BattleVoiceChannel::Vn},
                {
                    [this](const game::audio::BattleVoicePlayback& playback) {
                        return isBattleVoicePlaybackActive(playback);
                    },
                    [this](const game::audio::BattleVoicePlayback& playback) {
                        stopBattleVoicePlayback(playback);
                    },
                    []() -> std::optional<std::uint64_t> {
                        return 1;
                    }
                });
        }

        vn::showLine(
            line.text,
            speakerName,
            iconPath,
            allowVoice ? voicePath : std::string{},
            fontPath,
            line.autoAdvanceOnVoiceEnd,
            line.iconFrameCount,
            line.iconFps,
            backgroundPath,
            bgmPath,
            line.bgmVolume,
            line.bgmStop,
            line.bgmPause
        );

        activeVnVoiceSpeakerKey_ = allowVoice ? speakerKey : std::string();
    }

    void consumeBattleActionEvents(Uint64 nowMs,
                                   battle::render::BattleCameraStaging* cameraStaging = nullptr) {
        const battle::BattleState& battleState = manager_.getBattleState();
        const float voiceVolume = currentVoiceVolume();
        for (const battle::BattleActionEvent& event : manager_.getRecentActionEvents()) {
            if (cameraStaging != nullptr && event.actorType == battle::ParticipantType::Boss) {
                cameraStaging->queueCharacterTurnIntro();
            }

            resolveBattleHintsFromActionEvent(hudFeedback_, event);

            const bool actorIsBoss = event.actorType == battle::ParticipantType::Boss;
            const std::string actorSpeakerKey = actorIsBoss
                ? bossVoiceSpeakerKey()
                : characterVoiceSpeakerKey(event.actorPartyIndex);
            const std::string actorAssetName = actorIsBoss
                ? battleState.boss.assets
                : ((event.actorPartyIndex >= 0 &&
                    static_cast<size_t>(event.actorPartyIndex) < battleState.party.size())
                    ? combatVoiceAssetId(battleState.party[static_cast<size_t>(event.actorPartyIndex)])
                    : std::string());

            if (!event.abilityVoicesHandledDuringPresentation && event.action == battle::BattleAction::Skill) {
                if (!requestCombatVoiceClip(actorSpeakerKey,
                                            actorAssetName,
                                            game::audio::BattleVoiceKind::Ability,
                                            voiceVolume,
                                            {"ability", "skill"}) &&
                    actorIsBoss) {
                    (void)requestCombatVoiceClip(actorSpeakerKey,
                                                 battleState.boss.key,
                                                 game::audio::BattleVoiceKind::Ability,
                                                 voiceVolume,
                                                 {"ability", "skill"});
                }
            } else if (!event.abilityVoicesHandledDuringPresentation && event.action == battle::BattleAction::Ultimate) {
                if (!requestCombatVoiceClip(actorSpeakerKey,
                                            actorAssetName,
                                            game::audio::BattleVoiceKind::Ultimate,
                                            voiceVolume,
                                            {"ultimate", "ability"}) &&
                    actorIsBoss) {
                    (void)requestCombatVoiceClip(actorSpeakerKey,
                                                 battleState.boss.key,
                                                 game::audio::BattleVoiceKind::Ultimate,
                                                 voiceVolume,
                                                 {"ultimate", "ability"});
                }
            }

            if (event.bossHpAfter < event.bossHpBefore) {
                markBossHit(hudFeedback_, nowMs);
            } else if (event.bossHpAfter > event.bossHpBefore) {
                (void)playBossHealedVoice(voiceVolume);
            }

            if (!event.hitVoicesHandledDuringPresentation && event.bossHpAfter < event.bossHpBefore) {
                if (event.bossHpBefore > 0 && event.bossHpAfter <= 0) {
                    const auto deadVoice = resolveBossDeadVoicePath(battleState);
                    if (deadVoice.has_value()) {
                        (void)requestBattleVoice(
                            bossVoiceSpeakerKey(),
                            game::audio::BattleVoiceKind::Dead,
                            game::audio::BattleVoiceChannel::OneShot,
                            *deadVoice,
                            voiceVolume);
                    } else {
                        (void)lockBossSpeakerState(game::audio::BattleVoiceKind::Dead);
                    }
                } else {
                    (void)playBossHitVoice(voiceVolume);
                }
            }

            if (event.hitVoicesHandledDuringPresentation) {
                for (size_t i = 0; i < event.targetPartyIndices.size() &&
                                   i < event.targetHpBefore.size() &&
                                   i < event.targetHpAfter.size(); ++i) {
                    if (event.targetHpAfter[i] < event.targetHpBefore[i]) {
                        markUnitHit(hudFeedback_, event.targetPartyIndices[i], nowMs);
                    }
                }
            }

            const std::size_t targetCount = std::min(
                {event.targetPartyIndices.size(),
                 event.targetHpBefore.size(),
                 event.targetHpAfter.size(),
                 event.targetShieldBefore.size(),
                 event.targetShieldAfter.size()});
            for (std::size_t i = 0; i < targetCount; ++i) {
                const int partyIndex = event.targetPartyIndices[i];
                if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= battleState.party.size()) {
                    continue;
                }

                const int hpBefore = event.targetHpBefore[i];
                const int hpAfter = event.targetHpAfter[i];
                const int shieldBefore = event.targetShieldBefore[i];
                const int shieldAfter = event.targetShieldAfter[i];

                if (hpAfter < hpBefore) {
                    if (event.hitVoicesHandledDuringPresentation) {
                        continue;
                    }

                    markUnitHit(hudFeedback_, partyIndex, nowMs);
                    if (hpBefore > 0 && hpAfter <= 0) {
                        if (!playPartyVoiceClip(partyIndex, game::audio::BattleVoiceKind::Dead, voiceVolume, {"dead"})) {
                            (void)lockPartySpeakerState(partyIndex, game::audio::BattleVoiceKind::Dead);
                        }
                    } else if (hpAfter > 0) {
                        (void)playPartyVoiceClip(partyIndex, game::audio::BattleVoiceKind::Hit, voiceVolume, {"hit"});
                    }
                    continue;
                }

                if (hpBefore <= 0 && hpAfter > 0) {
                    if (!playPartyVoiceClip(partyIndex, game::audio::BattleVoiceKind::Revived, voiceVolume, {"revived"})) {
                        (void)lockPartySpeakerState(partyIndex, game::audio::BattleVoiceKind::Revived);
                    }
                    continue;
                }

                if (hpAfter > hpBefore) {
                    (void)playPartyVoiceClip(partyIndex, game::audio::BattleVoiceKind::Healed, voiceVolume, {"healed"});
                    continue;
                }

                if (shieldAfter > shieldBefore) {
                    (void)playPartyVoiceClip(partyIndex, game::audio::BattleVoiceKind::Shielded, voiceVolume, {"shielded"});
                }
            }
        }

        manager_.clearRecentActionEvents();
    }

    bool areBattleHintTimeoutsSuppressed() const {
        return paused_ || resultOverlay_.active || isBattleVsIntroBlocking();
    }

    void advanceBattleHintTimers(float deltaSeconds) {
        const Uint64 elapsedMs = static_cast<Uint64>(std::lround(std::max(0.0f, deltaSeconds) * 1000.0f));
        if (elapsedMs == 0) {
            return;
        }

        tickBattleHints(hudFeedback_, elapsedMs, areBattleHintTimeoutsSuppressed());
    }

    std::string nextTransientBattleHintKey(const char* prefix) {
        return std::string(prefix) + "_" + std::to_string(nextTransientBattleHintId_++);
    }

    std::string tutorialHintSourceTag() const {
        switch (tutorialOverlay_.step) {
            case TutorialStep::Standard:
                return "BASIC ACTION";
            case TutorialStep::Skill:
                return "SKILL ACTION";
            case TutorialStep::Ultimate:
                return "ULTIMATE ACTION";
            case TutorialStep::None:
            default:
                return "COMBAT GUIDE";
        }
    }

    void enqueueBattleHint(BattleHintRequest request, bool playDefaultSfx = true) {
        if (request.showSfxPath.empty() && playDefaultSfx) {
            request.showSfxPath = defaultBattleHintSfxPath(request.family);
        }
        const BattleHintFamily family = request.family;
        const std::string sfxPath = request.showSfxPath;
        const std::string voicePath = request.voicePath;
        upsertBattleHint(hudFeedback_, std::move(request));

        if (!sfxPath.empty()) {
            BattleHintRequest audioRequest;
            audioRequest.family = family;
            audioRequest.showSfxPath = sfxPath;
            playBattleHintSfx(audioRequest);
        }
        if (!voicePath.empty()) {
            (void)gOneShotAudio.playWavOneShot(
                platform::path::resolvePath(voicePath),
                currentVoiceVolume(),
                false);
        }
    }

    void showPauseBlockedHint() {
        BattleHintRequest request;
        request.stableKey = kBattleHintKeyPauseBlocked;
        request.family = BattleHintFamily::Warning;
        request.kicker = "CUTSCENE LOCK";
        request.sourceTag = "BATTLE FLOW";
        request.badgeText = "WAIT";
        request.message = "CANNOT PAUSE DURING A CUTSCENE.";
        request.resolveRule.kind = BattleHintResolveKind::Timeout;
        request.resolveRule.durationMs = 1800;
        enqueueBattleHint(std::move(request));
    }

    void showWaitForAllyTurnHint() {
        BattleHintRequest request;
        request.stableKey = kBattleHintKeyWaitForAllyTurn;
        request.family = BattleHintFamily::Warning;
        request.kicker = "TURN ORDER";
        request.sourceTag = "PLAYER ACTION";
        request.badgeText = "WAIT";
        request.message = "WAIT FOR AN ALLY TURN.";
        request.resolveRule.kind = BattleHintResolveKind::Timeout;
        request.resolveRule.durationMs = 1800;
        enqueueBattleHint(std::move(request));
    }

    void dismissPresentationHint(bool immediate = false) {
        dismissBattleHintByKey(hudFeedback_, kBattleHintKeyPresentationInstruction, immediate);
    }

    void showPresentationHint(const battle::AbilityDefinition* abilityDef, bool isBoss) {
        if (abilityDef == nullptr || abilityDef->instructionHint.empty()) {
            dismissPresentationHint(true);
            return;
        }

        BattleHintRequest request;
        request.stableKey = kBattleHintKeyPresentationInstruction;
        request.family = isBoss ? BattleHintFamily::Warning : BattleHintFamily::Info;
        request.message = abilityDef->instructionHint;
        request.resolveRule.kind = BattleHintResolveKind::PresentationEnd;
        enqueueBattleHint(std::move(request));
    }

    void dismissTutorialHint(bool immediate = false) {
        dismissBattleHintByKey(hudFeedback_, kBattleHintKeyTutorialOverlay, immediate);
    }

    bool battleInputPromptTypeUsesFollowUpKeys(battle::InputPromptType type) const {
        switch (type) {
            case battle::InputPromptType::Wild:
            case battle::InputPromptType::Custom:
            case battle::InputPromptType::Arrows:
            case battle::InputPromptType::UpDown:
            case battle::InputPromptType::LeftRight:
            case battle::InputPromptType::SpamSpace:
                return true;
            case battle::InputPromptType::Space:
            case battle::InputPromptType::None:
            default:
                return false;
        }
    }

    std::vector<std::string> battleInputPromptFollowUpKeys(const battle::AbilityDefinition& abilityDef) const {
        switch (abilityDef.inputPromptType) {
            case battle::InputPromptType::Wild:
                return {"Q", "W", "E", "R", "A"};
            case battle::InputPromptType::Custom:
                return abilityDef.inputPromptKeys;
            case battle::InputPromptType::Arrows:
                return {"<", "^", "V", ">"};
            case battle::InputPromptType::UpDown:
                return {"^", "V"};
            case battle::InputPromptType::LeftRight:
                return {"<", ">"};
            case battle::InputPromptType::SpamSpace:
            case battle::InputPromptType::Space:
                return {"SPACE"};
            case battle::InputPromptType::None:
            default:
                return {};
        }
    }

    BattleInputPromptState makeBattleInputPromptState(const battle::AbilityDefinition& abilityDef,
                                                      bool showPrimaryKey,
                                                      bool showFollowUpKeys) const {
        BattleInputPromptState prompt;
        prompt.type = abilityDef.inputPromptType;
        if (prompt.type == battle::InputPromptType::None) {
            return prompt;
        }

        prompt.abilityId = abilityDef.id;
        prompt.showPrimaryKey = showPrimaryKey;
        prompt.primaryLabel = "SPACE";
        std::vector<std::string> followUpKeys = battleInputPromptFollowUpKeys(abilityDef);

        if (prompt.type == battle::InputPromptType::Custom && followUpKeys.empty()) {
            prompt.type = battle::InputPromptType::Space;
            if (!showPrimaryKey) {
                followUpKeys = {"SPACE"};
            }
        }

        if (showFollowUpKeys) {
            prompt.followUpKeys = std::move(followUpKeys);
        }

        prompt.visible = prompt.showPrimaryKey || !prompt.followUpKeys.empty();
        return prompt;
    }

    bool shouldPreserveBattleInputPromptTiming(const BattleInputPromptState& nextPrompt) const {
        return battleInputPrompt_.visible &&
            nextPrompt.visible &&
            battleInputPrompt_.type == nextPrompt.type &&
            battleInputPrompt_.abilityId == nextPrompt.abilityId &&
            battleInputPrompt_.showPrimaryKey == nextPrompt.showPrimaryKey &&
            battleInputPrompt_.primaryLabel == nextPrompt.primaryLabel &&
            battleInputPrompt_.followUpKeys == nextPrompt.followUpKeys;
    }

    void setBattleInputPromptState(BattleInputPromptState prompt,
                                   Uint64 nowMs,
                                   bool preserveTimingIfSame) {
        if (!prompt.visible) {
            battleInputPrompt_ = BattleInputPromptState{};
            return;
        }

        if (preserveTimingIfSame && shouldPreserveBattleInputPromptTiming(prompt)) {
            prompt.startedMs = battleInputPrompt_.startedMs;
        } else {
            prompt.startedMs = nowMs;
        }
        battleInputPrompt_ = std::move(prompt);
    }

    const battle::AbilityDefinition* resolveBattlePreviewPromptAbility() const {
        const battle::flow::PreviewAbilityContext previewAbility = battle::flow::inspectPreviewAbility(manager_);
        if (!previewAbility.valid || previewAbility.actorType != battle::ParticipantType::Character ||
            previewAbility.abilityId.empty()) {
            return nullptr;
        }
        return manager_.findAbilityDefinition(previewAbility.abilityId);
    }

    void showBattlePreviewPrompt(const battle::AbilityDefinition* abilityDef, Uint64 nowMs) {
        if (abilityDef == nullptr) {
            battleInputPrompt_ = BattleInputPromptState{};
            return;
        }

        setBattleInputPromptState(
            makeBattleInputPromptState(
                *abilityDef,
                true,
                battleInputPromptTypeUsesFollowUpKeys(abilityDef->inputPromptType)),
            nowMs,
            true);
    }

    void showBattlePresentationPrompt(const battle::AbilityDefinition* abilityDef,
                                      bool isBoss,
                                      Uint64 nowMs) {
        if (abilityDef == nullptr) {
            battleInputPrompt_ = BattleInputPromptState{};
            return;
        }

        const bool showFollowUpKeys = isBoss
            ? abilityDef->inputPromptType != battle::InputPromptType::None
            : battleInputPromptTypeUsesFollowUpKeys(abilityDef->inputPromptType);
        setBattleInputPromptState(
            makeBattleInputPromptState(*abilityDef, false, showFollowUpKeys),
            nowMs,
            false);
    }

    void clearBattleInputPrompt() {
        battleInputPrompt_ = BattleInputPromptState{};
    }

    void refreshBattleInputPromptPreview(Uint64 nowMs) {
        if (presentationPlaybackActive_ ||
            paused_ ||
            resultOverlay_.active ||
            isBattleVsIntroBlocking() ||
            bossPhaseIntro_.active ||
            combatBeginAnimation_.isActive() ||
            manager_.isBattleOver()) {
            clearBattleInputPrompt();
            return;
        }

        showBattlePreviewPrompt(resolveBattlePreviewPromptAbility(), nowMs);
    }

    void showTutorialHint(Uint64 nowMs) {
        if (tutorialOverlay_.step == TutorialStep::None) {
            dismissTutorialHint();
            return;
        }

        BattleHintRequest request;
        request.stableKey = kBattleHintKeyTutorialOverlay;
        request.family = BattleHintFamily::Tutorial;
        request.kicker = vn::getDisplaySpeakerName(tutorialOverlay_.entry);
        request.sourceTag = tutorialHintSourceTag();
        request.badgeText = "GUIDE";
        request.message = tutorialOverlay_.entry.text;
        request.dismissLabel = "BACKSPACE SKIP";
        request.manualDismissAllowed = true;
        request.resolveRule.kind = BattleHintResolveKind::TutorialStepComplete;
        request.resolveRule.tutorialStep = tutorialOverlay_.step;
        enqueueBattleHint(std::move(request));
        refreshTutorialHintCopy(nowMs);
    }

    void refreshTutorialHintCopy(Uint64 nowMs) {
        if (tutorialOverlay_.step == TutorialStep::None) {
            return;
        }

        BattleHintInstance* hint = findBattleHint(hudFeedback_, kBattleHintKeyTutorialOverlay);
        if (hint == nullptr) {
            return;
        }

        hint->request.kicker = vn::getDisplaySpeakerName(tutorialOverlay_.entry);
        hint->request.sourceTag = tutorialHintSourceTag();
        if (hint->request.message != tutorialOverlay_.entry.text) {
            hint->request.message = tutorialOverlay_.entry.text;
            hint->measurementDirty = true;
        }
        hint->visibleMessage = revealNarrationText(
            tutorialOverlay_.entry.text,
            tutorialOverlay_.startedMs,
            nowMs,
            settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond);
    }

    /**
     * @brief Executes a sequence of presentation audio commands, applying SFX, looped presentation audio, and BGM control.
     *
     * Processes each command in order and performs the corresponding audio action: play one-shot SFX (optionally allowing overlap),
     * start or stop a presentation audio loop (using the session's loop player and master-volume scaling), stop all presentation SFX,
     * and pause or resume the battle BGM controller.
     *
     * @param context Presentation execution context (currently unused; provided for API symmetry).
     * @param commands Ordered list of audio commands to execute. Each command's `type`, `id`, and `volume` fields determine the action.
     */
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
                case battle::PresentationAudioCommandType::PlayVoiceOneShot:
                    if (!requestBattleVoicePath(
                            presentationCasterVoiceSpeakerKey(context),
                            game::audio::BattleVoiceKind::Ability,
                            command.id,
                            std::clamp(command.volume, 0.0f, 1.0f) * currentVoiceVolume())) {
                        (void)playResolvedOneShot(
                            gPresentationSfxAudio,
                            command.id,
                            command.volume,
                            false
                        );
                    }
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
                    gBattleBgmController.pauseWithFade();
                    break;
                case battle::PresentationAudioCommandType::ResumeBgm:
                    gBattleBgmController.resumeWithFade();
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
        const std::string speakerKey = presentationCasterVoiceSpeakerKey(context);
        if (context.isBoss && context.presentationId == "qr_code_attack") {
            for (int i = 0; i < cueCount; ++i) {
                (void)requestCombatVoiceClip(speakerKey,
                                             battleState.boss.key,
                                             game::audio::BattleVoiceKind::Ability,
                                             voiceVolume,
                                             {"ability"});
            }
            manager_.markPresentationAbilityAudioPlayed();
            return;
        }

        if (context.isBoss) {
            const std::string audioSequenceId = speakerKey + "|" + context.presentationId;
            if (presentationAudioSequenceId_ != audioSequenceId) {
                presentationAudioSequenceId_ = audioSequenceId;
                presentationAudioCueIndex_ = 0;
            }

            for (int i = 0; i < cueCount; ++i) {
                ++presentationAudioCueIndex_;
                if (requestCombatVoiceClip(speakerKey,
                                           battleState.boss.key,
                                           game::audio::BattleVoiceKind::Ability,
                                           voiceVolume,
                                           {"ability"})) {
                    continue;
                }
                (void)requestCombatVoiceClip(speakerKey,
                                             battleState.boss.assets,
                                             game::audio::BattleVoiceKind::Ability,
                                             voiceVolume,
                                             {"ability"});
            }

            manager_.markPresentationAbilityAudioPlayed();
            return;
        }

        const std::string casterAssetName = getPresentationCasterVoiceKey(context, manager_);
        const std::string audioSequenceId = speakerKey + "|" + context.presentationId;
        if (presentationAudioSequenceId_ != audioSequenceId) {
            presentationAudioSequenceId_ = audioSequenceId;
            presentationAudioCueIndex_ = 0;
        }

        for (int i = 0; i < cueCount; ++i) {
            ++presentationAudioCueIndex_;

            std::string clipName = "ability";
            if (casterAssetName == "cupcakke" && context.presentationId == "drum_attack") {
                clipName = (presentationAudioCueIndex_ == 1) ? "ability" : "ability2";
            } else if (casterAssetName == "cupcakke" && context.presentationId == "niagara_falls_ultimate") {
                clipName = (presentationAudioCueIndex_ == 1) ? "ultimate" : "ability2";
            } else if (context.isUltimate) {
                clipName = "ultimate";
            }

            const game::audio::BattleVoiceKind kind =
                clipName == "ultimate"
                ? game::audio::BattleVoiceKind::Ultimate
                : game::audio::BattleVoiceKind::Ability;
            if (requestCombatVoiceClip(speakerKey, casterAssetName, kind, voiceVolume, {clipName.c_str()})) {
                continue;
            }

            if ((clipName == "ultimate" || clipName == "ability2") &&
                requestCombatVoiceClip(speakerKey,
                                       casterAssetName,
                                       game::audio::BattleVoiceKind::Ability,
                                       voiceVolume,
                                       {"ability"})) {
                continue;
            }
        }

        manager_.markPresentationAbilityAudioPlayed();
    }

    void handlePresentationHealAudio(const battle::PresentationContext& context, int hitEvents) {
        (void)context;
        (void)hitEvents;
    }

    /**
     * @brief Plays appropriate hit/heal/death voice audio and updates HUD and manager state for a presentation's hit events.
     *
     * This function triggers voice one-shots (character/boss hit, dead, healed, or special ability clips as applicable),
     * updates HUD feedback (unit/boss hit markers), and notifies the battle manager that presentation hit audio has been played.
     *
     * @param context Presentation context describing the caster and presentation metadata.
     * @param hitEvents Number of hit audio events to play (used to repeat hit clips); non-positive values cause no audio to be played.
     * @param targetPartyIndex Index of the targeted party member, or -1 to target all party members.
     * @param bossHpBefore Boss HP value before the presentation ability was applied.
     * @param partyHpBefore Vector of party members' HP values before the presentation ability was applied; indexed by party position.
     */
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
                (void)repeatCount;
                if (partyIndex < 0 || partyIndex >= static_cast<int>(battleState.party.size()) ||
                    static_cast<size_t>(partyIndex) >= partyHpBefore.size()) {
                    return;
                }

                const int nowHp = manager_.getCharacterCurrentHp(partyIndex);
                const int prevHp = partyHpBefore[static_cast<size_t>(partyIndex)];
                const bool diedNow = prevHp > 0 && nowHp <= 0;
                if (diedNow) {
                    if (!playPartyVoiceClip(partyIndex, game::audio::BattleVoiceKind::Dead, voiceVolume, {"dead"})) {
                        (void)lockPartySpeakerState(partyIndex, game::audio::BattleVoiceKind::Dead);
                    }
                    return;
                }

                if (nowHp <= 0 && !diedNow) {
                    return;
                }

                (void)playPartyVoiceClip(partyIndex, game::audio::BattleVoiceKind::Hit, voiceVolume, {"hit"});
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

        const int nowBossHp = manager_.getBossCurrentHp();
        if (manager_.playerDamageHealsBoss() && nowBossHp > bossHpBefore) {
            (void)playBossHealedVoice(voiceVolume);
            manager_.markPresentationHitAudioPlayed();
            return;
        }
        if (nowBossHp < bossHpBefore) {
            markBossHit(hudFeedback_, SDL_GetTicks64());
        }
        const bool bossDiedNow = bossHpBefore > 0 && nowBossHp <= 0;
        if (bossDiedNow) {
            if (const auto bossDeadVoice = resolveBossDeadVoicePath(battleState); bossDeadVoice.has_value()) {
                (void)requestBattleVoice(
                    bossVoiceSpeakerKey(),
                    game::audio::BattleVoiceKind::Dead,
                    game::audio::BattleVoiceChannel::OneShot,
                    *bossDeadVoice,
                    voiceVolume);
            } else {
                (void)lockBossSpeakerState(game::audio::BattleVoiceKind::Dead);
            }
            manager_.markPresentationHitAudioPlayed();
            return;
        }

        (void)playBossHitVoice(voiceVolume);
        manager_.markPresentationHitAudioPlayed();
    }

    /**
     * @brief Produce the short reward text shown after a presentation ability resolves.
     *
     * Constructs a concise label describing the reward/effect provided by a presentation feedback
     * event. If `feedback.rewardText` is non-empty that text is returned verbatim. Otherwise a
     * string is derived from the ability type and the feedback multiplier (for example "+20% DMG",
     * "-30% DMG TAKEN", "+15% HEAL", "+50% SHIELD", "+10% ATK").
     *
     * @param context Presentation context; used to determine boss-vs-character semantics.
     * @param abilityDef Pointer to the ability definition driving the presentation; if null and
     *                   no explicit `feedback.rewardText` is provided, an empty string is returned.
     * @param feedback Presentation feedback containing an optional explicit rewardText and a
     *                 numeric multiplier used to derive percent-based labels.
     * @return std::string The reward label to display, or an empty string if no label can be derived.
     */
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
                if (abilityDef->damageBuff != 0) {
                    const int damagePercent = static_cast<int>(std::lround(
                        static_cast<float>(abilityDef->damageBuff) * feedback.multiplier
                    ));
                    return formatSignedPercent(damagePercent, "DMG");
                }
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

    /**
     * @brief Applies a resolved presentation feedback event to game state and HUD.
     *
     * If the feedback is valid, classifies its judgement, derives any reward text,
     * applies the feedback to the battle manager, displays the judgement on the HUD,
     * and plays the corresponding judgement sound effect.
     *
     * @param context Presentation context containing metadata for the feedback (e.g., whether the caster is the boss).
     * @param abilityDef Pointer to the ability definition associated with the presentation, or `nullptr` if none.
     * @param feedback The presentation feedback event to apply; no action is taken if `feedback.valid()` is false.
     */
    void applyPresentationFeedbackEvent(const battle::PresentationContext& context,
                                        const battle::AbilityDefinition* abilityDef,
                                        const battle::PresentationFeedbackEvent& feedback) {
        if (!feedback.valid()) {
            return;
        }

        const battle::CombatJudgement judgement = battle::classifyCombatJudgement(feedback.signal);
        const std::string rewardText = resolvePresentationRewardText(context, abilityDef, feedback);
        manager_.applyPresentationFeedback(context.isBoss, feedback);
        syncHudFeedbackState(hudFeedback_, manager_);
        showJudgement(hudFeedback_, judgement, rewardText, SDL_GetTicks64());
        playJudgementSfx(judgement);
    }

    /**
     * @brief Runs an ability presentation and applies its resulting effects to the session.
     *
     * Executes the presentation runtime for the provided context, updating scene entities,
     * camera staging, HUD/feedback state, audio playback, and manager-side effects produced
     * by the presentation. Clears transient overlay state when the presentation completes.
     *
     * @param context PresentationContext describing the presentation id, ability id,
     *                caster/target indices and whether the caster is the boss.
     * @return float Multiplier produced by the presentation (used to scale subsequent effects). 
     */
    float runPresentationInteraction(const battle::PresentationContext& context) {
        if (!initialized_ || finished_) {
            clearBattleInputPrompt();
            return 1.0f;
        }
        if (context.presentationId.empty()) {
            clearBattleInputPrompt();
            return 1.0f;
        }

        const battle::AbilityDefinition* abilityDef = manager_.findAbilityDefinition(context.abilityId);
        const Uint64 promptStartedMs = SDL_GetTicks64();
        beginPresentationDamageTracking(hudFeedback_);
        showBattlePresentationPrompt(abilityDef, context.isBoss, promptStartedMs);
        showPresentationHint(abilityDef, context.isBoss);
        syncHudDocument(promptStartedMs);

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
        {
            const battle::BattleState& state = manager_.getBattleState();
            callbacks.partyTargetableStates.reserve(state.party.size());
            for (std::size_t i = 0; i < state.party.size(); ++i) {
                callbacks.partyTargetableStates.push_back(
                    manager_.isCharacterAlive(static_cast<int>(i))
                );
            }
        }
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
            const bool handlesAbilityVoice = std::any_of(
                commands.begin(),
                commands.end(),
                [](const battle::PresentationAudioCommand& command) {
                    return command.type == battle::PresentationAudioCommandType::PlayVoiceOneShot;
                }
            );
            if (handlesAbilityVoice) {
                manager_.markPresentationAbilityAudioPlayed();
            }
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
                const int casterMaxHp = context.isBoss
                    ? manager_.getBossMaxHp()
                    : manager_.getCharacterMaxHp(context.casterIndex);
                const int totalHeal = battle::ability::resolveSupportAmount(
                    *abilityDef,
                    casterMaxHp,
                    healMultiplier,
                    abilityDef->flatHeal
                );
                const int perHitHeal = totalHeal / std::max(1, hitEvents);
                const int targetPartyIndex =
                    (abilityDef->targetRule == battle::TargetRule::SingleAlly && activePresentation_ != nullptr)
                    ? activePresentation_->getFocusedPartyIndex()
                    : -1;
                const int totalRequestedHealing = manager_.applyPresentationHealing(
                    context.isBoss,
                    perHitHeal,
                    hitEvents,
                    abilityDef->targetRule,
                    targetPartyIndex,
                    abilityDef->reviveDeadAllies,
                    abilityDef->specialDamageSource != battle::SpecialDamageSource::StoredHealingTally
                );
                feedback_.queuePresentationHealFeedback(
                    context.isBoss,
                    hitEvents,
                    perHitHeal,
                    manager_,
                    targetPartyIndex
                );
                handlePresentationHealAudio(context, hitEvents);

                int specialDamage = 0;
                if (!context.isBoss && abilityDef->specialDamageSource == battle::SpecialDamageSource::AppliedHeal) {
                    specialDamage = manager_.applyConvertedPlayerSpecialDamageToBoss(
                        totalRequestedHealing,
                        abilityDef->multiplier,
                        true,
                        context.casterIndex
                    );
                } else if (!context.isBoss &&
                           abilityDef->specialDamageSource == battle::SpecialDamageSource::StoredHealingTally) {
                    specialDamage = manager_.applyConvertedPlayerSpecialDamageToBoss(
                        manager_.consumeTetoHealingTally(),
                        abilityDef->multiplier,
                        true,
                        context.casterIndex
                    );
                }

                if (specialDamage > 0) {
                    feedback_.queuePresentationHitFeedback(
                        context.isBoss,
                        1,
                        specialDamage,
                        manager_,
                        -1
                    );
                }
                return;
            }

            const int baseAtk = context.isBoss
                ? manager_.getBossEffectiveAtk()
                : manager_.getCharacterEffectiveAtk(context.casterIndex);

            const int bossHpBefore = manager_.getBossCurrentHp();
            std::vector<int> partyHpBefore;
            const battle::BattleState& state = manager_.getBattleState();
            partyHpBefore.reserve(state.party.size());
            for (size_t i = 0; i < state.party.size(); ++i) {
                partyHpBefore.push_back(manager_.getCharacterCurrentHp(static_cast<int>(i)));
            }

            if (battle::ability::isTeamShieldBurstUltimate(*abilityDef)) {
                const int totalDamage = manager_.applyCurrentTeamShieldDamageToBoss(
                    true,
                    abilityDef->multiplier,
                    context.casterIndex
                );
                if (totalDamage > 0) {
                    handlePresentationHitAudio(
                        context,
                        hitEvents,
                        -1,
                        bossHpBefore,
                        partyHpBefore
                    );
                    const int spawnedDamagePopups = feedback_.queuePresentationHitFeedback(
                        context.isBoss,
                        hitEvents,
                        totalDamage,
                        manager_,
                        -1
                    );
                    registerPresentationDamage(
                        hudFeedback_,
                        totalDamage * std::max(0, spawnedDamagePopups),
                        SDL_GetTicks64());
                }
                return;
            }

            const float hitDamageMultiplier = activePresentation_ != nullptr
                ? std::max(0.0f, activePresentation_->consumeHitDamageMultiplier())
                : 1.0f;
            const float damageBuffMultiplier = context.isBoss
                ? 1.0f
                : manager_.getCharacterDamageBuffMultiplier(context.casterIndex);
            const float comboMultiplier = context.isBoss
                ? 1.0f
                : battle::comboDamageMultiplier(manager_.getComboState().comboCount);
            const float totalDamageRaw =
                static_cast<float>(baseAtk) *
                abilityDef->multiplier *
                hitDamageMultiplier *
                damageBuffMultiplier *
                comboMultiplier;
            const int totalDamage = std::max(1, static_cast<int>(totalDamageRaw));
            const int perHitDamage = std::max(1, totalDamage / std::max(1, damageLabelHitCount));
            const int presentationTargetPartyIndex = context.isBoss ? context.targetIndex : -1;

            manager_.applyPresentationHitDamage(
                context.isBoss,
                perHitDamage,
                hitEvents,
                presentationTargetPartyIndex,
                context.casterIndex
            );
            handlePresentationHitAudio(
                context,
                hitEvents,
                presentationTargetPartyIndex,
                bossHpBefore,
                partyHpBefore
            );
            const int spawnedDamagePopups = feedback_.queuePresentationHitFeedback(
                context.isBoss,
                hitEvents,
                perHitDamage,
                manager_,
                presentationTargetPartyIndex
            );
            registerPresentationDamage(
                hudFeedback_,
                perHitDamage * std::max(0, spawnedDamagePopups),
                SDL_GetTicks64());
        };
        bool consumedPresentationFeedback = false;
        callbacks.onPostUpdate = [this, &context, abilityDef, &consumedPresentationFeedback](float deltaSeconds) {
            const bool useCenteredPartyLayout =
                activePresentation_ != nullptr &&
                activePresentation_->shouldUseCenteredPartyLayout();
            const bool bossActingLayout = context.isBoss || useCenteredPartyLayout;
            const int actingPartyIndex = bossActingLayout ? -1 : context.casterIndex;
            const Uint64 nowMs = SDL_GetTicks64();

            // Presentation playback runs in its own update loop, so battle-music fades
            // need an explicit tick here or they will stall until the cut-in ends.
            gBattleBgmController.update(deltaSeconds);

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
            advanceBattleHintTimers(deltaSeconds);
            if (tutorialOverlay_.step != TutorialStep::None) {
                refreshTutorialHintCopy(nowMs);
            }
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
        callbacks.onPauseBlocked = [this]() {
            const Uint64 nowMs = SDL_GetTicks64();
            showPauseBlockedHint();
            syncHudDocument(nowMs);
        };
        callbacks.onSplashArtStart = [this, &context]() {
            const std::string speakerKey = presentationCasterVoiceSpeakerKey(context);
            const std::string assetName = getPresentationCasterVoiceKey(context, manager_);
            if (!requestCombatVoiceClip(speakerKey,
                                        assetName,
                                        game::audio::BattleVoiceKind::UltimateActivation,
                                        currentVoiceVolume(),
                                        {"ready", "special"}) &&
                context.isBoss) {
                (void)requestCombatVoiceClip(speakerKey,
                                             manager_.getBattleState().boss.key,
                                             game::audio::BattleVoiceKind::UltimateActivation,
                                             currentVoiceVolume(),
                                             {"ready", "special"});
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
        if (!context.isBoss && context.abilityId == "LoveAndBeautyShock") {
            manager_.addSailorVenusSpaceTally(context.casterIndex, result.scoreValue);
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
            std::cout << "[Battle] Applied presentation feedback signal (mode=" << (int)result.feedbackSignal.mode << ")\n";
        } else {
            std::cout << "[Battle] No feedback signal or already consumed (valid=" << result.feedbackSignal.valid() << ", consumed=" << consumedPresentationFeedback << ")\n";
        }

        finishPresentationDamageTracking(hudFeedback_, SDL_GetTicks64());
        clearBattleInputPrompt();
        refreshBattleInputPromptPreview(SDL_GetTicks64());
        dismissBattleHintsByResolveKind(hudFeedback_, BattleHintResolveKind::PresentationEnd);
        syncHudDocument(SDL_GetTicks64());

        discardNextUpdateDelta_ = true;
        return result.multiplier;
    }

    /**
     * @brief Loads the UI fonts used by the battle HUD into RmlUi if present.
     *
     * Attempts to load the project's preferred font files and a system fallback;
     * succeeds if at least one usable font is found and registered.
     *
     * @return `true` if one or more fonts were loaded, `false` otherwise.
     */
    bool loadFonts() {
        bool anyLoaded = false;
        anyLoaded |= loadRmlFontIfPresent(platform::path::resolvePath("assets/fonts/SpaceMono-Regular.ttf"));
        anyLoaded |= loadRmlFontIfPresent(platform::path::resolvePath("assets/fonts/SpaceMono-Bold.ttf"));
        anyLoaded |= loadRmlFontIfPresent(platform::path::findFontPath(), true);
        anyLoaded |= loadRmlFontIfPresent(platform::path::resolvePath("assets/fonts/NotoSansCJK-Regular.ttc"), true);
        anyLoaded |= loadRmlFontIfPresent(platform::path::resolvePath("assets/fonts/NotoSansCJK-Bold.ttc"), true);
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

    /**
     * @brief Synchronizes HUD feedback state and updates the battle HUD Rml document.
     *
     * Updates internal HUD feedback from the battle manager and refreshes the visible HUD
     * document so time-based animations, expirations, and overlay states reflect the
     * current runtime state.
     *
     * @param nowMs Current time in milliseconds used to drive HUD animations and expirations.
     */
    void syncHudDocument(Uint64 nowMs) {
        syncHudFeedbackState(hudFeedback_, manager_);
        battle::app::ui::updateBattleHudDocument(document_,
                                                 manager_,
                                                 hudAnimationState_,
                                                 hudFeedback_,
                                                 resultOverlay_,
                                                 vsIntroOverlay_,
                                                 tutorialOverlay_,
                                                 battleInputPrompt_,
                                                 rhythmChallenge_,
                                                 paused_,
                                                 pauseOverlayMode_,
                                                 pauseSelection_,
                                                 settingsSelection_,
                                                 settings_,
                                                 settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond,
                                                 nowMs,
                                                 makeBattleHudDocumentDependencies());
    }

    /**
     * @brief Constructs a snapshot of the current frame state for rendering.
     *
     * @param focusedIndex Index of the focused entity in the session's entities vector.
     * @return battle::BattleSessionCore::BattleFrameSnapshot Snapshot populated with the current camera, stage pointer, entity list, focused entity index, frame accumulator, presentation flags, active presentation/splash pointers, feedback anchors, and per-entity shake offsets.
     */
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

    bool ensureWorldAssetAvailable(const std::string& assetName) {
        if (assetName.empty() ||
            std::find(worldAssets_.begin(), worldAssets_.end(), assetName) != worldAssets_.end()) {
            return true;
        }

        worldAssets_.push_back(assetName);
        return refreshSceneRenderers();
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
            const std::string assetId = manager_.resolveCharacterAssetId(static_cast<int>(partyIndex));
            entities_.push_back(SceneEntity{
                character.key,
                assetId,
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
        for (SceneEntity& entity : entities_) {
            if (entity.isBoss || entity.partyIndex < 0) {
                continue;
            }

            const std::string assetId = manager_.resolveCharacterAssetId(entity.partyIndex);
            if (!ensureWorldAssetAvailable(assetId)) {
                finished_ = true;
                return;
            }
            entity.assetName = assetId;
        }

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

        const std::string abilityId = manager_.resolveCharacterAbilityId(
            preview.partyIndex,
            preview.extraTurnAction,
            preview.abilityKitOverride
        );
        if (abilityId.empty()) {
            return;
        }
        const battle::AbilityDefinition* abilityDef = manager_.findAbilityDefinition(abilityId);

        battle::SplashArtConfig cfg;
        cfg.abilityName = abilityDef != nullptr ? abilityDef->name : abilityId;
        const std::string assetId = manager_.resolveCharacterAssetId(preview.partyIndex, preview.abilityKitOverride);
        if (!ensureWorldAssetAvailable(assetId)) {
            finished_ = true;
            return;
        }
        cfg.sprite = resolveOverlayTextureByAsset(assetId);
        if (cfg.sprite == nullptr) {
            cfg.sprite = resolveSceneTextureByAsset(assetId);
        }

        activeUltimateTurnSplash_ = std::make_unique<battle::SplashArtAnimation>(cfg);
        activeUltimateTurnSplash_->start();
        previewUltimateSplashPartyIndex_ = preview.partyIndex;
    }

    /**
     * @brief Determines whether the boss death fade-out has completed.
     *
     * The fade is considered complete if the boss still has HP, if there is no boss entity,
     * or if the boss entity is marked hidden or effectively fully transparent.
     *
     * @return true if the boss is alive, no boss entity is present, or the boss entity is hidden or has near-zero alpha; false otherwise.
     */
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

    /**
     * @brief Get the delay before the battle result confirmation button becomes interactive.
     *
     * @return float Time in seconds before the result overlay's confirm button is enabled, derived from the current result overlay state.
     */
    float battleResultButtonInteractiveTimeSeconds() const {
        return battle::app::ui::battleResultButtonInteractiveSeconds(resultOverlay_);
    }

    /**
     * @brief Initialize and display the battle result overlay for the given outcome.
     *
     * Sets up internal result overlay state (texts, button labels, subcopy, action) based
     * on the resolved outcome and the configured `resultPresentationConfig_`, begins a
     * BGM fade-out, and synchronizes the HUD document to reflect the newly active overlay.
     *
     * @param outcome The final battle outcome to present (victory, defeat, etc.).
     * @param nowMs Current timestamp in milliseconds used to stamp overlay start times and timers.
     */
    void enterBattleResultOverlay(BattleOutcome outcome, Uint64 nowMs) {
        if (resultOverlay_.active || outcome == BattleOutcome::None) {
            return;
        }

        resultOverlay_ = BattleResultOverlayState{};
        resultOverlay_.active = true;
        resultOverlay_.startedMs = nowMs;
        resultOverlay_.outcome = outcome == BattleOutcome::Victory
            ? BattleResultOverlayOutcome::Victory
            : BattleResultOverlayOutcome::Defeat;
        resultOverlay_.action =
            outcome == BattleOutcome::Defeat &&
            resultPresentationConfig_.defeatAction == BattleDefeatResultAction::RestartStory
                ? BattleResultOverlayAction::RestartStory
                : BattleResultOverlayAction::Continue;
        resultOverlay_.word = outcome == BattleOutcome::Victory ? "VICTORY" : "FAILED";
        resultOverlay_.buttonLabel =
            resultOverlay_.action == BattleResultOverlayAction::RestartStory ? "RESTART STORY" : "CONTINUE";
        if (outcome == BattleOutcome::Victory) {
            resultOverlay_.buttonSubcopy = "Advance to the next story beat.";
        } else if (resultOverlay_.action == BattleResultOverlayAction::RestartStory) {
            resultOverlay_.buttonSubcopy = "Relaunch this chapter from the beginning.";
        } else if (resultPresentationConfig_.defeatAction == BattleDefeatResultAction::ContinueStory) {
            resultOverlay_.buttonSubcopy = "Continue into the scripted defeat scene.";
        } else {
            resultOverlay_.buttonSubcopy = "Return to the post-battle flow.";
        }

        gBattleBgmController.fadeOutAndStop(2.1f);
        syncHudDocument(nowMs);
    }

    /**
     * @brief Advances and processes the battle result overlay presentation.
     *
     * Advances the overlay's internal presentation timer by up to a capped step, triggers letter-by-letter
     * sound effects as the result text is revealed, plays the reveal and victory-applause sounds at the
     * configured times, and enables input when the overlay becomes interactive.
     *
     * @param deltaSeconds Frame time to advance the overlay presentation, clamped to a maximum step.
     */
    void updateBattleResultOverlay(float deltaSeconds) {
        if (!resultOverlay_.active) {
            return;
        }

        resultOverlay_.presentationElapsedSeconds +=
            std::clamp(deltaSeconds, 0.0f, kBattleResultMaxPresentationStepSeconds);

        const float elapsedSeconds = battle::app::ui::battleResultElapsedSeconds(resultOverlay_);
        const int visibleLetters = battle::app::ui::battleResultVisibleLetterCount(resultOverlay_);
        while (resultOverlay_.nextLetterSfxIndex < visibleLetters) {
            const std::size_t soundIndex =
                static_cast<std::size_t>(resultOverlay_.nextLetterSfxIndex) % kBattleResultLetterSfxPaths.size();
            (void)playResolvedOneShot(
                gPresentationSfxAudio,
                kBattleResultLetterSfxPaths[soundIndex],
                0.46f,
                false);
            ++resultOverlay_.nextLetterSfxIndex;
        }

        const float revealSeconds = battle::app::ui::battleResultRevealImpactSeconds(resultOverlay_);
        if (!resultOverlay_.revealSfxPlayed && elapsedSeconds >= revealSeconds) {
            const char* revealPath = resultOverlay_.outcome == BattleResultOverlayOutcome::Victory
                ? kBattleResultVictoryRevealSfxPath
                : kBattleResultDefeatRevealSfxPath;
            (void)playResolvedOneShot(gPresentationSfxAudio, revealPath, 0.9f, false);
            resultOverlay_.revealSfxPlayed = true;
        }

        if (!resultOverlay_.applausePlayed &&
            resultOverlay_.outcome == BattleResultOverlayOutcome::Victory &&
            elapsedSeconds >= revealSeconds) {
            (void)playResolvedOneShot(gPresentationSfxAudio, kBattleResultVictoryApplauseSfxPath, 0.82f, false);
            resultOverlay_.applausePlayed = true;
        }

        if (!resultOverlay_.inputEnabled && elapsedSeconds >= battleResultButtonInteractiveTimeSeconds()) {
            resultOverlay_.inputEnabled = true;
        }
    }

    /**
     * @brief Plays the result-overlay button hover sound when the overlay is interactable.
     *
     * If the battle result overlay is active, input is enabled, and the overlay has not been acknowledged,
     * this function triggers the configured hover SFX; otherwise it has no effect.
     */
    void handleBattleResultButtonHover() {
        if (!resultOverlay_.active || !resultOverlay_.inputEnabled || resultOverlay_.acknowledged) {
            return;
        }

        (void)playResolvedOneShot(gPresentationSfxAudio, kBattleResultButtonHoverSfxPath, 0.58f, true);
    }

    /**
     * @brief Acknowledges the active battle result overlay and ends the session.
     *
     * If the result overlay is active, input-enabled, and not yet acknowledged,
     * this marks the overlay as acknowledged, plays the result selection sound,
     * and marks the session as finished.
     */
    void confirmBattleResultOverlay() {
        if (!resultOverlay_.active || !resultOverlay_.inputEnabled || resultOverlay_.acknowledged) {
            return;
        }

        resultOverlay_.acknowledged = true;
        (void)playResolvedOneShot(gPresentationSfxAudio, kBattleResultButtonSelectSfxPath, 0.9f, true);
        finished_ = true;
    }

    /**
     * @brief Handle keyboard input for the active battle result overlay.
     *
     * If the result overlay is active and the event is a keydown of Return,
     * keypad Enter, or Space, the overlay confirmation action is invoked.
     *
     * @param event SDL event to process; ignored unless it is a keydown event
     *              and the result overlay is active.
     */
    void handleBattleResultEvent(const SDL_Event& event) {
        if (!resultOverlay_.active) {
            return;
        }

        if (event.type != SDL_KEYDOWN) {
            return;
        }

        if (event.key.keysym.sym == SDLK_RETURN ||
            event.key.keysym.sym == SDLK_KP_ENTER ||
            event.key.keysym.sym == SDLK_SPACE) {
            confirmBattleResultOverlay();
        }
    }

    /**
     * @brief Attaches a persistent RmlUi event listener to an element within the current document.
     *
     * If the document or the element with the given `id` is not present, this function does nothing.
     * The provided `callback` is wrapped and retained internally so the listener remains valid
     * for the lifetime of the document or until listeners are cleared.
     *
     * @param id Element id to attach the listener to.
     * @param eventId Rml event identifier (e.g., "click", "mouseover").
     * @param callback Function invoked when the event fires; receives the Rml::Event reference.
     */
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

    /**
     * @brief Apply UI settings from an AppState to the running session and update live audio and narrative state.
     *
     * Copies the settings from the provided AppState into the session's active settings (if present),
     * updates master music volumes for the pause-menu BGM player, the battle BGM controller, and the
     * presentation loop audio (scaled by the session's presentationLoopBaseVolume_), and then applies
     * narrative-related settings.
     *
     * If the session has no active settings pointer (`settings_ == nullptr`), this function performs no action.
     *
     * @param state Source application state containing the UI settings to apply.
     */
    void applyLiveSettingsFromUiState(const AppState& state) {
        if (settings_ == nullptr) {
            return;
        }

        *settings_ = state.settings;
        const float musicMasterVolume = currentMusicMasterVolume();
        gPauseMenuBgmPlayer.setVolume(musicMasterVolume);
        gBattleBgmController.setMasterVolume(musicMasterVolume);
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

    /**
     * @brief Begins playback of the pause-menu music and preserves/updates battle BGM pause state.
     *
     * Stops any existing pause-menu music, records whether the battle BGM was playing and whether it
     * was paused, and pauses the battle BGM if it was actively playing. Then selects a pause-menu
     * track and starts playing it at the current music volume from settings (defaults to 1.0 if
     * settings are unavailable).
     */
    void startPauseMenuMusic() {
        stopPauseMenuMusic();

        battleBgmWasPlayingBeforePause_ = gBattleBgmController.isPlaying();
        battleBgmWasPausedBeforePause_ = gBattleBgmController.isPaused();
        if (battleBgmWasPlayingBeforePause_ && !battleBgmWasPausedBeforePause_) {
            gBattleBgmController.pause();
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

    /**
     * @brief Closes pause/settings UI and restores runtime state for normal gameplay.
     *
     * Closes the settings and pause documents, stops the pause-menu music, resets pause-related
     * audio volume/state to defaults, clears pause flags, and returns the pause overlay mode
     * and selection to their default values. If `resumeBattleMusic` is true and the battle
     * BGM was playing before the pause (and was not already paused), resumes the battle BGM.
     *
     * @param resumeBattleMusic If true, attempt to resume the battle BGM when appropriate.
     */
    void leavePauseMenu(bool resumeBattleMusic) {
        closeSettingsMenuDocument();
        closePauseMenuDocument();
        stopPauseMenuMusic();
        if (resumeBattleMusic && battleBgmWasPlayingBeforePause_ && !battleBgmWasPausedBeforePause_) {
            gBattleBgmController.resume();
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

    /**
     * @brief Starts a rhythm challenge for the given party member.
     *
     * Activates the internal rhythm-challenge state for the specified party index, recording the actor title,
     * a default ability name ("Rhythm Skill"), and the start time used for timing the challenge. If the index
     * is out of range, the function does nothing.
     *
     * @param partyIndex Index of the party member who will perform the rhythm challenge.
     */
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

    /**
     * @brief Display HUD feedback and visual cues for the outcome of a manual ultimate request.
     *
     * Shows either a transient toast or a timed hint and triggers a missing-orb blink when appropriate,
     * depending on the provided result and whether the request was buffered.
     *
     * @param partyIndex Zero-based party index for which the ultimate request was attempted.
     * @param result Enum value describing the request outcome (queued, meter not ready, already queued, unavailable).
     * @param nowMs Current time in milliseconds used to schedule hint/toast expiration and visual effects.
     * @param buffered If true, prefer timed hint overlays (short-lived text) over immediate toasts.
     */
    void handleManualUltimateRequestResult(int partyIndex,
                                           battle::ManualUltimateRequestResult result,
                                           Uint64 nowMs,
                                           bool buffered) {
        auto showManualUltimateHint = [&](std::string message, Uint64 durationMs) {
            BattleHintRequest request;
            request.stableKey = nextTransientBattleHintKey(kBattleHintKeyManualUltimateStatus);
            request.family = BattleHintFamily::Warning;
            request.kicker = "MANUAL ULT";
            request.sourceTag = "ALLY " + std::to_string(partyIndex + 1);
            request.badgeText = "ALERT";
            request.message = std::move(message);
            request.resolveRule.kind = BattleHintResolveKind::Timeout;
            request.resolveRule.durationMs = durationMs;
            enqueueBattleHint(std::move(request));
        };

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
                (void)buffered;
                showManualUltimateHint(message, 1800);
                blinkMissingOrbs(hudFeedback_, partyIndex, charge, required - 1, nowMs);
                return;
            }
            case battle::ManualUltimateRequestResult::AlreadyQueued:
                (void)buffered;
                showManualUltimateHint("ULTIMATE ALREADY QUEUED.", 1400);
                return;
            case battle::ManualUltimateRequestResult::Unavailable:
                (void)buffered;
                showManualUltimateHint("ULTIMATE NOT AVAILABLE.", 1400);
                return;
        }
    }

    /**
     * @brief Processes and clears any queued manual-ultimate requests.
     *
     * Invokes a manual-ultimate request for each buffered party index and forwards each request's result to the manual-ultimate result handler, then clears the buffer.
     *
     * @param nowMs Current time in milliseconds, used by the result handler for timestamping or timing decisions.
     */
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

    /**
     * @brief Handles a manual-ultimate hotkey by mapping the key to a party member and requesting an ultimate turn.
     *
     * If the key does not map to any party index the function does not consume the key.
     * When the battle or input state prevents immediate requests (free view, tutorial overlay,
     * dialogue in progress, space actions disabled, or battle over) the key is consumed but no request is made.
     * If a rhythm challenge is active the request is queued and a HUD hint is shown.
     *
     * @param key SDL key code pressed; numeric keypad / number keys map to party indices.
     * @param nowMs Current time in milliseconds used for HUD hint timing.
     * @return true if the key was consumed (handled or queued), false if the key did not map to a party index.
     */
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
                BattleHintRequest request;
                request.stableKey = nextTransientBattleHintKey(kBattleHintKeyManualUltimateStatus);
                request.family = BattleHintFamily::Warning;
                request.kicker = "MANUAL ULT";
                request.sourceTag = "ALLY " + std::to_string(partyIndex + 1);
                request.badgeText = "QUEUE";
                request.message = "ULTIMATE WILL QUEUE AFTER THIS SKILL.";
                request.resolveRule.kind = BattleHintResolveKind::Timeout;
                request.resolveRule.durationMs = 1200;
                enqueueBattleHint(std::move(request));
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

    /**
     * @brief Triggers the appropriate tutorial step when its entry conditions are met.
     *
     * Checks tutorial enablement, library availability, overlay state, and rhythm blocking, then starts
     * the first applicable tutorial step (Standard, Skill, or Ultimate) when the corresponding player
     * action becomes ready or a queued manual-ultimate request exists.
     *
     * @param nowMs Current timestamp in milliseconds used to start the tutorial timing.
     */
    void maybeStartTutorial(Uint64 nowMs) {
        if (!tutorialEnabled_ || !tutorialLibrary_.loaded ||
            tutorialOverlay_.dismissed || rhythmChallenge_.active || tutorialOverlay_.step != TutorialStep::None) {
            return;
        }
        if (!tutorialOverlay_.standardShown && manager_.isPlayerActionReady(battle::BattleAction::Standard)) {
            startTutorial(tutorialOverlay_, TutorialStep::Standard, tutorialLibrary_.standard, nowMs);
            showTutorialHint(nowMs);
            return;
        }
        if (!tutorialOverlay_.skillShown && manager_.isPlayerActionReady(battle::BattleAction::Skill)) {
            startTutorial(tutorialOverlay_, TutorialStep::Skill, tutorialLibrary_.skill, nowMs);
            showTutorialHint(nowMs);
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
            showTutorialHint(nowMs);
        }
    }

    void completeTutorialForAction(battle::BattleAction action) {
        if (action == battle::BattleAction::Standard && tutorialOverlay_.step == TutorialStep::Standard) {
            completeTutorialStep(tutorialOverlay_, &hudFeedback_);
        } else if (action == battle::BattleAction::Skill && tutorialOverlay_.step == TutorialStep::Skill) {
            completeTutorialStep(tutorialOverlay_, &hudFeedback_);
        } else if (action == battle::BattleAction::Ultimate && tutorialOverlay_.step == TutorialStep::Ultimate) {
            completeTutorialStep(tutorialOverlay_, &hudFeedback_);
        }
    }

    /**
     * @brief Finalizes an active rhythm (skill) challenge and attempts the player's skill action.
     *
     * If a rhythm challenge is active, stops the challenge and attempts to execute the player's skill.
     * On failure, clears any buffered manual-ultimate requests and shows an "ACTION NOT AVAILABLE." toast.
     * On success, shows an on-beat/late toast, marks the skill tutorial step complete, flushes buffered manual-ultimate
     * requests, advances automatic turns, and consumes resulting battle action events (updating HUD/voice/camera staging).
     *
     * @param onBeat True to indicate the input was on-beat (displays "ON-BEAT INPUT."), false to indicate late input
     *               (displays "LATE INPUT.").
     */
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
        (void)processNextAutomaticTurnWithIndicator(nowMs);
        consumeBattleActionEvents(nowMs, &cameraStaging_);
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
            showWaitForAllyTurnHint();
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
        (void)processNextAutomaticTurnWithIndicator(nowMs);
        consumeBattleActionEvents(nowMs, &cameraStaging_);
    }

    void onActiveActorChanged(const battle::TurnActor* actor, const battle::BattleState& state, Uint64 nowMs) {
        if (actor == nullptr || actor->type != battle::ParticipantType::Character ||
            actor->partyIndex < 0 || actor->partyIndex >= static_cast<int>(state.party.size())) {
            resetIdleVoicelineState();
            return;
        }

        const battle::CharacterDefinition& character = state.party[static_cast<size_t>(actor->partyIndex)];
        scheduleIdleVoiceline(combatVoiceAssetId(character), actor->partyIndex, nowMs);
    }

    void scheduleIdleVoiceline(const std::string& assetName, int partyIndex, Uint64 nowMs) {
        stopIdleVoicelinePlayback();
        idleActorPartyIndex_ = partyIndex;
        idleVoiceClipPath_ = findIdleVoicePath(assetName);
        idlePlayingHandle_.reset();
        idleNextPlayMs_ = idleVoiceClipPath_.has_value() ? nowMs + kIdleDelayMs : 0;
    }

    void resetIdleVoicelineState() {
        stopIdleVoicelinePlayback();
        idleActorPartyIndex_.reset();
        idleVoiceClipPath_.reset();
        idleNextPlayMs_ = 0;
    }

    void stopIdleVoicelinePlayback() {
        if (idlePlayingHandle_.has_value()) {
            gOneShotAudio.stopPlayback(*idlePlayingHandle_);
            idlePlayingHandle_.reset();
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

        if (tutorialOverlay_.step != TutorialStep::None ||
            isDialogueInProgress() ||
            presentationPlaybackActive_ ||
            activeUltimateTurnSplash_ != nullptr ||
            rhythmChallenge_.active ||
            manager_.isBattleOver()) {
            stopIdleVoicelinePlayback();
            return;
        }

        const std::optional<int> activeIndex = getActiveCharacterPartyIndex(manager_);
        if (!activeIndex.has_value() || activeIndex != idleActorPartyIndex_) {
            resetIdleVoicelineState();
            return;
        }

        if (idlePlayingHandle_.has_value()) {
            if (!gOneShotAudio.isPlaying(*idlePlayingHandle_)) {
                idlePlayingHandle_.reset();
                idleNextPlayMs_ = nowMs + kIdleDelayMs;
            }
            return;
        }

        if (idleNextPlayMs_ == 0 || nowMs < idleNextPlayMs_) {
            return;
        }

        std::optional<game::audio::WavOneShotPlayer::PlaybackHandle> startedHandle;
        if (requestBattleVoice(characterVoiceSpeakerKey(*idleActorPartyIndex_),
                               game::audio::BattleVoiceKind::Idle,
                               game::audio::BattleVoiceChannel::OneShot,
                               *idleVoiceClipPath_,
                               currentVoiceVolume(),
                               &startedHandle)) {
            idlePlayingHandle_ = startedHandle;
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
    BattleResultPresentationConfig resultPresentationConfig_{};
    PauseOverlayMode pauseOverlayMode_ = PauseOverlayMode::Menu;
    PauseSelection pauseSelection_ = PauseSelection::Continue;
    SettingsSelection settingsSelection_ = SettingsSelection::DisplayMode;
    battle::BattleDefinition battleDefinition_{};
    battle::render::StageDefinition stageDefinition_{};
    std::vector<std::string> activePartyLineup_;
    battle::BattleManager manager_;
    battle::postbattle::Summary postBattleSummaryCache_{};
    battle::demo::DemoNarrativeFlow narrative_;
    TutorialScriptLibrary tutorialLibrary_;
    HudFeedbackState hudFeedback_;
    HudAnimationState hudAnimationState_;
    BattleResultOverlayState resultOverlay_;
    BattleVsIntroOverlayState vsIntroOverlay_;
    TutorialOverlayState tutorialOverlay_;
    RhythmChallengeState rhythmChallenge_;
    BattleInputPromptState battleInputPrompt_;
    std::vector<int> bufferedManualUltimatePartyIndices_;
    Uint64 nextTransientBattleHintId_ = 1;
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
    std::string initialBattleBgmPath_;
    float presentationLoopBaseVolume_ = 1.0f;
    battle::Camera3D camera_;
    battle::render::BattleCameraStaging cameraStaging_;
    battle::render::BattleCombatBeginAnimation combatBeginAnimation_;
    battle::render::BattleFeedbackSystem feedback_;
    CameraIntroAnimation cameraIntro_;
    CameraIntroAnimation bossPhaseIntro_;
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
    std::optional<game::audio::WavOneShotPlayer::PlaybackHandle> idlePlayingHandle_;
    Uint64 idleNextPlayMs_ = 0;
    std::unordered_map<std::string, std::optional<std::string>> idleVoicePathCache_;
    game::audio::BattleVoiceArbiter voiceArbiter_;
    std::optional<game::audio::WavOneShotPlayer::PlaybackHandle> tutorialVoiceHandle_;
    std::string activeVnVoiceSpeakerKey_;
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
/**
 * @brief Initialize the battle session with the provided window, settings, and battle configuration.
 *
 * Initializes internal runtime state, renderers, audio, UI, and battle manager and prepares the session
 * to begin running and rendering the specified battle.
 *
 * @param window The application window and GL context to use for rendering.
 * @param settings User-adjustable game settings (audio volumes, text speed, display mode, etc.).
 * @param battleKey Key identifying the battle definition to load.
 * @param progression Player progression state used to seed enemy/boss scaling and unlocks.
 * @param initialPartyLineup Optional explicit party lineup; when present it overrides the battle's default lineup.
 * @param resultPresentation Configuration controlling the post-battle result overlay presentation.
 * @return true if initialization completed successfully and the session is ready; false on failure.
 */
bool Session::initialize(Window& window,
                         GameSettings& settings,
                         const std::string& battleKey,
                         const battle::PlayerProgression& progression,
                         std::optional<std::vector<std::string>> initialPartyLineup,
                         BattleResultPresentationConfig resultPresentation) {
    return impl_->initialize(
        window,
        settings,
        battleKey,
        progression,
        std::move(initialPartyLineup),
        resultPresentation);
}
/**
 * @brief Shuts down and releases all resources held by the session.
 *
 * Performs a full teardown of the underlying implementation, stopping audio,
 * closing UI contexts, releasing renderers and assets, and resetting internal state.
 */
void Session::shutdown() { impl_->shutdown(); }
/**
 * @brief Process a single SDL event for the battle session.
 *
 * Dispatches input and window events to the session runtime so the session
 * can handle gameplay input, UI interaction, pause/result overlays, and
 * window resizing.
 *
 * @param event SDL event to process.
 */
void Session::handleEvent(const SDL_Event& event) { impl_->handleEvent(event); }
void Session::update(float deltaSeconds) { impl_->update(deltaSeconds); }
void Session::render() { impl_->render(); }
void Session::setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) { impl_->setLoadingOverlay(state); }
bool Session::isFinished() const { return impl_->isFinished(); }
/**
 * @brief Reports whether the session exited to the main menu.
 *
 * @return bool `true` if the session exited to the main menu, `false` otherwise.
 */
bool Session::exitedToMainMenu() const { return impl_->exitedToMainMenu(); }
/**
 * @brief Gets the current outcome of the battle session.
 *
 * @return BattleOutcome The session's current outcome: the resolved final outcome if the battle has finished,
 * or the current in-progress state otherwise.
 */
BattleOutcome Session::outcome() const { return impl_->outcome(); }
/**
 * @brief Retrieve the current party lineup as asset keys in slot order.
 *
 * @return const std::vector<std::string>& A reference to the active party lineup vector, ordered by slot; may be empty.
 */
const std::vector<std::string>& Session::currentPartyLineup() const { return impl_->currentPartyLineup(); }
/**
 * @brief Retrieve the post-battle summary for the completed session.
 *
 * @return const battle::postbattle::Summary& The cached post-battle summary containing aggregated telemetry and per-character statistics (e.g., total action value consumed, per-character total damage, and outcome).
 */
const battle::postbattle::Summary& Session::postBattleSummary() const { return impl_->postBattleSummary(); }

} // namespace battle::app
