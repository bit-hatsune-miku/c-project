#include "jiafei_scream_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr int kProjectileCount = 5;
constexpr float kProjectileSpawnInterval = 0.055f;
constexpr float kProjectileTravelSeconds = 0.24f;
constexpr float kCasterChestOffsetZ = -150.0f;
constexpr float kTargetChestOffsetZ = -130.0f;

std::string resolveHeadTexturePath() {
    return platform::path::resolvePath("assets/combat/presentations/jiafei/head0.png");
}

} // namespace

JiafeiScreamPresentation::JiafeiScreamPresentation(
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

JiafeiScreamPresentation::~JiafeiScreamPresentation() {
    if (headTexture_ != nullptr) {
        SDL_DestroyTexture(headTexture_);
    }
}

void JiafeiScreamPresentation::start() {
    elapsedTime_ = 0.0f;
    projectiles_.clear();
    spawnedProjectiles_ = 0;
    pendingAbilityAudioCues_ = 1;
    pendingHitEvents_ = 0;
    hitTriggered_ = false;
    nextSpawnTime_ = 0.0f;
}

void JiafeiScreamPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    while (spawnedProjectiles_ < kProjectileCount && elapsedTime_ >= nextSpawnTime_) {
        spawnProjectile();
        ++spawnedProjectiles_;
        nextSpawnTime_ += kProjectileSpawnInterval;
    }

    for (HeadProjectile& projectile : projectiles_) {
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

void JiafeiScreamPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)screenW;
    (void)screenH;
    ensureTextureLoaded(renderer);

    for (const HeadProjectile& projectile : projectiles_) {
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

        if (headTexture_ != nullptr) {
            SDL_RenderCopy(renderer, headTexture_, nullptr, &dest);
            continue;
        }

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        SDL_RenderFillRect(renderer, &dest);
        SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
        SDL_RenderDrawRect(renderer, &dest);
    }
}

bool JiafeiScreamPresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

int JiafeiScreamPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int JiafeiScreamPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

bool JiafeiScreamPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool JiafeiScreamPresentation::shouldRenderAboveHud() const {
    return false;
}

void JiafeiScreamPresentation::ensureTextureLoaded(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_ || renderer == nullptr) {
        return;
    }

    attemptedTextureLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolveHeadTexturePath().c_str());
    if (surface == nullptr) {
        return;
    }

    headTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
    textureWidth_ = surface->w;
    textureHeight_ = surface->h;
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

void JiafeiScreamPresentation::spawnProjectile() {
    const float laneOffset = (static_cast<float>(spawnedProjectiles_) - 2.0f) * 10.0f;
    const float verticalOffset = ((spawnedProjectiles_ % 2) == 0) ? -6.0f : 6.0f;

    HeadProjectile projectile;
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

} // namespace battle
