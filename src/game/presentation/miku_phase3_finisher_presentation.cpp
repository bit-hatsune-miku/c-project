#include "miku_phase3_finisher_presentation.h"

#include "../core/easing.h"
#include "../../platform/path_resolution.h"

#include <algorithm>
#include <cmath>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kGazeDurationSeconds = 0.95f;
constexpr float kPullBackDurationSeconds = 1.20f;
constexpr float kHoldDurationSeconds = 0.30f;
constexpr float kBeamDurationSeconds = 0.50f;
constexpr float kWhiteHoldDurationSeconds = 0.70f;
constexpr float kSettleDurationSeconds = 0.75f;

constexpr float kPullBackStartSeconds = kGazeDurationSeconds;
constexpr float kHoldStartSeconds = kPullBackStartSeconds + kPullBackDurationSeconds;
constexpr float kBeamStartSeconds = kHoldStartSeconds + kHoldDurationSeconds;
constexpr float kWhiteHoldStartSeconds = kBeamStartSeconds + kBeamDurationSeconds;
constexpr float kSettleStartSeconds = kWhiteHoldStartSeconds + kWhiteHoldDurationSeconds;
constexpr float kTotalDurationSeconds = kSettleStartSeconds + kSettleDurationSeconds;

constexpr char kMikuTexturePath[] = "assets/combat/presentations/special/mikumikubeam.png";
constexpr char kFinisherVoicePath[] = "assets/combat/voices/miku/phase3_finisher/1.wav";
constexpr float kPi = 3.14159265359f;
constexpr float kMinimumDepth = 1.0f;
constexpr float kMikuWorldHeightUnits = 250.0f;
constexpr float kLyooWorldHeightUnits = 560.0f;

float lerpF(float a, float b, float t) {
    return a + ((b - a) * t);
}

float phaseProgress(float elapsed, float start, float duration) {
    return easing::clamp01((elapsed - start) / std::max(0.001f, duration));
}

float easeInOutCubicLocal(float t) {
    const float clamped = easing::clamp01(t);
    if (clamped < 0.5f) {
        return 4.0f * clamped * clamped * clamped;
    }
    const float f = (-2.0f * clamped) + 2.0f;
    return 1.0f - ((f * f * f) * 0.5f);
}

Camera3D makeLookCamera(float posX,
                        float posY,
                        float posZ,
                        float lookX,
                        float lookY,
                        float lookZ,
                        float focalLength) {
    Camera3D camera;
    camera.posX = posX;
    camera.posY = posY;
    camera.posZ = posZ;
    camera.focalLength = focalLength;

    const float dx = lookX - posX;
    const float dy = lookY - posY;
    const float dz = lookZ - posZ;
    const float horizontal = std::sqrt((dx * dx) + (dy * dy));
    camera.yawDegrees = std::atan2(-dx, dy) * 180.0f / kPi;
    camera.pitchDegrees = std::atan2(dz, std::max(1.0f, horizontal)) * 180.0f / kPi;
    return camera;
}

void blendCamera(Camera3D& output, const Camera3D& from, const Camera3D& to, float progress) {
    const float eased = easeInOutCubicLocal(progress);
    output.posX = lerpF(from.posX, to.posX, eased);
    output.posY = lerpF(from.posY, to.posY, eased);
    output.posZ = lerpF(from.posZ, to.posZ, eased);
    output.pitchDegrees = lerpF(from.pitchDegrees, to.pitchDegrees, eased);
    output.yawDegrees = lerpF(from.yawDegrees, to.yawDegrees, eased);
    output.focalLength = lerpF(from.focalLength, to.focalLength, eased);
}

void drawThickLine(SDL_Renderer* renderer,
                   const SDL_FPoint& start,
                   const SDL_FPoint& end,
                   float thickness,
                   SDL_Color color) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::max(1.0f, std::sqrt((dx * dx) + (dy * dy)));
    const float normalX = -dy / length;
    const float normalY = dx / length;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    const int bandCount = std::max(1, static_cast<int>(std::ceil(thickness)));
    for (int band = 0; band < bandCount; ++band) {
        const float offset = static_cast<float>(band) - (static_cast<float>(bandCount - 1) * 0.5f);
        SDL_RenderDrawLineF(
            renderer,
            start.x + (normalX * offset),
            start.y + (normalY * offset),
            end.x + (normalX * offset),
            end.y + (normalY * offset));
    }
}

} // namespace

