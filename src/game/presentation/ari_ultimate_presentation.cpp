#include "ari_ultimate_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr int kFrameCount = 289;
constexpr float kUiFramesPerSecond = 22.0f;
constexpr float kUiAnimationDurationSeconds =
    static_cast<float>(kFrameCount) / kUiFramesPerSecond;
constexpr float kRevealDurationSeconds = 0.70f;
constexpr int kFramePrefetchAhead = 2;
constexpr int kFrameKeepBehind = 2;
constexpr float kStartOffsetX = -170.0f;
constexpr float kStartOffsetY = 40.0f;
constexpr float kStartOffsetZ = -125.0f;
constexpr float kEndOffsetX = -180.0f;
constexpr float kEndOffsetY = 80.0f;
constexpr float kEndOffsetZ = -129.0f;

Camera3D makeSupportZoomStartCamera(float anchorX, float anchorY, float anchorZ) {
    Camera3D camera;
    camera.posX = anchorX + kStartOffsetX;
    camera.posY = anchorY + kStartOffsetY;
    camera.posZ = anchorZ + kStartOffsetZ;
    camera.pitchDegrees = -1.3f;
    camera.yawDegrees = 180.0f;
    camera.focalLength = 38000.0f;
    return camera;
}

Camera3D makeSupportZoomEndCamera(float anchorX, float anchorY, float anchorZ) {
    Camera3D camera;
    camera.posX = anchorX + kEndOffsetX;
    camera.posY = anchorY + kEndOffsetY;
    camera.posZ = anchorZ + kEndOffsetZ;
    camera.pitchDegrees = -1.0f;
    camera.yawDegrees = 180.0f;
    camera.focalLength = 36000.0f;
    return camera;
}

float lerpF(float a, float b, float t) {
    return a + ((b - a) * t);
}

int currentUiFrameIndex(float elapsedSeconds) {
    const int frameIndex = static_cast<int>(elapsedSeconds * kUiFramesPerSecond);
    return std::clamp(frameIndex, 0, kFrameCount - 1);
}

} // namespace

AriUltimatePresentation::AriUltimatePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) : targetX_(targetWorldX)
  , targetY_(targetWorldY)
  , targetZ_(targetWorldZ) {
    (void)casterWorldX;
    (void)casterWorldY;
    (void)casterWorldZ;
    totalDuration_ = kUiAnimationDurationSeconds + kRevealDurationSeconds;
}

AriUltimatePresentation::~AriUltimatePresentation() {
    releaseFrames();
}

void AriUltimatePresentation::start() {
    elapsedTime_ = 0.0f;
    phaseElapsed_ = 0.0f;
    phase_ = Phase::UiAnimation;
    pendingAbilityAudioCues_ = 1;
    frames_.assign(static_cast<size_t>(kFrameCount), FrameSlot{});
    pendingAudioCommands_.clear();
    bgmPaused_ = true;
    bgmResumeQueued_ = false;
    pendingAudioCommands_.push_back(PresentationAudioCommand{
        PresentationAudioCommandType::PauseBgm,
        "",
        1.0f
    });
}

void AriUltimatePresentation::update(float deltaTime) {
    if (phase_ == Phase::Complete) {
        return;
    }

    elapsedTime_ += deltaTime;
    phaseElapsed_ += deltaTime;

    if (phase_ == Phase::UiAnimation && phaseElapsed_ >= kUiAnimationDurationSeconds) {
        phase_ = Phase::Reveal;
        phaseElapsed_ = 0.0f;
        return;
    }

    if (phase_ == Phase::Reveal && phaseElapsed_ >= kRevealDurationSeconds) {
        phase_ = Phase::Complete;
        if (bgmPaused_ && !bgmResumeQueued_) {
            bgmResumeQueued_ = true;
            pendingAudioCommands_.push_back(PresentationAudioCommand{
                PresentationAudioCommandType::ResumeBgm,
                "",
                1.0f
            });
        }
    }
}

