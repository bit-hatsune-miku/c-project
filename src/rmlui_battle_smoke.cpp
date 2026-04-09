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
#include <numeric>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

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
#include "game/core/battle_manager.h"
#include "game/core/easing.h"
#include "game/core/turn_system.h"
#include "game/render/battle_scene_renderer.h"
#include "game/render/camera_3d.h"
#include "game/render/free_view_camera_debug_log.h"
#include "game/vn/vn_script_catalog.h"
#include "game/render/gl_screen_blitter.h"
#include "game/vn/vn_script.h"
#include "game/audio/wav_one_shot.h"
#include "platform/path_resolution.h"

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
constexpr float kRhythmPulseTravelPx = 278.0f;
constexpr float kRhythmWindowLeftPx = 206.0f;
constexpr float kRhythmWindowWidthPx = 62.0f;
constexpr float kNarrationCharsPerSecond = 42.0f;

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

struct HudFeedbackState {
    std::string toastText;
    Uint64 toastUntilMs = 0;
    int blinkUnitIndex = -1;
    int blinkMissingFrom = 0;
    int blinkMissingTo = 0;
    Uint64 blinkUntilMs = 0;
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

struct RhythmChallengeState {
    bool active = false;
    int partyIndex = -1;
    std::string actorTitle;
    std::string abilityName;
    Uint64 startedMs = 0;
    Uint64 durationMs = 1350;
    float targetCenter = (kRhythmWindowLeftPx + (kRhythmWindowWidthPx * 0.5f)) / kRhythmPulseTravelPx;
    float targetWindow = kRhythmWindowWidthPx / kRhythmPulseTravelPx;
};

class ClickListener final : public Rml::EventListener {
public:
    explicit ClickListener(std::function<void()> callback)
        : callback_(std::move(callback)) {}

    void ProcessEvent(Rml::Event&) override {
        if (callback_) {
            callback_();
        }
    }

private:
    std::function<void()> callback_;
};

game::audio::WavOneShotPlayer gOneShotAudio;

using battle::render::GlScreenBlitter;
using battle::render::SoftwareSceneRenderer;
using battle::render::WorldEntity;

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

void setElementText(Rml::ElementDocument* document, const std::string& id, const std::string& text) {
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetInnerRML(text);
    }
}

void setElementDisplay(Rml::ElementDocument* document, const std::string& id, bool visible) {
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetProperty("display", visible ? "block" : "none");
    }
}

void setElementClass(Rml::ElementDocument* document, const std::string& id, const std::string& className, bool enabled) {
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetClass(className, enabled);
    }
}

void setPortraitDecorator(Rml::ElementDocument* document, const std::string& id, const std::string& assetName) {
    if (Rml::Element* element = document->GetElementById(id)) {
        const std::string path = platform::path::findCombatImagePath("icons", assetName);
        if (!path.empty()) {
            element->SetProperty("decorator", "image(" + path + " cover center center)");
        }
    }
}