MikuPhase3FinisherPresentation::MikuPhase3FinisherPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    const float dx = targetWorldX - casterWorldX;
    const float dy = targetWorldY - casterWorldY;
    const float distance = std::max(0.001f, std::sqrt((dx * dx) + (dy * dy)));
    dirX_ = dx / distance;
    dirY_ = dy / distance;
    perpX_ = -dirY_;
    perpY_ = dirX_;
    totalDuration_ = kTotalDurationSeconds;
}

MikuPhase3FinisherPresentation::~MikuPhase3FinisherPresentation() {
    releaseAssets();
}

void MikuPhase3FinisherPresentation::start() {
    elapsedTime_ = 0.0f;
    pendingAudioCommands_.clear();
    pendingHitEvents_ = 0;
    pendingAbilityAudioCues_ = 0;
    beamHitQueued_ = false;
    introVoiceQueued_ = false;
}

void MikuPhase3FinisherPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (!introVoiceQueued_) {
        pendingAudioCommands_.push_back(PresentationAudioCommand{
            PresentationAudioCommandType::PlayVoiceOneShot,
            kFinisherVoicePath,
            1.0f,
        });
        introVoiceQueued_ = true;
    }

    if (!beamHitQueued_ && elapsedTime_ >= kWhiteHoldStartSeconds - 0.02f) {
        pendingHitEvents_ = 1;
        beamHitQueued_ = true;
    }
}

void MikuPhase3FinisherPresentation::preload(SDL_Renderer* renderer) {
    ensureAssetsLoaded(renderer);
}

void MikuPhase3FinisherPresentation::render(SDL_Renderer* renderer,
                                            int screenW,
                                            int screenH,
                                            const Camera3D& camera) {
    ensureAssetsLoaded(renderer);

    const Phase phase = currentPhase();
    const WorldPoint miku = currentMikuAnchor();
    const WorldPoint lyoo{targetX_, targetY_, targetZ_};
    const WorldPoint source = currentBeamSource();

    if (phase == Phase::WhiteHold) {
        drawWhiteOverlay(renderer, screenW, screenH, 1.0f);
        return;
    }

    if (phase == Phase::Settle) {
        drawWhiteOverlay(
            renderer,
            screenW,
            screenH,
            lerpF(0.54f, 0.0f, phaseProgress(elapsedTime_, kSettleStartSeconds, kSettleDurationSeconds))
        );
    }

    const Uint8 lyooAlpha = currentLyooAlpha();
    if (lyooAlpha > 0) {
        drawWorldSprite(
            renderer,
            camera,
            overlayTargetSpriteTexture_,
            0.74f,
            lyoo.x,
            lyoo.y,
            lyoo.z,
            kLyooWorldHeightUnits,
            1.0f,
            0.0,
            lyooAlpha,
            SDL_Color{18, 20, 30, lyooAlpha},
            true);
    }

    drawWorldSprite(
        renderer,
        camera,
        mikuTexture_ != nullptr ? mikuTexture_ : overlayCasterSpriteTexture_,
        1.84f,
        miku.x - (perpX_ * 4.0f),
        miku.y - (perpY_ * 4.0f),
        miku.z + 2.0f,
        kMikuWorldHeightUnits * 1.02f,
        1.0f,
        currentMikuAngleDegrees(),
        92,
        SDL_Color{22, 24, 38, 255},
        true);
    drawWorldSprite(
        renderer,
        camera,
        mikuTexture_ != nullptr ? mikuTexture_ : overlayCasterSpriteTexture_,
        1.84f,
        miku.x,
        miku.y,
        miku.z,
        kMikuWorldHeightUnits,
        1.0f,
        currentMikuAngleDegrees(),
        255,
        SDL_Color{255, 255, 255, 255},
        true);

    const float sourceIntensity =
        phase == Phase::Gaze ? lerpF(0.14f, 0.30f, phaseProgress(elapsedTime_, 0.0f, kGazeDurationSeconds)) :
        phase == Phase::PullBack ? lerpF(0.34f, 0.86f, phaseProgress(elapsedTime_, kPullBackStartSeconds, kPullBackDurationSeconds)) :
        phase == Phase::Hold ? 0.96f :
        phase == Phase::Beam ? 1.04f :
        0.0f;
    if (sourceIntensity > 0.0f) {
        drawChargeSource(renderer, camera, source, sourceIntensity);
    }

    if (phase == Phase::Beam) {
        const float t = phaseProgress(elapsedTime_, kBeamStartSeconds, kBeamDurationSeconds);
        drawBeam(renderer, camera, source, lyoo, screenW, screenH, t, 1.0f);
        drawWhiteOverlay(renderer, screenW, screenH, lerpF(0.0f, 0.88f, easing::easeInCubic(t)));
    }
}

