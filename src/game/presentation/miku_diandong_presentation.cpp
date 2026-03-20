#include "miku_diandong_presentation.h"

#include "../core/easing.h"
#include "../../platform/path_resolution.h"
#include <string>

#include <algorithm>
#include <cmath>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kChaseDuration = 1.8f;
constexpr float kImpactHoldDuration = 0.05f;
constexpr float kEaseOutDuration = 0.55f;
constexpr float kCameraCatchUp = 0.72f;
constexpr float kStartFocalLength = 44000.0f;
constexpr float kImpactFocalLength = 22000.0f;
constexpr float kEaseOutFocalLength = 52000.0f;
constexpr float kCameraStartOffsetX = -220.0f;
constexpr float kCameraStartOffsetY = 120.0f;
constexpr float kCameraStartOffsetZ = -170.0f;
constexpr float kCameraEndOffsetX = -48.0f;
constexpr float kCameraEndOffsetY = 20.0f;
constexpr float kCameraEndOffsetZ = -140.0f;
constexpr float kCameraStartPitch = -7.0f;
constexpr float kCameraEndPitch = -18.5f;
constexpr float kCameraStartYaw = 22.0f;
constexpr float kCameraEndYaw = -8.0f;
constexpr float kMinimumDepth = 1.0f;
constexpr float kDiandongBaseHeight = 230.0f;
constexpr float kBossSilhouetteHeight = 320.0f;
constexpr float kBossSilhouetteWidthScale = 0.65f;
constexpr float kPi = 3.14159265f;

std::string resolveDiandongAssetPath() {
    return platform::path::resolvePath("assets/combat/presentations/miku/diandong.png");
}

} // namespace

MikuDiandongPresentation::~MikuDiandongPresentation() {
    if (diandongTexture_ != nullptr) {
        SDL_DestroyTexture(diandongTexture_);
        diandongTexture_ = nullptr;
    }
}

MikuDiandongPresentation::MikuDiandongPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ)
    , mikuStartX_(0.0f)
    , mikuStartY_(0.0f)
    , mikuStartZ_(0.0f)
    , mikuTargetX_(0.0f)
    , mikuTargetY_(0.0f)
    , mikuTargetZ_(0.0f)
    , mikuWorldX_(0.0f)
    , mikuWorldY_(0.0f)
    , mikuWorldZ_(0.0f)
    , impactMikuWorldX_(0.0f)
    , impactMikuWorldY_(0.0f)
    , impactMikuWorldZ_(0.0f)
    , phaseElapsed_(0.0f)
    , chaseTime_(0.0f)
    , cameraProgress_(0.0f)
    , impactCameraProgress_(0.0f)
    , easeOutProgress_(0.0f)
    , impactHitTriggered_(false)
    , phase_(Phase::Complete) {
    const float dx = targetWorldX - casterWorldX;
    const float dy = targetWorldY - casterWorldY;
    const float distance = std::max(0.001f, std::sqrt((dx * dx) + (dy * dy)));
    const float dirX = dx / distance;
    const float dirY = dy / distance;
    constexpr float kStartDistance = 1180.0f;
    constexpr float kTargetOffset = 48.0f;
    mikuStartX_ = casterWorldX - (dirX * kStartDistance);
    mikuStartY_ = casterWorldY - (dirY * kStartDistance);
    mikuStartZ_ = casterWorldZ;
    mikuTargetX_ = targetWorldX - (dirX * kTargetOffset);
    mikuTargetY_ = targetWorldY - (dirY * kTargetOffset);
    mikuTargetZ_ = targetWorldZ;
    mikuWorldX_ = mikuStartX_;
    mikuWorldY_ = mikuStartY_;
    mikuWorldZ_ = mikuStartZ_;
    impactMikuWorldX_ = mikuTargetX_;
    impactMikuWorldY_ = mikuTargetY_;
    impactMikuWorldZ_ = mikuTargetZ_;
}

void MikuDiandongPresentation::start() {
    phase_ = Phase::Chase;
    phaseElapsed_ = 0.0f;
    chaseTime_ = 0.0f;
    cameraProgress_ = 0.0f;
    impactCameraProgress_ = 0.0f;
    impactHitTriggered_ = false;
    pendingHitEvents_ = 0;
    pendingAbilityAudioCues_ = 1;
    mikuWorldX_ = mikuStartX_;
    mikuWorldY_ = mikuStartY_;
    mikuWorldZ_ = mikuStartZ_;
}