std::vector<int> getSortedTurnActorIndices(const battle::TurnState& turnState) {
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

void setOrbState(Rml::Element* orb, bool visible, bool filled, bool gold = false) {
    if (orb == nullptr) {
        return;
    }
    orb->SetClass("hidden", !visible);
    orb->SetClass("full", filled);
    orb->SetClass("gold", filled && gold);
}

void showToast(HudFeedbackState& feedback, std::string message, Uint64 nowMs, Uint64 durationMs = 1800) {
    feedback.toastText = std::move(message);
    feedback.toastUntilMs = nowMs + durationMs;
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
    if (feedback.toastUntilMs != 0 && nowMs >= feedback.toastUntilMs) {
        feedback.toastText.clear();
        feedback.toastUntilMs = 0;
    }
    if (feedback.blinkUntilMs != 0 && nowMs >= feedback.blinkUntilMs) {
        feedback.blinkUnitIndex = -1;
        feedback.blinkMissingFrom = 0;
        feedback.blinkMissingTo = 0;
        feedback.blinkUntilMs = 0;
    }
}

float getRhythmProgress(const RhythmChallengeState& rhythm, Uint64 nowMs) {
    if (!rhythm.active || rhythm.durationMs == 0) {
        return 0.0f;
    }
    const double elapsed = static_cast<double>(nowMs - rhythm.startedMs);
    const double duration = static_cast<double>(rhythm.durationMs);
    return std::clamp(static_cast<float>(elapsed / duration), 0.0f, 1.0f);
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

std::string revealNarrationText(const std::string& fullText, Uint64 startedMs, Uint64 nowMs) {
    if (fullText.empty()) {
        return std::string();
    }

    if (nowMs <= startedMs) {
        return std::string();
    }

    const double elapsedSeconds = static_cast<double>(nowMs - startedMs) / 1000.0;
    const size_t visibleCodepoints = static_cast<size_t>(std::floor(elapsedSeconds * kNarrationCharsPerSecond));
    const size_t visibleBytes = utf8ByteOffsetForCodepoints(fullText, visibleCodepoints);
    if (visibleBytes >= fullText.size()) {
        return fullText;
    }
    return fullText.substr(0, visibleBytes);
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

bool loadTutorialScriptLibrary(TutorialScriptLibrary& outLibrary) {
    vn::Script script;
    if (!vn::loadScript(vn::resolveScriptPath("demo"), script)) {
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

std::optional<int> getActiveCharacterPartyIndex(const battle::BattleManager& manager) {
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

void updateBossOrbRow(Rml::ElementDocument* document, int charge, int required) {
    const int safeRequired = std::clamp(required, 1, 6);
    const int safeCharge = std::clamp(charge, 0, safeRequired);
    for (int i = 0; i < 6; ++i) {
        if (Rml::Element* orb = document->GetElementById("boss-orb-" + std::to_string(i + 1))) {
            setOrbState(orb, i < safeRequired, i < safeCharge, false);
        }
    }
}

void updateUnitOrbRow(Rml::ElementDocument* document, int unitIndex, int charge, int required) {
    const int safeRequired = std::clamp(required, 1, 6);
    const int safeCharge = std::clamp(charge, 0, safeRequired);
    for (int i = 0; i < 6; ++i) {
        if (Rml::Element* orb = document->GetElementById(
                "unit-" + std::to_string(unitIndex) + "-orb-" + std::to_string(i + 1))) {
            setOrbState(orb, i < safeRequired, i < safeCharge, safeCharge == safeRequired && i == safeRequired - 1);
        }
    }
}

std::string hpColorForRatio(float ratio) {
    if (ratio > 0.66f) {
        return "#48bf7b";
    }
    if (ratio > 0.33f) {
        return "#e4a943";
    }
    return "#d96b56";
}

void updateBattleHudDocument(Rml::ElementDocument* document,
                             const battle::BattleManager& manager,
                             const HudFeedbackState& feedback,
                             const TutorialOverlayState& tutorial,
                             const RhythmChallengeState& rhythm,
                             Uint64 nowMs) {
    const battle::BattleState& battleState = manager.getBattleState();
    const battle::TurnState& turnState = manager.getTurnState();
    const int activeActorIndex = manager.getPreviewNextActorIndex();

    setElementText(document, "boss-name", uppercase(battleState.boss.key));
    const int bossCurrentHp = manager.getBossCurrentHp();
    const int bossMaxHp = std::max(1, manager.getBossMaxHp());
    const int bossPercent = static_cast<int>(std::round((100.0f * bossCurrentHp) / bossMaxHp));
    setElementText(document, "boss-percent", "HP " + std::to_string(bossPercent) + "%");
    if (Rml::Element* bossFill = document->GetElementById("boss-fill")) {
        bossFill->SetProperty("width", std::to_string(bossPercent) + "%");
    }
    updateBossOrbRow(document, manager.getBossUltimateCharge(), manager.getBossUltimateRequired());

    const std::vector<int> sortedActorIndices = getSortedTurnActorIndices(turnState);
    for (int slot = 0; slot < 5; ++slot) {
        const std::string slotIndex = std::to_string(slot + 1);
        const bool hasActor = slot < static_cast<int>(sortedActorIndices.size());
        setElementDisplay(document, "turn-card-" + slotIndex, hasActor);
        if (!hasActor) {
            continue;
        }

        const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(sortedActorIndices[static_cast<size_t>(slot)])];
        setPortraitDecorator(document, "turn-portrait-" + slotIndex, actor.key);
        if (Rml::Element* card = document->GetElementById("turn-card-" + slotIndex)) {
            card->SetClass("active", sortedActorIndices[static_cast<size_t>(slot)] == activeActorIndex);
            card->SetClass("boss", actor.type == battle::ParticipantType::Boss);
        }
        if (Rml::Element* accent = document->GetElementById("turn-accent-" + slotIndex)) {
            accent->SetClass("boss-accent", actor.type == battle::ParticipantType::Boss);
        }
    }

    for (int i = 0; i < 4; ++i) {
        const std::string index = std::to_string(i + 1);
        const bool hasCharacter = i < static_cast<int>(battleState.party.size());
        setElementDisplay(document, "unit-card-" + index, hasCharacter);
        if (!hasCharacter) {
            continue;
        }

        const battle::CharacterDefinition& character = battleState.party[static_cast<size_t>(i)];
        const int currentHp = manager.getCharacterCurrentHp(i);
        const int maxHp = std::max(1, manager.getCharacterMaxHp(i));
        const float ratio = static_cast<float>(std::clamp(currentHp, 0, maxHp)) / static_cast<float>(maxHp);

        setElementText(document, "unit-name-" + index, uppercase(character.title));
        setElementText(document, "unit-hp-text-" + index, std::to_string(currentHp));
        setPortraitDecorator(document, "unit-portrait-" + index, character.assets);
        updateUnitOrbRow(document, i + 1, manager.getCharacterUltimateCharge(i), manager.getCharacterUltimateRequired(i));

        if (Rml::Element* hpFill = document->GetElementById("unit-hp-fill-" + index)) {
            const int fillWidth = std::clamp(static_cast<int>(std::round(ratio * 174.0f)), 0, 174);
            hpFill->SetProperty("width", std::to_string(fillWidth) + "px");
            hpFill->SetProperty("background-color", hpColorForRatio(ratio));
        }
        if (Rml::Element* card = document->GetElementById("unit-card-" + index)) {
            const bool isFocused = activeActorIndex >= 0 &&
                activeActorIndex < static_cast<int>(turnState.actors.size()) &&
                turnState.actors[static_cast<size_t>(activeActorIndex)].type == battle::ParticipantType::Character &&
                turnState.actors[static_cast<size_t>(activeActorIndex)].partyIndex == i;
            card->SetClass("focus", isFocused);
            card->SetClass("ghost", currentHp <= 0);
            card->SetClass(
                "ult-ready",
                currentHp > 0 && manager.getCharacterUltimateCharge(i) >= manager.getCharacterUltimateRequired(i)
            );
        }

        for (int orbIndex = 0; orbIndex < 6; ++orbIndex) {
            if (Rml::Element* orb = document->GetElementById(
                    "unit-" + std::to_string(i + 1) + "-orb-" + std::to_string(orbIndex + 1))) {
                const bool shouldBlink =
                    feedback.blinkUnitIndex == i &&
                    orbIndex >= feedback.blinkMissingFrom &&
                    orbIndex <= feedback.blinkMissingTo;
                orb->SetClass("missing", shouldBlink);
            }
        }
    }

    const bool playerCanAct = getActiveCharacterPartyIndex(manager).has_value();
    if (Rml::Element* actionStandard = document->GetElementById("action-standard")) {
        actionStandard->SetClass("disabled", rhythm.active || !playerCanAct || !manager.isPlayerActionReady(battle::BattleAction::Standard));
    }
    if (Rml::Element* actionSkill = document->GetElementById("action-skill")) {
        actionSkill->SetClass("disabled", rhythm.active || !playerCanAct || !manager.isPlayerActionReady(battle::BattleAction::Skill));
    }
    if (Rml::Element* toast = document->GetElementById("battle-toast")) {
        toast->SetInnerRML(feedback.toastText);
        toast->SetClass("visible", !feedback.toastText.empty());
    }

    setElementClass(document, "battle-tutorial", "visible", tutorial.step != TutorialStep::None);
    if (tutorial.step != TutorialStep::None) {
        setElementText(document, "battle-tutorial-speaker", vn::getDisplaySpeakerName(tutorial.entry));
        setElementText(document, "battle-tutorial-text", revealNarrationText(tutorial.entry.text, tutorial.startedMs, nowMs));
        if (!tutorial.entry.icon.empty()) {
            const std::string tutorialIconKey = std::filesystem::path(tutorial.entry.icon).stem().string();
            const std::string iconPath = platform::path::findCombatImagePath("icons", tutorialIconKey);
            if (!iconPath.empty()) {
                if (Rml::Element* element = document->GetElementById("battle-tutorial-portrait")) {
                    element->SetProperty("decorator", "image(" + iconPath + " cover center center)");
                }
            }
        }
    }

    setElementClass(document, "battle-rhythm", "visible", rhythm.active);
    if (rhythm.active) {
        setElementText(document, "battle-rhythm-title", "");
        setElementText(document, "battle-rhythm-body", "PRESS E WHEN THE CYAN PULSE CROSSES THE GOLD WINDOW.");
        if (Rml::Element* pulse = document->GetElementById("battle-rhythm-pulse")) {
            const float progress = getRhythmProgress(rhythm, nowMs);
            pulse->SetProperty("left", std::to_string(static_cast<int>(std::round(progress * kRhythmPulseTravelPx))) + "px");
        }
    }
}

void consumeBattleActionEvents(HudFeedbackState& feedback,
                               battle::BattleManager& manager,
                               Uint64 nowMs) {
    auto playResolvedVoicePath = [](const std::string& path) {
        if (path.empty()) {
            return false;
        }
        const std::string resolved = platform::path::resolvePath(path);
        if (!std::filesystem::exists(resolved)) {
            return false;
        }
        return gOneShotAudio.playWavOneShot(resolved);
    };
    auto playBossHitVoice = [&](const battle::BattleState& battleState) {
        if (playResolvedVoicePath(battleState.boss.voiceHit)) {
            return true;
        }
        if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.boss.assets, "hit"); hitVoice.has_value()) {
            return gOneShotAudio.playWavOneShot(*hitVoice);
        }
        if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.boss.key, "hit"); hitVoice.has_value()) {
            return gOneShotAudio.playWavOneShot(*hitVoice);
        }
        return false;
    };

    const battle::BattleState& battleState = manager.getBattleState();
    (void)feedback;
    (void)nowMs;
    for (const battle::BattleActionEvent& event : manager.getRecentActionEvents()) {
        const std::string actorVoiceKey = event.actorType == battle::ParticipantType::Boss
            ? battleState.boss.key
            : ((event.actorPartyIndex >= 0 && event.actorPartyIndex < static_cast<int>(battleState.party.size()))
                ? battleState.party[static_cast<size_t>(event.actorPartyIndex)].assets
                : std::string());

        if (!event.abilityVoicesHandledDuringPresentation && event.action == battle::BattleAction::Skill) {
            if (const auto skillVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "skill"); skillVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*skillVoice);
            }
        }

        if (event.bossHpAfter < event.bossHpBefore) {
            (void)playBossHitVoice(battleState);
        }

        for (size_t i = 0; i < event.targetPartyIndices.size() && i < event.targetHpBefore.size() && i < event.targetHpAfter.size(); ++i) {
            if (event.targetHpAfter[i] >= event.targetHpBefore[i]) {
                continue;
            }
            const int partyIndex = event.targetPartyIndices[i];
            if (partyIndex < 0 || partyIndex >= static_cast<int>(battleState.party.size())) {
                continue;
            }
            if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.party[static_cast<size_t>(partyIndex)].assets, "hit");
                hitVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*hitVoice);
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

