#pragma once

#include "ability_presentation.h"

#include <vector>

namespace battle {

class JiafeiScreamPresentation : public AbilityPresentation {
public:
    JiafeiScreamPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~JiafeiScreamPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderAboveHud() const override;

private:
    struct HeadProjectile {
        float startX = 0.0f;
        float startY = 0.0f;
        float startZ = 0.0f;
        float targetX = 0.0f;
        float targetY = 0.0f;
        float targetZ = 0.0f;
        float lifetime = 0.0f;
        float maxLifetime = 0.28f;
        float size = 25.0f;
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

    std::vector<HeadProjectile> projectiles_;
    SDL_Texture* headTexture_ = nullptr;
    bool attemptedTextureLoad_ = false;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    int spawnedProjectiles_ = 0;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    bool hitTriggered_ = false;
    float nextSpawnTime_ = 0.0f;
};

} // namespace battle
