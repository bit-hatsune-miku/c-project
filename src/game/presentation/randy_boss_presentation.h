#pragma once

#include "ability_presentation.h"

#include <deque>
#include <string>
#include <vector>

#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

namespace battle {

class RandyBossPresentation : public AbilityPresentation {
public:
    RandyBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~RandyBossPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    bool onKeyPressed(SDL_Keycode key) override;
    void preload(SDL_Renderer* renderer) override;
    void setTuningProfile(const PresentationTuningProfile& profile) override;

    bool shouldHideNonCasterCharacters() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldRenderAboveHud() const override;

    float consumeHitDamageMultiplier() override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    int consumeHitEvents() override;
    std::string getInputResultText() const override;

private:
    enum class Phase {
        Input,
        Result,
        Complete
    };

    void ensureAssetsLoaded(SDL_Renderer* renderer);
    void releaseAssets();
    void finalizeInput();
    float completionRatio() const;
    float failureDamageMultiplier() const;
    std::string wrappedTypedText() const;
    void drawPaperText(SDL_Renderer* renderer, const SDL_FRect& paperRect) const;
    bool appendPrintableCharacter(SDL_Keycode key);

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    Phase phase_ = Phase::Input;
    float inputElapsed_ = 0.0f;
    float resultElapsed_ = 0.0f;
    int quota_ = 36;
    float inputDurationSeconds_ = 7.0f;
    bool success_ = false;
    bool hitReady_ = false;
    bool hitDispatched_ = false;
    int validPressCount_ = 0;
    std::string typedText_;
    std::deque<SDL_Keycode> recentKeys_;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;

    SDL_Texture* logoTexture_ = nullptr;

#ifdef BATTLE_ENABLE_TTF
    TTF_Font* paperFont_ = nullptr;
#endif
};

} // namespace battle
