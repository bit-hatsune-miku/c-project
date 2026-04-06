#pragma once

#include "ability_presentation.h"

namespace battle {

class ZhouShenUltimatePresentation : public AbilityPresentation {
public:
    ZhouShenUltimatePresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~ZhouShenUltimatePresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void preload(SDL_Renderer* renderer) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    float getInputMultiplier() const override;
    PresentationFeedbackSignal getFeedbackSignal() const override;
    std::string getInputResultText() const override;
    void setPresentationValue(int value) override;

    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldRenderAboveHud() const override;

private:
    void ensureFishTextureLoaded(SDL_Renderer* renderer);
    static int clampSingerCount(int singerCount);

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    int singerCount_ = 1;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    bool hitTriggered_ = false;
    SDL_Texture* fishTexture_ = nullptr;
    bool attemptedFishLoad_ = false;
    int fishTextureWidth_ = 0;
    int fishTextureHeight_ = 0;
};

} // namespace battle
