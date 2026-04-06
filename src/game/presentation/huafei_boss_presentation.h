#pragma once

#include "ability_presentation.h"

#include <deque>
#include <string>
#include <vector>

namespace battle {

class HuafeiBossPresentation : public AbilityPresentation {
public:
    enum class Sequence {
        Opening,
        Failure
    };

    struct FrameSlot {
        SDL_Texture* texture = nullptr;
        bool attemptedLoad = false;
    };

    HuafeiBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~HuafeiBossPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void preload(SDL_Renderer* renderer) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    bool onKeyPressed(SDL_Keycode key) override;
    void setTuningProfile(const PresentationTuningProfile& profile) override;
    void setTargetPartyIndex(int index) override;
    int getFocusedPartyIndex() const override;
    void setPartyAssetNames(const std::vector<std::string>& assetNames) override;

    float getInputMultiplier() const override;
    float consumeHitDamageMultiplier() override;
    PresentationFeedbackSignal getFeedbackSignal() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;
    std::string getInputResultText() const override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;

    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderFocusedTargetEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldRenderFloor() const override;

private:
    enum class Phase {
        Opening,
        Failure,
        SuccessComplete,
        FailureComplete
    };

    void releaseTextures();
    void ensureTargetIconLoaded(SDL_Renderer* renderer);
    void ensureFrameLoaded(SDL_Renderer* renderer, Sequence sequence, int frameIndex);
    void maintainFrameWindow(SDL_Renderer* renderer, Sequence sequence, int frameIndex);
    float escapeRatio() const;
    float failureDamageMultiplier() const;
    SDL_Texture* currentFrameTexture() const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    Phase phase_ = Phase::Opening;
    int focusedPartyIndex_ = -1;
    int quota_ = 40;
    int validPressCount_ = 0;
    float openingElapsed_ = 0.0f;
    float failureElapsed_ = 0.0f;
    bool failureHitReady_ = false;
    bool hitDispatched_ = false;
    std::deque<SDL_Keycode> recentKeys_;

    std::string targetAssetName_;
    SDL_Texture* targetIconTexture_ = nullptr;
    std::vector<FrameSlot> openingFrames_;
    std::vector<FrameSlot> failureFrames_;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
    std::vector<PresentationAudioCommand> pendingAudioCommands_;
};

} // namespace battle
