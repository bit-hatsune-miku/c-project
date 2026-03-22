#include "wechatalipay_ultimate_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kSearchDurationSeconds = 0.95f;
constexpr float kLockDurationSeconds = 0.40f;
constexpr float kZoomDurationSeconds = 0.42f;
constexpr float kHoldDurationSeconds = 0.12f;
constexpr float kDamageTimeSeconds = kSearchDurationSeconds + kLockDurationSeconds + (kZoomDurationSeconds * 0.92f);
constexpr float kPhoneTargetHeight = 310.0f;
constexpr float kPhoneFallbackWidth = 250.0f;
constexpr float kCameraDistanceSearchStart = 500.0f;
constexpr float kCameraDistanceSearchEnd = 430.0f;
constexpr float kCameraDistanceLockEnd = 330.0f;
constexpr float kCameraDistanceZoomEnd = 95.0f;
constexpr float kCameraHeightSearch = -120.0f;
constexpr float kCameraHeightZoom = -178.0f;
constexpr float kCameraFocalSearchStart = 29000.0f;
constexpr float kCameraFocalSearchEnd = 36000.0f;
constexpr float kCameraFocalLockEnd = 47000.0f;
constexpr float kCameraFocalZoomEnd = 98000.0f;
constexpr float kPi = 3.14159265359f;
constexpr char kPhoneTexturePath[] = "assets/combat/presentations/wechatalipay/huaweisanzhedian.png";

float radiansToDegrees(float radians) {
    return radians * (180.0f / kPi);
}

SDL_FRect centeredRect(float centerX, float centerY, float width, float height) {
    return SDL_FRect{
        centerX - width * 0.5f,
        centerY - height * 0.5f,
        width,
        height
    };
}

} // namespace

WechatalipayUltimatePresentation::WechatalipayUltimatePresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = kSearchDurationSeconds + kLockDurationSeconds + kZoomDurationSeconds + kHoldDurationSeconds;
}

WechatalipayUltimatePresentation::~WechatalipayUltimatePresentation() {
    destroyTexture();
}

void WechatalipayUltimatePresentation::start() {
    elapsedTime_ = 0.0f;
    phaseElapsed_ = 0.0f;
    phase_ = Phase::Search;
    queuedUltimateAudio_ = false;
    hitTriggered_ = false;
    pendingAbilityAudioCues_ = 0;
    pendingHitEvents_ = 0;
}

void WechatalipayUltimatePresentation::update(float deltaTime) {
    if (phase_ == Phase::Complete) {
        return;
    }

    elapsedTime_ += deltaTime;
    phaseElapsed_ += deltaTime;

    if (!hitTriggered_ && elapsedTime_ >= kDamageTimeSeconds) {
        hitTriggered_ = true;
        ++pendingHitEvents_;
    }

    switch (phase_) {
    case Phase::Search:
        if (phaseElapsed_ >= kSearchDurationSeconds) {
            advancePhase(Phase::Lock);
        }
        break;
    case Phase::Lock:
        if (phaseElapsed_ >= kLockDurationSeconds) {
            if (!queuedUltimateAudio_) {
                queuedUltimateAudio_ = true;
                ++pendingAbilityAudioCues_;
            }
            advancePhase(Phase::Zoom);
        }
        break;
    case Phase::Zoom:
        if (phaseElapsed_ >= kZoomDurationSeconds) {
            advancePhase(Phase::Hold);
        }
        break;
    case Phase::Hold:
        if (phaseElapsed_ >= kHoldDurationSeconds) {
            advancePhase(Phase::Complete);
        }
        break;
    case Phase::Complete:
        break;
    }
}

