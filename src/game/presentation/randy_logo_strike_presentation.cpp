#include "randy_logo_strike_presentation.h"

#include "../core/easing.h"
#include "../render/battle_asset_loading.h"

#include <algorithm>
#include <cmath>

namespace battle {
namespace {

constexpr int kProjectileCount = 5;
constexpr float kProjectileSpawnInterval = 0.055f;
constexpr float kProjectileTravelSeconds = 0.24f;
constexpr float kCasterChestOffsetZ = -150.0f;
constexpr float kTargetChestOffsetZ = -130.0f;

} // namespace

RandyLogoStrikePresentation::RandyLogoStrikePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = 0.62f;
}

RandyLogoStrikePresentation::~RandyLogoStrikePresentation() {
    if (logoTexture_ != nullptr) {
        SDL_DestroyTexture(logoTexture_);
    }
}

void RandyLogoStrikePresentation::start() {
    elapsedTime_ = 0.0f;
    projectiles_.clear();
    spawnedProjectiles_ = 0;
    pendingHitEvents_ = 0;
    hitTriggered_ = false;
    nextSpawnTime_ = 0.0f;
}

void RandyLogoStrikePresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    while (spawnedProjectiles_ < kProjectileCount && elapsedTime_ >= nextSpawnTime_) {
        spawnProjectile();
        ++spawnedProjectiles_;
        nextSpawnTime_ += kProjectileSpawnInterval;
    }

    for (LogoProjectile& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }

        projectile.lifetime += deltaTime;
        if (projectile.lifetime >= projectile.maxLifetime) {
            projectile.active = false;
        }
    }

    const float hitTime =
        (static_cast<float>(kProjectileCount - 1) * kProjectileSpawnInterval) + kProjectileTravelSeconds;
    if (!hitTriggered_ && elapsedTime_ >= hitTime) {
        hitTriggered_ = true;
        ++pendingHitEvents_;
    }
}

void RandyLogoStrikePresentation::render(SDL_Renderer* renderer,
                                         int screenW,
                                         int screenH,
                                         const Camera3D& camera) {
    (void)screenW;
    (void)screenH;
    ensureTextureLoaded(renderer);

    for (const LogoProjectile& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }

        const float t = easing::clamp01(projectile.lifetime / std::max(0.001f, projectile.maxLifetime));
        const float worldX = easing::lerp(projectile.startX, projectile.targetX, t);
        const float worldY = easing::lerp(projectile.startY, projectile.targetY, t);
        const float worldZ = easing::lerp(projectile.startZ, projectile.targetZ, t);

        const float depth = camera.getDepth(worldX, worldY, worldZ);
        if (depth <= 1.0f) {
            continue;
        }

        const SDL_FPoint screen = camera.worldToScreen(worldX, worldY, worldZ);
        const float perspective = camera.getPerspectiveScale(worldX, worldY, worldZ);
        const SDL_FRect dest{
            screen.x - (projectile.width * perspective * 0.5f),
            screen.y - (projectile.height * perspective * 0.5f),
            projectile.width * perspective,
            projectile.height * perspective
        };

        if (logoTexture_ != nullptr) {
            SDL_RenderCopyF(renderer, logoTexture_, nullptr, &dest);
            continue;
        }

        SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
        SDL_RenderFillRectF(renderer, &dest);
        SDL_SetRenderDrawColor(renderer, 32, 44, 176, 255);
        SDL_RenderDrawRectF(renderer, &dest);
    }
}

bool RandyLogoStrikePresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

void RandyLogoStrikePresentation::preload(SDL_Renderer* renderer) {
    ensureTextureLoaded(renderer);
}

int RandyLogoStrikePresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

bool RandyLogoStrikePresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool RandyLogoStrikePresentation::shouldRenderAboveHud() const {
    return false;
}

void RandyLogoStrikePresentation::ensureTextureLoaded(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_ || renderer == nullptr) {
        return;
    }

    attemptedTextureLoad_ = true;
    const auto loaded = render::tryLoadTextureFromPath(renderer, "assets/combat/presentations/randy/lexue.png");
    if (loaded.has_value()) {
        logoTexture_ = *loaded;
    }
}

void RandyLogoStrikePresentation::spawnProjectile() {
    const float laneOffset = (static_cast<float>(spawnedProjectiles_) - 2.0f) * 12.0f;
    const float verticalOffset = ((spawnedProjectiles_ % 2) == 0) ? -6.0f : 6.0f;

    LogoProjectile projectile;
    projectile.startX = casterX_ + laneOffset;
    projectile.startY = casterY_;
    projectile.startZ = casterZ_ + kCasterChestOffsetZ + verticalOffset;
    projectile.targetX = targetX_ + (laneOffset * 0.15f);
    projectile.targetY = targetY_ - 18.0f;
    projectile.targetZ = targetZ_ + kTargetChestOffsetZ + (verticalOffset * 0.25f);
    projectile.maxLifetime = kProjectileTravelSeconds;
    projectiles_.push_back(projectile);
}

} // namespace battle
