#pragma once

#include "ability_presentation.h"

#include <vector>

namespace battle {

class RandyLogoStrikePresentation : public AbilityPresentation {
public:
    RandyLogoStrikePresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~RandyLogoStrikePresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void preload(SDL_Renderer* renderer) override;

    int consumeHitEvents() override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderAboveHud() const override;

private:
    struct LogoProjectile {
        float startX = 0.0f;
        float startY = 0.0f;
        float startZ = 0.0f;
        float targetX = 0.0f;
        float targetY = 0.0f;
        float targetZ = 0.0f;
        float lifetime = 0.0f;
        float maxLifetime = 0.32f;
        float width = 110.0f;
        float height = 54.0f;
        bool active = true;
    };

    void ensureTextureLoaded(SDL_Renderer* renderer);
    void spawnProjectile();

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    std::vector<LogoProjectile> projectiles_;
    SDL_Texture* logoTexture_ = nullptr;
    bool attemptedTextureLoad_ = false;
    int spawnedProjectiles_ = 0;
    int pendingHitEvents_ = 0;
    bool hitTriggered_ = false;
    float nextSpawnTime_ = 0.0f;
};

} // namespace battle
