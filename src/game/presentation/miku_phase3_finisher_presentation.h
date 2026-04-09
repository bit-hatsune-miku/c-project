#pragma once

#include "ability_presentation.h"

namespace battle {

class MikuPhase3FinisherPresentation : public AbilityPresentation {
public:
    MikuPhase3FinisherPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~MikuPhase3FinisherPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void preload(SDL_Renderer* renderer) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;

    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldRenderFloor() const override;

    void setOverlayTextures(SDL_Texture* casterSprite, SDL_Texture* targetSprite) override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;
    int consumeHitEvents() override;
    int consumeAbilityAudioCues() override;

private:
    enum class Phase {
        Gaze,
        PullBack,
        Hold,
        Beam,
        WhiteHold,
        Settle,
        Complete
    };

    struct WorldPoint {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    void ensureAssetsLoaded(SDL_Renderer* renderer);
    void releaseAssets();
    Phase currentPhase() const;
    WorldPoint currentMikuAnchor() const;
    WorldPoint currentBeamSource() const;
    float currentMikuAngleDegrees() const;
    Uint8 currentLyooAlpha() const;
    void drawWorldSprite(SDL_Renderer* renderer,
                         const Camera3D& camera,
                         SDL_Texture* texture,
                         float fallbackAspect,
                         float worldX,
                         float worldY,
                         float worldZ,
                         float worldHeightUnits,
                         float widthScale,
                         double angleDegrees,
                         Uint8 alpha,
                         SDL_Color tint,
                         bool anchorFeet) const;
    void drawChargeSource(SDL_Renderer* renderer,
                          const Camera3D& camera,
                          const WorldPoint& sourceWorld,
                          float intensity) const;
    void drawBeam(SDL_Renderer* renderer,
                  const Camera3D& camera,
                  const WorldPoint& sourceWorld,
                  const WorldPoint& targetWorld,
                  int screenW,
                  int screenH,
                  float beamProgress,
                  float alphaScale) const;
    void drawWhiteOverlay(SDL_Renderer* renderer, int screenW, int screenH, float alpha) const;

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

    SDL_Texture* mikuTexture_ = nullptr;
    int mikuTextureWidth_ = 0;
    int mikuTextureHeight_ = 0;
    bool attemptedLoad_ = false;
    SDL_Renderer* loadedRenderer_ = nullptr;

    SDL_Texture* overlayCasterSpriteTexture_ = nullptr;
    SDL_Texture* overlayTargetSpriteTexture_ = nullptr;

    std::vector<PresentationAudioCommand> pendingAudioCommands_;
    int pendingHitEvents_ = 0;
    int pendingAbilityAudioCues_ = 0;
    bool beamHitQueued_ = false;
    bool introVoiceQueued_ = false;
};

} // namespace battle