bool MikuPhase3FinisherPresentation::isComplete() const {
    return elapsedTime_ >= kTotalDurationSeconds;
}

bool MikuPhase3FinisherPresentation::overridesCamera() const {
    return true;
}

void MikuPhase3FinisherPresentation::applyCameraState(Camera3D& camera) const {
    const WorldPoint miku = currentMikuAnchor();

    const Camera3D gazeStart = makeLookCamera(
        targetX_ - (dirX_ * 54.0f),
        targetY_ - (dirY_ * 54.0f),
        targetZ_ - 142.0f,
        miku.x,
        miku.y,
        miku.z - 122.0f,
        30000.0f);
    const Camera3D gazeEnd = makeLookCamera(
        targetX_ - (dirX_ * 22.0f) + (perpX_ * 14.0f),
        targetY_ - (dirY_ * 22.0f) + (perpY_ * 14.0f),
        targetZ_ - 136.0f,
        miku.x,
        miku.y,
        miku.z - 124.0f,
        28000.0f);
    const Camera3D revealEnd = makeLookCamera(
        targetX_ + (dirX_ * 42.0f) + (perpX_ * 136.0f),
        targetY_ + (dirY_ * 42.0f) + (perpY_ * 136.0f),
        targetZ_ - 150.0f,
        lerpF(miku.x, targetX_, 0.42f),
        lerpF(miku.y, targetY_, 0.42f),
        lerpF(miku.z - 126.0f, targetZ_ - 150.0f, 0.42f),
        24400.0f);
    const Camera3D impactCamera = makeLookCamera(
        targetX_ - (dirX_ * 10.0f) + (perpX_ * 8.0f),
        targetY_ - (dirY_ * 10.0f) + (perpY_ * 8.0f),
        targetZ_ - 146.0f,
        miku.x + (dirX_ * 8.0f),
        miku.y + (dirY_ * 8.0f),
        miku.z - 122.0f,
        33200.0f);
    const Camera3D settleEnd = makeLookCamera(
        miku.x - (dirX_ * 84.0f) - (perpX_ * 92.0f),
        miku.y - (dirY_ * 84.0f) - (perpY_ * 92.0f),
        miku.z - 160.0f,
        miku.x + (dirX_ * 36.0f),
        miku.y + (dirY_ * 36.0f),
        miku.z - 126.0f,
        30800.0f);

    camera = gazeStart;
    const Phase phase = currentPhase();
    switch (phase) {
        case Phase::Gaze:
            blendCamera(camera, gazeStart, gazeEnd, phaseProgress(elapsedTime_, 0.0f, kGazeDurationSeconds));
            break;
        case Phase::PullBack:
            blendCamera(camera, gazeEnd, revealEnd, phaseProgress(elapsedTime_, kPullBackStartSeconds, kPullBackDurationSeconds));
            break;
        case Phase::Hold:
            camera = revealEnd;
            break;
        case Phase::Beam:
            blendCamera(camera, revealEnd, impactCamera, phaseProgress(elapsedTime_, kBeamStartSeconds, kBeamDurationSeconds));
            break;
        case Phase::WhiteHold:
            camera = impactCamera;
            break;
        case Phase::Settle:
        case Phase::Complete:
            blendCamera(camera, impactCamera, settleEnd, phaseProgress(elapsedTime_, kSettleStartSeconds, kSettleDurationSeconds));
            break;
    }

    if (phase == Phase::Beam) {
        const float t = phaseProgress(elapsedTime_, kBeamStartSeconds, kBeamDurationSeconds);
        const float shake = lerpF(1.8f, 9.6f, easing::easeInCubic(t));
        camera.posX += std::sin(elapsedTime_ * 82.0f) * shake;
        camera.posY += std::cos(elapsedTime_ * 74.0f) * shake * 0.28f;
        camera.posZ += std::sin(elapsedTime_ * 96.0f) * shake * 0.18f;
        camera.yawDegrees += std::sin(elapsedTime_ * 64.0f) * shake * 0.12f;
        camera.pitchDegrees += std::cos(elapsedTime_ * 58.0f) * shake * 0.10f;
    }
}

