#include "ari_boss_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr int kAttackCount = 3;
constexpr int kFrameCount = 31;
constexpr float kFrameDurationSeconds = 0.04f;
constexpr float kAttackDurationSeconds = kFrameCount * kFrameDurationSeconds;
constexpr float kWaitDurationMinSeconds = 1.0f;
constexpr float kWaitDurationMaxSeconds = 2.5f;
constexpr float kSpriteTargetHeightRatio = 0.40f;
constexpr float kSpriteMinHeightPixels = 220.0f;
constexpr float kSpriteMaxHeightPixels = 360.0f;
constexpr float kSpriteWorldAnchorZ = -105.0f;
constexpr float kPerfectReactionWindowSeconds = 0.12f;
constexpr float kLateReactionStartSeconds = 0.40f;
constexpr float kPerfectDamageMultiplier = 0.10f;
constexpr float kCameraPosYOffset = -675.0f;
constexpr float kCameraPosZ = -175.0f;
constexpr float kCameraPitchDegrees = -2.5f;
constexpr float kCameraYawDegrees = 0.0f;
constexpr float kCameraFocalLength = 32000.0f;

float randomFloat(float minValue, float maxValue) {
    const float unit = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minValue + (maxValue - minValue) * unit;
}

} // namespace

AriBossPresentation::AriBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
}

AriBossPresentation::~AriBossPresentation() {
    releaseFrames();
}

void AriBossPresentation::start() {
    elapsedTime_ = 0.0f;
    currentAttackIndex_ = 0;
    phaseElapsed_ = 0.0f;
    currentWaitDuration_ = 0.0f;
    attackInputCaptured_ = false;
    attackReactionSeconds_ = 0.0f;
    resolvedHitDamageMultipliers_.clear();
    pendingHitDamageMultipliers_.clear();
    pendingAbilityAudioCues_ = 0;
    pendingHitEvents_ = 0;
    beginAttack();
}

void AriBossPresentation::update(float deltaTime) {
    if (phase_ == Phase::Complete) {
        return;
    }

    elapsedTime_ += deltaTime;
    phaseElapsed_ += deltaTime;

    if (phase_ == Phase::Attacking) {
        if (phaseElapsed_ < kAttackDurationSeconds) {
            return;
        }

        const float hitDamageMultiplier = damageMultiplierForReaction(
            attackInputCaptured_,
            attackReactionSeconds_
        );
        resolvedHitDamageMultipliers_.push_back(hitDamageMultiplier);
        pendingHitDamageMultipliers_.push_back(hitDamageMultiplier);
        ++pendingHitEvents_;
        ++currentAttackIndex_;

        if (currentAttackIndex_ >= kAttackCount) {
            phase_ = Phase::Complete;
            return;
        }

        phase_ = Phase::Waiting;
        phaseElapsed_ = 0.0f;
        currentWaitDuration_ = randomWaitDuration();
        return;
    }

    if (phase_ == Phase::Waiting && phaseElapsed_ >= currentWaitDuration_) {
        beginAttack();
    }
}

void AriBossPresentation::preload(SDL_Renderer* renderer) {
    ensureFramesLoaded(renderer);
}

void AriBossPresentation::render(SDL_Renderer* renderer,
                                 int screenW,
                                 int screenH,
                                 const Camera3D& camera) {
    if (phase_ != Phase::Attacking && phase_ != Phase::Waiting) {
        return;
    }

    ensureFramesLoaded(renderer);
    renderAnimatedBoss(renderer, screenW, screenH, camera);
}

