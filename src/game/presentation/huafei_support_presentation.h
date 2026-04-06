#pragma once

#include "ability_presentation.h"

namespace battle {

class HuafeiSupportPresentation : public AbilityPresentation {
public:
    HuafeiSupportPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~HuafeiSupportPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void preload(SDL_Renderer* renderer) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    void renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    int consumeAbilityAudioCues() override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldRenderFloor() const override;
    bool shouldUseCenteredPartyLayout() const override;

private:
    void ensureBackdropLoaded(SDL_Renderer* renderer);
    void releaseBackdrop();

    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    float phaseElapsed_ = 0.0f;
    int pendingAbilityAudioCues_ = 0;
    SDL_Texture* backdropTexture_ = nullptr;
    bool backdropLoadAttempted_ = false;
};

} // namespace battle