void AriUltimatePresentation::render(SDL_Renderer* renderer,
                                     int screenW,
                                     int screenH,
                                     const Camera3D& camera) {
    (void)camera;
    if (renderer == nullptr || phase_ == Phase::Complete) {
        return;
    }

    const int frameIndex = (phase_ == Phase::UiAnimation)
        ? currentUiFrameIndex(phaseElapsed_)
        : (kFrameCount - 1);
    maintainFrameWindow(renderer, frameIndex);

    SDL_Texture* frameTexture = nullptr;
    if (frameIndex >= 0 && static_cast<size_t>(frameIndex) < frames_.size()) {
        frameTexture = frames_[static_cast<size_t>(frameIndex)].texture;
    }

    Uint8 alpha = 255;
    if (phase_ == Phase::Reveal) {
        const float t = easing::easeOutCubic(
            easing::clamp01(phaseElapsed_ / kRevealDurationSeconds)
        );
        alpha = static_cast<Uint8>(std::lround(255.0f * (1.0f - t)));
    }

    SDL_Rect dest{0, 0, std::max(1, screenW), std::max(1, screenH)};
    if (frameTexture != nullptr) {
        SDL_SetTextureBlendMode(frameTexture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(frameTexture, alpha);
        SDL_RenderCopy(renderer, frameTexture, nullptr, &dest);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
    SDL_RenderFillRect(renderer, &dest);
}

bool AriUltimatePresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

int AriUltimatePresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

bool AriUltimatePresentation::overridesCamera() const {
    return true;
}

void AriUltimatePresentation::applyCameraState(Camera3D& camera) const {
    const Camera3D startCamera = makeSupportZoomStartCamera(targetX_, targetY_, targetZ_);
    const Camera3D endCamera = makeSupportZoomEndCamera(targetX_, targetY_, targetZ_);

    if (phase_ == Phase::UiAnimation) {
        camera = startCamera;
        return;
    }

    const float t = easing::easeOutQuint(
        easing::clamp01(phaseElapsed_ / kRevealDurationSeconds)
    );
    camera.posX = lerpF(startCamera.posX, endCamera.posX, t);
    camera.posY = lerpF(startCamera.posY, endCamera.posY, t);
    camera.posZ = lerpF(startCamera.posZ, endCamera.posZ, t);
    camera.pitchDegrees = lerpF(startCamera.pitchDegrees, endCamera.pitchDegrees, t);
    camera.yawDegrees = lerpF(startCamera.yawDegrees, endCamera.yawDegrees, t);
    camera.focalLength = lerpF(startCamera.focalLength, endCamera.focalLength, t);
}

bool AriUltimatePresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool AriUltimatePresentation::shouldRenderCasterEntity() const {
    return true;
}

bool AriUltimatePresentation::shouldRenderBossEntity() const {
    return false;
}

bool AriUltimatePresentation::shouldRenderAboveHud() const {
    return true;
}

bool AriUltimatePresentation::shouldUseCenteredPartyLayout() const {
    return true;
}

std::vector<PresentationAudioCommand> AriUltimatePresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

std::string AriUltimatePresentation::resolvePath(const std::string& relativePath) {
    return platform::path::resolvePath(relativePath);
}

void AriUltimatePresentation::releaseFrames() {
    for (FrameSlot& frame : frames_) {
        if (frame.texture != nullptr) {
            SDL_DestroyTexture(frame.texture);
            frame.texture = nullptr;
        }
        frame.attemptedLoad = false;
    }
}

void AriUltimatePresentation::maintainFrameWindow(SDL_Renderer* renderer, int frameIndex) {
    if (frameIndex < 0 || frameIndex >= kFrameCount) {
        return;
    }

    const int minKeep = std::max(0, frameIndex - kFrameKeepBehind);
    const int maxKeep = std::min(kFrameCount - 1, frameIndex + kFramePrefetchAhead);

    for (int index = minKeep; index <= maxKeep; ++index) {
        ensureFrameLoaded(renderer, index);
    }

    for (int index = 0; index < minKeep; ++index) {
        FrameSlot& frame = frames_[static_cast<size_t>(index)];
        if (frame.texture != nullptr) {
            SDL_DestroyTexture(frame.texture);
            frame.texture = nullptr;
        }
    }
}

void AriUltimatePresentation::ensureFrameLoaded(SDL_Renderer* renderer, int frameIndex) {
    if (renderer == nullptr ||
        frameIndex < 0 ||
        frameIndex >= kFrameCount ||
        static_cast<size_t>(frameIndex) >= frames_.size()) {
        return;
    }

    FrameSlot& frame = frames_[static_cast<size_t>(frameIndex)];
    if (frame.texture != nullptr || frame.attemptedLoad) {
        return;
    }

    frame.attemptedLoad = true;

#ifdef BATTLE_ENABLE_IMAGE
    char framePath[160];
    std::snprintf(
        framePath,
        sizeof(framePath),
        "assets/combat/presentations/ari/frame_%04d.png",
        frameIndex + 1
    );

    SDL_Surface* surface = IMG_Load(resolvePath(framePath).c_str());
    if (surface == nullptr) {
        return;
    }

    frame.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

} // namespace battle