void WechatalipayUltimatePresentation::render(SDL_Renderer* renderer,
                                              int screenW,
                                              int screenH,
                                              const Camera3D& camera) {
    screenW_ = std::max(1, screenW);
    screenH_ = std::max(1, screenH);
    ensureTextureLoaded(renderer);

    const SDL_FPoint bossAnchor = bossScreenAnchor(camera);
    const SDL_FPoint searchEndOffset = searchPhoneOffset(1.0f);

    SDL_FPoint phoneCenter = bossAnchor;
    float phoneScale = 0.50f;
    Uint8 phoneAlpha = 240;
    float reticleRadius = 118.0f;
    Uint8 reticleAlpha = 130;

    if (phase_ == Phase::Search) {
        const float t = easing::clamp01(phaseElapsed_ / kSearchDurationSeconds);
        phoneCenter.x += searchPhoneOffset(t).x;
        phoneCenter.y += searchPhoneOffset(t).y;
        phoneScale = 0.48f + (std::sin(elapsedTime_ * 9.5f) * 0.02f);
        reticleRadius = 132.0f - (10.0f * t);
        reticleAlpha = static_cast<Uint8>(120.0f + (35.0f * t));
    } else if (phase_ == Phase::Lock) {
        const float t = easing::easeOutCubic(easing::clamp01(phaseElapsed_ / kLockDurationSeconds));
        phoneCenter.x += searchEndOffset.x * (1.0f - t);
        phoneCenter.y += searchEndOffset.y * (1.0f - t);
        phoneScale = easing::lerp(0.50f, 0.54f, t);
        reticleRadius = easing::lerp(118.0f, 92.0f, t);
        reticleAlpha = static_cast<Uint8>(easing::lerp(180.0f, 255.0f, t));
    } else if (phase_ == Phase::Zoom || phase_ == Phase::Hold || phase_ == Phase::Complete) {
        const float zoomProgress = (phase_ == Phase::Zoom)
            ? easing::clamp01(phaseElapsed_ / kZoomDurationSeconds)
            : 1.0f;
        const float pulse = std::sin((elapsedTime_ - kSearchDurationSeconds) * 22.0f);
        phoneScale = 0.54f + (0.02f * pulse * (1.0f - zoomProgress * 0.5f));
        reticleRadius = easing::lerp(92.0f, 68.0f, zoomProgress);
        reticleAlpha = 255;
    }

    if (phase_ != Phase::Complete) {
        const SDL_FRect phoneDst = phoneRect(phoneCenter, phoneScale);
        drawPhone(renderer, phoneDst, phoneAlpha);

        if (phase_ != Phase::Search) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 210);
            SDL_RenderDrawLineF(renderer, phoneCenter.x, phoneCenter.y, bossAnchor.x, bossAnchor.y);
        }

        drawLockReticle(renderer, bossAnchor, reticleRadius, reticleAlpha);
    }
}

bool WechatalipayUltimatePresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

bool WechatalipayUltimatePresentation::overridesCamera() const {
    return true;
}

void WechatalipayUltimatePresentation::applyCameraState(Camera3D& camera) const {
    float distance = kCameraDistanceSearchStart;
    float cameraHeight = kCameraHeightSearch;
    float focalLength = kCameraFocalSearchStart;
    float lateralOffset = std::sin(elapsedTime_ * 1.7f) * 16.0f;

    if (phase_ == Phase::Search) {
        const float t = easing::easeOutCubic(easing::clamp01(phaseElapsed_ / kSearchDurationSeconds));
        distance = easing::lerp(kCameraDistanceSearchStart, kCameraDistanceSearchEnd, t);
        focalLength = easing::lerp(kCameraFocalSearchStart, kCameraFocalSearchEnd, t);
    } else if (phase_ == Phase::Lock) {
        const float t = easing::easeOutCubic(easing::clamp01(phaseElapsed_ / kLockDurationSeconds));
        distance = easing::lerp(kCameraDistanceSearchEnd, kCameraDistanceLockEnd, t);
        focalLength = easing::lerp(kCameraFocalSearchEnd, kCameraFocalLockEnd, t);
        lateralOffset *= (1.0f - t);
    } else {
        const float t = (phase_ == Phase::Zoom)
            ? easing::easeInQuint(easing::clamp01(phaseElapsed_ / kZoomDurationSeconds))
            : 1.0f;
        distance = easing::lerp(kCameraDistanceLockEnd, kCameraDistanceZoomEnd, t);
        cameraHeight = easing::lerp(kCameraHeightSearch, kCameraHeightZoom, t);
        focalLength = easing::lerp(kCameraFocalLockEnd, kCameraFocalZoomEnd, t);
        lateralOffset = 0.0f;
    }

    camera.posX = targetX_ + lateralOffset;
    camera.posY = targetY_ - distance;
    camera.posZ = targetZ_ + cameraHeight;

    const float lookX = targetX_;
    const float lookY = targetY_ - 10.0f;
    const float lookZ = targetZ_ - 115.0f;
    const float dx = lookX - camera.posX;
    const float dy = lookY - camera.posY;
    const float dz = lookZ - camera.posZ;
    const float horizontalDist = std::sqrt((dx * dx) + (dy * dy));

    camera.yawDegrees = radiansToDegrees(std::atan2(-dx, dy));
    camera.pitchDegrees = radiansToDegrees(std::atan2(dz, std::max(1.0f, horizontalDist)));
    camera.focalLength = focalLength;
}

bool WechatalipayUltimatePresentation::shouldRenderCasterEntity() const {
    return false;
}

int WechatalipayUltimatePresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int WechatalipayUltimatePresentation::consumeHitEvents() {
    const int hitEvents = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hitEvents;
}

std::string WechatalipayUltimatePresentation::resolvePath(const std::string& relativePath) {
    return platform::path::resolvePath(relativePath);
}