bool MikuPhase3FinisherPresentation::shouldHideNonCasterCharacters() const {
    return true;
}

bool MikuPhase3FinisherPresentation::shouldRenderCasterEntity() const {
    return false;
}

bool MikuPhase3FinisherPresentation::shouldRenderBossEntity() const {
    return false;
}

bool MikuPhase3FinisherPresentation::shouldRenderAboveHud() const {
    return false;
}

bool MikuPhase3FinisherPresentation::shouldRenderFloor() const {
    return true;
}

void MikuPhase3FinisherPresentation::setOverlayTextures(SDL_Texture* casterSprite, SDL_Texture* targetSprite) {
    overlayCasterSpriteTexture_ = casterSprite;
    overlayTargetSpriteTexture_ = targetSprite;
}

std::vector<PresentationAudioCommand> MikuPhase3FinisherPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

int MikuPhase3FinisherPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

int MikuPhase3FinisherPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

void MikuPhase3FinisherPresentation::ensureAssetsLoaded(SDL_Renderer* renderer) {
    if (renderer == nullptr) {
        return;
    }
    if (loadedRenderer_ != nullptr && loadedRenderer_ != renderer) {
        releaseAssets();
    }
    if (attemptedLoad_) {
        return;
    }

    attemptedLoad_ = true;
    loadedRenderer_ = renderer;

#ifdef BATTLE_ENABLE_IMAGE
    if (SDL_Surface* surface = IMG_Load(platform::path::resolvePath(kMikuTexturePath).c_str())) {
        mikuTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
        mikuTextureWidth_ = surface->w;
        mikuTextureHeight_ = surface->h;
        SDL_FreeSurface(surface);
    }
#endif
}

void MikuPhase3FinisherPresentation::releaseAssets() {
    if (mikuTexture_ != nullptr) {
        SDL_DestroyTexture(mikuTexture_);
        mikuTexture_ = nullptr;
    }
    mikuTextureWidth_ = 0;
    mikuTextureHeight_ = 0;
    attemptedLoad_ = false;
    loadedRenderer_ = nullptr;
}

MikuPhase3FinisherPresentation::Phase MikuPhase3FinisherPresentation::currentPhase() const {
    if (elapsedTime_ < kPullBackStartSeconds) {
        return Phase::Gaze;
    }
    if (elapsedTime_ < kHoldStartSeconds) {
        return Phase::PullBack;
    }
    if (elapsedTime_ < kBeamStartSeconds) {
        return Phase::Hold;
    }
    if (elapsedTime_ < kWhiteHoldStartSeconds) {
        return Phase::Beam;
    }
    if (elapsedTime_ < kSettleStartSeconds) {
        return Phase::WhiteHold;
    }
    if (elapsedTime_ < kTotalDurationSeconds) {
        return Phase::Settle;
    }
    return Phase::Complete;
}

MikuPhase3FinisherPresentation::WorldPoint MikuPhase3FinisherPresentation::currentMikuAnchor() const {
    WorldPoint point{casterX_, casterY_, casterZ_};
    const Phase phase = currentPhase();

    if (phase == Phase::PullBack || phase == Phase::Hold || phase == Phase::Beam) {
        const float t = phase == Phase::PullBack
            ? phaseProgress(elapsedTime_, kPullBackStartSeconds, kPullBackDurationSeconds)
            : 1.0f;
        point.x += (dirX_ * lerpF(0.0f, 20.0f, t)) - (perpX_ * lerpF(0.0f, 8.0f, t));
        point.y += (dirY_ * lerpF(0.0f, 20.0f, t)) - (perpY_ * lerpF(0.0f, 8.0f, t));
        point.z -= std::sin(t * kPi) * 8.0f;
        return point;
    }

    if (phase == Phase::Settle || phase == Phase::Complete) {
        const float t = phaseProgress(elapsedTime_, kSettleStartSeconds, kSettleDurationSeconds);
        point.x += (dirX_ * lerpF(20.0f, 8.0f, t)) - (perpX_ * lerpF(8.0f, 2.0f, t));
        point.y += (dirY_ * lerpF(20.0f, 8.0f, t)) - (perpY_ * lerpF(8.0f, 2.0f, t));
        point.z -= lerpF(4.0f, 0.0f, t);
        return point;
    }

    return point;
}