class RenderInterfaceGL3SDL final : public RenderInterface_GL3 {
public:
    Rml::TextureHandle LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) override {
#ifdef BATTLE_ENABLE_IMAGE
        Rml::FileInterface* fileInterface = Rml::GetFileInterface();
        Rml::FileHandle fileHandle = fileInterface->Open(source);
        if (!fileHandle) {
            return {};
        }

        fileInterface->Seek(fileHandle, 0, SEEK_END);
        const size_t bufferSize = fileInterface->Tell(fileHandle);
        fileInterface->Seek(fileHandle, 0, SEEK_SET);

        using Rml::byte;
        Rml::UniquePtr<byte[]> buffer(new byte[bufferSize]);
        fileInterface->Read(buffer.get(), bufferSize, fileHandle);
        fileInterface->Close(fileHandle);

        const size_t extIndex = source.rfind('.');
        const Rml::String extension = (extIndex == Rml::String::npos ? Rml::String() : source.substr(extIndex + 1));

        SDL_Surface* surface = IMG_LoadTyped_RW(SDL_RWFromMem(buffer.get(), static_cast<int>(bufferSize)), 1, extension.c_str());
        if (surface == nullptr) {
            Rml::Log::Message(Rml::Log::LT_ERROR, "Could not load texture: %s", source.c_str());
            return {};
        }

        if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
            SDL_Surface* convertedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(surface);
            if (convertedSurface == nullptr) {
                return {};
            }
            surface = convertedSurface;
        }

        textureDimensions = {surface->w, surface->h};

        byte* pixels = static_cast<byte*>(surface->pixels);
        const size_t pixelBytes = static_cast<size_t>(surface->w) * static_cast<size_t>(surface->h) * 4;
        for (size_t i = 0; i < pixelBytes; i += 4) {
            const byte alpha = pixels[i + 3];
            pixels[i + 0] = byte((int(pixels[i + 0]) * int(alpha)) / 255);
            pixels[i + 1] = byte((int(pixels[i + 1]) * int(alpha)) / 255);
            pixels[i + 2] = byte((int(pixels[i + 2]) * int(alpha)) / 255);
        }

        const Rml::TextureHandle textureHandle = GenerateTexture({pixels, pixelBytes}, textureDimensions);
        SDL_FreeSurface(surface);
        return textureHandle;
