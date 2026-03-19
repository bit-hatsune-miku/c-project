#include "cupcakke_ultimate_presentation.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kCameraEaseDuration = 0.4f;
constexpr float kCameraHoldDuration = 0.5f;
constexpr float kRectFallDuration = 1.5f;
constexpr float kRectStageStart = kCameraEaseDuration + kCameraHoldDuration;
constexpr float kRectStageEnd = kRectStageStart + kRectFallDuration;

constexpr float kStage2Transition = 0.35f;
constexpr float kSpawnInterval = 0.35f;
constexpr float kAbility2Interval = 0.35f;
constexpr float kFocalInitial = 50000.0f;
constexpr float kFocalStage1 = 47000.0f;
constexpr float kFocalStage2Start = 49500.0f;
constexpr float kFocalStage2End = 42000.0f;
constexpr float kCameraPosX = 60.0f;
constexpr float kCameraPosY = 375.0f;
constexpr float kCameraPosZ = -175.0f;
constexpr float kCameraStartPitch = -9.0f;
constexpr float kCameraStage1Pitch = -37.0f;
constexpr float kCameraStage2Pitch = -1.0f;
constexpr float kCameraYaw = 10.6f;

float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

float easeOutCubic(float t) {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

std::string resolveAssetPath(const std::string& relativePath) {
    const std::filesystem::path candidate(relativePath);
    if (std::filesystem::exists(candidate)) {
        return candidate.string();
    }
    const std::filesystem::path alternative = std::filesystem::path("assets") /
        "combat" / "presentations" / "cupcakke" / candidate.filename();
    if (std::filesystem::exists(alternative)) {
        return alternative.string();
    }
    return relativePath;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace

CupcakkeUltimatePresentation::CupcakkeUltimatePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) :
    casterX_(casterWorldX),
    casterY_(casterWorldY),
    casterZ_(casterWorldZ),
    targetX_(targetWorldX),
    targetY_(targetWorldY),
    targetZ_(targetWorldZ),
    rng_(std::random_device{}()) {
    totalDuration_ = kRectStageEnd;
}

CupcakkeUltimatePresentation::~CupcakkeUltimatePresentation() {
    releaseTexture();
}

void CupcakkeUltimatePresentation::start() {
    elapsedTime_ = 0.0f;
    stageTwoActive_ = false;
    spawnAccumulator_ = 0.0f;
    ability2Accumulator_ = 0.0f;
    rectangles_.clear();
    pendingAbilityAudioCues_ = 1;
}

void CupcakkeUltimatePresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    const bool stageTwoNow = elapsedTime_ >= kRectStageStart;
    if (stageTwoNow && !stageTwoActive_) {
        stageTwoActive_ = true;
        spawnAccumulator_ = 0.0f;
        ability2Accumulator_ = 0.0f;
        spawnRectangles(4);
    }

    if (stageTwoActive_) {
        spawnAccumulator_ += deltaTime;
        while (spawnAccumulator_ >= kSpawnInterval) {
            spawnAccumulator_ -= kSpawnInterval;
            spawnRectangles(1);
        }

        ability2Accumulator_ += deltaTime;
        while (ability2Accumulator_ >= kAbility2Interval) {
            ability2Accumulator_ -= kAbility2Interval;
            ++pendingAbilityAudioCues_;
        }

        updateRectangles(deltaTime);
    }
}

void CupcakkeUltimatePresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)screenW;
    (void)screenH;
    loadRectTexture(renderer);
    if (rectTexture_ == nullptr) {
        return;
    }

    const SDL_FPoint screenPos = camera.worldToScreen(targetX_, targetY_, targetZ_);
    if (screenPos.x < -500000.0f || screenPos.y < -500000.0f) {
        return;
    }

    for (const FallingRectangle& rect : rectangles_) {
        if (!rect.active) {
            continue;
        }

        const float drawX = screenPos.x + rect.offsetX;
        const float drawY = screenPos.y + rect.offsetY;
        const int destW = std::max(1, static_cast<int>(rectTextureWidth_ * rect.scale));
        const int destH = std::max(1, static_cast<int>(rectTextureHeight_ * rect.scale));
        SDL_Rect dest{
            static_cast<int>(drawX) - destW / 2,
            static_cast<int>(drawY) - destH / 2,
            destW,
            destH
        };
        SDL_Point center{destW / 2, destH / 2};
        SDL_RenderCopyEx(renderer, rectTexture_, nullptr, &dest, rect.angle, &center, SDL_FLIP_NONE);
    }
}

bool CupcakkeUltimatePresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

