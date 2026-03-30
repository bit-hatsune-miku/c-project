#include "jiafei_ultimate_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kSpinPhaseSeconds = 5.0f;
constexpr float kOverlayPhaseSeconds = 1.5f;
constexpr float kTotalDurationSeconds = 7.0f;
constexpr float kDamageApplyTimeSeconds = 6.85f;
constexpr float kCasterForwardOffset = 165.0f;
constexpr float kCameraRadiusStart = 500.0f;
constexpr float kCameraRadiusEnd = 315.0f;
constexpr float kCameraHeight = -150.0f;
constexpr float kPi = 3.14159265359f;

std::string resolveHeadTexturePath() {
    return platform::path::resolvePath("assets/combat/presentations/jiafei/head0.png");
}

float radiansToDegrees(float radians) {
    return radians * (180.0f / kPi);
}

} // namespace

JiafeiUltimatePresentation::JiafeiUltimatePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = kTotalDurationSeconds;
}

JiafeiUltimatePresentation::~JiafeiUltimatePresentation() {
    if (headTexture_ != nullptr) {
        SDL_DestroyTexture(headTexture_);
    }
}

void JiafeiUltimatePresentation::start() {
    elapsedTime_ = 0.0f;
    pendingAbilityAudioCues_ = 1;
    pendingHitEvents_ = 0;
    hitTriggered_ = false;
    stagedCasterX_ = targetX_;
    stagedCasterY_ = targetY_ - kCasterForwardOffset;
    stagedCasterZ_ = targetZ_;
}

void JiafeiUltimatePresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (!hitTriggered_ && elapsedTime_ >= kDamageApplyTimeSeconds) {
        hitTriggered_ = true;
        ++pendingHitEvents_;
    }
}

void JiafeiUltimatePresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)camera;
    if (elapsedTime_ < kSpinPhaseSeconds) {
        return;
    }

    ensureTextureLoaded(renderer);

    const float t = overlayProgress();
    const float eased = easing::easeOutCubic(t);
    const float maxSide = static_cast<float>(std::max(screenW, screenH)) * 1.55f;
    const float side = easing::lerp(6.0f, maxSide, eased);
    const float angle = std::pow(1.0f - eased, 1.15f) * 14400.0f;
    SDL_Rect dest{
        static_cast<int>(std::lround((screenW * 0.5f) - (side * 0.5f))),
        static_cast<int>(std::lround((screenH * 0.5f) - (side * 0.5f))),
        static_cast<int>(std::lround(side)),
        static_cast<int>(std::lround(side))
    };

    if (headTexture_ != nullptr) {
        SDL_Point center{dest.w / 2, dest.h / 2};
        SDL_RenderCopyEx(renderer, headTexture_, nullptr, &dest, angle, &center, SDL_FLIP_NONE);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &dest);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &dest);
}

bool JiafeiUltimatePresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

int JiafeiUltimatePresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int JiafeiUltimatePresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

bool JiafeiUltimatePresentation::overridesCamera() const {
    return true;
}

void JiafeiUltimatePresentation::applyCameraState(Camera3D& camera) const {
    const float spinT = std::clamp(elapsedTime_ / kSpinPhaseSeconds, 0.0f, 1.0f);
    const float angle = cameraOrbitAngleRadians();
    const float radius = easing::lerp(kCameraRadiusStart, kCameraRadiusEnd, easing::easeOutCubic(spinT));

    camera.posX = targetX_ + std::cos(angle) * radius;
    camera.posY = targetY_ + std::sin(angle) * radius;
    camera.posZ = targetZ_ + kCameraHeight;

    const float lookX = targetX_;
    const float lookY = targetY_ - 20.0f;
    const float lookZ = targetZ_ - 80.0f;
    const float dx = lookX - camera.posX;
    const float dy = lookY - camera.posY;
    const float dz = lookZ - camera.posZ;
    const float horizontalDist = std::sqrt((dx * dx) + (dy * dy));

    camera.yawDegrees = radiansToDegrees(std::atan2(-dx, dy));
    camera.pitchDegrees = radiansToDegrees(std::atan2(dz, std::max(1.0f, horizontalDist)));
    camera.focalLength = easing::lerp(34500.0f, 25500.0f, spinT);
}

bool JiafeiUltimatePresentation::getCasterWorldOverride(float& outX, float& outY, float& outZ) const {
    outX = stagedCasterX_;
    outY = stagedCasterY_;
    outZ = stagedCasterZ_;
    return true;
}

bool JiafeiUltimatePresentation::shouldRenderAboveHud() const {
    return true;
}

bool JiafeiUltimatePresentation::shouldBlackoutWorld() const {
    return false;
}

void JiafeiUltimatePresentation::ensureTextureLoaded(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_ || renderer == nullptr) {
        return;
    }

    attemptedTextureLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolveHeadTexturePath().c_str());
    if (surface == nullptr) {
        return;
    }

    headTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

float JiafeiUltimatePresentation::cameraOrbitAngleRadians() const {
    const float spinT = std::clamp(std::min(elapsedTime_, kSpinPhaseSeconds) / kSpinPhaseSeconds, 0.0f, 1.0f);
    const float turns = 0.35f + (1.35f * spinT) + (5.6f * spinT * spinT);
    return (turns * 2.0f * kPi) + 0.8f;
}

float JiafeiUltimatePresentation::overlayProgress() const {
    return std::clamp((elapsedTime_ - kSpinPhaseSeconds) / kOverlayPhaseSeconds, 0.0f, 1.0f);
}

} // namespace battle
