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
#include <RmlUi/Core/Elements/ElementFormControl.h>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "core/battle_manager.h"
#include "render/camera_3d.h"
#include "core/easing.h"
#include "core/turn_system.h"
#include "vn/vn_script.h"
#include "app_battle_session.h"
#include "../window.h"

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kSpriteFrameTime = 0.15f;

constexpr float kBossCharacterDistanceWorld = 420.0f;
constexpr float kCharacterGapWorld = 1200.0f;
constexpr float kDuelCharacterSlotX = -0.5f * kCharacterGapWorld;
constexpr float kDuelBossSlotX = 0.0f;
constexpr float kDuelCharacterBaseY = 300.0f;

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
constexpr float kMinSettingsTextSpeed = 18.0f;
constexpr float kMaxSettingsTextSpeed = 90.0f;

struct WorldEntity {
    std::string key;
    std::string assetName;
    bool isBoss = false;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    SDL_Color fallbackColor{200, 200, 200, 255};
};

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

struct PauseOverlayCopy {
    const char* title = "";
    const char* hint = "";
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

struct ActiveOneShotAudio {
    SDL_AudioDeviceID device = 0;
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

std::vector<ActiveOneShotAudio> gActiveOneShotAudio;

struct SoftwareSceneRenderer {
    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* floorTileTexture = nullptr;
    std::map<std::string, SDL_Texture*> textureByAsset;
    std::vector<std::string> loadedAssets;
    int width = 0;
    int height = 0;

    ~SoftwareSceneRenderer() {
        destroy();
    }

    void destroy() {
        if (floorTileTexture != nullptr) {
            SDL_DestroyTexture(floorTileTexture);
            floorTileTexture = nullptr;
        }
        for (auto& [_, texture] : textureByAsset) {
            if (texture != nullptr) {
                SDL_DestroyTexture(texture);
            }
        }
        textureByAsset.clear();
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (surface != nullptr) {
            SDL_FreeSurface(surface);
            surface = nullptr;
        }
        width = 0;
        height = 0;
    }

    bool initialize(int newWidth, int newHeight, const std::vector<std::string>& assetNames);
};

struct GlScreenBlitter {
    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;
    int textureWidth = 0;
    int textureHeight = 0;

    ~GlScreenBlitter() {
        destroy();
    }