void WechatalipayUltimatePresentation::ensureTextureLoaded(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_ || renderer == nullptr) {
        return;
    }

    attemptedTextureLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolvePath(kPhoneTexturePath).c_str());
    if (surface == nullptr) {
        return;
    }

    phoneTexture_.texture = SDL_CreateTextureFromSurface(renderer, surface);
    phoneTexture_.width = surface->w;
    phoneTexture_.height = surface->h;
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

void WechatalipayUltimatePresentation::destroyTexture() {
    if (phoneTexture_.texture != nullptr) {
        SDL_DestroyTexture(phoneTexture_.texture);
        phoneTexture_.texture = nullptr;
    }
    phoneTexture_.width = 0;
    phoneTexture_.height = 0;
}

void WechatalipayUltimatePresentation::advancePhase(Phase nextPhase) {
    phase_ = nextPhase;
    phaseElapsed_ = 0.0f;
}

SDL_FPoint WechatalipayUltimatePresentation::bossScreenAnchor(const Camera3D& camera) const {
    const SDL_FPoint projected = camera.worldToScreen(targetX_, targetY_, targetZ_ - 110.0f);
    if (projected.x <= -500000.0f || projected.y <= -500000.0f) {
        return SDL_FPoint{
            static_cast<float>(screenW_) * 0.5f,
            static_cast<float>(screenH_) * 0.46f
        };
    }
    return projected;
}

SDL_FPoint WechatalipayUltimatePresentation::searchPhoneOffset(float progress) const {
    const float angle = progress * 2.0f * kPi;
    return SDL_FPoint{
        (std::sin(angle * 1.25f + 0.35f) * 230.0f) + (std::cos(angle * 3.0f) * 42.0f),
        (std::cos(angle * 0.92f + 1.1f) * 118.0f) + (std::sin(angle * 2.35f) * 34.0f)
    };
}

SDL_FRect WechatalipayUltimatePresentation::phoneRect(const SDL_FPoint& center, float scale) const {
    const float textureWidth = phoneTexture_.width > 0 ? static_cast<float>(phoneTexture_.width) : kPhoneFallbackWidth;
    const float textureHeight = phoneTexture_.height > 0 ? static_cast<float>(phoneTexture_.height) : kPhoneTargetHeight;
    const float baseScale = kPhoneTargetHeight / std::max(1.0f, textureHeight);
    const float width = textureWidth * baseScale * scale;
    const float height = kPhoneTargetHeight * scale;
    return centeredRect(center.x, center.y, width, height);
}

void WechatalipayUltimatePresentation::drawPhone(SDL_Renderer* renderer,
                                                 const SDL_FRect& rect,
                                                 Uint8 alpha) const {
    if (renderer == nullptr) {
        return;
    }

    if (phoneTexture_.texture != nullptr) {
        SDL_SetTextureBlendMode(phoneTexture_.texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(phoneTexture_.texture, alpha);
        SDL_RenderCopyExF(renderer, phoneTexture_.texture, nullptr, &rect, 0.0, nullptr, SDL_FLIP_NONE);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 245, 245, 245, alpha);
    SDL_RenderFillRectF(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 12, 12, 12, alpha);
    SDL_RenderDrawRectF(renderer, &rect);
}

void WechatalipayUltimatePresentation::drawLockReticle(SDL_Renderer* renderer,
                                                       const SDL_FPoint& center,
                                                       float radius,
                                                       Uint8 alpha) const {
    if (renderer == nullptr) {
        return;
    }

    const float innerRadius = radius * 0.62f;
    const float bracket = radius * 0.24f;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);

    SDL_RenderDrawLineF(renderer, center.x - radius, center.y - innerRadius, center.x - radius, center.y - radius);
    SDL_RenderDrawLineF(renderer, center.x - radius, center.y - radius, center.x - innerRadius, center.y - radius);

    SDL_RenderDrawLineF(renderer, center.x + innerRadius, center.y - radius, center.x + radius, center.y - radius);
    SDL_RenderDrawLineF(renderer, center.x + radius, center.y - radius, center.x + radius, center.y - innerRadius);

    SDL_RenderDrawLineF(renderer, center.x - radius, center.y + innerRadius, center.x - radius, center.y + radius);
    SDL_RenderDrawLineF(renderer, center.x - radius, center.y + radius, center.x - innerRadius, center.y + radius);

    SDL_RenderDrawLineF(renderer, center.x + innerRadius, center.y + radius, center.x + radius, center.y + radius);
    SDL_RenderDrawLineF(renderer, center.x + radius, center.y + innerRadius, center.x + radius, center.y + radius);

    SDL_RenderDrawLineF(renderer, center.x - bracket, center.y, center.x + bracket, center.y);
    SDL_RenderDrawLineF(renderer, center.x, center.y - bracket, center.x, center.y + bracket);
}

} // namespace battle