MikuPhase3FinisherPresentation::WorldPoint MikuPhase3FinisherPresentation::currentBeamSource() const {
    const WorldPoint miku = currentMikuAnchor();
    return WorldPoint{
        miku.x + (dirX_ * 34.0f) - (perpX_ * 10.0f),
        miku.y + (dirY_ * 34.0f) - (perpY_ * 10.0f),
        miku.z - 122.0f
    };
}

float MikuPhase3FinisherPresentation::currentMikuAngleDegrees() const {
    const Phase phase = currentPhase();
    if (phase == Phase::PullBack) {
        return lerpF(0.0f, -8.0f, phaseProgress(elapsedTime_, kPullBackStartSeconds, kPullBackDurationSeconds));
    }
    if (phase == Phase::Hold || phase == Phase::Beam) {
        return -8.0f;
    }
    if (phase == Phase::Settle || phase == Phase::Complete) {
        return lerpF(-8.0f, 0.0f, phaseProgress(elapsedTime_, kSettleStartSeconds, kSettleDurationSeconds));
    }
    return 0.0f;
}

Uint8 MikuPhase3FinisherPresentation::currentLyooAlpha() const {
    const Phase phase = currentPhase();
    if (phase == Phase::PullBack) {
        return static_cast<Uint8>(std::lround(
            lerpF(0.0f, 210.0f, phaseProgress(elapsedTime_, kPullBackStartSeconds, kPullBackDurationSeconds))
        ));
    }
    if (phase == Phase::Hold) {
        return 210;
    }
    if (phase == Phase::Beam) {
        return static_cast<Uint8>(std::lround(
            lerpF(210.0f, 0.0f, phaseProgress(elapsedTime_, kBeamStartSeconds, kBeamDurationSeconds))
        ));
    }
    return 0;
}

void MikuPhase3FinisherPresentation::drawWorldSprite(SDL_Renderer* renderer,
                                                     const Camera3D& camera,
                                                     SDL_Texture* texture,
                                                     float fallbackAspect,
                                                     float worldX,
                                                     float worldY,
                                                     float worldZ,
                                                     float worldHeightUnits,
                                                     float widthScale,
                                                     double angleDegrees,
                                                     Uint8 alpha,
                                                     SDL_Color tint,
                                                     bool anchorFeet) const {
    if (renderer == nullptr || alpha == 0) {
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

    float aspect = fallbackAspect;
    if (texture != nullptr) {
        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);
        if (texW > 0 && texH > 0) {
            aspect = static_cast<float>(texW) / static_cast<float>(texH);
        }
    }

    const float drawHeight = std::max(10.0f, worldHeightUnits * perspectiveScale);
    const float drawWidth = drawHeight * aspect * std::max(0.1f, widthScale);
    SDL_FRect dst{
        screen.x - (drawWidth * 0.5f),
        anchorFeet ? (screen.y - drawHeight) : (screen.y - (drawHeight * 0.5f)),
        drawWidth,
        drawHeight
    };

    if (texture != nullptr) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(texture, tint.r, tint.g, tint.b);
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderCopyExF(renderer, texture, nullptr, &dst, angleDegrees, nullptr, SDL_FLIP_NONE);
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(texture, 255);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, tint.r, tint.g, tint.b, alpha);
    SDL_RenderFillRectF(renderer, &dst);
}

void MikuPhase3FinisherPresentation::drawChargeSource(SDL_Renderer* renderer,
                                                      const Camera3D& camera,
                                                      const WorldPoint& sourceWorld,
                                                      float intensity) const {
    if (renderer == nullptr || intensity <= 0.0f) {
        return;
    }

    const float depth = camera.getDepth(sourceWorld.x, sourceWorld.y, sourceWorld.z);
    if (depth <= kMinimumDepth) {
        return;
    }

    const SDL_FPoint screen = camera.worldToScreen(sourceWorld.x, sourceWorld.y, sourceWorld.z);
    const float scale = camera.getPerspectiveScale(sourceWorld.x, sourceWorld.y, sourceWorld.z);
    const float baseRadius = std::max(8.0f, 38.0f * scale);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 5; ++i) {
        const float t = static_cast<float>(i) / 4.0f;
        const float radius = lerpF(baseRadius * (1.75f + (intensity * 0.8f)), baseRadius * 0.45f, t);
        const Uint8 alpha = static_cast<Uint8>(std::lround((1.0f - t) * 92.0f * intensity));
        SDL_SetRenderDrawColor(renderer, 190, 240, 255, alpha);
        const SDL_FRect rect{
            screen.x - radius,
            screen.y - radius,
            radius * 2.0f,
            radius * 2.0f
        };
        SDL_RenderFillRectF(renderer, &rect);
    }

    const float coreRadius = std::max(4.0f, baseRadius * (0.38f + (intensity * 0.12f)));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(std::lround(224.0f * std::min(intensity, 1.0f))));
    const SDL_FRect core{
        screen.x - coreRadius,
        screen.y - coreRadius,
        coreRadius * 2.0f,
        coreRadius * 2.0f
    };
    SDL_RenderFillRectF(renderer, &core);
}