#else
        return RenderInterface_GL3::LoadTexture(textureDimensions, source);
#endif
    }
};

} // namespace

int main(int argc, char** argv) {
    std::string bossKey = "lyooBoss";
    std::vector<std::string> partyKeys = {"iroha", "kaguya", "miku", "cupcakke"};

    if (argc >= 2) {
        bossKey = normalizeCombatKey(argv[1]);
    }
    if (argc >= 3) {
        partyKeys.clear();
        for (int i = 2; i < argc; ++i) {
            partyKeys.emplace_back(normalizeCombatKey(argv[i]));
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

#ifdef BATTLE_ENABLE_IMAGE
    const int requiredImageFlags = IMG_INIT_PNG | IMG_INIT_WEBP;
    if ((IMG_Init(requiredImageFlags) & requiredImageFlags) != requiredImageFlags) {
        std::cerr << "SDL_image init failed: " << IMG_GetError() << "\n";
    }
#endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "Battle Testing - RmlUi HUD Smoke",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kWindowWidth,
        kWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    if (window == nullptr) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (glContext == nullptr) {
        std::cerr << "GL context creation failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);
    SDL_StopTextInput();

    Rml::String glInitMessage;
    if (!RmlGL3::Initialize(&glInitMessage)) {
        std::cerr << "RmlGL3 initialization failed: " << glInitMessage << "\n";
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GlScreenBlitter screenBlitter;
    if (!screenBlitter.initialize()) {
        std::cerr << "Failed to initialize screen blitter\n";
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const auto destroyNativeRenderers = [&](SoftwareSceneRenderer* sceneRenderer = nullptr) {
        if (sceneRenderer != nullptr) {
            sceneRenderer->destroy();
        }
        screenBlitter.destroy();
    };

    SystemInterface_SDL systemInterface;
    systemInterface.SetWindow(window);
    RenderInterfaceGL3SDL renderInterface;
    if (!renderInterface) {
        std::cerr << "RmlUi GL3 render interface construction failed\n";
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Rml::SetSystemInterface(&systemInterface);
    Rml::SetRenderInterface(&renderInterface);
    if (!Rml::Initialise()) {
        std::cerr << "RmlUi core initialization failed\n";
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const std::string fontPath = platform::path::findFontPath();
    if (!fontPath.empty()) {
        Rml::LoadFontFace(fontPath);
    }

    battle::BattleManager manager;
    if (!manager.initialize(bossKey, partyKeys)) {
        std::cerr << "[Battle] Initialization failed.\n";
        destroyNativeRenderers();
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const battle::BattleState& state = manager.getBattleState();
    TutorialScriptLibrary tutorialLibrary;
    if (!loadTutorialScriptLibrary(tutorialLibrary)) {
        std::cerr << "Failed to load tutorial script: demo\n";
        destroyNativeRenderers();
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::vector<std::string> worldAssets;
    for (const battle::CharacterDefinition& character : state.party) {
        worldAssets.push_back(character.assets);
    }
    worldAssets.push_back(state.boss.assets);

    SoftwareSceneRenderer sceneRenderer;
    if (!sceneRenderer.initialize(kWindowWidth, kWindowHeight, worldAssets, resolveBattleSpritePath)) {
        destroyNativeRenderers(&sceneRenderer);
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::vector<WorldEntity> entities;
    entities.reserve(2);
    entities.push_back(WorldEntity{
        state.party.front().key,
        state.party.front().assets,
        false,
        battle::render::kDuelCharacterSlotX,
        battle::render::kDuelCharacterBaseY,
        0.0f,
        battle::render::colorFromKey(state.party.front().key, false)
    });
    entities.push_back(WorldEntity{
        state.boss.key,
        state.boss.assets,
        true,
        battle::render::kDuelBossSlotX,
        battle::render::kDuelCharacterBaseY + battle::render::kBossCharacterDistanceWorld,
        0.0f,
        battle::render::colorFromKey(state.boss.key, true)
    });

    int windowWidth = kWindowWidth;
    int windowHeight = kWindowHeight;
    renderInterface.SetViewport(windowWidth, windowHeight);
    Rml::Context* context = Rml::CreateContext("battle-smoke", Rml::Vector2i(windowWidth, windowHeight));
    if (context == nullptr) {
        std::cerr << "Failed to create RmlUi context\n";
        destroyNativeRenderers(&sceneRenderer);
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const std::string documentPath = platform::path::resolvePath("assets/rmlui/battle_hud.rml");
    Rml::ElementDocument* document = context->LoadDocument(documentPath);
    if (document == nullptr) {
        std::cerr << "Failed to load RmlUi document: " << documentPath << "\n";
        destroyNativeRenderers(&sceneRenderer);
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    document->Show();

    HudFeedbackState hudFeedback;
    TutorialOverlayState tutorialOverlay;
    RhythmChallengeState rhythmChallenge;
    std::vector<std::unique_ptr<ClickListener>> clickListeners;

    const auto attachClick = [&](const std::string& id, std::function<void()> callback) {
        if (Rml::Element* element = document->GetElementById(id)) {
            auto listener = std::make_unique<ClickListener>(std::move(callback));
            element->AddEventListener(Rml::EventId::Click, listener.get());
            clickListeners.push_back(std::move(listener));
        }
    };

    const auto beginRhythmChallenge = [&](int partyIndex) {
        if (partyIndex < 0 || partyIndex >= static_cast<int>(state.party.size())) {
            return;
        }
        const battle::CharacterDefinition& character = state.party[static_cast<size_t>(partyIndex)];
        rhythmChallenge.active = true;
        rhythmChallenge.partyIndex = partyIndex;
        rhythmChallenge.actorTitle = character.title;
        rhythmChallenge.abilityName = "Rhythm Skill";
        rhythmChallenge.startedMs = SDL_GetTicks64();
    };

    const auto maybeStartTutorial = [&](Uint64 nowMs) {
        if (tutorialOverlay.dismissed || rhythmChallenge.active || tutorialOverlay.step != TutorialStep::None) {
            return;
        }

        const std::optional<int> activePartyIndex = getActiveCharacterPartyIndex(manager);
        if (!activePartyIndex.has_value()) {
            return;
        }

        if (!tutorialOverlay.standardShown && manager.isPlayerActionReady(battle::BattleAction::Standard)) {
            startTutorial(
                tutorialOverlay,
                TutorialStep::Standard,
                tutorialLibrary.standard,
                nowMs
            );
            return;
        }
        if (!tutorialOverlay.skillShown && manager.isPlayerActionReady(battle::BattleAction::Skill)) {
            startTutorial(
                tutorialOverlay,
                TutorialStep::Skill,
                tutorialLibrary.skill,
                nowMs
            );
            return;
        }
        if (!tutorialOverlay.ultimateShown &&
            manager.isPlayerActionReady(battle::BattleAction::Ultimate) &&
            manager.getCharacterUltimateCharge(*activePartyIndex) >= manager.getCharacterUltimateRequired(*activePartyIndex)) {
            startTutorial(
                tutorialOverlay,
                TutorialStep::Ultimate,
                tutorialLibrary.ultimate,
                nowMs
            );
        }
    };

    const auto completeTutorialForAction = [&](battle::BattleAction action) {
        if (action == battle::BattleAction::Standard && tutorialOverlay.step == TutorialStep::Standard) {
            tutorialOverlay.standardShown = true;
            tutorialOverlay.step = TutorialStep::None;
        } else if (action == battle::BattleAction::Skill && tutorialOverlay.step == TutorialStep::Skill) {
            tutorialOverlay.skillShown = true;
            tutorialOverlay.step = TutorialStep::None;
        } else if (action == battle::BattleAction::Ultimate && tutorialOverlay.step == TutorialStep::Ultimate) {
            tutorialOverlay.ultimateShown = true;
            tutorialOverlay.step = TutorialStep::None;
        }
    };

    const auto finalizeSkillChallenge = [&](bool onBeat) {
        if (!rhythmChallenge.active) {
            return;
        }
        const Uint64 nowMs = SDL_GetTicks64();
        rhythmChallenge = RhythmChallengeState{};
        if (!manager.executePlayerAction(battle::BattleAction::Skill)) {
            showToast(hudFeedback, "ACTION NOT AVAILABLE.", nowMs);
            return;
        }
        showToast(hudFeedback, onBeat ? "ON-BEAT INPUT." : "LATE INPUT.", nowMs, 1100);
        completeTutorialForAction(battle::BattleAction::Skill);
        manager.processAutomaticTurns();
        consumeBattleActionEvents(hudFeedback, manager, nowMs);
    };

    const auto attemptAction = [&](battle::BattleAction action) {
        const Uint64 nowMs = SDL_GetTicks64();
        if (rhythmChallenge.active) {
            showToast(hudFeedback, "FINISH THE RHYTHM INPUT.", nowMs, 1200);
            return;
        }
        const std::optional<int> activePartyIndex = getActiveCharacterPartyIndex(manager);
        if (!activePartyIndex.has_value()) {
            showToast(hudFeedback, "WAIT FOR AN ALLY TURN.", nowMs);
            return;
        }

        if (action == battle::BattleAction::Ultimate && !manager.isPlayerActionReady(action)) {
            const int charge = manager.getCharacterUltimateCharge(*activePartyIndex);
            const int required = manager.getCharacterUltimateRequired(*activePartyIndex);
            const int missing = std::max(0, required - charge);
            showToast(
                hudFeedback,
                "ULTIMATE NEEDS " + std::to_string(missing) + " MORE ORB" + (missing == 1 ? "" : "S") + ".",
                nowMs
            );
            blinkMissingOrbs(hudFeedback, *activePartyIndex, charge, required - 1, nowMs);
            return;
        }

        if (action == battle::BattleAction::Skill && !manager.isPlayerActionReady(action)) {
            showToast(hudFeedback, "SKILL NOT AVAILABLE.", nowMs);
            return;
        }

        if (action == battle::BattleAction::Skill) {
            beginRhythmChallenge(*activePartyIndex);
            return;
        }

        if (!manager.executePlayerAction(action)) {
            showToast(hudFeedback, "ACTION NOT AVAILABLE.", nowMs);
            return;
        }

        completeTutorialForAction(action);
        manager.processAutomaticTurns();
        consumeBattleActionEvents(hudFeedback, manager, nowMs);
    };

    attachClick("action-standard", [&]() {
        attemptAction(battle::BattleAction::Standard);
    });
    attachClick("action-skill", [&]() {
        attemptAction(battle::BattleAction::Skill);
    });
    for (int i = 0; i < 4; ++i) {
        attachClick("unit-card-" + std::to_string(i + 1), [&, i]() {
            const Uint64 nowMs = SDL_GetTicks64();
            const std::optional<int> activePartyIndex = getActiveCharacterPartyIndex(manager);
            if (!activePartyIndex.has_value()) {
                showToast(hudFeedback, "WAIT FOR AN ALLY TURN.", nowMs);
                return;
            }
            if (*activePartyIndex != i) {
                showToast(hudFeedback, "ONLY THE ACTIVE UNIT CAN ULTIMATE.", nowMs);
                return;
            }
            attemptAction(battle::BattleAction::Ultimate);
        });
    }

    attachClick("battle-narration", [&]() {
        if (!rhythmChallenge.active) {
            return;
        }
        const Uint64 hitTime = SDL_GetTicks64();
        const float progress = getRhythmProgress(rhythmChallenge, hitTime);
        const float halfWindow = rhythmChallenge.targetWindow * 0.5f;
        finalizeSkillChallenge(progress >= rhythmChallenge.targetCenter - halfWindow &&
                               progress <= rhythmChallenge.targetCenter + halfWindow);
    });

    updateBattleHudDocument(document, manager, hudFeedback, tutorialOverlay, rhythmChallenge, SDL_GetTicks64());

    battle::Camera3D camera;
    camera.screenCenterX = windowWidth * 0.5f;
    camera.screenCenterY = windowHeight * 0.5f;
    applyGoalCamera(camera);

    CameraIntroAnimation cameraIntro;
    bool freeViewEnabled = false;
    battle::render::FreeViewCameraDebugLog freeViewCameraDebugLog;
    float cameraOscillationTime = 0.0f;
    float frameAccumulator = 0.0f;
    Uint64 lastFrameTime = SDL_GetTicks64();
    std::string lastTurnToken;

    bool running = true;
    while (running) {
        manager.processAutomaticTurns();
        consumeBattleActionEvents(hudFeedback, manager, SDL_GetTicks64());
        maybeStartTutorial(SDL_GetTicks64());
        if (tutorialOverlay.step != TutorialStep::None && !tutorialOverlay.audioPlayed && !tutorialOverlay.entry.voice.empty()) {
            if (gOneShotAudio.playWavOneShot(platform::path::resolvePath(tutorialOverlay.entry.voice))) {
                tutorialOverlay.audioPlayed = true;
            }
        }

        tickHudFeedback(hudFeedback, SDL_GetTicks64());
        gOneShotAudio.cleanupFinishedPlayback();

        const Uint64 currentTime = SDL_GetTicks64();
        const float deltaTime = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;
        frameAccumulator += deltaTime;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
                continue;
            }

            RmlSDL::InputEventHandler(context, window, event);

            const bool isRepeatedKeydown = event.type == SDL_KEYDOWN && event.key.repeat != 0;
            if (event.type == SDL_KEYDOWN && !isRepeatedKeydown) {
                if (event.key.keysym.sym == SDLK_f) {
                    freeViewEnabled = !freeViewEnabled;
                    if (!freeViewEnabled) {
                        applyGoalCamera(camera);
                        freeViewCameraDebugLog.clear();
                    }
                } else if (event.key.keysym.sym == SDLK_BACKSPACE && tutorialOverlay.step != TutorialStep::None) {
                    tutorialOverlay.standardShown = true;
                    tutorialOverlay.skillShown = true;
                    tutorialOverlay.ultimateShown = true;
                    tutorialOverlay.dismissed = true;
                    tutorialOverlay.step = TutorialStep::None;
                    tutorialOverlay.entry = vn::ScriptEntry{};
                    tutorialOverlay.startedMs = 0;
                    tutorialOverlay.audioPlayed = false;
                } else if (rhythmChallenge.active) {
                    if (event.key.keysym.sym == SDLK_e) {
                        const Uint64 hitTime = SDL_GetTicks64();
                        const float progress = getRhythmProgress(rhythmChallenge, hitTime);
                        const float halfWindow = rhythmChallenge.targetWindow * 0.5f;
                        finalizeSkillChallenge(progress >= rhythmChallenge.targetCenter - halfWindow &&
                                               progress <= rhythmChallenge.targetCenter + halfWindow);
                    }
                } else if (!freeViewEnabled && event.key.keysym.sym == SDLK_SPACE) {
                    attemptAction(battle::BattleAction::Ultimate);
                } else if (!freeViewEnabled && event.key.keysym.sym == SDLK_q) {
                    attemptAction(battle::BattleAction::Standard);
                } else if (!freeViewEnabled && event.key.keysym.sym == SDLK_e) {
                    attemptAction(battle::BattleAction::Skill);
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                if (freeViewEnabled && !cameraIntro.active) {
                    camera.focalLength += event.wheel.y * 500.0f;
                    camera.focalLength = std::clamp(camera.focalLength, 1000.0f, 50000.0f);
                }
            }

            if (isWindowResizeEvent(event)) {
                int resizedWidth = 0;
                int resizedHeight = 0;
                SDL_GetWindowSize(window, &resizedWidth, &resizedHeight);
                if (resizedWidth <= 0 || resizedHeight <= 0) {
                    continue;
                }

                windowWidth = resizedWidth;
                windowHeight = resizedHeight;
                renderInterface.SetViewport(windowWidth, windowHeight);
                context->SetDimensions(Rml::Vector2i(windowWidth, windowHeight));
                camera.screenCenterX = windowWidth * 0.5f;
                camera.screenCenterY = windowHeight * 0.5f;
                if (!sceneRenderer.initialize(windowWidth, windowHeight, worldAssets, resolveBattleSpritePath)) {
                    running = false;
                    break;
                }
            }
        }

        if (!running) {
            continue;
        }

        if (rhythmChallenge.active && currentTime >= rhythmChallenge.startedMs + rhythmChallenge.durationMs) {
            finalizeSkillChallenge(false);
        }

        int previewActorIndex = manager.getPreviewNextActorIndex();
        std::string turnToken = "none";
        bool nextIsCharacter = false;
        if (previewActorIndex >= 0) {
            const battle::TurnState& turnState = manager.getTurnState();
            if (previewActorIndex < static_cast<int>(turnState.actors.size())) {
                const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(previewActorIndex)];
                turnToken = (actor.type == battle::ParticipantType::Boss ? "B:" : "C:") +
                            actor.key + ":" + std::to_string(actor.partyIndex) + ":" +
                            (actor.isExtraTurn ? "E" : "N");
                nextIsCharacter = actor.type == battle::ParticipantType::Character;

                if (nextIsCharacter && actor.partyIndex >= 0 &&
                    actor.partyIndex < static_cast<int>(state.party.size())) {
                    const battle::CharacterDefinition& currentChar = state.party[static_cast<size_t>(actor.partyIndex)];
                    entities[0].key = currentChar.key;
                    entities[0].assetName = currentChar.assets;
                    entities[0].fallbackColor = battle::render::colorFromKey(currentChar.key, false);
                }
            }
        }

        if (turnToken != lastTurnToken) {
            if (nextIsCharacter && !freeViewEnabled) {
                startActionIntroCamera(camera, cameraIntro);
            }
            lastTurnToken = turnToken;
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float camMovementSpeed = 15.0f;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_W]) camera.posY += camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_S]) camera.posY -= camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_A]) camera.posX -= camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_D]) camera.posX += camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_E]) camera.posZ += camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_Q]) camera.posZ -= camMovementSpeed;

        const float rotationSpeed = 2.0f;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_LEFT]) camera.yawDegrees -= rotationSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_RIGHT]) camera.yawDegrees += rotationSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_UP]) camera.pitchDegrees -= rotationSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_DOWN]) camera.pitchDegrees += rotationSpeed;
        camera.pitchDegrees = battle::clampFreeViewPitchDegrees(camera.pitchDegrees);

        updateActionIntroCamera(camera, cameraIntro, deltaTime);

        cameraOscillationTime += deltaTime;
        if (!freeViewEnabled && !cameraIntro.active) {
            applyGoalCamera(camera);
            if (nextIsCharacter) {
                constexpr float kOscillationAmplitudeDegrees = 1.8f;
                constexpr float kOscillationSpeed = 0.55f;
                camera.yawDegrees = kGoalCameraYaw + std::sin(cameraOscillationTime * kOscillationSpeed) * kOscillationAmplitudeDegrees;
            }
        }

        freeViewCameraDebugLog.update(freeViewEnabled && !cameraIntro.active, camera);

        battle::render::renderBattleScene(sceneRenderer, camera, entities, nextIsCharacter ? 0 : 1, frameAccumulator);
        screenBlitter.uploadSurface(sceneRenderer.surface);

        updateBattleHudDocument(document, manager, hudFeedback, tutorialOverlay, rhythmChallenge, currentTime);

        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.035f, 0.043f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        screenBlitter.draw();

        renderInterface.BeginFrame();
        context->Update();
        context->Render();
        renderInterface.EndFrame();

        SDL_GL_SwapWindow(window);
    }

    freeViewCameraDebugLog.clear();
    document->Close();
    gOneShotAudio.shutdown();
    destroyNativeRenderers(&sceneRenderer);
    Rml::Shutdown();
    RmlGL3::Shutdown();
#ifdef BATTLE_ENABLE_IMAGE
    IMG_Quit();
#endif
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
