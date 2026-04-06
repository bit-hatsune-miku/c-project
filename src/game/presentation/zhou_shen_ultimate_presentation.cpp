#include "zhou_shen_ultimate_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kCameraDurationSeconds = 1.20f;
constexpr float kFishLaunchDelaySeconds = 0.18f;
constexpr float kFishTravelDurationSeconds = 0.60f;
constexpr float kHitTimeSeconds = kFishLaunchDelaySeconds + kFishTravelDurationSeconds;
constexpr float kCameraStartOffsetX = 40.0f;
constexpr float kCameraStartOffsetY = -760.0f;
constexpr float kCameraStartOffsetZ = -200.0f;
constexpr float kCameraEndOffsetX = 0.0f;
constexpr float kCameraEndOffsetY = -675.0f;
constexpr float kCameraEndOffsetZ = -175.0f;

float lerpF(float a, float b, float t) {
    return a + ((b - a) * t);
}

int clampedSingerCount(int singerCount) {
    return std::clamp(singerCount, 1, 4);
}

const char* judgementTextForSingerCount(int singerCount) {
    switch (clampedSingerCount(singerCount)) {
        case 1: return "FLOP";
        case 2: return "OKAY";
        case 3: return "GOOD";
        case 4:
        default:
            return "PERFECT";
    }
}

std::string resolveFishTexturePath() {
    return platform::path::resolvePath("assets/combat/presentations/zhouShen/fish.png");
}

} // namespace

ZhouShenUltimatePresentation::ZhouShenUltimatePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = kCameraDurationSeconds;
}

ZhouShenUltimatePresentation::~ZhouShenUltimatePresentation() {
    if (fishTexture_ != nullptr) {
        SDL_DestroyTexture(fishTexture_);
    }
}

void ZhouShenUltimatePresentation::start() {
    elapsedTime_ = 0.0f;
    pendingAbilityAudioCues_ = 1;
    pendingHitEvents_ = 0;
    hitTriggered_ = false;
}

void ZhouShenUltimatePresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (!hitTriggered_ && elapsedTime_ >= kHitTimeSeconds) {
        hitTriggered_ = true;
        pendingHitEvents_ = 1;
    }
}

void ZhouShenUltimatePresentation::preload(SDL_Renderer* renderer) {
    ensureFishTextureLoaded(renderer);
}

void ZhouShenUltimatePresentation::render(SDL_Renderer* renderer,
                                          int screenW,
                                          int screenH,
                                          const Camera3D& camera) {
    ensureFishTextureLoaded(renderer);

    if (elapsedTime_ < kFishLaunchDelaySeconds) {
        return;
    }

    const SDL_FPoint targetScreen = camera.worldToScreen(targetX_, targetY_, targetZ_ - 25.0f);
    const float fallbackTargetX = static_cast<float>(screenW) * 0.72f;
    const float fallbackTargetY = static_cast<float>(screenH) * 0.48f;
    const float endX = (targetScreen.x <= -500000.0f) ? fallbackTargetX : targetScreen.x;
    const float endY = (targetScreen.y <= -500000.0f) ? fallbackTargetY : targetScreen.y;

    const float rawTravelT = easing::clamp01(
        (elapsedTime_ - kFishLaunchDelaySeconds) / kFishTravelDurationSeconds
    );
    const float easedTravel = easing::easeOutCubic(rawTravelT);
    const float fishCenterX = lerpF(-static_cast<float>(screenW) * 0.18f, endX, easedTravel);
    const float fishCenterY = lerpF(static_cast<float>(screenH) * 0.76f, endY, easedTravel);
    const float fishScale = lerpF(0.42f, 1.02f, easedTravel);

    const int baseWidth = fishTextureWidth_ > 0 ? fishTextureWidth_ : 512;
    const int baseHeight = fishTextureHeight_ > 0 ? fishTextureHeight_ : 512;
    const int drawWidth = std::max(1, static_cast<int>(static_cast<float>(baseWidth) * fishScale));
    const int drawHeight = std::max(1, static_cast<int>(static_cast<float>(baseHeight) * fishScale));
    SDL_Rect destination{
        static_cast<int>(fishCenterX) - (drawWidth / 2),
        static_cast<int>(fishCenterY) - (drawHeight / 2),
        drawWidth,
        drawHeight
    };

    if (fishTexture_ != nullptr) {
        SDL_Point center{drawWidth / 2, drawHeight / 2};
        SDL_RenderCopyEx(renderer, fishTexture_, nullptr, &destination, -18.0, &center, SDL_FLIP_NONE);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 229, 247, 255, 220);
    SDL_RenderFillRect(renderer, &destination);
}

bool ZhouShenUltimatePresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

int ZhouShenUltimatePresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int ZhouShenUltimatePresentation::consumeHitEvents() {
    const int events = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return events;
}

float ZhouShenUltimatePresentation::getInputMultiplier() const {
    return static_cast<float>(clampSingerCount(singerCount_));
}

PresentationFeedbackSignal ZhouShenUltimatePresentation::getFeedbackSignal() const {
    switch (clampSingerCount(singerCount_)) {
        case 1: return PresentationFeedbackSignal::graded(0.0f);
        case 2: return PresentationFeedbackSignal::graded(0.35f);
        case 3: return PresentationFeedbackSignal::graded(0.70f);
        case 4:
        default:
            return PresentationFeedbackSignal::forcedPerfect();
    }
}

std::string ZhouShenUltimatePresentation::getInputResultText() const {
    return judgementTextForSingerCount(singerCount_);
}

void ZhouShenUltimatePresentation::setPresentationValue(int value) {
    singerCount_ = clampSingerCount(value);
}

bool ZhouShenUltimatePresentation::overridesCamera() const {
    return true;
}

void ZhouShenUltimatePresentation::applyCameraState(Camera3D& camera) const {
    const float t = easing::easeOutCubic(easing::clamp01(elapsedTime_ / totalDuration_));
    camera.posX = targetX_ + lerpF(kCameraStartOffsetX, kCameraEndOffsetX, t);
    camera.posY = targetY_ + lerpF(kCameraStartOffsetY, kCameraEndOffsetY, t);
    camera.posZ = lerpF(kCameraStartOffsetZ, kCameraEndOffsetZ, t);
    camera.pitchDegrees = lerpF(-4.2f, -2.5f, t);
    camera.yawDegrees = lerpF(4.0f, 0.0f, t);
    camera.focalLength = lerpF(29200.0f, 32000.0f, t);
}

bool ZhouShenUltimatePresentation::shouldRenderAboveHud() const {
    return false;
}

void ZhouShenUltimatePresentation::ensureFishTextureLoaded(SDL_Renderer* renderer) {
    if (attemptedFishLoad_ || renderer == nullptr) {
        return;
    }

    attemptedFishLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolveFishTexturePath().c_str());
    if (surface == nullptr) {
        return;
    }

    fishTextureWidth_ = surface->w;
    fishTextureHeight_ = surface->h;
    fishTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

int ZhouShenUltimatePresentation::clampSingerCount(int singerCount) {
    return clampedSingerCount(singerCount);
}

} // namespace battle
