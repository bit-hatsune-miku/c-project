#pragma once

#include "ability_presentation.h"

#include <string>
#include <vector>

namespace battle {

class LyooPlotTwistPresentation : public AbilityPresentation {
public:
    LyooPlotTwistPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~LyooPlotTwistPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;

    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldBlackoutWorld() const override;

    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;

private:
    enum class Phase {
        IntroFrames,
        TransitionToVoid,
        RingStars,
        SkyStars,
        ApproachAllies,
        MassiveStar,
        FullBlackout,
        FinalShake,
        Complete
    };

    struct StarInstance {
        float spawnTime = 0.0f;
        float lifetime = 0.6f;
        float orbitAngleDegrees = 0.0f;
        float orbitRadius = 0.0f;
        float baseSize = 42.0f;
        float rotationSpeed = 180.0f;
        float normalizedX = 0.5f;
        float normalizedY = 0.5f;
        float endNormalizedX = 0.5f;
        float endNormalizedY = 0.5f;
    };

    static std::string resolvePath(const std::string& relativePath);

    void ensureAssetsLoaded(SDL_Renderer* renderer);
    void releaseAssets();
    void seedStarTimelines();

    Phase currentPhase() const;
    int currentIntroFrameIndex() const;

    void renderIntroFrames(SDL_Renderer* renderer, int screenW, int screenH);
    void renderTransitionToVoid(SDL_Renderer* renderer, int screenW, int screenH);
    void renderRingStars(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera);
    void renderSkyStars(SDL_Renderer* renderer, int screenW, int screenH);
    void renderApproachOverlay(SDL_Renderer* renderer, int screenW, int screenH);
    void renderScatterStars(SDL_Renderer* renderer, int screenW, int screenH);
    void renderMassiveStar(SDL_Renderer* renderer, int screenW, int screenH);
    void renderFullBlackout(SDL_Renderer* renderer, int screenW, int screenH);

    void drawCenteredTexture(SDL_Renderer* renderer,
                             SDL_Texture* texture,
                             float centerX,
                             float centerY,
                             float width,
                             float height,
                             double angleDegrees,
                             Uint8 alpha) const;
    void drawStarSprite(SDL_Renderer* renderer,
                        float centerX,
                        float centerY,
                        float size,
                        double angleDegrees,
                        Uint8 alpha) const;
    float starScaleForLifeProgress(float progress) const;
    void applyShake(Camera3D& camera, float timeSeconds, float magnitude) const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    std::vector<SDL_Texture*> frames_;
    SDL_Texture* starTexture_ = nullptr;
    bool attemptedLoad_ = false;
    int starTextureWidth_ = 0;
    int starTextureHeight_ = 0;

    Camera3D frontCamera_{};
    Camera3D skyCamera_{};
    Camera3D wideCamera_{};

    std::vector<StarInstance> ringStars_;
    std::vector<StarInstance> skyStars_;
    std::vector<StarInstance> scatterStars_;

    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    bool impactTriggered_ = false;
    int hitBurstsEmitted_ = 0;
};

} // namespace battle
