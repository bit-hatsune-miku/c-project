#include "miku_diandong_presentation.h"
#include "../core/easing.h"
#include <cmath>

namespace battle {

MikuDiandongPresentation::MikuDiandongPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ)
    , elapsed_(0.0f)
    , totalDuration_(kFirstViewDuration + kSideViewDuration + kLastViewDuration)
    , currentPhase_(Phase::FirstView)
    , phaseElapsed_(0.0f)
    , hasTriggeredHit_(false)
    , pendingHitEvents_(0)
{
}

void MikuDiandongPresentation::start() {
    elapsed_ = 0.0f;
    currentPhase_ = Phase::FirstView;
    phaseElapsed_ = 0.0f;
    hasTriggeredHit_ = false;
    pendingHitEvents_ = 0;
}

void MikuDiandongPresentation::update(float deltaSeconds) {
    if (currentPhase_ == Phase::Complete) {
        return;
    }

    elapsed_ += deltaSeconds;
    phaseElapsed_ += deltaSeconds;

    // Phase transitions
    if (currentPhase_ == Phase::FirstView && phaseElapsed_ >= kFirstViewDuration) {
        currentPhase_ = Phase::SideView;
        phaseElapsed_ = 0.0f;
    } else if (currentPhase_ == Phase::SideView && phaseElapsed_ >= kSideViewDuration) {
        currentPhase_ = Phase::LastView;
        phaseElapsed_ = 0.0f;
    } else if (currentPhase_ == Phase::LastView && phaseElapsed_ >= kLastViewDuration) {
        // Trigger hit at the end of last view
        if (!hasTriggeredHit_) {
            ++pendingHitEvents_;
            hasTriggeredHit_ = true;
        }
        currentPhase_ = Phase::Complete;
    }
}

bool MikuDiandongPresentation::isComplete() const {
    return currentPhase_ == Phase::Complete;
}

bool MikuDiandongPresentation::overridesCamera() const {
    return true;
}

void MikuDiandongPresentation::applyCameraState(Camera3D& camera) const {
    // Immediate cuts between three camera angles - no interpolation
    
    if (currentPhase_ == Phase::FirstView) {
        camera.posX = -504.099f;
        camera.posY = 681.015f;
        camera.posZ = -175.0f;
        camera.pitchDegrees = -11.0f;
        camera.yawDegrees = 157.8f;
        camera.focalLength = 50000.0f;
        
    } else if (currentPhase_ == Phase::SideView) {
        camera.posX = 30.0214f;
        camera.posY = 69.7355f;
        camera.posZ = -175.0f;
        camera.pitchDegrees = -11.0f;
        camera.yawDegrees = 31.7996f;
        camera.focalLength = 50000.0f;
        
    } else if (currentPhase_ == Phase::LastView || currentPhase_ == Phase::Complete) {
        camera.posX = -850.871f;
        camera.posY = 74.5422f;
        camera.posZ = -175.0f;
        camera.pitchDegrees = -17.0f;
        camera.yawDegrees = -48.2159f;
        camera.focalLength = 29000.0f;
    }
}

bool MikuDiandongPresentation::getCasterWorldOverride(float& outX, float& outY, float& outZ) const {
    // Miku continuously moves toward boss across all phases
    // On each camera cut, reset her position slightly back to create illusion of constant movement
    
    float progress = 0.0f;
    
    if (currentPhase_ == Phase::FirstView) {
        // First third of journey, within this view
        const float localT = easing::clamp01(phaseElapsed_ / kFirstViewDuration);
        progress = localT * 0.33f;  // 0% to 33%
        
    } else if (currentPhase_ == Phase::SideView) {
        // Second third - reset back slightly then continue
        const float localT = easing::clamp01(phaseElapsed_ / kSideViewDuration);
        // Start at 20% (jump back from 33%) and go to 60%
        progress = 0.20f + localT * 0.40f;
        
    } else if (currentPhase_ == Phase::LastView || currentPhase_ == Phase::Complete) {
        // Final third - reset back slightly then reach target
        const float localT = easing::clamp01(phaseElapsed_ / kLastViewDuration);
        // Start at 50% (jump back from 60%) and go to 100%
        progress = 0.50f + localT * 0.50f;
    }
    
    progress = easing::easeOutCubic(progress);
    
    outX = easing::lerp(casterX_, targetX_, progress);
    outY = easing::lerp(casterY_, targetY_, progress);
    outZ = casterZ_;
    
    return true;
}

void MikuDiandongPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    // No custom rendering needed - just using sprite position override
    // The main demo loop will render Miku at the overridden position
}

int MikuDiandongPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

} // namespace battle