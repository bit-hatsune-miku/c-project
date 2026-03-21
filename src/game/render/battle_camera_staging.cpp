#include "battle_camera_staging.h"

#include <algorithm>
#include <cmath>

#include "../core/easing.h"

namespace battle::render {
namespace {

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

} // namespace

void BattleCameraStaging::reset(Camera3D& camera) {
    intro_ = IntroAnimation{};
    lastTurnToken_.clear();
    pendingCharacterTurnToken_.clear();
    queuedCharacterTurnIntro_ = false;
    cameraOscillationTime_ = 0.0f;
    applyGoalCamera(camera);
}

void BattleCameraStaging::snapToGoalCamera(Camera3D& camera) const {
    applyGoalCamera(camera);
}

void BattleCameraStaging::applyGoalCamera(Camera3D& camera) const {
    camera.posX = kGoalCameraPosX;
    camera.posY = kGoalCameraPosY;
    camera.posZ = kGoalCameraPosZ;
    camera.pitchDegrees = kGoalCameraPitch;
    camera.yawDegrees = kGoalCameraYaw;
    camera.focalLength = kGoalCameraFocal;
}

void BattleCameraStaging::startActionIntro(Camera3D& camera) {
    intro_.active = true;
    intro_.elapsed = 0.0f;
    intro_.duration = kActionIntroDurationSeconds;
    intro_.startX = kGoalCameraPosX + kActionIntroOffsetX;
    intro_.startY = kGoalCameraPosY + kActionIntroOffsetY;
    intro_.startZ = kGoalCameraPosZ + kActionIntroOffsetZ;
    intro_.startPitch = kGoalCameraPitch;
    intro_.startYaw = kGoalCameraYaw;
    intro_.startFocal = kGoalCameraFocal;
    intro_.goalX = kGoalCameraPosX;
    intro_.goalY = kGoalCameraPosY;
    intro_.goalZ = kGoalCameraPosZ;
    intro_.goalPitch = kGoalCameraPitch;
    intro_.goalYaw = kGoalCameraYaw;
    intro_.goalFocal = kGoalCameraFocal;

    camera.posX = intro_.startX;
    camera.posY = intro_.startY;
    camera.posZ = intro_.startZ;
    camera.pitchDegrees = intro_.startPitch;
    camera.yawDegrees = intro_.startYaw;
    camera.focalLength = intro_.startFocal;
}

void BattleCameraStaging::updateActionIntro(Camera3D& camera, float deltaSeconds) {
    if (!intro_.active) {
        return;
    }

    intro_.elapsed += deltaSeconds;
    const float t = battle::easing::clamp01(intro_.elapsed / std::max(0.001f, intro_.duration));
    const float eased = battle::easing::easeOutCubic(t);

    camera.posX = battle::easing::lerp(intro_.startX, intro_.goalX, eased);
    camera.posY = battle::easing::lerp(intro_.startY, intro_.goalY, eased);
    camera.posZ = battle::easing::lerp(intro_.startZ, intro_.goalZ, eased);
    camera.pitchDegrees = battle::easing::lerp(intro_.startPitch, intro_.goalPitch, eased);
    camera.yawDegrees = battle::easing::lerp(intro_.startYaw, intro_.goalYaw, eased);
    camera.focalLength = battle::easing::lerp(intro_.startFocal, intro_.goalFocal, eased);

    if (t >= 1.0f) {
        intro_.active = false;
        applyGoalCamera(camera);
    }
}

void BattleCameraStaging::notifyTurnPreview(const TurnState& turnState,
                                            int previewActorIndex,
                                            bool dialogueInProgress,
                                            bool freeViewEnabled,
                                            Camera3D& camera) {
    std::string turnToken = "none";
    bool nextIsCharacter = false;

    if (previewActorIndex >= 0 && previewActorIndex < static_cast<int>(turnState.actors.size())) {
        const TurnActor& actor = turnState.actors[static_cast<size_t>(previewActorIndex)];
        turnToken = (actor.type == ParticipantType::Boss ? "B:" : "C:") +
                    actor.key + ":" +
                    std::to_string(actor.partyIndex) + ":" +
                    (actor.isExtraTurn ? "E" : "N") + ":" +
                    std::to_string(static_cast<int>(actor.extraTurnAction)) + ":" +
                    (actor.autoExecute ? "A" : "M");
        nextIsCharacter = actor.type == ParticipantType::Character;
    }

    if (turnToken != lastTurnToken_) {
        if (nextIsCharacter) {
            if (dialogueInProgress || freeViewEnabled) {
                pendingCharacterTurnToken_ = turnToken;
            } else {
                startActionIntro(camera);
                pendingCharacterTurnToken_.clear();
            }
        } else {
            // Avoid playing stale character intro after the flow has moved on.
            pendingCharacterTurnToken_.clear();
        }
        lastTurnToken_ = std::move(turnToken);
    }

    if (queuedCharacterTurnIntro_) {
        if (nextIsCharacter) {
            if (dialogueInProgress || freeViewEnabled) {
                pendingCharacterTurnToken_ = lastTurnToken_;
            } else {
                startActionIntro(camera);
                pendingCharacterTurnToken_.clear();
                queuedCharacterTurnIntro_ = false;
            }
        }
    }

    if (!dialogueInProgress && !freeViewEnabled) {
        if (!pendingCharacterTurnToken_.empty() && nextIsCharacter && pendingCharacterTurnToken_ == lastTurnToken_) {
            startActionIntro(camera);
            pendingCharacterTurnToken_.clear();
        }
    }
}

void BattleCameraStaging::queueCharacterTurnIntro() {
    queuedCharacterTurnIntro_ = true;
}

void BattleCameraStaging::update(Camera3D& camera, float deltaSeconds, bool freeViewEnabled) {
    updateActionIntro(camera, deltaSeconds);

    if (!freeViewEnabled && !intro_.active) {
        applyGoalCamera(camera);
        cameraOscillationTime_ += deltaSeconds;
        camera.yawDegrees = kGoalCameraYaw + std::sin(cameraOscillationTime_ * 0.55f) * 1.8f;
    }
}

bool BattleCameraStaging::isIntroActive() const {
    return intro_.active;
}

} // namespace battle::render
