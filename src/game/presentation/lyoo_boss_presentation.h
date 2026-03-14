#pragma once

#include "ability_presentation.h"

#include <array>
#include <string>
#include <vector>

namespace battle {

class LyooBossPresentation : public AbilityPresentation {
public:
    LyooBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~LyooBossPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderAboveHud() const override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;
    std::optional<SplashArtConfig> getSplashConfig(SDL_Texture* sprite) const override;

private:
    enum class Phase {
        IntroFrames,
        AccelLoop,
        HoldFrame13Ui,
        ZoomOut,
        Complete
    };

    struct RedPulse {
        float startX = 0.0f;
        float startY = 0.0f;
        float startZ = 0.0f;
        float targetX = 0.0f;
        float targetY = 0.0f;
        float targetZ = 0.0f;
        float elapsed = 0.0f;
        float duration = 0.55f;
        bool active = true;
    };

    static std::string resolvePath(const std::string& relativePath);
    void ensureFramesLoaded(SDL_Renderer* renderer);
    void releaseFrames();
    void updateUiFrameStepper(float deltaTime);
    void enterZoomOutPhase(const Camera3D& referenceCamera, int screenW, int screenH);
    void spawnPulse();
    void updatePulses(float deltaTime);
    bool allPulsesFinished() const;

    void renderUiFrame(SDL_Renderer* renderer, int screenW, int screenH);
    void renderWorldBossFrame(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera);
    void renderPulses(SDL_Renderer* renderer, const Camera3D& camera);

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    std::vector<SDL_Texture*> frames_;
    bool attemptedLoad_ = false;

    Phase phase_ = Phase::IntroFrames;
    int uiFrameIndex_ = 0;
    float uiFrameTimer_ = 0.0f;
    float currentLoopFps_ = 6.0f;
    int loopCycles_ = 0;
    float holdFrame13Timer_ = 0.0f;

    bool zoomInitialized_ = false;
    float zoomElapsed_ = 0.0f;
    float postZoomHold_ = 0.0f;
    float worldSpriteHeightUnits_ = 480.0f;

    Camera3D frontCamera_{};
    Camera3D gameplayCamera_{};

    std::array<float, 4> pulseSpawnTimes_{{0.12f, 0.36f, 0.62f, 0.86f}};
    int pulsesSpawned_ = 0;
    std::vector<RedPulse> pulses_;
    int pendingHitEvents_ = 0;
};

} // namespace battle
