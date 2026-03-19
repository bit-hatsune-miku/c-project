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
#include "core/ability_system.h"
#include "core/battle_manager.h"
#include "core/easing.h"
#include "presentation/ability_presentation.h"
#include "render/battle_scene_renderer.h"
#include "render/camera_3d.h"
#include "render/free_view_camera_debug_log.h"
#include "render/gl_screen_blitter.h"
#include "ui/battle_session_document_updates.h"
#include "ui/battle_session_overlay_bindings.h"
#include "ui/battle_session_ui_state.h"
#include "vn/vn_script.h"
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
constexpr float kMinSettingsTextSpeed = 18.0f;
constexpr float kMaxSettingsTextSpeed = 90.0f;

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

using battle::app::ui::HudFeedbackState;
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
                               Uint64 nowMs) {
    const battle::BattleState& battleState = manager.getBattleState();
    (void)feedback;
    (void)nowMs;
    for (const battle::BattleActionEvent& event : manager.getRecentActionEvents()) {
        const std::string actorVoiceKey = event.actorType == battle::ParticipantType::Boss
            ? battleState.boss.key
            : ((event.actorPartyIndex >= 0 && event.actorPartyIndex < static_cast<int>(battleState.party.size()))
                ? battleState.party[static_cast<size_t>(event.actorPartyIndex)].assets
                : std::string());

        if (event.action == battle::BattleAction::Skill) {
            if (const auto skillVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "skill"); skillVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*skillVoice, voiceVolume);
            }
        }

        if (event.bossHpAfter < event.bossHpBefore) {
            if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.boss.key, "hit"); hitVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*hitVoice, voiceVolume);
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
            if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.party[static_cast<size_t>(partyIndex)].assets, "hit");
                hitVoice.has_value()) {
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

        const std::string fontPath = platform::path::findFontPath();
        if (!fontPath.empty()) {
            Rml::LoadFontFace(fontPath);
        }

        if (!manager_.initialize("lyoo", {"miku", "cupcakke", "lyoo"})) {
            std::cerr << "[Battle] Initialization failed.\n";
            shutdown();
            return false;
        }

        battle::ability::setPresentationInteractionRunner([this](const battle::PresentationContext& context) {
            return runPresentationInteraction(context);
        });

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
        if (!sceneRenderer_.initialize(windowWidth_, windowHeight_, worldAssets_, resolveBattleSpritePath)) {
            shutdown();
            return false;
        }

        entities_.clear();
        entities_.reserve(2);
        entities_.push_back(WorldEntity{
            state.party.front().key,
            state.party.front().assets,
            false,
            battle::render::kDuelCharacterSlotX,
            battle::render::kDuelCharacterBaseY,
            0.0f,
            battle::render::colorFromKey(state.party.front().key, false)
        });
        entities_.push_back(WorldEntity{
            state.boss.key,
            state.boss.assets,
            true,
            battle::render::kDuelBossSlotX,
            battle::render::kDuelCharacterBaseY + battle::render::kBossCharacterDistanceWorld,
            0.0f,
            battle::render::colorFromKey(state.boss.key, true)
        });

        renderInterface_->SetViewport(windowWidth_, windowHeight_);
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

        // Legacy flow: actions are triggered with Space only (executePlayerTurn).
        // Keeping button listeners commented for future per-character input design.
        // attachListener("action-standard", Rml::EventId::Click, [this](Rml::Event&) {
        //     attemptAction(battle::BattleAction::Standard);
        // });
        // attachListener("action-skill", Rml::EventId::Click, [this](Rml::Event&) {
        //     attemptAction(battle::BattleAction::Skill);
        // });
        // Legacy flow: no manual ultimate trigger from unit cards.
        // Keep listeners commented for future per-character custom input work.
        // for (int i = 0; i < 4; ++i) {
        //     attachListener("unit-card-" + std::to_string(i + 1), Rml::EventId::Click, [this, i](Rml::Event&) {
        //         const Uint64 nowMs = SDL_GetTicks64();
        //         const std::optional<int> activePartyIndex = getActiveCharacterPartyIndex(manager_);
        //         if (!activePartyIndex.has_value()) {
        //             showToast(hudFeedback_, "WAIT FOR AN ALLY TURN.", nowMs);
        //             return;
        //         }
        //         if (*activePartyIndex != i) {
        //             showToast(hudFeedback_, "ONLY THE ACTIVE UNIT CAN ULTIMATE.", nowMs);
        //             return;
        //         }
        //         attemptAction(battle::BattleAction::Ultimate);
        //     });
        // }
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

        camera_.screenCenterX = windowWidth_ * 0.5f;
        camera_.screenCenterY = windowHeight_ * 0.5f;
        applyGoalCamera(camera_);
        battle::app::ui::updateBattleHudDocument(document_, manager_, hudFeedback_, tutorialOverlay_, rhythmChallenge_,
                                                 paused_, pauseOverlayMode_, pauseSelection_, settingsSelection_,
                                                 settings_,
                                                 settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond,
                                                 SDL_GetTicks64(),
                                                 makeBattleHudDocumentDependencies());

        initialized_ = true;
        return true;
    }

    void shutdown() {
        initialized_ = false;
        battle::ability::setPresentationInteractionRunner(nullptr);
        freeViewCameraDebugLog_.clear();
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
            if (!sceneRenderer_.initialize(windowWidth_, windowHeight_, worldAssets_, resolveBattleSpritePath)) {
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
            const Uint64 nowMs = SDL_GetTicks64();
            if (event.key.keysym.sym == SDLK_f) {
                freeViewEnabled_ = !freeViewEnabled_;
                if (!freeViewEnabled_) {
                    applyGoalCamera(camera_);
                    freeViewCameraDebugLog_.clear();
                }
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
                if (event.key.keysym.sym == SDLK_e) {
                    const Uint64 hitTime = nowMs;
                    const float progress = getRhythmProgress(rhythmChallenge_, hitTime);
                    const float halfWindow = rhythmChallenge_.targetWindow * 0.5f;
                    finalizeSkillChallenge(progress >= rhythmChallenge_.targetCenter - halfWindow &&
                                           progress <= rhythmChallenge_.targetCenter + halfWindow);
                }
            } else if (!freeViewEnabled_ && event.key.keysym.sym == SDLK_SPACE) {
                if (!manager_.executePlayerTurn()) {
                    showToast(hudFeedback_, "WAIT FOR AN ALLY TURN.", nowMs);
                    return;
                }
                manager_.processAutomaticTurns();
                consumeBattleActionEvents(hudFeedback_, manager_, settings_ != nullptr ? settings_->voiceVolume : 1.0f, nowMs);

                const int previewActorIndex = manager_.getPreviewNextActorIndex();
                if (previewActorIndex >= 0) {
                    const battle::TurnState& turnState = manager_.getTurnState();
                    if (previewActorIndex < static_cast<int>(turnState.actors.size())) {
                        const battle::TurnActor& previewActor = turnState.actors[static_cast<size_t>(previewActorIndex)];
                        if (previewActor.type == battle::ParticipantType::Character) {
                            startActionIntroCamera(camera_, cameraIntro_);
                        }
                    }
                }
                return;
                // Legacy replacement of the multi-action controls:
                // attemptAction(battle::BattleAction::Ultimate);
                // attemptAction(battle::BattleAction::Standard);
                // attemptAction(battle::BattleAction::Skill);
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
        gOneShotAudio.cleanupFinishedPlayback();

        if (paused_) {
            battle::app::ui::updateBattleHudDocument(document_, manager_, hudFeedback_, tutorialOverlay_,
                                                     rhythmChallenge_, paused_, pauseOverlayMode_, pauseSelection_,
                                                     settingsSelection_, settings_,
                                                     settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond,
                                                     nowMs,
                                                     makeBattleHudDocumentDependencies());
            return;
        }

        manager_.processAutomaticTurns();
        consumeBattleActionEvents(hudFeedback_, manager_, settings_ != nullptr ? settings_->voiceVolume : 1.0f, nowMs);
        maybeStartTutorial(nowMs);
        if (tutorialOverlay_.step != TutorialStep::None && !tutorialOverlay_.audioPlayed && !tutorialOverlay_.entry.voice.empty()) {
            if (gOneShotAudio.playWavOneShot(platform::path::resolvePath(tutorialOverlay_.entry.voice),
                                             settings_ != nullptr ? settings_->voiceVolume : 1.0f)) {
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
                    entities_[0].fallbackColor = battle::render::colorFromKey(currentChar.key, false);
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
        camera_.pitchDegrees = battle::clampFreeViewPitchDegrees(camera_.pitchDegrees);

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

        freeViewCameraDebugLog_.update(freeViewEnabled_ && !cameraIntro_.active, camera_);

        battle::app::ui::updateBattleHudDocument(document_, manager_, hudFeedback_, tutorialOverlay_,
                                                 rhythmChallenge_, paused_, pauseOverlayMode_, pauseSelection_,
                                                 settingsSelection_, settings_,
                                                 settings_ != nullptr ? settings_->textSpeed : kNarrationCharsPerSecond,
                                                 nowMs,
                                                 makeBattleHudDocumentDependencies());
    }

    void render() {
        if (!initialized_ || finished_) {
            return;
        }

        int focusedIndex = getActiveCharacterPartyIndex(manager_).has_value() ? 0 : 1;
        if (presentationPlaybackActive_) {
            if (presentationCasterIsBoss_) {
                focusedIndex = 1;
            } else if (presentationCasterPartyIndex_ >= 0) {
                focusedIndex = 0;
            }
        }
        battle::render::renderBattleScene(sceneRenderer_, camera_, entities_, focusedIndex, frameAccumulator_);
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
    float runPresentationInteraction(const battle::PresentationContext& context) {
        if (!initialized_ || finished_) {
            return 1.0f;
        }
        if (context.presentationId.empty()) {
            return 1.0f;
        }

        float casterX = battle::render::kDuelCharacterSlotX;
        float casterY = battle::render::kDuelCharacterBaseY;
        float casterZ = 0.0f;
        float targetX = battle::render::kDuelBossSlotX;
        float targetY = battle::render::kDuelCharacterBaseY + battle::render::kBossCharacterDistanceWorld;
        float targetZ = 0.0f;

        if (context.isBoss) {
            casterX = battle::render::kDuelBossSlotX;
            casterY = battle::render::kDuelCharacterBaseY + battle::render::kBossCharacterDistanceWorld;
            casterZ = 0.0f;
            targetX = battle::render::kDuelCharacterSlotX;
            targetY = battle::render::kDuelCharacterBaseY;
            targetZ = 0.0f;
        }

        std::unique_ptr<battle::AbilityPresentation> presentation = battle::PresentationRegistry::instance().create(
            context.presentationId,
            casterX, casterY, casterZ,
            targetX, targetY, targetZ
        );
        if (!presentation) {
            std::cerr << "[Presentation] Missing presentation id: " << context.presentationId << "\n";
            return 1.0f;
        }

        presentationPlaybackActive_ = true;
        presentationCasterIsBoss_ = context.isBoss;
        presentationCasterPartyIndex_ = context.casterIndex;

        presentation->start();
        Uint64 lastCounter = SDL_GetPerformanceCounter();

        while (!finished_ && !presentation->isComplete()) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    finished_ = true;
                    break;
                }

                if (isWindowResizeEvent(event)) {
                    windowHost_->handleEvent(event);
                    windowWidth_ = windowHost_->getWidth();
                    windowHeight_ = windowHost_->getHeight();
                    renderInterface_->SetViewport(windowWidth_, windowHeight_);
                    context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
                    camera_.screenCenterX = windowWidth_ * 0.5f;
                    camera_.screenCenterY = windowHeight_ * 0.5f;
                    if (!sceneRenderer_.initialize(windowWidth_, windowHeight_, worldAssets_, resolveBattleSpritePath)) {
                        finished_ = true;
                        break;
                    }
                }

                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        finished_ = true;
                        break;
                    }
                    if (event.key.keysym.sym == SDLK_SPACE) {
                        presentation->onSpacePressed();
                    }
                    presentation->onKeyPressed(event.key.keysym.sym);
                }
            }

            const Uint64 now = SDL_GetPerformanceCounter();
            const float deltaSeconds = static_cast<float>(now - lastCounter) /
                static_cast<float>(SDL_GetPerformanceFrequency());
            lastCounter = now;

            presentation->update(deltaSeconds);

            battle::Camera3D previousCamera = camera_;
            std::vector<WorldEntity> previousEntities = entities_;

            if (presentation->overridesCamera()) {
                presentation->applyCameraState(camera_);
            }

            float overrideX = 0.0f;
            float overrideY = 0.0f;
            float overrideZ = 0.0f;
            if (presentation->getCasterWorldOverride(overrideX, overrideY, overrideZ)) {
                const size_t casterEntityIndex = context.isBoss ? 1u : 0u;
                if (casterEntityIndex < entities_.size()) {
                    entities_[casterEntityIndex].worldX = overrideX;
                    entities_[casterEntityIndex].worldY = overrideY;
                    entities_[casterEntityIndex].worldZ = overrideZ;
                }
            }

            render();

            if (SDL_Renderer* renderer = windowHost_ != nullptr ? windowHost_->getRenderer() : nullptr) {
                presentation->render(renderer, windowWidth_, windowHeight_, camera_);
            }

            if (windowHost_ != nullptr) {
                windowHost_->present();
            }

            entities_ = std::move(previousEntities);
            camera_ = previousCamera;
        }

        presentationPlaybackActive_ = false;
        presentationCasterIsBoss_ = false;
        presentationCasterPartyIndex_ = -1;

        return presentation->getInputMultiplier();
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
        (void)sceneRenderer_.initialize(windowWidth_, windowHeight_, worldAssets_, resolveBattleSpritePath);
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
        if (tutorialOverlay_.dismissed || rhythmChallenge_.active || tutorialOverlay_.step != TutorialStep::None) {
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
    battle::render::FreeViewCameraDebugLog freeViewCameraDebugLog_;
    bool freeViewEnabled_ = false;
    bool presentationPlaybackActive_ = false;
    bool presentationCasterIsBoss_ = false;
    int presentationCasterPartyIndex_ = -1;
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
