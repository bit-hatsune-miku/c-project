#include "minako_strike_presentation.h"

#include "../core/easing.h"
#include "../render/battle_camera_staging.h"

#include <algorithm>
#include <cmath>

namespace battle {
namespace {

constexpr float kWindupDurationSeconds = 0.10f;
constexpr float kDashDurationSeconds = 0.22f;
constexpr float kImpactHoldDurationSeconds = 0.18f;
constexpr float kRecoveryDurationSeconds = 0.44f;
constexpr float kWindupBackstepDistance = 36.0f;
constexpr float kImpactOffsetDistance = 38.0f;
constexpr float kDashArcHeight = 16.0f;
constexpr float kImpactShakeOffset = 6.0f;
constexpr float kPi = 3.14159265f;

Camera3D makeLookCamera(float posX,
                        float posY,
                        float posZ,
                        float lookX,
                        float lookY,
                        float lookZ,
                        float focalLength) {
    Camera3D camera;
    camera.posX = posX;
    camera.posY = posY;
    camera.posZ = posZ;
    camera.focalLength = focalLength;

    const float dx = lookX - posX;
    const float dy = lookY - posY;
    const float dz = lookZ - posZ;
    const float horizontalDist = std::sqrt((dx * dx) + (dy * dy));
    camera.yawDegrees = std::atan2(-dx, dy) * 180.0f / kPi;
    camera.pitchDegrees = std::atan2(dz, std::max(1.0f, horizontalDist)) * 180.0f / kPi;
    return camera;
}

void blendCamera(Camera3D& output, const Camera3D& from, const Camera3D& to, float progress) {
    const float eased = easing::easeOutCubic(easing::clamp01(progress));
    output.posX = easing::lerp(from.posX, to.posX, eased);
    output.posY = easing::lerp(from.posY, to.posY, eased);
    output.posZ = easing::lerp(from.posZ, to.posZ, eased);
    output.pitchDegrees = easing::lerp(from.pitchDegrees, to.pitchDegrees, eased);
    output.yawDegrees = easing::lerp(from.yawDegrees, to.yawDegrees, eased);
    output.focalLength = easing::lerp(from.focalLength, to.focalLength, eased);
}

} // namespace

MinakoStrikePresentation::MinakoStrikePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ)
    , windupZ_(casterWorldZ)
    , impactZ_(casterWorldZ)
    , casterWorldX_(casterWorldX)
    , casterWorldY_(casterWorldY)
    , casterWorldZ_(casterWorldZ) {
    const float dx = targetWorldX - casterWorldX;
    const float dy = targetWorldY - casterWorldY;
    const float distance = std::max(0.001f, std::sqrt((dx * dx) + (dy * dy)));
    dirX_ = dx / distance;
    dirY_ = dy / distance;
    perpX_ = -dirY_;
    perpY_ = dirX_;

    windupX_ = casterWorldX - (dirX_ * kWindupBackstepDistance);
    windupY_ = casterWorldY - (dirY_ * kWindupBackstepDistance);
    impactX_ = targetWorldX - (dirX_ * kImpactOffsetDistance);
    impactY_ = targetWorldY - (dirY_ * kImpactOffsetDistance);

    totalDuration_ = kWindupDurationSeconds + kDashDurationSeconds +
        kImpactHoldDurationSeconds + kRecoveryDurationSeconds;
}

void MinakoStrikePresentation::start() {
    elapsedTime_ = 0.0f;
    phaseElapsed_ = 0.0f;
    pendingHitEvents_ = 0;
    pendingAbilityAudioCues_ = 1;
    impactHitTriggered_ = false;
    phase_ = Phase::Windup;
    casterWorldX_ = casterX_;
    casterWorldY_ = casterY_;
    casterWorldZ_ = casterZ_;
}

