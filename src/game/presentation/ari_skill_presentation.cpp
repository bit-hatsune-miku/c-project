#include "ari_skill_presentation.h"

#include "../core/easing.h"

namespace battle {
namespace {

constexpr float kOutroDurationSeconds = 0.90f;
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

AriSkillPresentation::AriSkillPresentation(
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

void AriSkillPresentation::start() {
    elapsedTime_ = 0.0f;
    outroElapsed_ = 0.0f;
    pendingAbilityAudioCues_ = 1;
}

void AriSkillPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
    outroElapsed_ += deltaTime;
}

void AriSkillPresentation::render(SDL_Renderer* renderer,
                                  int screenW,
                                  int screenH,
                                  const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool AriSkillPresentation::isComplete() const {
    return outroElapsed_ >= kOutroDurationSeconds;
}

int AriSkillPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

bool AriSkillPresentation::overridesCamera() const {
    return true;
}

void AriSkillPresentation::applyCameraState(Camera3D& camera) const {
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

bool AriSkillPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool AriSkillPresentation::shouldRenderCasterEntity() const {
    return true;
}

bool AriSkillPresentation::shouldRenderBossEntity() const {
    return false;
}

bool AriSkillPresentation::shouldRenderAboveHud() const {
    return false;
}

bool AriSkillPresentation::shouldUseCenteredPartyLayout() const {
    return true;
}

} // namespace battle
