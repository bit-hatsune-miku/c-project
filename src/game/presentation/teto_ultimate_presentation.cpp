#include "teto_ultimate_presentation.h"

#include "../core/easing.h"

namespace battle {
namespace {

constexpr float kOutroDurationSeconds = 1.5f;
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

} // namespace

TetoUltimatePresentation::TetoUltimatePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) : targetX_(targetWorldX)
  , targetY_(targetWorldY)
  , targetZ_(targetWorldZ) {
    (void)casterWorldX;
    (void)casterWorldY;
    (void)casterWorldZ;
    totalDuration_ = kOutroDurationSeconds;
}

void TetoUltimatePresentation::start() {
    elapsedTime_ = 0.0f;
    outroElapsed_ = 0.0f;
    pendingAbilityAudioCues_ = 1;
    pendingHitEvents_ = 1;
}

void TetoUltimatePresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
    outroElapsed_ += deltaTime;
}

void TetoUltimatePresentation::render(SDL_Renderer* renderer,
                                       int screenW,
                                       int screenH,
                                       const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool TetoUltimatePresentation::isComplete() const {
    return outroElapsed_ >= kOutroDurationSeconds;
}

int TetoUltimatePresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int TetoUltimatePresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

bool TetoUltimatePresentation::overridesCamera() const {
    return true;
}

void TetoUltimatePresentation::applyCameraState(Camera3D& camera) const {
    const Camera3D startCamera = makeSupportZoomStartCamera(targetX_, targetY_, targetZ_);
    const Camera3D endCamera = makeSupportZoomEndCamera(targetX_, targetY_, targetZ_);
    const float t = easing::easeOutQuint(
        easing::clamp01(outroElapsed_ / kOutroDurationSeconds)
    );

    camera.posX = lerpF(startCamera.posX, endCamera.posX, t);
    camera.posY = lerpF(startCamera.posY, endCamera.posY, t);
    camera.posZ = lerpF(startCamera.posZ, endCamera.posZ, t);
    camera.pitchDegrees = lerpF(startCamera.pitchDegrees, endCamera.pitchDegrees, t);
    camera.yawDegrees = lerpF(startCamera.yawDegrees, endCamera.yawDegrees, t);
    camera.focalLength = lerpF(startCamera.focalLength, endCamera.focalLength, t);
}

bool TetoUltimatePresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool TetoUltimatePresentation::shouldRenderCasterEntity() const {
    return false;
}

bool TetoUltimatePresentation::shouldRenderBossEntity() const {
    return true;
}

bool TetoUltimatePresentation::shouldRenderAboveHud() const {
    return false;
}

bool TetoUltimatePresentation::shouldUseCenteredPartyLayout() const {
    return true;
}

} // namespace battle
