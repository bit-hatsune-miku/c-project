#pragma once
#include "ability_presentation.h"

namespace battle {

class MikuDiandongPresentation : public AbilityPresentation {
public:
    ~MikuDiandongPresentation() override;
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
    int consumeAbilityAudioCues() override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderAboveHud() const override;
    void setExternalTextures(SDL_Texture* casterSprite, SDL_Texture* targetSprite) override;
    std::optional<SplashArtConfig> getSplashConfig(SDL_Texture* sprite) const override;

private:
    enum class Phase {
        Chase,
        ImpactFreeze,
        EaseOut,
        Complete
    };

    void updateMikuPosition(float progress);
    Camera3D computeChaseCamera(float cameraProgress, float lookX, float lookY, float lookZ) const;
    void drawDiandongSprite(SDL_Renderer* renderer, const Camera3D& camera,
                            float worldX, float worldY, float worldZ,
                            bool silhouette, float scaleMultiplier = 1.0f) const;
    void drawSilhouette(SDL_Renderer* renderer, const Camera3D& camera,
                        SDL_Texture* texture,
                        float worldX, float worldY, float worldZ,
                        float baseHeight, float widthScale) const;
    bool ensureDiandongTexture(SDL_Renderer* renderer);
    void drawFreezeOverlay(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) const;

    float casterX_;
    float casterY_;
    float casterZ_;
    float targetX_;
    float targetY_;
    float targetZ_;

    float mikuStartX_;
    float mikuStartY_;
    float mikuStartZ_;
    float mikuTargetX_;
    float mikuTargetY_;
    float mikuTargetZ_;

    float mikuWorldX_;
    float mikuWorldY_;
    float mikuWorldZ_;

    float impactMikuWorldX_;
    float impactMikuWorldY_;
    float impactMikuWorldZ_;

    float phaseElapsed_;
    float chaseTime_;
    float cameraProgress_;
    float impactCameraProgress_;
    float easeOutProgress_;
    bool impactHitTriggered_;
    int pendingHitEvents_ = 0;
    int pendingAbilityAudioCues_ = 0;

    Phase phase_;

    SDL_Texture* diandongTexture_ = nullptr;
    int diandongTextureWidth_ = 0;
    int diandongTextureHeight_ = 0;
    bool attemptedTextureLoad_ = false;

    SDL_Texture* casterSpriteTexture_ = nullptr;
    SDL_Texture* targetSpriteTexture_ = nullptr;
};

} // namespace battle