    bool initialize();
    void destroy();
    void ensureTextureSize(int width, int height);
    void uploadSurface(SDL_Surface* surface);
    void draw();
};

std::string resolveBattlePath(const std::string& relativePath) {
    const std::array<std::string, 3> candidates = {
        relativePath,
        "../" + relativePath,
        "../../" + relativePath
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return relativePath;
}

std::string findFontPath() {
    const std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        resolveBattlePath("assets/rmlui/DejaVuSans.ttf"),
        resolveBattlePath("assets/rmlui/DejaVuSans-Bold.ttf")
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
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

std::string findCombatImagePath(const std::string& folder, const std::string& assetName) {
    const std::array<std::pair<std::string, std::string>, 2> candidates = {
        std::pair<std::string, std::string>{
            resolveBattlePath("assets/combat/" + folder + "/" + assetName + ".png"),
            "../combat/" + folder + "/" + assetName + ".png"
        },
        std::pair<std::string, std::string>{
            resolveBattlePath("assets/combat/" + folder + "/" + assetName + ".webp"),
            "../combat/" + folder + "/" + assetName + ".webp"
        }
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate.first)) {
            return candidate.second;
        }
    }
    return std::string();
}

std::optional<std::string> resolveCombatVoicePath(const std::string& assetName, const std::string& clipName) {
    if (assetName.empty() || clipName.empty()) {
        return std::nullopt;
    }

    const std::array<std::string, 2> candidates = {
        resolveBattlePath("assets/combat/voices/" + assetName + "/" + clipName + ".wav"),
        resolveBattlePath("assets/combat/voices/" + assetName + "." + clipName + ".wav")
    };
    for (const std::string& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

void cleanupFinishedOneShotAudio() {
    std::vector<ActiveOneShotAudio> stillPlaying;
    stillPlaying.reserve(gActiveOneShotAudio.size());
    for (const ActiveOneShotAudio& voice : gActiveOneShotAudio) {
        if (voice.device == 0) {
            continue;
        }
        if (SDL_GetQueuedAudioSize(voice.device) == 0) {
            SDL_CloseAudioDevice(voice.device);
        } else {
            stillPlaying.push_back(voice);
        }
    }
    gActiveOneShotAudio.swap(stillPlaying);
}

void shutdownOneShotAudio() {
    for (const ActiveOneShotAudio& voice : gActiveOneShotAudio) {
        if (voice.device != 0) {
            SDL_CloseAudioDevice(voice.device);
        }
    }
    gActiveOneShotAudio.clear();
}

bool playWavOneShot(const std::string& wavPath, float volume = 1.0f) {
    SDL_AudioSpec wavSpec{};
    Uint8* wavBuffer = nullptr;
    Uint32 wavLength = 0;
    if (SDL_LoadWAV(wavPath.c_str(), &wavSpec, &wavBuffer, &wavLength) == nullptr) {
        return false;
    }

    SDL_AudioDeviceID device = SDL_OpenAudioDevice(nullptr, 0, &wavSpec, nullptr, 0);
    if (device == 0) {
        SDL_FreeWAV(wavBuffer);
        return false;
    }

    const int mixVolume = std::clamp(static_cast<int>(std::lround(std::clamp(volume, 0.0f, 1.0f) * SDL_MIX_MAXVOLUME)),
                                     0,
                                     SDL_MIX_MAXVOLUME);
    std::vector<Uint8> playbackBuffer(static_cast<size_t>(wavLength), 0);
    SDL_MixAudioFormat(playbackBuffer.data(), wavBuffer, wavSpec.format, wavLength, mixVolume);
    const int queueResult = SDL_QueueAudio(device, playbackBuffer.data(), wavLength);
    SDL_FreeWAV(wavBuffer);
    if (queueResult != 0) {
        SDL_CloseAudioDevice(device);
        return false;
    }

    SDL_PauseAudioDevice(device, 0);
    gActiveOneShotAudio.push_back(ActiveOneShotAudio{device});
    return true;
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

void setRangeValue(Rml::ElementDocument* document, const std::string& id, const std::string& value) {
    if (Rml::Element* element = document->GetElementById(id)) {
        if (auto* control = dynamic_cast<Rml::ElementFormControl*>(element)) {
            control->SetValue(value);
        }
    }
}

void setPortraitDecorator(Rml::ElementDocument* document, const std::string& id, const std::string& assetName) {
    if (Rml::Element* element = document->GetElementById(id)) {
        const std::string path = findCombatImagePath("icons", assetName);
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

std::string revealNarrationText(const std::string& fullText, Uint64 startedMs, Uint64 nowMs, float charsPerSecond) {
    if (fullText.empty()) {
        return std::string();
    }

    if (nowMs <= startedMs) {
        return std::string();
    }

    const double elapsedSeconds = static_cast<double>(nowMs - startedMs) / 1000.0;
    const size_t visibleChars = static_cast<size_t>(std::floor(elapsedSeconds * charsPerSecond));
    if (visibleChars >= fullText.size()) {
        return fullText;
    }
    return fullText.substr(0, visibleChars);
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
    if (!vn::loadScript(resolveBattlePath("assets/vn/json/demo.json"), script)) {
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

PauseOverlayCopy getPauseOverlayCopy(PauseOverlayMode mode) {
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

void applyPauseOverlayDocumentState(Rml::ElementDocument* document,
                                    PauseOverlayMode pauseOverlayMode,
                                    PauseSelection pauseSelection,
                                    SettingsSelection settingsSelection,
                                    const GameSettings* settings) {
    const bool showingSettings = pauseOverlayMode == PauseOverlayMode::Settings;
    const PauseOverlayCopy copy = getPauseOverlayCopy(pauseOverlayMode);

    setElementText(document, "battle-pause-title", copy.title);
    setElementText(document, "battle-pause-hint", copy.hint);
    setElementClass(document, "battle-pause-shell", "settings-open", showingSettings);
    setElementClass(document, "battle-pause-menu", "visible", pauseOverlayMode == PauseOverlayMode::Menu);
    setElementClass(document, "battle-pause-settings", "visible", showingSettings);

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
        setElementText(document, "battle-settings-display-value", settings->fullscreen ? "Fullscreen" : "Windowed");
        setElementText(document, "battle-settings-voice-value",
                       std::to_string(static_cast<int>(std::lround(settings->voiceVolume * 100.0f))) + "%");
        setElementText(document, "battle-settings-text-value",
                       std::to_string(static_cast<int>(std::lround(settings->textSpeed))) + " cps");
        setRangeValue(document, "battle-settings-voice-slider",
                      std::to_string(static_cast<int>(std::lround(settings->voiceVolume * 100.0f))));
        setRangeValue(document, "battle-settings-text-slider",
                      std::to_string(static_cast<int>(std::lround(settings->textSpeed))));
    }

    setElementClass(document, "battle-settings-row-display", "selected", settingsSelection == SettingsSelection::DisplayMode);
    setElementClass(document, "battle-settings-row-voice", "selected", settingsSelection == SettingsSelection::VoiceVolume);
    setElementClass(document, "battle-settings-row-text", "selected", settingsSelection == SettingsSelection::TextSpeed);
    setElementClass(document, "battle-settings-row-back", "selected", settingsSelection == SettingsSelection::Back);
}

void updateBattleHudDocument(Rml::ElementDocument* document,
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
        actionStandard->SetClass("disabled", paused || rhythm.active || !playerCanAct || !manager.isPlayerActionReady(battle::BattleAction::Standard));
    }
    if (Rml::Element* actionSkill = document->GetElementById("action-skill")) {
        actionSkill->SetClass("disabled", paused || rhythm.active || !playerCanAct || !manager.isPlayerActionReady(battle::BattleAction::Skill));
    }
    if (Rml::Element* toast = document->GetElementById("battle-toast")) {
        toast->SetInnerRML(feedback.toastText);
        toast->SetClass("visible", !feedback.toastText.empty());
    }

    setElementClass(document, "battle-tutorial", "visible", tutorial.step != TutorialStep::None);
    if (tutorial.step != TutorialStep::None) {
        setElementText(document, "battle-tutorial-speaker", vn::getDisplaySpeakerName(tutorial.entry));
        setElementText(document, "battle-tutorial-text", revealNarrationText(tutorial.entry.text, tutorial.startedMs, nowMs, narrationCharsPerSecond));
        if (!tutorial.entry.icon.empty()) {
            const std::string tutorialIconKey = std::filesystem::path(tutorial.entry.icon).stem().string();
            const std::string iconPath = findCombatImagePath("icons", tutorialIconKey);
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

    setElementClass(document, "battle-pause", "visible", paused);
    if (paused) {
        applyPauseOverlayDocumentState(document, pauseOverlayMode, pauseSelection, settingsSelection, settings);
    }
}

void consumeBattleActionEvents(HudFeedbackState& feedback,
                               battle::BattleManager& manager,
                               float voiceVolume,
                               Uint64 nowMs) {
    const battle::BattleState& battleState = manager.getBattleState();
    (void)feedback;
    (void)nowMs;
    for (const battle::BattleActionEvent& event : manager.getRecentActionEvents()) {
        const std::string actorAsset = event.actorType == battle::ParticipantType::Boss
            ? battleState.boss.assets
            : ((event.actorPartyIndex >= 0 && event.actorPartyIndex < static_cast<int>(battleState.party.size()))
                ? battleState.party[static_cast<size_t>(event.actorPartyIndex)].assets
                : std::string());

        if (event.action == battle::BattleAction::Skill) {
            if (const auto skillVoice = resolveCombatVoicePath(actorAsset, "skill"); skillVoice.has_value()) {
                (void)playWavOneShot(*skillVoice, voiceVolume);
            }
        }

        if (event.bossHpAfter < event.bossHpBefore) {
            if (const auto hitVoice = resolveCombatVoicePath(battleState.boss.assets, "hit"); hitVoice.has_value()) {
                (void)playWavOneShot(*hitVoice, voiceVolume);
            }
        }

        for (size_t i = 0; i < event.targetPartyIndices.size() && i < event.targetHpBefore.size() && i < event.targetHpAfter.size(); ++i) {
            if (event.targetHpAfter[i] >= event.targetHpBefore[i]) {
                continue;
            }
            const int partyIndex = event.targetPartyIndices[i];
            if (partyIndex < 0 || partyIndex >= static_cast<int>(battleState.party.size())) {
                continue;
            }
            if (const auto hitVoice = resolveCombatVoicePath(battleState.party[static_cast<size_t>(partyIndex)].assets, "hit");
                hitVoice.has_value()) {
                (void)playWavOneShot(*hitVoice, voiceVolume);
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

SDL_Color colorFromKey(const std::string& key, bool boss) {
    unsigned hash = 2166136261u;
    for (char c : key) {
        hash ^= static_cast<unsigned>(static_cast<unsigned char>(c));
        hash *= 16777619u;
    }
    const Uint8 r = static_cast<Uint8>(80 + (hash & 0x7F));
    const Uint8 g = static_cast<Uint8>(80 + ((hash >> 8) & 0x7F));
    const Uint8 b = static_cast<Uint8>(80 + ((hash >> 16) & 0x7F));
    return boss ? SDL_Color{static_cast<Uint8>(std::min(255, r + 30)), 90, 90, 255} : SDL_Color{r, g, b, 255};
}

SDL_Texture* createFloorTileTexture(SDL_Renderer* renderer) {
    constexpr int texSize = 64;
    constexpr int cell = 16;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, texSize, texSize, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return nullptr;
    }

    const Uint32 c0 = SDL_MapRGBA(surface->format, 46, 49, 60, 255);
    const Uint32 c1 = SDL_MapRGBA(surface->format, 52, 56, 69, 255);

    SDL_Rect r{0, 0, cell, cell};
    for (int y = 0; y < texSize; y += cell) {
        for (int x = 0; x < texSize; x += cell) {
            r.x = x;
            r.y = y;
            const bool alt = ((x / cell) + (y / cell)) % 2 == 0;
            SDL_FillRect(surface, &r, alt ? c0 : c1);
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

std::optional<SDL_Texture*> tryLoadTexture(SDL_Renderer* renderer, const std::string& assetName) {
#ifdef BATTLE_ENABLE_IMAGE
    const std::array<std::string, 2> candidates = {
        resolveBattlePath("assets/combat/sprites/" + assetName + ".png"),
        resolveBattlePath("assets/combat/sprites/" + assetName + ".webp")
    };

    for (const std::string& path : candidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface == nullptr) {
            continue;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (texture != nullptr) {
            return texture;
        }
    }
#else
    (void)renderer;
    (void)assetName;
#endif
    return std::nullopt;
}

void drawFloorGridLines(SDL_Renderer* renderer, int screenW, int screenH, const battle::Camera3D& camera) {
    const float floorZ = 0.0f;
    const float centerX = -300.0f;
    const float centerY = 510.0f;
    const float tileSize = 200.0f;
    const int tilesX = 14;
    const int tilesY = 16;

    const float startX = centerX - (tilesX * tileSize * 0.5f);
    const float startY = centerY - (tilesY * tileSize * 0.30f);

    SDL_SetRenderDrawColor(renderer, 62, 70, 90, 160);
    for (int tx = 0; tx <= tilesX; ++tx) {
        const float x = startX + tx * tileSize;
        const SDL_FPoint p0 = camera.worldToScreen(x, startY, floorZ);
        const SDL_FPoint p1 = camera.worldToScreen(x, startY + tilesY * tileSize, floorZ);
        if ((p0.x > -200.0f || p1.x > -200.0f) && (p0.x < screenW + 200.0f || p1.x < screenW + 200.0f) &&
            (p0.y > -200.0f || p1.y > -200.0f) && (p0.y < screenH + 200.0f || p1.y < screenH + 200.0f)) {
            SDL_RenderDrawLineF(renderer, p0.x, p0.y, p1.x, p1.y);
        }
    }
    for (int ty = 0; ty <= tilesY; ++ty) {
        const float y = startY + ty * tileSize;
        const SDL_FPoint p0 = camera.worldToScreen(startX, y, floorZ);
        const SDL_FPoint p1 = camera.worldToScreen(startX + tilesX * tileSize, y, floorZ);
        if ((p0.x > -200.0f || p1.x > -200.0f) && (p0.x < screenW + 200.0f || p1.x < screenW + 200.0f) &&
            (p0.y > -200.0f || p1.y > -200.0f) && (p0.y < screenH + 200.0f || p1.y < screenH + 200.0f)) {
            SDL_RenderDrawLineF(renderer, p0.x, p0.y, p1.x, p1.y);
        }
    }
}

void drawFloor(SDL_Renderer* renderer, int screenW, int screenH, const battle::Camera3D& camera, SDL_Texture* floorTileTexture) {
    if (floorTileTexture != nullptr) {
        const float floorZ = 0.0f;
        const float centerX = -300.0f;
        const float centerY = 510.0f;
        const float tileSize = 200.0f;
        const int tilesX = 14;
        const int tilesY = 16;

        const float startX = centerX - (tilesX * tileSize * 0.5f);
        const float startY = centerY - (tilesY * tileSize * 0.30f);

        const int indices[6] = {0, 1, 2, 0, 2, 3};
        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                const float x0 = startX + tx * tileSize;
                const float y0 = startY + ty * tileSize;
                const float x1 = x0 + tileSize;
                const float y1 = y0 + tileSize;

                const float d00 = camera.getDepth(x0, y0, floorZ);
                const float d10 = camera.getDepth(x1, y0, floorZ);
                const float d11 = camera.getDepth(x1, y1, floorZ);
                const float d01 = camera.getDepth(x0, y1, floorZ);
                if (d00 <= 1.0f && d10 <= 1.0f && d11 <= 1.0f && d01 <= 1.0f) {
                    continue;
                }

                const SDL_FPoint p00 = camera.worldToScreen(x0, y0, floorZ);
                const SDL_FPoint p10 = camera.worldToScreen(x1, y0, floorZ);
                const SDL_FPoint p11 = camera.worldToScreen(x1, y1, floorZ);
                const SDL_FPoint p01 = camera.worldToScreen(x0, y1, floorZ);

                const float minX = std::min(std::min(p00.x, p10.x), std::min(p11.x, p01.x));
                const float maxX = std::max(std::max(p00.x, p10.x), std::max(p11.x, p01.x));
                const float minY = std::min(std::min(p00.y, p10.y), std::min(p11.y, p01.y));
                const float maxY = std::max(std::max(p00.y, p10.y), std::max(p11.y, p01.y));
                if (maxX < -200.0f || minX > screenW + 200.0f || maxY < -200.0f || minY > screenH + 200.0f) {
                    continue;
                }

                SDL_Vertex verts[4];
                verts[0].position = p00;
                verts[1].position = p10;
                verts[2].position = p11;
                verts[3].position = p01;
                verts[0].color = SDL_Color{255, 255, 255, 255};
                verts[1].color = SDL_Color{255, 255, 255, 255};
                verts[2].color = SDL_Color{255, 255, 255, 255};
                verts[3].color = SDL_Color{255, 255, 255, 255};
                verts[0].tex_coord = SDL_FPoint{0.0f, 0.0f};
                verts[1].tex_coord = SDL_FPoint{1.0f, 0.0f};
                verts[2].tex_coord = SDL_FPoint{1.0f, 1.0f};
                verts[3].tex_coord = SDL_FPoint{0.0f, 1.0f};

                SDL_RenderGeometry(renderer, floorTileTexture, verts, 4, indices, 6);
            }
        }
    }

    drawFloorGridLines(renderer, screenW, screenH, camera);
}

void renderBattleScene(SoftwareSceneRenderer& sceneRenderer,
                       const battle::Camera3D& camera,
                       const std::vector<WorldEntity>& entities,
                       int focusedEntityIndex,
                       float frameAccumulator) {
    SDL_SetRenderDrawBlendMode(sceneRenderer.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sceneRenderer.renderer, 16, 18, 26, 255);
    SDL_RenderClear(sceneRenderer.renderer);

    drawFloor(sceneRenderer.renderer, sceneRenderer.width, sceneRenderer.height, camera, sceneRenderer.floorTileTexture);

    struct DrawCall {
        size_t index;
        float depth;
        SDL_FPoint screen;
    };

    std::vector<DrawCall> drawList;
    drawList.reserve(entities.size());
    for (size_t i = 0; i < entities.size(); ++i) {
        const WorldEntity& entity = entities[i];
        drawList.push_back(DrawCall{
            i,
            camera.getDepth(entity.worldX, entity.worldY, entity.worldZ),
            camera.worldToScreen(entity.worldX, entity.worldY, entity.worldZ)
        });
    }

    std::sort(drawList.begin(), drawList.end(), [](const DrawCall& a, const DrawCall& b) {
        return a.depth > b.depth;
    });

    const WorldEntity* focusedEntity = &entities[std::clamp(focusedEntityIndex, 0, static_cast<int>(entities.size() - 1))];

    for (const DrawCall& call : drawList) {
        const WorldEntity& entity = entities[call.index];
        const float scale = camera.getPerspectiveScale(entity.worldX, entity.worldY, entity.worldZ);
        const bool isFocused = (&entity == focusedEntity);
        const float focusScale = isFocused ? 1.13f : 1.0f;

        const int drawW = static_cast<int>(kSuggestedBaseSpriteWidth * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
        const int drawH = static_cast<int>(kSuggestedBaseSpriteHeight * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
        SDL_Rect dst{
            static_cast<int>(call.screen.x) - drawW / 2,
            static_cast<int>(call.screen.y) - drawH,
            std::max(8, drawW),
            std::max(8, drawH)
        };

        SDL_Texture* texture = nullptr;
        const auto texIt = sceneRenderer.textureByAsset.find(entity.assetName);
        if (texIt != sceneRenderer.textureByAsset.end()) {
            texture = texIt->second;
        }

        if (texture != nullptr) {
            int texW = 0;
            int texH = 0;
            SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);
            const int frameCount = texW / kSuggestedBaseSpriteWidth;
            const bool isAnimated = (frameCount > 1) && (texW % kSuggestedBaseSpriteWidth == 0);
            if (isAnimated) {
                const int currentFrame = static_cast<int>(frameAccumulator / kSpriteFrameTime) % frameCount;
                SDL_Rect srcRect{currentFrame * kSuggestedBaseSpriteWidth, 0, kSuggestedBaseSpriteWidth, texH};
                SDL_RenderCopy(sceneRenderer.renderer, texture, &srcRect, &dst);
            } else {
                SDL_RenderCopy(sceneRenderer.renderer, texture, nullptr, &dst);
            }
        } else {
            SDL_SetRenderDrawColor(sceneRenderer.renderer, entity.fallbackColor.r, entity.fallbackColor.g, entity.fallbackColor.b, 255);
            SDL_RenderFillRect(sceneRenderer.renderer, &dst);
            SDL_SetRenderDrawColor(sceneRenderer.renderer, 16, 16, 20, 255);
            SDL_RenderDrawRect(sceneRenderer.renderer, &dst);
        }

        if (isFocused) {
            SDL_SetRenderDrawColor(sceneRenderer.renderer, 250, 230, 96, 255);
            SDL_Rect ring{dst.x - 6, dst.y - 6, dst.w + 12, dst.h + 12};
            SDL_RenderDrawRect(sceneRenderer.renderer, &ring);
        }
    }

    SDL_RenderPresent(sceneRenderer.renderer);
}

bool SoftwareSceneRenderer::initialize(int newWidth, int newHeight, const std::vector<std::string>& assetNames) {
    destroy();

    width = newWidth;
    height = newHeight;
    loadedAssets = assetNames;

    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        std::cerr << "Failed to create software surface: " << SDL_GetError() << "\n";
        return false;
    }

    renderer = SDL_CreateSoftwareRenderer(surface);
    if (renderer == nullptr) {
        std::cerr << "Failed to create software renderer: " << SDL_GetError() << "\n";
        destroy();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    floorTileTexture = createFloorTileTexture(renderer);

    for (const std::string& assetName : loadedAssets) {
        const auto loaded = tryLoadTexture(renderer, assetName);
        textureByAsset[assetName] = loaded.has_value() ? *loaded : nullptr;
    }

    return true;
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<size_t>(std::max(1, logLength)), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    std::cerr << "Shader compilation failed: " << log << "\n";
    glDeleteShader(shader);
    return 0;
}

bool GlScreenBlitter::initialize() {
    static const char* kVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 in_position;
        layout (location = 1) in vec2 in_uv;
        out vec2 frag_uv;
        void main() {
            frag_uv = in_uv;
            gl_Position = vec4(in_position, 0.0, 1.0);
        }
    )";

    static const char* kFragmentShader = R"(
        #version 330 core
        in vec2 frag_uv;
        uniform sampler2D scene_texture;
        out vec4 out_color;
        void main() {
            out_color = texture(scene_texture, frag_uv);
        }
    )";

    vertexShader = compileShader(GL_VERTEX_SHADER, kVertexShader);
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertexShader == 0 || fragmentShader == 0) {
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<size_t>(std::max(1, logLength)), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::cerr << "Program link failed: " << log << "\n";
        destroy();
        return false;
    }

    const float quadVertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void GlScreenBlitter::destroy() {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (program != 0) {
        glDeleteProgram(program);
        program = 0;
    }
    if (vertexShader != 0) {
        glDeleteShader(vertexShader);
        vertexShader = 0;
    }
    if (fragmentShader != 0) {
        glDeleteShader(fragmentShader);
        fragmentShader = 0;
    }
    textureWidth = 0;
    textureHeight = 0;
}

void GlScreenBlitter::ensureTextureSize(int width, int height) {
    if (textureWidth == width && textureHeight == height) {
        return;
    }

    textureWidth = width;
    textureHeight = height;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GlScreenBlitter::uploadSurface(SDL_Surface* surface) {
    if (surface == nullptr) {
        return;
    }

    ensureTextureSize(surface->w, surface->h);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface->w, surface->h, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GlScreenBlitter::draw() {
    glDisable(GL_BLEND);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "scene_texture"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
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

namespace battle::app {

class SessionImpl {
public:
    bool initialize(Window& hostWindow, GameSettings& settings) {
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

        systemInterface_.SetWindow(window_);
        renderInterface_ = std::make_unique<RenderInterfaceGL3SDL>();
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

        const std::string fontPath = findFontPath();
        if (!fontPath.empty()) {
            Rml::LoadFontFace(fontPath);
        }

        if (!manager_.initialize("lyoo", {"iroha", "kaguya", "miku", "cupcakke"})) {
            std::cerr << "[Battle] Initialization failed.\n";
            shutdown();
            return false;
        }

        if (!loadTutorialScriptLibrary(tutorialLibrary_)) {
            std::cerr << "Failed to load tutorial script: assets/vn/json/demo.json\n";
            shutdown();
            return false;
        }

        const battle::BattleState& state = manager_.getBattleState();
        worldAssets_.clear();
        for (const battle::CharacterDefinition& character : state.party) {
            worldAssets_.push_back(character.assets);
        }
        worldAssets_.push_back(state.boss.assets);

        windowWidth_ = windowHost_->getWidth();
        windowHeight_ = windowHost_->getHeight();
        if (!sceneRenderer_.initialize(windowWidth_, windowHeight_, worldAssets_)) {
            shutdown();
            return false;
        }

        entities_.clear();
        entities_.reserve(2);
        entities_.push_back(WorldEntity{
            state.party.front().key,
            state.party.front().assets,
            false,
            kDuelCharacterSlotX,
            kDuelCharacterBaseY,
            0.0f,
            colorFromKey(state.party.front().key, false)
        });
        entities_.push_back(WorldEntity{
            state.boss.key,
            state.boss.assets,
            true,
            kDuelBossSlotX,
            kDuelCharacterBaseY + kBossCharacterDistanceWorld,
            0.0f,
            colorFromKey(state.boss.key, true)
        });

        renderInterface_->SetViewport(windowWidth_, windowHeight_);
        context_ = Rml::CreateContext("battle-app", Rml::Vector2i(windowWidth_, windowHeight_));
        if (context_ == nullptr) {
            std::cerr << "Failed to create RmlUi context\n";
            shutdown();
            return false;
        }

        const std::string documentPath = resolveBattlePath("assets/rmlui/battle_hud.rml");
        document_ = context_->LoadDocument(documentPath);
        if (document_ == nullptr) {
            std::cerr << "Failed to load RmlUi document: " << documentPath << "\n";
            shutdown();
            return false;
        }
        document_->Show();

        attachListener("action-standard", Rml::EventId::Click, [this](Rml::Event&) {
            attemptAction(battle::BattleAction::Standard);
        });
        attachListener("action-skill", Rml::EventId::Click, [this](Rml::Event&) {
            attemptAction(battle::BattleAction::Skill);
        });
        for (int i = 0; i < 4; ++i) {
            attachListener("unit-card-" + std::to_string(i + 1), Rml::EventId::Click, [this, i](Rml::Event&) {
                const Uint64 nowMs = SDL_GetTicks64();
                const std::optional<int> activePartyIndex = getActiveCharacterPartyIndex(manager_);
                if (!activePartyIndex.has_value()) {
                    showToast(hudFeedback_, "WAIT FOR AN ALLY TURN.", nowMs);
                    return;
                }
                if (*activePartyIndex != i) {
                    showToast(hudFeedback_, "ONLY THE ACTIVE UNIT CAN ULTIMATE.", nowMs);
                    return;
                }
                attemptAction(battle::BattleAction::Ultimate);
            });
        }
        attachListener("battle-rhythm", Rml::EventId::Click, [this](Rml::Event&) {
            if (!rhythmChallenge_.active) {
                return;
            }
            const Uint64 hitTime = SDL_GetTicks64();
            const float progress = getRhythmProgress(rhythmChallenge_, hitTime);
            const float halfWindow = rhythmChallenge_.targetWindow * 0.5f;
            finalizeSkillChallenge(progress >= rhythmChallenge_.targetCenter - halfWindow &&
                                   progress <= rhythmChallenge_.targetCenter + halfWindow);
        });
        attachListener("battle-pause-continue", Rml::EventId::Click, [this](Rml::Event&) {
            pauseOverlayMode_ = PauseOverlayMode::Menu;
            paused_ = false;
            pauseSelection_ = PauseSelection::Continue;
        });
        attachListener("battle-pause-settings-button", Rml::EventId::Click, [this](Rml::Event&) {
            pauseOverlayMode_ = PauseOverlayMode::Settings;
        });
        attachListener("battle-pause-exit", Rml::EventId::Click, [this](Rml::Event&) {
            finished_ = true;
        });
        attachListener("battle-settings-row-display", Rml::EventId::Click, [this](Rml::Event&) {
            toggleDisplayMode();
        });
        attachListener("battle-settings-row-back", Rml::EventId::Click, [this](Rml::Event&) {
            pauseOverlayMode_ = PauseOverlayMode::Menu;
        });
        attachListener("battle-pause-continue", Rml::EventId::Mouseover, [this](Rml::Event&) {
            pauseSelection_ = PauseSelection::Continue;
        });
        attachListener("battle-pause-settings-button", Rml::EventId::Mouseover, [this](Rml::Event&) {
            pauseSelection_ = PauseSelection::Settings;
        });
        attachListener("battle-pause-exit", Rml::EventId::Mouseover, [this](Rml::Event&) {
            pauseSelection_ = PauseSelection::ExitToMainMenu;
        });
        attachListener("battle-settings-row-display", Rml::EventId::Mouseover, [this](Rml::Event&) {
            settingsSelection_ = SettingsSelection::DisplayMode;
        });
        attachListener("battle-settings-row-voice", Rml::EventId::Mouseover, [this](Rml::Event&) {
            settingsSelection_ = SettingsSelection::VoiceVolume;
        });
        attachListener("battle-settings-row-text", Rml::EventId::Mouseover, [this](Rml::Event&) {
            settingsSelection_ = SettingsSelection::TextSpeed;
        });
        attachListener("battle-settings-row-back", Rml::EventId::Mouseover, [this](Rml::Event&) {
            settingsSelection_ = SettingsSelection::Back;
        });
        attachListener("battle-settings-voice-slider", Rml::EventId::Mouseover, [this](Rml::Event&) {
            settingsSelection_ = SettingsSelection::VoiceVolume;
        });
        attachListener("battle-settings-text-slider", Rml::EventId::Mouseover, [this](Rml::Event&) {
            settingsSelection_ = SettingsSelection::TextSpeed;
        });
        attachListener("battle-settings-voice-slider", Rml::EventId::Change, [this](Rml::Event& event) {
            settingsSelection_ = SettingsSelection::VoiceVolume;
            if (settings_ == nullptr) {
                return;
            }
            settings_->voiceVolume =
                std::clamp(event.GetParameter<float>("value", settings_->voiceVolume * 100.0f) / 100.0f, 0.0f, 1.0f);
        });
        attachListener("battle-settings-text-slider", Rml::EventId::Change, [this](Rml::Event& event) {
            settingsSelection_ = SettingsSelection::TextSpeed;
            if (settings_ == nullptr) {
                return;
            }
            settings_->textSpeed = std::clamp(event.GetParameter<float>("value", settings_->textSpeed),
                                              kMinSettingsTextSpeed, kMaxSettingsTextSpeed);
        });

        camera_.screenCenterX = windowWidth_ * 0.5f;
        camera_.screenCenterY = windowHeight_ * 0.5f;
        applyGoalCamera(camera_);
        updateBattleHudDocument(document_, manager_, hudFeedback_, tutorialOverlay_, rhythmChallenge_, paused_,
                                pauseOverlayMode_, pauseSelection_, settingsSelection_, settings_,
                                settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond,
                                SDL_GetTicks64());

        initialized_ = true;
        return true;
    }

    void shutdown() {
        initialized_ = false;
        if (document_ != nullptr) {
            document_->Close();
            document_ = nullptr;
        }
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
        sceneRenderer_.destroy();
        shutdownOneShotAudio();
        cleanupFinishedOneShotAudio();
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
        paused_ = false;
        freeViewEnabled_ = false;
        pauseOverlayMode_ = PauseOverlayMode::Menu;
        pauseSelection_ = PauseSelection::Continue;
        settingsSelection_ = SettingsSelection::DisplayMode;
        settings_ = nullptr;
        tutorialOverlay_ = TutorialOverlayState{};
        tutorialLibrary_ = TutorialScriptLibrary{};
        hudFeedback_ = HudFeedbackState{};
        rhythmChallenge_ = RhythmChallengeState{};
        worldAssets_.clear();
        entities_.clear();
        lastTurnToken_.clear();
        cameraOscillationTime_ = 0.0f;
        frameAccumulator_ = 0.0f;
    }

    void handleEvent(const SDL_Event& event) {
        if (!initialized_ || context_ == nullptr || window_ == nullptr) {
            return;
        }

        if (isWindowResizeEvent(event)) {
            windowWidth_ = windowHost_->getWidth();
            windowHeight_ = windowHost_->getHeight();
            renderInterface_->SetViewport(windowWidth_, windowHeight_);
            context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
            camera_.screenCenterX = windowWidth_ * 0.5f;
            camera_.screenCenterY = windowHeight_ * 0.5f;
            if (!sceneRenderer_.initialize(windowWidth_, windowHeight_, worldAssets_)) {
                finished_ = true;
            }
        }

        if (event.type == SDL_KEYDOWN && event.key.repeat != 0) {
            return;
        }

        if (paused_) {
            SDL_Event mutablePausedEvent = event;
            RmlSDL::InputEventHandler(context_, window_, mutablePausedEvent);
            handlePauseEvent(event);
            return;
        }

        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            paused_ = true;
            pauseOverlayMode_ = PauseOverlayMode::Menu;
            pauseSelection_ = PauseSelection::Continue;
            return;
        }

        SDL_Event mutableEvent = event;
        RmlSDL::InputEventHandler(context_, window_, mutableEvent);

        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_f) {
                freeViewEnabled_ = !freeViewEnabled_;
                if (!freeViewEnabled_) {
                    applyGoalCamera(camera_);
                }
            } else if (rhythmChallenge_.active) {
                if (event.key.keysym.sym == SDLK_e) {
                    const Uint64 hitTime = SDL_GetTicks64();
                    const float progress = getRhythmProgress(rhythmChallenge_, hitTime);
                    const float halfWindow = rhythmChallenge_.targetWindow * 0.5f;
                    finalizeSkillChallenge(progress >= rhythmChallenge_.targetCenter - halfWindow &&
                                           progress <= rhythmChallenge_.targetCenter + halfWindow);
                }
            } else if (!freeViewEnabled_ && event.key.keysym.sym == SDLK_SPACE) {
                attemptAction(battle::BattleAction::Ultimate);
            } else if (!freeViewEnabled_ && event.key.keysym.sym == SDLK_q) {
                attemptAction(battle::BattleAction::Standard);
            } else if (!freeViewEnabled_ && event.key.keysym.sym == SDLK_e) {
                attemptAction(battle::BattleAction::Skill);
            }
        } else if (event.type == SDL_MOUSEWHEEL) {
            if (freeViewEnabled_ && !cameraIntro_.active) {
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
        cleanupFinishedOneShotAudio();

        if (paused_) {
            updateBattleHudDocument(document_, manager_, hudFeedback_, tutorialOverlay_, rhythmChallenge_, paused_,
                                    pauseOverlayMode_, pauseSelection_, settingsSelection_, settings_,
                                    settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond,
                                    nowMs);
            return;
        }

        manager_.processAutomaticTurns();
        consumeBattleActionEvents(hudFeedback_, manager_, settings_ != nullptr ? settings_->voiceVolume : 1.0f, nowMs);
        maybeStartTutorial(nowMs);
        if (tutorialOverlay_.step != TutorialStep::None && !tutorialOverlay_.audioPlayed && !tutorialOverlay_.entry.voice.empty()) {
            if (playWavOneShot(resolveBattlePath(tutorialOverlay_.entry.voice), settings_ != nullptr ? settings_->voiceVolume : 1.0f)) {
                tutorialOverlay_.audioPlayed = true;
            }
        }

        if (rhythmChallenge_.active && nowMs >= rhythmChallenge_.startedMs + rhythmChallenge_.durationMs) {
            finalizeSkillChallenge(false);
        }

        frameAccumulator_ += deltaSeconds;

        int previewActorIndex = manager_.getPreviewNextActorIndex();
        std::string turnToken = "none";
        bool nextIsCharacter = false;
        const battle::BattleState& state = manager_.getBattleState();
        if (previewActorIndex >= 0) {
            const battle::TurnState& turnState = manager_.getTurnState();
            if (previewActorIndex < static_cast<int>(turnState.actors.size())) {
                const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(previewActorIndex)];
                turnToken = (actor.type == battle::ParticipantType::Boss ? "B:" : "C:") +
                            actor.key + ":" + std::to_string(actor.partyIndex) + ":" +
                            (actor.isExtraTurn ? "E" : "N");
                nextIsCharacter = actor.type == battle::ParticipantType::Character;
                if (nextIsCharacter && actor.partyIndex >= 0 &&
                    actor.partyIndex < static_cast<int>(state.party.size())) {
                    const battle::CharacterDefinition& currentChar = state.party[static_cast<size_t>(actor.partyIndex)];
                    entities_[0].key = currentChar.key;
                    entities_[0].assetName = currentChar.assets;
                    entities_[0].fallbackColor = colorFromKey(currentChar.key, false);
                }
            }
        }

        if (turnToken != lastTurnToken_) {
            if (nextIsCharacter && !freeViewEnabled_) {
                startActionIntroCamera(camera_, cameraIntro_);
            }
            lastTurnToken_ = turnToken;
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float camMovementSpeed = 15.0f;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_W]) camera_.posY += camMovementSpeed;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_S]) camera_.posY -= camMovementSpeed;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_A]) camera_.posX -= camMovementSpeed;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_D]) camera_.posX += camMovementSpeed;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_E]) camera_.posZ += camMovementSpeed;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_Q]) camera_.posZ -= camMovementSpeed;

        const float rotationSpeed = 2.0f;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_LEFT]) camera_.yawDegrees -= rotationSpeed;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_RIGHT]) camera_.yawDegrees += rotationSpeed;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_UP]) camera_.pitchDegrees -= rotationSpeed;
        if (freeViewEnabled_ && !cameraIntro_.active && keys[SDL_SCANCODE_DOWN]) camera_.pitchDegrees += rotationSpeed;
        camera_.pitchDegrees = std::clamp(camera_.pitchDegrees, 5.0f, 85.0f);

        updateActionIntroCamera(camera_, cameraIntro_, deltaSeconds);

        cameraOscillationTime_ += deltaSeconds;
        if (!freeViewEnabled_ && !cameraIntro_.active) {
            applyGoalCamera(camera_);
            if (nextIsCharacter) {
                constexpr float kOscillationAmplitudeDegrees = 1.8f;
                constexpr float kOscillationSpeed = 0.55f;
                camera_.yawDegrees = kGoalCameraYaw + std::sin(cameraOscillationTime_ * kOscillationSpeed) * kOscillationAmplitudeDegrees;
            }
        }

        updateBattleHudDocument(document_, manager_, hudFeedback_, tutorialOverlay_, rhythmChallenge_, paused_,
                                pauseOverlayMode_, pauseSelection_, settingsSelection_, settings_,
                                settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond,
                                nowMs);
    }

    void render() {
        if (!initialized_ || finished_) {
            return;
        }

        const int focusedIndex = getActiveCharacterPartyIndex(manager_).has_value() ? 0 : 1;
        renderBattleScene(sceneRenderer_, camera_, entities_, focusedIndex, frameAccumulator_);
        screenBlitter_.uploadSurface(sceneRenderer_.surface);

        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.035f, 0.043f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        screenBlitter_.draw();

        renderInterface_->BeginFrame();
        context_->Update();
        context_->Render();
        renderInterface_->EndFrame();
    }

    bool isFinished() const {
        return finished_;
    }

