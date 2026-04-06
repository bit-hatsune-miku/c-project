#pragma once
#include "ability_presentation.h"

#include <vector>

namespace battle {

class SailorVenusBossPresentation : public AbilityPresentation {
public:
    enum class Variant {
        BossParry,
        LoveAndBeautyShock,
        Transformation,
        CrescentBeam,
        LoveMeChain
    };

    SailorVenusBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ,
        Variant variant = Variant::BossParry
    );
    ~SailorVenusBossPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void preload(SDL_Renderer* renderer) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void setExternalTextures(SDL_Texture* caster, SDL_Texture* target) override;
    void setOverlayTextures(SDL_Texture* caster, SDL_Texture* target) override;
    void setTargetWorldPosition(float x, float y, float z) override;

    void onSpacePressed() override;
    float getInputMultiplier() const override;
    float consumeHitDamageMultiplier() override;
    PresentationFeedbackSignal getFeedbackSignal() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override { return 1; }
    std::string getInputResultText() const override;
    int getScoreValue() const override;
    void setPresentationValue(int value) override;

    bool shouldRenderAboveHud() const override { return false; }
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    void setTuningProfile(const PresentationTuningProfile& profile) override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;

private:
    struct FrameSlot {
        struct SDL_Texture* texture = nullptr;
        bool attemptedLoad = false;
    };

    void releaseFrames();
    void maintainFrameWindow(struct SDL_Renderer* renderer, int frameIndex);
    void ensureFrameLoaded(struct SDL_Renderer* renderer, int frameIndex);
    float bossParryInputRatio() const;
    float loveAndBeautyShockFinalMultiplier() const;
    float loveMeChainFinalMultiplier() const;
    bool usesImpactShot() const;
    bool isImpactShotActive() const;
    float sequencePlaybackDuration() const;
    float hitTriggerTime() const;
    float impactShotProgress() const;
    void drawImpactSilhouette(SDL_Renderer* renderer,
                              const Camera3D& camera,
                              SDL_Texture* texture,
                              float worldX,
                              float worldY,
                              float worldZ,
                              bool isBoss,
                              float alpha) const;

    Variant variant_ = Variant::BossParry;
    int quota_ = 10;
    int spacesPressed_ = 0;
    int presentationValue_ = 0;
    bool hasHit_ = false;
    bool bgmPaused_ = false;
    bool bgmResumeQueued_ = false;
    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    SDL_Texture* casterSpriteTexture_ = nullptr;
    SDL_Texture* targetSpriteTexture_ = nullptr;
    SDL_Texture* overlayCasterSpriteTexture_ = nullptr;
    SDL_Texture* overlayTargetSpriteTexture_ = nullptr;

    std::vector<FrameSlot> frames_;
    std::vector<PresentationAudioCommand> audioCmds_;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
};

}