void MikuDiandongPresentation::update(float deltaSeconds) {
    if (phase_ == Phase::Complete) {
        return;
    }

    phaseElapsed_ += deltaSeconds;

    if (phase_ == Phase::Chase) {
        chaseTime_ += deltaSeconds;
        const float rawProgress = easing::clamp01(chaseTime_ / kChaseDuration);
        const float linearProgress = rawProgress;
        const float speedProgress = (rawProgress > 0.9f) ? 1.0f : linearProgress;
        updateMikuPosition(speedProgress);
        cameraProgress_ = std::min(1.0f, rawProgress * kCameraCatchUp);

        if (rawProgress >= 1.0f) {
            phase_ = Phase::ImpactFreeze;
            phaseElapsed_ = 0.0f;
            impactCameraProgress_ = 1.0f;
            impactMikuWorldX_ = mikuWorldX_;
            impactMikuWorldY_ = mikuWorldY_;
            impactMikuWorldZ_ = mikuWorldZ_;
            if (!impactHitTriggered_) {
                ++pendingHitEvents_;
                impactHitTriggered_ = true;
            }
        }
        return;
    }

    if (phase_ == Phase::ImpactFreeze) {
        if (phaseElapsed_ >= kImpactHoldDuration) {
            phase_ = Phase::EaseOut;
            phaseElapsed_ = 0.0f;
            easeOutProgress_ = 0.0f;
        }
        return;
    }

    if (phase_ == Phase::EaseOut) {
        easeOutProgress_ += deltaSeconds / std::max(0.001f, kEaseOutDuration);
        if (easeOutProgress_ >= 1.0f) {
            phase_ = Phase::Complete;
            phaseElapsed_ = 0.0f;
        }
    }
}

bool MikuDiandongPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

bool MikuDiandongPresentation::overridesCamera() const {
    return true;
}

void MikuDiandongPresentation::applyCameraState(Camera3D& camera) const {
    if (phase_ == Phase::Chase) {
        camera = computeChaseCamera(cameraProgress_, mikuWorldX_, mikuWorldY_, mikuWorldZ_);
        return;
    }

    Camera3D impactCamera = computeChaseCamera(
        impactCameraProgress_, impactMikuWorldX_, impactMikuWorldY_, impactMikuWorldZ_);
    impactCamera.focalLength = kImpactFocalLength;

    if (phase_ == Phase::ImpactFreeze) {
        camera = impactCamera;
        return;
    }

    const float easeT = easing::easeOutCubic(easing::clamp01(easeOutProgress_));
    Camera3D zoomCamera = computeChaseCamera(0.0f, impactMikuWorldX_, impactMikuWorldY_, impactMikuWorldZ_);
    zoomCamera.focalLength = kEaseOutFocalLength;

    camera.posX = easing::lerp(impactCamera.posX, zoomCamera.posX, easeT);
    camera.posY = easing::lerp(impactCamera.posY, zoomCamera.posY, easeT);
    camera.posZ = easing::lerp(impactCamera.posZ, zoomCamera.posZ, easeT);
    camera.yawDegrees = easing::lerp(impactCamera.yawDegrees, zoomCamera.yawDegrees, easeT);
    camera.pitchDegrees = easing::lerp(impactCamera.pitchDegrees, zoomCamera.pitchDegrees, easeT);
    camera.focalLength = easing::lerp(kImpactFocalLength, kEaseOutFocalLength, easeT);
}

bool MikuDiandongPresentation::getCasterWorldOverride(float& outX, float& outY, float& outZ) const {
    outX = mikuWorldX_;
    outY = mikuWorldY_;
    outZ = mikuWorldZ_;
    return true;
}

void MikuDiandongPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)screenW;
    (void)screenH;

    ensureDiandongTexture(renderer);

    if (phase_ == Phase::ImpactFreeze) {
        drawFreezeOverlay(renderer, screenW, screenH, camera);
        return;
    }

    drawDiandongSprite(renderer, camera, mikuWorldX_, mikuWorldY_, mikuWorldZ_, false, 1.0f);
}

int MikuDiandongPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

int MikuDiandongPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

bool MikuDiandongPresentation::shouldRenderCasterEntity() const {
    return false;
}

