#pragma once

#include "ability_presentation.h"

namespace battle {

class TetoUltimatePresentation : public AbilityPresentation {
public:
    TetoUltimatePresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldUseCenteredPartyLayout() const override;

private:
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    float outroElapsed_ = 0.0f;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
};

} // namespace battle
