#pragma once

#include "ability_presentation.h"

#include <string>
#include <vector>

namespace battle {

class TetoBossPresentation : public AbilityPresentation {
public:
    TetoBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~TetoBossPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    void renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void setTuningProfile(const PresentationTuningProfile& profile) override;

    bool onKeyPressed(SDL_Keycode key) override;
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

    struct Baguette {
        int lane;       // 0 = bottom, 1 = middle, 2 = top
        bool fromRight; // true = right to left, false = left to right
        float progress; // 0.0 to 1.0
    };

    void ensureTexturesLoaded(SDL_Renderer* renderer);
    SDL_Texture* loadTextureFallback(SDL_Renderer* renderer, const std::string& path) const;
    void spawnBaguette();

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    Phase phase_ = Phase::Intro;
    int focusedPartyIndex_ = -1;
    
    float survivalDurationSeconds_ = 5.0f;
    float survivalElapsed_ = 0.0f;
    
    float baseBaguetteSpeed_ = 500.0f;
    float baseSpawnInterval_ = 0.8f;
    
    float spawnTimer_ = 0.0f;
    
    int playerLane_ = 0; // 0 = bottom, 1 = middle, 2 = top
    
    std::vector<Baguette> baguettes_;

    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;

    float damageMultiplier_ = 1.0f;
    float targetDamageMultiplier_ = 1.0f;

    SDL_Texture* baguetteTexture_ = nullptr;
    
    std::string targetAssetName_;
    SDL_Texture* targetIconTexture_ = nullptr;
    
    float currentLaneOffset_ = 0.0f;
    bool texturesLoaded_ = false;
};

} // namespace battle