private:
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

    void handlePauseEvent(const SDL_Event& event) {
        if (event.type != SDL_KEYDOWN) {
            return;
        }
        if (pauseOverlayMode_ == PauseOverlayMode::Menu) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                paused_ = false;
            } else if (event.key.keysym.sym == SDLK_F11) {
                toggleDisplayMode();
            } else if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
                if (pauseSelection_ == PauseSelection::Settings) {
                    pauseSelection_ = PauseSelection::Continue;
                } else if (pauseSelection_ == PauseSelection::ExitToMainMenu) {
                    pauseSelection_ = PauseSelection::Settings;
                }
            } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
                if (pauseSelection_ == PauseSelection::Continue) {
                    pauseSelection_ = PauseSelection::Settings;
                } else if (pauseSelection_ == PauseSelection::Settings) {
                    pauseSelection_ = PauseSelection::ExitToMainMenu;
                }
            } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                       event.key.keysym.sym == SDLK_SPACE) {
                if (pauseSelection_ == PauseSelection::Continue) {
                    paused_ = false;
                } else if (pauseSelection_ == PauseSelection::Settings) {
                    pauseOverlayMode_ = PauseOverlayMode::Settings;
                } else {
                    finished_ = true;
                }
            }
            return;
        }

        if (event.key.keysym.sym == SDLK_ESCAPE) {
            pauseOverlayMode_ = PauseOverlayMode::Menu;
            return;
        }

        if (event.key.keysym.sym == SDLK_F11) {
            toggleDisplayMode();
            return;
        }

        if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
            if (settingsSelection_ == SettingsSelection::VoiceVolume) {
                settingsSelection_ = SettingsSelection::DisplayMode;
            } else if (settingsSelection_ == SettingsSelection::TextSpeed) {
                settingsSelection_ = SettingsSelection::VoiceVolume;
            } else if (settingsSelection_ == SettingsSelection::Back) {
                settingsSelection_ = SettingsSelection::TextSpeed;
            }
        } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
            if (settingsSelection_ == SettingsSelection::DisplayMode) {
                settingsSelection_ = SettingsSelection::VoiceVolume;
            } else if (settingsSelection_ == SettingsSelection::VoiceVolume) {
                settingsSelection_ = SettingsSelection::TextSpeed;
            } else if (settingsSelection_ == SettingsSelection::TextSpeed) {
                settingsSelection_ = SettingsSelection::Back;
            }
        } else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) {
            applySettingsStep(-1);
        } else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) {
            applySettingsStep(1);
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                   event.key.keysym.sym == SDLK_SPACE) {
            if (settingsSelection_ == SettingsSelection::Back) {
                pauseOverlayMode_ = PauseOverlayMode::Menu;
            } else {
                applySettingsStep(1);
            }
        }
    }

    void applySettingsStep(int direction) {
        if (settings_ == nullptr) {
            return;
        }
        if (settingsSelection_ == SettingsSelection::DisplayMode) {
            toggleDisplayMode();
        } else if (settingsSelection_ == SettingsSelection::VoiceVolume) {
            adjustVoiceVolume(direction);
        } else if (settingsSelection_ == SettingsSelection::TextSpeed) {
            adjustTextSpeed(direction);
        } else if (settingsSelection_ == SettingsSelection::Back) {
            pauseOverlayMode_ = PauseOverlayMode::Menu;
        }
    }

    void toggleDisplayMode() {
        if (settings_ == nullptr || windowHost_ == nullptr) {
            return;
        }
        const bool targetFullscreen = !settings_->fullscreen;
        if (windowHost_->setFullscreen(targetFullscreen)) {
            settings_->fullscreen = targetFullscreen;
        }
        windowWidth_ = windowHost_->getWidth();
        windowHeight_ = windowHost_->getHeight();
        if (renderInterface_ != nullptr) {
            renderInterface_->SetViewport(windowWidth_, windowHeight_);
        }
        if (context_ != nullptr) {
            context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
        }
        camera_.screenCenterX = windowWidth_ * 0.5f;
        camera_.screenCenterY = windowHeight_ * 0.5f;
        (void)sceneRenderer_.initialize(windowWidth_, windowHeight_, worldAssets_);
    }

    void adjustVoiceVolume(int direction) {
        if (settings_ == nullptr) {
            return;
        }
        settings_->voiceVolume = std::clamp(settings_->voiceVolume + 0.05f * static_cast<float>(direction), 0.0f, 1.0f);
    }

    void adjustTextSpeed(int direction) {
        if (settings_ == nullptr) {
            return;
        }
        settings_->textSpeed = std::clamp(settings_->textSpeed + 6.0f * static_cast<float>(direction),
                                          kMinSettingsTextSpeed, kMaxSettingsTextSpeed);
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

    void maybeStartTutorial(Uint64 nowMs) {
        if (rhythmChallenge_.active || tutorialOverlay_.step != TutorialStep::None) {
            return;
        }
        const std::optional<int> activePartyIndex = getActiveCharacterPartyIndex(manager_);
        if (!activePartyIndex.has_value()) {
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
        if (!tutorialOverlay_.ultimateShown &&
            manager_.isPlayerActionReady(battle::BattleAction::Ultimate) &&
            manager_.getCharacterUltimateCharge(*activePartyIndex) >= manager_.getCharacterUltimateRequired(*activePartyIndex)) {
            startTutorial(tutorialOverlay_, TutorialStep::Ultimate, tutorialLibrary_.ultimate, nowMs);
        }
    }

    void completeTutorialForAction(battle::BattleAction action) {
        if (action == battle::BattleAction::Standard && tutorialOverlay_.step == TutorialStep::Standard) {
            tutorialOverlay_.standardShown = true;
            tutorialOverlay_.step = TutorialStep::None;
        } else if (action == battle::BattleAction::Skill && tutorialOverlay_.step == TutorialStep::Skill) {
            tutorialOverlay_.skillShown = true;
            tutorialOverlay_.step = TutorialStep::None;
        } else if (action == battle::BattleAction::Ultimate && tutorialOverlay_.step == TutorialStep::Ultimate) {
            tutorialOverlay_.ultimateShown = true;
            tutorialOverlay_.step = TutorialStep::None;
        }
    }

    void finalizeSkillChallenge(bool onBeat) {
        if (!rhythmChallenge_.active) {
            return;
        }
        const Uint64 nowMs = SDL_GetTicks64();
        rhythmChallenge_ = RhythmChallengeState{};
        if (!manager_.executePlayerAction(battle::BattleAction::Skill)) {
            showToast(hudFeedback_, "ACTION NOT AVAILABLE.", nowMs);
            return;
        }
        showToast(hudFeedback_, onBeat ? "ON-BEAT INPUT." : "LATE INPUT.", nowMs, 1100);
        completeTutorialForAction(battle::BattleAction::Skill);
        manager_.processAutomaticTurns();
        consumeBattleActionEvents(hudFeedback_, manager_, settings_ != nullptr ? settings_->voiceVolume : 1.0f, nowMs);
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
            showToast(hudFeedback_, "SKILL COSTS 1 ORB.", nowMs);
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
        consumeBattleActionEvents(hudFeedback_, manager_, settings_ != nullptr ? settings_->voiceVolume : 1.0f, nowMs);
    }

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool initialized_ = false;
    bool finished_ = false;
    bool paused_ = false;
    bool rmlInitialized_ = false;
    bool rmlGlInitialized_ = false;
    bool imageInitialized_ = false;
    int windowWidth_ = kWindowWidth;
    int windowHeight_ = kWindowHeight;
    GameSettings* settings_ = nullptr;
    PauseOverlayMode pauseOverlayMode_ = PauseOverlayMode::Menu;
    PauseSelection pauseSelection_ = PauseSelection::Continue;
    SettingsSelection settingsSelection_ = SettingsSelection::DisplayMode;
    battle::BattleManager manager_;
    TutorialScriptLibrary tutorialLibrary_;
    HudFeedbackState hudFeedback_;
    TutorialOverlayState tutorialOverlay_;
    RhythmChallengeState rhythmChallenge_;
    std::vector<std::unique_ptr<Rml::EventListener>> uiListeners_;
    std::vector<std::string> worldAssets_;
    std::vector<WorldEntity> entities_;
    SoftwareSceneRenderer sceneRenderer_;
    GlScreenBlitter screenBlitter_;
    SystemInterface_SDL systemInterface_;
    std::unique_ptr<RenderInterfaceGL3SDL> renderInterface_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    battle::Camera3D camera_;
    CameraIntroAnimation cameraIntro_;
    bool freeViewEnabled_ = false;
    float cameraOscillationTime_ = 0.0f;
    float frameAccumulator_ = 0.0f;
    std::string lastTurnToken_;
};

Session::Session() : impl_(std::make_unique<SessionImpl>()) {}
Session::~Session() = default;
bool Session::initialize(Window& window, GameSettings& settings) { return impl_->initialize(window, settings); }
void Session::shutdown() { impl_->shutdown(); }
void Session::handleEvent(const SDL_Event& event) { impl_->handleEvent(event); }
void Session::update(float deltaSeconds) { impl_->update(deltaSeconds); }
void Session::render() { impl_->render(); }
bool Session::isFinished() const { return impl_->isFinished(); }

} // namespace battle::app
