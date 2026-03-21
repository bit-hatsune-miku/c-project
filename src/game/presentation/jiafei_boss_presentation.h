#pragma once

#include "ability_presentation.h"

#include <string>
#include <vector>

namespace battle {

class JiafeiBossPresentation : public AbilityPresentation {
public:
    JiafeiBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~JiafeiBossPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    void renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    void onKeyPressed(SDL_Keycode key) override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool getTargetWorldOverride(float& outX, float& outY, float& outZ) const override;
    int getFocusedPartyIndex() const override;
    void setTargetPartyIndex(int index) override;
    void setTargetWorldPosition(float x, float y, float z) override;

    float getInputMultiplier() const override;
    float consumeHitDamageMultiplier() override;
    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;
    std::string getInputResultText() const override;
    std::optional<SplashArtConfig> getSplashConfig(SDL_Texture* sprite) const override;

private:
    enum class Phase {
        Intro,
        Attack,
        Complete
    };

    void ensureTexturesLoaded(SDL_Renderer* renderer);
    SDL_Texture* loadTextureFallback(SDL_Renderer* renderer, const std::string& path) const;
    float interp(float a, float b, float t) const;
    float waveImpactTimeSeconds() const;
    bool isCorrectDodgeDirection(int direction) const;
    int resolveWaveHeadHits() const;
    void updateDodgeMotion(float deltaTime);
    float currentDodgeOffsetWorld() const;
    bool computeHeadRenderState(const Camera3D& camera,
                                bool& outBehindTarget,
                                SDL_FPoint& outMidScreen,
                                SDL_FPoint& outSideScreen,
                                float& outHeadSize,
                                Uint8& outAlpha) const;
    void renderHeads(SDL_Renderer* renderer, const Camera3D& camera, bool behindTargetPass);

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    Phase phase_ = Phase::Intro;
    int focusedPartyIndex_ = -1;
    int totalWaves_ = 8;
    int currentWave_ = 0;
    bool sideFromLeft_ = false;
    float waveElapsed_ = 0.0f;
    float waveDuration_ = 1.35f;
    bool inputReceived_ = false;
    bool inputCorrect_ = false;
    bool canReceiveInput_ = false;
    int successfulDodges_ = 0;
    bool doStartRandom_ = false;
    float dodgeElapsed_ = 0.0f;
    int dodgeDirection_ = 0;
    bool waveImpactResolved_ = false;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;

    float damageMultiplier_ = 1.0f;

    SDL_Texture* middleHeadTexture_ = nullptr;
    SDL_Texture* sideHeadTexture_ = nullptr;
    bool texturesLoaded_ = false;

    enum class DodgeMotionState {
        Idle,
        Outbound,
        Hold,
        Return
    };

    DodgeMotionState dodgeMotionState_ = DodgeMotionState::Idle;
};

} // namespace battle