void MinakoStrikePresentation::update(float deltaSeconds) {
    if (phase_ == Phase::Complete) {
        return;
    }

    elapsedTime_ += deltaSeconds;
    phaseElapsed_ += deltaSeconds;

    switch (phase_) {
        case Phase::Windup: {
            const float progress = easing::clamp01(phaseElapsed_ / kWindupDurationSeconds);
            const float eased = easing::easeOutCubic(progress);
            casterWorldX_ = easing::lerp(casterX_, windupX_, eased);
            casterWorldY_ = easing::lerp(casterY_, windupY_, eased);
            casterWorldZ_ = casterZ_;
            if (progress >= 1.0f) {
                phase_ = Phase::Dash;
                phaseElapsed_ = 0.0f;
            }
            break;
        }
        case Phase::Dash: {
            const float progress = easing::clamp01(phaseElapsed_ / kDashDurationSeconds);
            const float eased = std::pow(progress, 2.6f);
            casterWorldX_ = easing::lerp(windupX_, impactX_, eased);
            casterWorldY_ = easing::lerp(windupY_, impactY_, eased);
            casterWorldZ_ = casterZ_ - (std::sin(progress * kPi) * kDashArcHeight);
            if (progress >= 1.0f) {
                phase_ = Phase::ImpactHold;
                phaseElapsed_ = 0.0f;
                casterWorldX_ = impactX_;
                casterWorldY_ = impactY_;
                casterWorldZ_ = impactZ_;
                if (!impactHitTriggered_) {
                    ++pendingHitEvents_;
                    impactHitTriggered_ = true;
                }
            }
            break;
        }
        case Phase::ImpactHold: {
            casterWorldX_ = impactX_;
            casterWorldY_ = impactY_;
            casterWorldZ_ = impactZ_;
            if (phaseElapsed_ >= kImpactHoldDurationSeconds) {
                phase_ = Phase::Recovery;
                phaseElapsed_ = 0.0f;
            }
            break;
        }
        case Phase::Recovery: {
            const float progress = easing::clamp01(phaseElapsed_ / kRecoveryDurationSeconds);
            const float eased = easing::easeOutCubic(progress);
            casterWorldX_ = easing::lerp(impactX_, casterX_, eased);
            casterWorldY_ = easing::lerp(impactY_, casterY_, eased);
            casterWorldZ_ = easing::lerp(impactZ_, casterZ_, eased);
            if (progress >= 1.0f) {
                phase_ = Phase::Complete;
            }
            break;
        }
        case Phase::Complete:
        default:
            break;
    }
}

void MinakoStrikePresentation::render(SDL_Renderer* renderer,
                                      int screenW,
                                      int screenH,
                                      const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool MinakoStrikePresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

bool MinakoStrikePresentation::overridesCamera() const {
    return true;
}

void MinakoStrikePresentation::applyCameraState(Camera3D& camera) const {
    const Camera3D introCamera = render::makeDefaultBattleActionIntroCamera();
    const Camera3D goalCamera = render::makeDefaultBattleCamera();
    const Camera3D trackingCamera = makeTrackingCamera(
        casterWorldX_,
        casterWorldY_,
        casterWorldZ_,
        0.72f,
        130.0f,
        92.0f,
        126.0f,
        33500.0f
    );
    const Camera3D impactCamera = makeTrackingCamera(
        impactX_,
        impactY_,
        impactZ_,
        0.92f,
        76.0f,
        54.0f,
        108.0f,
        24500.0f
    );

    switch (phase_) {
        case Phase::Windup: {
            blendCamera(camera, introCamera, trackingCamera, phaseElapsed_ / kWindupDurationSeconds);
            break;
        }
        case Phase::Dash: {
            camera = trackingCamera;
            const float progress = easing::clamp01(phaseElapsed_ / kDashDurationSeconds);
            camera.focalLength = easing::lerp(36000.0f, 25000.0f, progress);
            break;
        }
        case Phase::ImpactHold: {
            camera = impactCamera;
            const float shake = std::sin(phaseElapsed_ * 52.0f) * kImpactShakeOffset;
            camera.posX += perpX_ * shake;
            camera.posY += perpY_ * shake;
            camera.posZ -= std::cos(phaseElapsed_ * 38.0f) * 4.0f;
            break;
        }
        case Phase::Recovery: {
            blendCamera(camera, impactCamera, goalCamera, phaseElapsed_ / kRecoveryDurationSeconds);
            break;
        }
        case Phase::Complete:
        default:
            camera = goalCamera;
            break;
    }
}

bool MinakoStrikePresentation::getCasterWorldOverride(float& outX, float& outY, float& outZ) const {
    outX = casterWorldX_;
    outY = casterWorldY_;
    outZ = casterWorldZ_;
    return true;
}

int MinakoStrikePresentation::consumeHitEvents() {
    const int hitEvents = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hitEvents;
}

int MinakoStrikePresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

Camera3D MinakoStrikePresentation::makeTrackingCamera(float followX,
                                                      float followY,
                                                      float followZ,
                                                      float forwardLookBias,
                                                      float pullback,
                                                      float sideOffset,
                                                      float heightOffset,
                                                      float focalLength) const {
    const float lookX = easing::lerp(followX, targetX_, std::clamp(forwardLookBias, 0.0f, 1.0f));
    const float lookY = easing::lerp(followY, targetY_, std::clamp(forwardLookBias, 0.0f, 1.0f));
    const float lookZ = easing::lerp(followZ - 18.0f, targetZ_ - 96.0f, 0.78f);
    const float cameraX = followX - (dirX_ * pullback) - (perpX_ * sideOffset);
    const float cameraY = followY - (dirY_ * pullback) - (perpY_ * sideOffset);
    const float cameraZ = followZ - heightOffset;
    return makeLookCamera(cameraX, cameraY, cameraZ, lookX, lookY, lookZ, focalLength);
}

} // namespace battle
