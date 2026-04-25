#pragma once

#include "ability_presentation.h"

#include <string>
#include <vector>

namespace battle {

class AriBossPresentation : public AbilityPresentation {
public:
    struct NativeRenderState {
        bool active = false;
        float casterX = 0.0f;
        float casterY = 0.0f;
        float casterZ = 0.0f;
        int frameIndex = 0;
        int frameWidth = 500;
        int frameHeight = 198;
    };

    AriBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~AriBossPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void preload(SDL_Renderer* renderer) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    void renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void onSpacePressed() override;
    void setTuningProfile(const PresentationTuningProfile& profile) override;

    bool shouldHideNonCasterCharacters() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderAboveHud() const override;
    NativeRenderState buildNativeRenderState() const;

    float getInputMultiplier() const override;
    PresentationFeedbackSignal getFeedbackSignal() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    float consumeHitDamageMultiplier() override;
    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;
    std::string getInputResultText() const override;

private:
    enum class Phase {
        Attacking,
        Waiting,
        Complete
    };

    static std::string resolvePath(const std::string& relativePath);

    void ensureFramesLoaded(SDL_Renderer* renderer);
    void releaseFrames();
    void beginAttack();
    float randomWaitDuration() const;
    float averageDamageMultiplier() const;
    float damageMultiplierForReaction(bool pressed, float reactionSeconds) const;
    PresentationFeedbackEvent buildAttackFeedbackEvent(bool pressed, float reactionSeconds) const;
    int currentFrameIndex() const;
    void renderAnimatedBoss(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    std::vector<SDL_Texture*> frames_;
    bool attemptedLoad_ = false;
    int frameWidth_ = 500;
    int frameHeight_ = 198;

    Phase phase_ = Phase::Attacking;
    int currentAttackIndex_ = 0;
    float phaseElapsed_ = 0.0f;
    float currentWaitDuration_ = 0.0f;
    bool attackInputCaptured_ = false;
    float attackReactionOffsetSeconds_ = 0.0f;
    bool attackFeedbackQueued_ = false;

    std::vector<float> resolvedHitDamageMultipliers_;
    std::vector<float> pendingHitDamageMultipliers_;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    int attackCount_ = 3;
    float waitDurationMinSeconds_ = 1.0f;
    float waitDurationMaxSeconds_ = 2.5f;
    float reactionCueSeconds_ = 0.46f;
    float perfectWindowSeconds_ = 0.10f;
    float maxReactionOffsetSeconds_ = 0.24f;
};

} // namespace battle
