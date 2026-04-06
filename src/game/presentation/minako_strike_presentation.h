#pragma once

#include "ability_presentation.h"

namespace battle {

class MinakoStrikePresentation : public AbilityPresentation {
public:
    MinakoStrikePresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaSeconds) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool getCasterWorldOverride(float& outX, float& outY, float& outZ) const override;
    int consumeHitEvents() override;
    int consumeAbilityAudioCues() override;

private:
    enum class Phase {
        Windup,
        Dash,
        ImpactHold,
        Recovery,
        Complete
    };

    Camera3D makeTrackingCamera(float followX,
                                float followY,
                                float followZ,
                                float forwardLookBias,
                                float pullback,
                                float sideOffset,
                                float heightOffset,
                                float focalLength) const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    float dirX_ = 0.0f;
    float dirY_ = 1.0f;
    float perpX_ = -1.0f;
    float perpY_ = 0.0f;

    float windupX_ = 0.0f;
    float windupY_ = 0.0f;
    float windupZ_ = 0.0f;
    float impactX_ = 0.0f;
    float impactY_ = 0.0f;
    float impactZ_ = 0.0f;

    float casterWorldX_ = 0.0f;
    float casterWorldY_ = 0.0f;
    float casterWorldZ_ = 0.0f;
    float phaseElapsed_ = 0.0f;

    int pendingHitEvents_ = 0;
    int pendingAbilityAudioCues_ = 0;
    bool impactHitTriggered_ = false;
    Phase phase_ = Phase::Complete;
};

} // namespace battle
