#pragma once

#include "ability_presentation.h"

#include <string>
#include <vector>

namespace battle {

class PomPomBossPresentation : public AbilityPresentation {
public:
    PomPomBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~PomPomBossPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    void renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void setTuningProfile(const PresentationTuningProfile& profile) override;

    void onSpacePressed() override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderFocusedTargetEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    int getFocusedPartyIndex() const override;
    void setTargetPartyIndex(int index) override;
    void setTargetWorldPosition(float x, float y, float z) override;
    void setPartyAssetNames(const std::vector<std::string>& assetNames) override;

    float getInputMultiplier() const override;
    PresentationFeedbackSignal getFeedbackSignal() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
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

    struct Obstacle {
        float progress = 0.0f;
        bool resolved = false;
    };

    void updateJump(float deltaTime);
    void ensureTexturesLoaded(SDL_Renderer* renderer);
    void spawnObstacle();
    void queueDamageHit(float multiplier);
    void resolveObstacleJudgement(Obstacle& obstacle);

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    Phase phase_ = Phase::Intro;
    int focusedPartyIndex_ = -1;

    float survivalDurationSeconds_ = 6.0f;
    float survivalElapsed_ = 0.0f;
    float baseObstacleSpeed_ = 550.0f;
    float baseSpawnInterval_ = 0.78f;
    float judgementProgress_ = 0.645f;
    float okayClearancePixels_ = 30.0f;
    float goodClearancePixels_ = 50.0f;
    float perfectClearancePixels_ = 68.0f;
    float spawnTimer_ = 0.0f;

    bool jumpActive_ = false;
    float jumpElapsedSeconds_ = 0.0f;
    float jumpOffsetPixels_ = 0.0f;

    std::vector<Obstacle> obstacles_;
    int pendingAbilityAudioCues_ = 0;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
    std::vector<float> pendingHitDamageMultipliers_;

    float damageMultiplier_ = 1.0f;
    int landedHitCount_ = 0;

    std::string targetAssetName_;
    SDL_Texture* obstacleTexture_ = nullptr;
    SDL_Texture* targetSpriteTexture_ = nullptr;
    bool texturesLoaded_ = false;
};

} // namespace battle
