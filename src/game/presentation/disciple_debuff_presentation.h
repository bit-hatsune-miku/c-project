#pragma once

#include "ability_presentation.h"

namespace battle {

class DiscipleDebuffPresentation : public AbilityPresentation {
public:
    enum class Variant {
        Skill,
        Ultimate
    };

    DiscipleDebuffPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ,
        Variant variant
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    int consumeAbilityAudioCues() override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderAboveHud() const override;

private:
    Variant variant_ = Variant::Skill;
    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    int pendingAbilityAudioCues_ = 0;
};

} // namespace battle
