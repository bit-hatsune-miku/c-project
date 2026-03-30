#ifndef CUPCAKKE_ULTIMATE_PRESENTATION_H
#define CUPCAKKE_ULTIMATE_PRESENTATION_H

#include "ability_presentation.h"

#include <random>
#include <vector>

namespace battle {

class CupcakkeUltimatePresentation final : public AbilityPresentation {
public:
    CupcakkeUltimatePresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~CupcakkeUltimatePresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void preload(SDL_Renderer* renderer) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    bool overridesCamera() const override { return true; }
    void applyCameraState(Camera3D& camera) const override;
    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;

    bool shouldHideNonCasterCharacters() const override { return true; }
    bool shouldRenderCasterEntity() const override { return false; }
    bool shouldRenderBossEntity() const override { return true; }
    bool shouldRenderAboveHud() const override { return false; }
    bool shouldRenderFloor() const override { return false; }

private:
    struct FallingRectangle {
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float dropSpeed = 0.0f;
        float swaySpeed = 0.0f;
        float swayPhase = 0.0f;
        float swayAmplitude = 0.0f;
        float twitchSpeed = 0.0f;
        float tiltDirection = 1.0f;
        float scale = 1.0f;
        float elapsed = 0.0f;
        float angle = 0.0f;
        bool active = true;
        bool hitRegistered = false;
    };

    void spawnRectangles(int count);
    void updateRectangles(float deltaTime);
    void loadRectTexture(SDL_Renderer* renderer);
    void releaseTexture();

    float casterX_;
    float casterY_;
    float casterZ_;
    float targetX_;
    float targetY_;
    float targetZ_;

    bool stageTwoActive_ = false;
    float spawnAccumulator_ = 0.0f;
    float ability2Accumulator_ = 0.0f;
    std::vector<FallingRectangle> rectangles_;
    SDL_Texture* rectTexture_ = nullptr;
    int rectTextureWidth_ = 1;
    int rectTextureHeight_ = 1;
    bool attemptedTextureLoad_ = false;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    int damageLabelHitCount_ = 0;
    int totalRectanglesSpawned_ = 0;
    std::mt19937 rng_;
};

} // namespace battle

#endif // CUPCAKKE_ULTIMATE_PRESENTATION_H
