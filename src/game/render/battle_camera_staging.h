#ifndef BATTLE_CAMERA_STAGING_H
#define BATTLE_CAMERA_STAGING_H

#include <string>

#include "camera_3d.h"
#include "../core/battle_manager.h"

namespace battle::render {

Camera3D makeDefaultBattleCamera();

class BattleCameraStaging {
public:
    BattleCameraStaging();

    void setGoalCamera(const Camera3D& camera);
    void reset(Camera3D& camera);
    void snapToGoalCamera(Camera3D& camera) const;

    void notifyTurnPreview(const TurnState& turnState,
                           int previewActorIndex,
                           bool dialogueInProgress,
                           bool freeViewEnabled,
                           Camera3D& camera);

    void queueCharacterTurnIntro();

    void update(Camera3D& camera, float deltaSeconds, bool freeViewEnabled);
    bool isIntroActive() const;

private:
    struct IntroAnimation {
        bool active = false;
        float elapsed = 0.0f;
        float duration = 0.22f;
        float startX = 0.0f;
        float startY = 0.0f;
        float startZ = 0.0f;
        float startPitch = 0.0f;
        float startYaw = 0.0f;
        float startFocal = 0.0f;
        float goalX = 0.0f;
        float goalY = 0.0f;
        float goalZ = 0.0f;
        float goalPitch = 0.0f;
        float goalYaw = 0.0f;
        float goalFocal = 0.0f;
    };

    void applyGoalCamera(Camera3D& camera) const;
    void startActionIntro(Camera3D& camera);
    void updateActionIntro(Camera3D& camera, float deltaSeconds);

    IntroAnimation intro_;
    std::string lastTurnToken_;
    std::string pendingCharacterTurnToken_;
    bool queuedCharacterTurnIntro_ = false;
    float cameraOscillationTime_ = 0.0f;
    Camera3D goalCamera_{};
};

} // namespace battle::render

#endif