bool MikuDiandongPresentation::shouldRenderAboveHud() const {
    return true;
}

void MikuDiandongPresentation::setExternalTextures(SDL_Texture* casterSprite, SDL_Texture* targetSprite) {
    casterSpriteTexture_ = casterSprite;
    targetSpriteTexture_ = targetSprite;
}

std::optional<SplashArtConfig> MikuDiandongPresentation::getSplashConfig(SDL_Texture* sprite) const {
    SplashArtConfig cfg;
    cfg.abilityName = "DianDong Power";
    cfg.sprite = sprite;
    cfg.enterDuration = 0.40f;
    cfg.holdDuration = 1.20f;
    cfg.exitDuration = 0.35f;
    cfg.maxDimAlpha = 0.65f;
    return cfg;
}

void MikuDiandongPresentation::updateMikuPosition(float progress) {
    mikuWorldX_ = easing::lerp(mikuStartX_, mikuTargetX_, progress);
    mikuWorldY_ = easing::lerp(mikuStartY_, mikuTargetY_, progress);
    const float arc = std::sin(progress * kPi) * 12.0f;
    mikuWorldZ_ = mikuStartZ_ - arc;
}

Camera3D MikuDiandongPresentation::computeChaseCamera(float cameraProgress,
                                                     float lookX, float lookY, float lookZ) const {
    Camera3D camera;
    const float startX = casterX_ + kCameraStartOffsetX;
    const float startY = casterY_ + kCameraStartOffsetY;
    const float startZ = casterZ_ + kCameraStartOffsetZ;
    const float endX = targetX_ + kCameraEndOffsetX;
    const float endY = targetY_ + kCameraEndOffsetY;
    const float endZ = targetZ_ + kCameraEndOffsetZ;

    camera.posX = easing::lerp(startX, endX, cameraProgress);
    camera.posY = easing::lerp(startY, endY, cameraProgress);
    camera.posZ = easing::lerp(startZ, endZ, cameraProgress);

    camera.yawDegrees = easing::lerp(kCameraStartYaw, kCameraEndYaw, cameraProgress) +
        (std::sin(chaseTime_ * 2.3f) * 1.2f);
    camera.pitchDegrees = easing::lerp(kCameraStartPitch, kCameraEndPitch, cameraProgress);
    camera.focalLength = easing::lerp(kStartFocalLength, kImpactFocalLength, cameraProgress) +
        (std::sin(chaseTime_ * 2.9f) * 1400.0f * (1.0f - cameraProgress));

    const float lookBias = std::clamp(0.32f + (cameraProgress * 0.56f), 0.0f, 1.0f);
    camera.posX += std::sin(lookBias * kPi) * 6.0f;
    camera.posY += std::cos(lookBias * kPi) * 3.0f;

    const float finalLookX = easing::lerp(lookX, targetX_, lookBias);
    const float finalLookY = easing::lerp(lookY, targetY_, lookBias);
    const float finalLookZ = easing::lerp(lookZ - 20.0f, targetZ_ - 90.0f, lookBias);
    const float dx = finalLookX - camera.posX;
    const float dy = finalLookY - camera.posY;
    const float dz = finalLookZ - camera.posZ;
    const float horizontalDist = std::sqrt((dx * dx) + (dy * dy));
    camera.yawDegrees = std::atan2(-dx, dy) * 180.0f / kPi;
    camera.pitchDegrees = std::atan2(dz, std::max(1.0f, horizontalDist)) * 180.0f / kPi;

    return camera;
}