void AriBossPresentation::renderBelowWorld(SDL_Renderer* renderer,
                                           int screenW,
                                           int screenH,
                                           const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool AriBossPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

void AriBossPresentation::onSpacePressed() {
    if (phase_ != Phase::Attacking || attackInputCaptured_) {
        return;
    }

    attackInputCaptured_ = true;
    attackReactionSeconds_ = phaseElapsed_;
}

bool AriBossPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool AriBossPresentation::overridesCamera() const {
    return true;
}

void AriBossPresentation::applyCameraState(Camera3D& camera) const {
    camera.posX = casterX_;
    camera.posY = casterY_ + kCameraPosYOffset;
    camera.posZ = kCameraPosZ;
    camera.pitchDegrees = kCameraPitchDegrees;
    camera.yawDegrees = kCameraYawDegrees;
    camera.focalLength = kCameraFocalLength;
}

bool AriBossPresentation::shouldRenderCasterEntity() const {
    return false;
}

bool AriBossPresentation::shouldRenderAboveHud() const {
    return false;
}

void AriBossPresentation::renderAnimatedBoss(SDL_Renderer* renderer,
                                             int screenW,
                                             int screenH,
                                             const Camera3D& camera) const {
    const SDL_FPoint center = camera.worldToScreen(casterX_, casterY_, casterZ_ + kSpriteWorldAnchorZ);
    if (center.x <= -500000.0f || center.y <= -500000.0f) {
        return;
    }

    const float drawHeight = std::clamp(
        static_cast<float>(screenH) * kSpriteTargetHeightRatio,
        kSpriteMinHeightPixels,
        kSpriteMaxHeightPixels
    );
    const float aspectRatio = static_cast<float>(frameWidth_) / std::max(1.0f, static_cast<float>(frameHeight_));
    const float drawWidth = drawHeight * aspectRatio;
    SDL_FRect destination{
        center.x - drawWidth * 0.5f,
        center.y - drawHeight * 0.5f,
        drawWidth,
        drawHeight
    };

    const int frameIndex = (phase_ == Phase::Waiting) ? 0 : currentFrameIndex();
    if (frameIndex >= 0 && frameIndex < static_cast<int>(frames_.size()) && frames_[static_cast<size_t>(frameIndex)] != nullptr) {
        SDL_RenderCopyExF(renderer, frames_[static_cast<size_t>(frameIndex)], nullptr, &destination, 0.0, nullptr, SDL_FLIP_NONE);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRectF(renderer, &destination);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderDrawRectF(renderer, &destination);
}

float AriBossPresentation::getInputMultiplier() const {
    return averageDamageMultiplier();
}

float AriBossPresentation::consumeHitDamageMultiplier() {
    if (pendingHitDamageMultipliers_.empty()) {
        return getInputMultiplier();
    }

    const float multiplier = pendingHitDamageMultipliers_.front();
    pendingHitDamageMultipliers_.erase(pendingHitDamageMultipliers_.begin());
    return multiplier;
}

int AriBossPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int AriBossPresentation::consumeHitEvents() {
    const int hitEvents = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hitEvents;
}

int AriBossPresentation::getDamageLabelHitCount() const {
    return kAttackCount;
}

std::string AriBossPresentation::getInputResultText() const {
    const float damageMultiplier = averageDamageMultiplier();
    const int damageReductionPercent = static_cast<int>(std::lround((1.0f - damageMultiplier) * 100.0f));

    int capturedInputCount = 0;
    for (float multiplier : resolvedHitDamageMultipliers_) {
        if (multiplier < 0.999f) {
            ++capturedInputCount;
        }
    }

    if (capturedInputCount <= 0) {
        return "No attack was answered in time. Damage reduced 0%.";
    }

    char buffer[160];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "Reacted to %d/%d attacks. Damage reduced %d%%.",
        capturedInputCount,
        kAttackCount,
        damageReductionPercent
    );
    return std::string(buffer);
}

std::string AriBossPresentation::resolvePath(const std::string& relativePath) {
    return platform::path::resolvePath(relativePath);
}

void AriBossPresentation::ensureFramesLoaded(SDL_Renderer* renderer) {
    if (attemptedLoad_ || renderer == nullptr) {
        return;
    }

    attemptedLoad_ = true;
    frames_.assign(kFrameCount, nullptr);

#ifdef BATTLE_ENABLE_IMAGE
    for (int index = 0; index < kFrameCount; ++index) {
        char framePath[160];
        std::snprintf(
            framePath,
            sizeof(framePath),
            "assets/combat/presentations/ariBoss/frame_%02d_delay-0.04s.png",
            index
        );

        SDL_Surface* surface = IMG_Load(resolvePath(framePath).c_str());
        if (surface == nullptr) {
            continue;
        }

        frameWidth_ = surface->w;
        frameHeight_ = surface->h;
        frames_[static_cast<size_t>(index)] = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
#else
    (void)renderer;
#endif
}

void AriBossPresentation::releaseFrames() {
    for (SDL_Texture*& texture : frames_) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }
    frames_.clear();
}

void AriBossPresentation::beginAttack() {
    phase_ = Phase::Attacking;
    phaseElapsed_ = 0.0f;
    currentWaitDuration_ = 0.0f;
    attackInputCaptured_ = false;
    attackReactionSeconds_ = 0.0f;
    ++pendingAbilityAudioCues_;
}

float AriBossPresentation::randomWaitDuration() const {
    return randomFloat(kWaitDurationMinSeconds, kWaitDurationMaxSeconds);
}

float AriBossPresentation::averageDamageMultiplier() const {
    if (resolvedHitDamageMultipliers_.empty()) {
        return 1.0f;
    }

    float total = 0.0f;
    for (float multiplier : resolvedHitDamageMultipliers_) {
        total += multiplier;
    }
    return total / static_cast<float>(resolvedHitDamageMultipliers_.size());
}

float AriBossPresentation::damageMultiplierForReaction(bool pressed, float reactionSeconds) const {
    if (!pressed) {
        return 1.0f;
    }

    if (reactionSeconds <= kPerfectReactionWindowSeconds) {
        return kPerfectDamageMultiplier;
    }

    if (reactionSeconds >= kLateReactionStartSeconds) {
        return 1.0f;
    }

    const float t = easing::clamp01(
        (reactionSeconds - kPerfectReactionWindowSeconds) /
        std::max(0.001f, kLateReactionStartSeconds - kPerfectReactionWindowSeconds)
    );
    return easing::lerp(kPerfectDamageMultiplier, 1.0f, easing::easeInCubic(t));
}

int AriBossPresentation::currentFrameIndex() const {
    const int frameIndex = static_cast<int>(phaseElapsed_ / kFrameDurationSeconds);
    return std::clamp(frameIndex, 0, kFrameCount - 1);
}

} // namespace battle
