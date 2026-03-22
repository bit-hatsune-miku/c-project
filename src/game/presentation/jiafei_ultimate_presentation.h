#pragma once

#include "ability_presentation.h"

namespace battle {

class JiafeiUltimatePresentation : public AbilityPresentation {
public:
    JiafeiUltimatePresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~JiafeiUltimatePresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool getCasterWorldOverride(float& outX, float& outY, float& outZ) const override;
    bool shouldRenderAboveHud() const override;
    bool shouldBlackoutWorld() const override;

private:
    void ensureTextureLoaded(SDL_Renderer* renderer);
    float cameraOrbitAngleRadians() const;
    float overlayProgress() const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    float stagedCasterX_ = 0.0f;
    float stagedCasterY_ = 0.0f;
    float stagedCasterZ_ = 0.0f;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    bool hitTriggered_ = false;
    SDL_Texture* headTexture_ = nullptr;
    bool attemptedTextureLoad_ = false;
};

} // namespace battle