void MikuDiandongPresentation::drawDiandongSprite(SDL_Renderer* renderer, const Camera3D& camera,
                                                 float worldX, float worldY, float worldZ,
                                                 bool silhouette, float scaleMultiplier) const {
    if (renderer == nullptr) {
        return;
    }

    const float depth = camera.getDepth(worldX, worldY, worldZ);
    if (depth <= kMinimumDepth) {
        return;
    }

    const SDL_FPoint screen = camera.worldToScreen(worldX, worldY, worldZ);
    const float perspectiveScale = camera.getPerspectiveScale(worldX, worldY, worldZ);
    if (perspectiveScale <= 0.0f) {
        return;
    }

    const float textureHeight = (diandongTextureHeight_ > 0) ? static_cast<float>(diandongTextureHeight_) : kDiandongBaseHeight;
    const float textureWidth = (diandongTextureWidth_ > 0) ? static_cast<float>(diandongTextureWidth_)
                                                            : (textureHeight * 1.4f);
    const int drawHeight = std::max(4, static_cast<int>(textureHeight * perspectiveScale * scaleMultiplier));
    const int drawWidth = std::max(4, static_cast<int>(textureWidth / std::max(1.0f, textureHeight) * drawHeight));

    SDL_Rect dstRect{
        static_cast<int>(screen.x) - (drawWidth / 2),
        static_cast<int>(screen.y) - drawHeight,
        drawWidth,
        drawHeight
    };

    if (diandongTexture_ != nullptr) {
        SDL_SetTextureBlendMode(diandongTexture_, SDL_BLENDMODE_BLEND);
        if (silhouette) {
            SDL_SetTextureColorMod(diandongTexture_, 0, 0, 0);
        } else {
            SDL_SetTextureColorMod(diandongTexture_, 255, 255, 255);
        }
        SDL_SetTextureAlphaMod(diandongTexture_, 255);
        SDL_RenderCopy(renderer, diandongTexture_, nullptr, &dstRect);
        SDL_SetTextureColorMod(diandongTexture_, 255, 255, 255);
        SDL_SetTextureAlphaMod(diandongTexture_, 255);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, silhouette ? 0 : 125, silhouette ? 0 : 255, silhouette ? 0 : 255, 220);
    SDL_RenderFillRect(renderer, &dstRect);
}

void MikuDiandongPresentation::drawSilhouette(SDL_Renderer* renderer, const Camera3D& camera,
                                              SDL_Texture* texture,
                                              float worldX, float worldY, float worldZ,
                                              float baseHeight, float widthScale) const {
    if (renderer == nullptr) {
        return;
    }

    const float depth = camera.getDepth(worldX, worldY, worldZ);
    if (depth <= kMinimumDepth) {
        return;
    }

    const SDL_FPoint screen = camera.worldToScreen(worldX, worldY, worldZ);
    const float perspectiveScale = camera.getPerspectiveScale(worldX, worldY, worldZ);
    if (perspectiveScale <= 0.0f) {
        return;
    }

    const int drawHeight = std::max(4, static_cast<int>((baseHeight > 0.0f ? baseHeight : kBossSilhouetteHeight) *
                                                        perspectiveScale));
    int drawWidth = drawHeight;
    if (texture != nullptr) {
        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);
        if (texH > 0 && texW > 0) {
            drawWidth = std::max(4, static_cast<int>(drawHeight *
                (static_cast<float>(texW) / static_cast<float>(texH)) * widthScale));
        } else {
            drawWidth = std::max(4, static_cast<int>(drawHeight * widthScale));
        }
    } else {
        drawWidth = std::max(4, static_cast<int>(drawHeight * widthScale));
    }

    SDL_Rect dstRect{
        static_cast<int>(screen.x) - (drawWidth / 2),
        static_cast<int>(screen.y) - drawHeight,
        drawWidth,
        drawHeight
    };

    if (texture != nullptr) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(texture, 0, 0, 0);
        SDL_SetTextureAlphaMod(texture, 255);
        SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(texture, 255);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &dstRect);
}

bool MikuDiandongPresentation::ensureDiandongTexture(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_) {
        return diandongTexture_ != nullptr;
    }

    attemptedTextureLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    if (renderer == nullptr) {
        return false;
    }
    const std::string path = resolveDiandongAssetPath();
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (surface != nullptr) {
        diandongTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_QueryTexture(diandongTexture_, nullptr, nullptr, &diandongTextureWidth_, &diandongTextureHeight_);
        SDL_FreeSurface(surface);
    }
#endif

    return diandongTexture_ != nullptr;
}

void MikuDiandongPresentation::drawFreezeOverlay(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) const {
    if (renderer == nullptr) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    const SDL_Rect full{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &full);

    drawDiandongSprite(renderer, camera, mikuWorldX_, mikuWorldY_, mikuWorldZ_, true, 1.05f);
    drawSilhouette(renderer, camera, targetSpriteTexture_, targetX_, targetY_, targetZ_, kBossSilhouetteHeight, kBossSilhouetteWidthScale);
}

} // namespace battle
