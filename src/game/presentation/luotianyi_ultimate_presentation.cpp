#include "luotianyi_ultimate_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kProjectileSpawnInterval = 0.055f;
constexpr float kProjectileTravelSeconds = 0.24f;
constexpr float kCasterChestOffsetZ = -150.0f;
constexpr float kTargetChestOffsetZ = -130.0f;
constexpr float kEmptyImpactDelaySeconds = 0.18f;
constexpr float kPresentationTailSeconds = 0.10f;

std::string resolveSquareTexturePath() {
    return platform::path::resolvePath("assets/combat/presentations/jiafei/head0.png");
}

} // namespace

LuotianyiUltimatePresentation::LuotianyiUltimatePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = kEmptyImpactDelaySeconds + kPresentationTailSeconds;
}

LuotianyiUltimatePresentation::~LuotianyiUltimatePresentation() {
    if (squareTexture_ != nullptr) {
        SDL_DestroyTexture(squareTexture_);
    }
}

void LuotianyiUltimatePresentation::setPresentationValue(int value) {
    projectileCount_ = std::max(0, value);
}

void LuotianyiUltimatePresentation::start() {
    elapsedTime_ = 0.0f;
    projectiles_.clear();
    spawnedProjectiles_ = 0;
    pendingAbilityAudioCues_ = 1;
    pendingHitEvents_ = 0;
    hitTriggered_ = false;
    nextSpawnTime_ = 0.0f;
    totalDuration_ = hitTimeSeconds() + kPresentationTailSeconds;
}

void LuotianyiUltimatePresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    while (spawnedProjectiles_ < projectileCount_ && elapsedTime_ >= nextSpawnTime_) {
        spawnProjectile();
        ++spawnedProjectiles_;
        nextSpawnTime_ += kProjectileSpawnInterval;
    }

    for (SquareProjectile& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }

        projectile.lifetime += deltaTime;
        if (projectile.lifetime >= projectile.maxLifetime) {
            projectile.active = false;
        }
    }

    if (!hitTriggered_ && elapsedTime_ >= hitTimeSeconds()) {
        hitTriggered_ = true;
        ++pendingHitEvents_;
    }
}

void LuotianyiUltimatePresentation::render(SDL_Renderer* renderer,
                                           int screenW,
                                           int screenH,
                                           const Camera3D& camera) {
    (void)screenW;
    (void)screenH;
    ensureTextureLoaded(renderer);

    for (const SquareProjectile& projectile : projectiles_) {
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
        const float screenSize = std::max(8.0f, projectile.size * camera.getPerspectiveScale(worldX, worldY, worldZ));
        SDL_Rect dest{
            static_cast<int>(std::lround(screen.x - (screenSize * 0.5f))),
            static_cast<int>(std::lround(screen.y - (screenSize * 0.5f))),
            static_cast<int>(std::lround(screenSize)),
            static_cast<int>(std::lround(screenSize))
        };

        if (squareTexture_ != nullptr) {
            SDL_RenderCopy(renderer, squareTexture_, nullptr, &dest);
            continue;
        }

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        SDL_RenderFillRect(renderer, &dest);
        SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
        SDL_RenderDrawRect(renderer, &dest);
    }
}

bool LuotianyiUltimatePresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

int LuotianyiUltimatePresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int LuotianyiUltimatePresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

bool LuotianyiUltimatePresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool LuotianyiUltimatePresentation::shouldRenderAboveHud() const {
    return false;
}

void LuotianyiUltimatePresentation::ensureTextureLoaded(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_ || renderer == nullptr) {
        return;
    }

    attemptedTextureLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolveSquareTexturePath().c_str());
    if (surface == nullptr) {
        return;
    }

    squareTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

void LuotianyiUltimatePresentation::spawnProjectile() {
    const float laneOffset = (static_cast<float>(spawnedProjectiles_) - (static_cast<float>(projectileCount_ - 1) * 0.5f)) * 10.0f;
    const float verticalOffset = ((spawnedProjectiles_ % 2) == 0) ? -6.0f : 6.0f;

    SquareProjectile projectile;
    projectile.startX = casterX_ + laneOffset;
    projectile.startY = casterY_;
    projectile.startZ = casterZ_ + kCasterChestOffsetZ + verticalOffset;
    projectile.targetX = targetX_ + (laneOffset * 0.20f);
    projectile.targetY = targetY_ - 18.0f;
    projectile.targetZ = targetZ_ + kTargetChestOffsetZ + (verticalOffset * 0.25f);
    projectile.maxLifetime = kProjectileTravelSeconds;
    projectile.size = 25.0f;
    projectiles_.push_back(projectile);
}

float LuotianyiUltimatePresentation::hitTimeSeconds() const {
    if (projectileCount_ <= 0) {
        return kEmptyImpactDelaySeconds;
    }
    return (static_cast<float>(projectileCount_ - 1) * kProjectileSpawnInterval) + kProjectileTravelSeconds;
}

} // namespace battle