void CupcakkeUltimatePresentation::applyCameraState(Camera3D& camera) const {
    camera.posX = kCameraPosX;
    camera.posY = kCameraPosY;
    camera.posZ = kCameraPosZ;
    camera.yawDegrees = kCameraYaw;

    if (elapsedTime_ <= kCameraEaseDuration) {
        const float t = easeOutCubic(clamp01(elapsedTime_ / kCameraEaseDuration));
        camera.pitchDegrees = lerp(kCameraStartPitch, kCameraStage1Pitch, t);
        camera.focalLength = lerp(kFocalInitial, kFocalStage1, t);
        return;
    }

    if (elapsedTime_ <= kRectStageStart) {
        camera.pitchDegrees = kCameraStage1Pitch;
        camera.focalLength = kFocalStage1;
        return;
    }

    const float transitionElapsed = elapsedTime_ - kRectStageStart;
    if (transitionElapsed <= kStage2Transition) {
        const float t = easeOutCubic(clamp01(transitionElapsed / kStage2Transition));
        camera.pitchDegrees = lerp(kCameraStage1Pitch, kCameraStage2Pitch, t);
        camera.focalLength = lerp(kFocalStage2Start, kFocalStage2End, t);
        return;
    }

    const float rectElapsed = std::min(transitionElapsed - kStage2Transition, kRectFallDuration);
    const float fallT = easeOutCubic(clamp01(rectElapsed / kRectFallDuration));
    camera.pitchDegrees = kCameraStage2Pitch;
    camera.focalLength = lerp(kFocalStage2Start, kFocalStage2End, fallT);
}

int CupcakkeUltimatePresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int CupcakkeUltimatePresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    damageLabelHitCount_ = 0;
    return hits;
}

int CupcakkeUltimatePresentation::getDamageLabelHitCount() const {
    return std::max(1, damageLabelHitCount_);
}

void CupcakkeUltimatePresentation::spawnRectangles(int count) {
    std::uniform_real_distribution<float> offsetXDist(-120.0f, 120.0f);
    std::uniform_real_distribution<float> offsetYDist(-620.0f, -520.0f);
    std::uniform_real_distribution<float> dropSpeedDist(720.0f, 920.0f);
    std::uniform_real_distribution<float> swaySpeedDist(3.6f, 6.4f);
    std::uniform_real_distribution<float> swayAmpDist(32.0f, 70.0f);
    std::uniform_real_distribution<float> twitchSpeedDist(30.0f, 60.0f);
    std::uniform_real_distribution<float> scaleDist(0.68f, 1.08f);
    std::uniform_real_distribution<float> phaseDist(0.0f, 6.283185f);
    std::bernoulli_distribution tiltDist(0.5);

    for (int i = 0; i < count && rectangles_.size() < 20; ++i) {
        FallingRectangle rect;
        rect.offsetX = offsetXDist(rng_);
        rect.offsetY = offsetYDist(rng_);
        rect.dropSpeed = dropSpeedDist(rng_);
        rect.swaySpeed = swaySpeedDist(rng_);
        rect.swayAmplitude = swayAmpDist(rng_);
        rect.twitchSpeed = twitchSpeedDist(rng_);
        rect.scale = scaleDist(rng_);
        rect.swayPhase = phaseDist(rng_);
        rect.tiltDirection = tiltDist(rng_) ? 1.0f : -1.0f;
        rect.elapsed = 0.0f;
        rect.active = true;
        rect.hitRegistered = false;
        rectangles_.push_back(rect);
    }
}

void CupcakkeUltimatePresentation::updateRectangles(float deltaTime) {
    for (FallingRectangle& rect : rectangles_) {
        if (!rect.active) {
            continue;
        }

        rect.elapsed += deltaTime;
        rect.offsetY += rect.dropSpeed * deltaTime;
        const float sway = std::sin(rect.elapsed * rect.swaySpeed + rect.swayPhase);
        const float twitch = std::sin(rect.elapsed * rect.twitchSpeed);
        rect.angle = (sway * rect.swayAmplitude + twitch * 12.0f) * rect.tiltDirection;
        rect.offsetX += sway * rect.scale * 12.0f * deltaTime;
        if (!rect.hitRegistered && rect.offsetY >= 420.0f) {
            rect.hitRegistered = true;
            ++pendingHitEvents_;
            ++damageLabelHitCount_;
            rect.active = false;
            continue;
        }
        if (rect.offsetY > 500.0f || rect.elapsed >= kRectFallDuration + 0.4f) {
            rect.active = false;
        }
    }

    rectangles_.erase(
        std::remove_if(rectangles_.begin(), rectangles_.end(), [](const FallingRectangle& rect) {
            return !rect.active;
        }),
        rectangles_.end()
    );
}

void CupcakkeUltimatePresentation::loadRectTexture(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_) {
        return;
    }
    attemptedTextureLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    const std::string path = resolveAssetPath("assets/combat/presentations/cupcakke/niagarafalls.png");
    if (!std::filesystem::exists(path)) {
        return;
    }

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (surface == nullptr) {
        return;
    }

    rectTextureWidth_ = surface->w > 0 ? surface->w : 1;
    rectTextureHeight_ = surface->h > 0 ? surface->h : 1;
    rectTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (rectTexture_ != nullptr) {
        SDL_SetTextureBlendMode(rectTexture_, SDL_BLENDMODE_BLEND);
    }
#else
    (void)renderer;
#endif
}

void CupcakkeUltimatePresentation::releaseTexture() {
    if (rectTexture_ != nullptr) {
        SDL_DestroyTexture(rectTexture_);
        rectTexture_ = nullptr;
    }
}

} // namespace battle