void MikuPhase3FinisherPresentation::drawBeam(SDL_Renderer* renderer,
                                              const Camera3D& camera,
                                              const WorldPoint& sourceWorld,
                                              const WorldPoint& targetWorld,
                                              int screenW,
                                              int screenH,
                                              float beamProgress,
                                              float alphaScale) const {
    if (renderer == nullptr || alphaScale <= 0.0f) {
        return;
    }

    const float sourceDepth = camera.getDepth(sourceWorld.x, sourceWorld.y, sourceWorld.z);
    const float targetDepth = camera.getDepth(targetWorld.x, targetWorld.y, targetWorld.z - 170.0f);
    if (sourceDepth <= kMinimumDepth || targetDepth <= kMinimumDepth) {
        return;
    }

    const SDL_FPoint source = camera.worldToScreen(sourceWorld.x, sourceWorld.y, sourceWorld.z);
    const SDL_FPoint target = camera.worldToScreen(targetWorld.x, targetWorld.y, targetWorld.z - 170.0f);
    const float clamped = easing::clamp01(beamProgress);
    const float eased = easing::easeOutCubic(clamped);
    const float cameraT = easing::clamp01((clamped - 0.30f) / 0.70f);
    const SDL_FPoint impactPoint{
        static_cast<float>(screenW) * 0.5f + std::sin(elapsedTime_ * 46.0f) * lerpF(18.0f, 2.0f, cameraT),
        static_cast<float>(screenH) * 0.49f + std::cos(elapsedTime_ * 38.0f) * lerpF(12.0f, 1.5f, cameraT)
    };

    SDL_FPoint end = target;
    if (clamped < 0.30f) {
        const float travelT = easing::clamp01(clamped / 0.30f);
        end.x = lerpF(source.x, target.x, easing::easeOutCubic(travelT));
        end.y = lerpF(source.y, target.y, easing::easeOutCubic(travelT));
    } else {
        end.x = lerpF(target.x, impactPoint.x, easing::easeInCubic(cameraT));
        end.y = lerpF(target.y, impactPoint.y, easing::easeInCubic(cameraT));
    }

    const float glowThickness = lerpF(28.0f, 168.0f, eased);
    const float shellThickness = lerpF(12.0f, 74.0f, eased);
    const float coreThickness = lerpF(5.0f, 30.0f, eased);
    const SDL_Color glowColor{190, 240, 255, static_cast<Uint8>(std::lround(156.0f * eased * alphaScale))};
    const SDL_Color shellColor{245, 250, 255, static_cast<Uint8>(std::lround(235.0f * eased * alphaScale))};
    const SDL_Color coreColor{255, 255, 255, static_cast<Uint8>(std::lround(255.0f * eased * alphaScale))};

    drawThickLine(renderer, source, end, glowThickness, glowColor);
    drawThickLine(renderer, source, end, shellThickness, shellColor);
    drawThickLine(renderer, source, end, coreThickness, coreColor);

    if (clamped >= 0.30f) {
        const float flashT = easing::easeInCubic(cameraT);
        const float flashRadius = lerpF(28.0f, static_cast<float>(std::max(screenW, screenH)) * 0.40f, flashT);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(std::lround(180.0f * flashT * alphaScale)));
        const SDL_FRect flash{
            impactPoint.x - flashRadius,
            impactPoint.y - flashRadius,
            flashRadius * 2.0f,
            flashRadius * 2.0f
        };
        SDL_RenderFillRectF(renderer, &flash);
    }
}

void MikuPhase3FinisherPresentation::drawWhiteOverlay(SDL_Renderer* renderer,
                                                      int screenW,
                                                      int screenH,
                                                      float alpha) const {
    if (renderer == nullptr || alpha <= 0.0f) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(
        renderer,
        255,
        255,
        255,
        static_cast<Uint8>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f))
    );
    const SDL_Rect rect{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &rect);
}

} // namespace battle
