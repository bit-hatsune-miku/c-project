#pragma once
#include "ability_presentation.h"

namespace battle {

class MikuDiandongPresentation : public AbilityPresentation {
public:
    MikuDiandongPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaSeconds) override;
    bool isComplete() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool getCasterWorldOverride(float& outX, float& outY, float& outZ) const override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    int consumeHitEvents() override;

private:
    float casterX_, casterY_, casterZ_;
    float targetX_, targetY_, targetZ_;
    float elapsed_;
    float totalDuration_;
    
    // Phase durations
    static constexpr float kFirstViewDuration = 1.2f;   // Initial angle
    static constexpr float kSideViewDuration = 1.2f;    // Side angle
    static constexpr float kLastViewDuration = 1.0f;    // Final impact angle
    
    enum class Phase {
        FirstView,
        SideView,
        LastView,
        Complete
    };
    
    Phase currentPhase_;
    float phaseElapsed_;
    bool hasTriggeredHit_;
    int pendingHitEvents_;
};

} // namespace battle
