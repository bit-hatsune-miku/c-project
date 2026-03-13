#include "cupcakke_drum_presentation.h"

#include <cmath>

#include "../core/easing.h"

namespace battle {

CupcakkeDrumPresentation::CupcakkeDrumPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterStartX_(casterWorldX)
    , casterStartY_(casterWorldY)
    , casterStartZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ)
    , casterCurrentX_(casterWorldX)
    , casterCurrentY_(casterWorldY)
    , casterCurrentZ_(casterWorldZ) {
    totalDuration_ = orbitDuration_ + holdDuration_ + dashDuration_;
}

void CupcakkeDrumPresentation::start() {
    elapsedTime_ = 0.0f;
    casterCurrentX_ = casterStartX_;
    casterCurrentY_ = casterStartY_;
    casterCurrentZ_ = casterStartZ_;
}

void CupcakkeDrumPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    const float dashStart = orbitDuration_ + holdDuration_;
    if (elapsedTime_ < dashStart) {
        casterCurrentX_ = casterStartX_;
        casterCurrentY_ = casterStartY_;
        casterCurrentZ_ = casterStartZ_;
        return;
    }

    const float t = easing::clamp01((elapsedTime_ - dashStart) / std::max(0.001f, dashDuration_));
    const float eased = easing::easeOutBack(t);

    // Fast violent lunge to boss.
    casterCurrentX_ = easing::lerp(casterStartX_, targetX_ - 95.0f, eased);
    casterCurrentY_ = easing::lerp(casterStartY_, targetY_ - 95.0f, eased);
    casterCurrentZ_ = easing::lerp(casterStartZ_ - 35.0f, targetZ_ - 45.0f, eased);
}

void CupcakkeDrumPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool CupcakkeDrumPresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

bool CupcakkeDrumPresentation::overridesCamera() const {
    return true;
}

void CupcakkeDrumPresentation::applyCameraState(Camera3D& camera) const {
    // Storyboard anchor requested by user.
    constexpr float kAnchorX = -630.0f;
    constexpr float kAnchorY = 240.0f;
    constexpr float kAnchorZ = -280.0f;
    constexpr float kAnchorPitch = 1.0f;
    constexpr float kAnchorYaw = -50.9168f;
    constexpr float kAnchorFocal = 28500.0f;

    const float tOrbit = easing::clamp01(elapsedTime_ / std::max(0.001f, orbitDuration_));
    const float orbitEnd = orbitDuration_;
    const float holdEnd = orbitDuration_ + holdDuration_;

    if (elapsedTime_ <= orbitEnd) {
        const float a = tOrbit * 3.14159265f;
        camera.posX = kAnchorX + std::cos(a) * 40.0f;
        camera.posY = kAnchorY + std::sin(a) * 28.0f;
        camera.posZ = kAnchorZ + std::sin(a * 0.7f) * 14.0f;
        camera.pitchDegrees = kAnchorPitch + std::sin(a) * 1.4f;
        camera.yawDegrees = kAnchorYaw + std::sin(a) * 10.0f;
        camera.focalLength = kAnchorFocal;
        return;
    }

    if (elapsedTime_ <= holdEnd) {
        camera.posX = kAnchorX;
        camera.posY = kAnchorY;
        camera.posZ = kAnchorZ;
        camera.pitchDegrees = kAnchorPitch;
        camera.yawDegrees = kAnchorYaw;
        camera.focalLength = kAnchorFocal;
        return;
    }

    const float dashT = easing::clamp01((elapsedTime_ - holdEnd) / std::max(0.001f, dashDuration_));
    const float eased = easing::easeOutBack(dashT);

    // Twin camera with Cupcakke during lunge: keep the same initial distance/offset.
    const float offsetX = kAnchorX - casterStartX_;
    const float offsetY = kAnchorY - casterStartY_;
    const float offsetZ = kAnchorZ - casterStartZ_;

    const float casterX = easing::lerp(casterStartX_, targetX_ - 95.0f, eased);
    const float casterY = easing::lerp(casterStartY_, targetY_ - 95.0f, eased);
    const float casterZ = easing::lerp(casterStartZ_ - 35.0f, targetZ_ - 45.0f, eased);

    camera.posX = casterX + offsetX;
    camera.posY = casterY + offsetY;
    camera.posZ = casterZ + offsetZ;

    // Keep framing stable and increase FOV (lower focal length) during dash.
    camera.pitchDegrees = kAnchorPitch + 0.8f;
    camera.yawDegrees = kAnchorYaw + 2.0f;
    camera.focalLength = easing::lerp(kAnchorFocal, 20500.0f, dashT);
}

bool CupcakkeDrumPresentation::getCasterWorldOverride(float& outX, float& outY, float& outZ) const {
    const float dashStart = orbitDuration_ + holdDuration_;
    if (elapsedTime_ < dashStart) {
        return false;
    }

    outX = casterCurrentX_;
    outY = casterCurrentY_;
    outZ = casterCurrentZ_;
    return true;
}

} // namespace battle
