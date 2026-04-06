#include "huafei_support_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kSupportDurationSeconds = 0.90f;
constexpr float kBackdropFadeInSeconds = 0.12f;
constexpr float kStartOffsetX = -170.0f;
constexpr float kStartOffsetY = 40.0f;
constexpr float kStartOffsetZ = -125.0f;
constexpr float kEndOffsetX = -180.0f;
constexpr float kEndOffsetY = 80.0f;
constexpr float kEndOffsetZ = -129.0f;

float lerpF(float a, float b, float t) {
    return a + ((b - a) * t);
}

Camera3D makeSupportZoomStartCamera(float anchorX, float anchorY, float anchorZ) {
    Camera3D camera;
    camera.posX = anchorX + kStartOffsetX;
    camera.posY = anchorY + kStartOffsetY;
    camera.posZ = anchorZ + kStartOffsetZ;
    camera.pitchDegrees = -1.3f;
    camera.yawDegrees = 180.0f;
    camera.focalLength = 38000.0f;
    return camera;
}

Camera3D makeSupportZoomEndCamera(float anchorX, float anchorY, float anchorZ) {
    Camera3D camera;
    camera.posX = anchorX + kEndOffsetX;
    camera.posY = anchorY + kEndOffsetY;
    camera.posZ = anchorZ + kEndOffsetZ;
    camera.pitchDegrees = -1.0f;
    camera.yawDegrees = 180.0f;
    camera.focalLength = 36000.0f;
    return camera;
}

std::string resolveBackdropPath() {
    return platform::path::resolvePath("assets/combat/presentations/huaFei/gugong.png");
}

} // namespace

HuafeiSupportPresentation::HuafeiSupportPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) : targetX_(targetWorldX)
  , targetY_(targetWorldY)
  , targetZ_(targetWorldZ) {
    (void)casterWorldX;
    (void)casterWorldY;
    (void)casterWorldZ;
    totalDuration_ = kSupportDurationSeconds;
}

HuafeiSupportPresentation::~HuafeiSupportPresentation() {
    releaseBackdrop();
}

void HuafeiSupportPresentation::start() {
    elapsedTime_ = 0.0f;
    phaseElapsed_ = 0.0f;
    pendingAbilityAudioCues_ = 1;
}

void HuafeiSupportPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
    phaseElapsed_ += deltaTime;
}

void HuafeiSupportPresentation::preload(SDL_Renderer* renderer) {
    ensureBackdropLoaded(renderer);
}

void HuafeiSupportPresentation::render(SDL_Renderer* renderer,
                                       int screenW,
                                       int screenH,
                                       const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

void HuafeiSupportPresentation::renderBelowWorld(SDL_Renderer* renderer,
                                                 int screenW,
                                                 int screenH,
                                                 const Camera3D& camera) {
    (void)camera;
    if (renderer == nullptr) {
        return;
    }

    ensureBackdropLoaded(renderer);
    const float fadeT = easing::clamp01(phaseElapsed_ / kBackdropFadeInSeconds);
    const Uint8 alpha = static_cast<Uint8>(255.0f * easing::easeOutCubic(fadeT));

    SDL_FRect destination{
        0.0f,
        0.0f,
        static_cast<float>(screenW),
        static_cast<float>(screenH)
    };

    if (backdropTexture_ != nullptr) {
        SDL_SetTextureBlendMode(backdropTexture_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(backdropTexture_, alpha);
        SDL_RenderCopyF(renderer, backdropTexture_, nullptr, &destination);
        SDL_SetTextureAlphaMod(backdropTexture_, 255);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 232, 237, 246, alpha);
    SDL_RenderFillRectF(renderer, &destination);
}

bool HuafeiSupportPresentation::isComplete() const {
    return phaseElapsed_ >= kSupportDurationSeconds;
}

int HuafeiSupportPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

bool HuafeiSupportPresentation::overridesCamera() const {
    return true;
}

void HuafeiSupportPresentation::applyCameraState(Camera3D& camera) const {
    const Camera3D startCamera = makeSupportZoomStartCamera(targetX_, targetY_, targetZ_);
    const Camera3D endCamera = makeSupportZoomEndCamera(targetX_, targetY_, targetZ_);
    const float t = easing::easeOutQuint(
        easing::clamp01(phaseElapsed_ / kSupportDurationSeconds)
    );

    camera.posX = lerpF(startCamera.posX, endCamera.posX, t);
    camera.posY = lerpF(startCamera.posY, endCamera.posY, t);
    camera.posZ = lerpF(startCamera.posZ, endCamera.posZ, t);
    camera.pitchDegrees = lerpF(startCamera.pitchDegrees, endCamera.pitchDegrees, t);
    camera.yawDegrees = lerpF(startCamera.yawDegrees, endCamera.yawDegrees, t);
    camera.focalLength = lerpF(startCamera.focalLength, endCamera.focalLength, t);
}

bool HuafeiSupportPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool HuafeiSupportPresentation::shouldRenderCasterEntity() const {
    return true;
}

bool HuafeiSupportPresentation::shouldRenderBossEntity() const {
    return false;
}

bool HuafeiSupportPresentation::shouldRenderAboveHud() const {
    return false;
}

bool HuafeiSupportPresentation::shouldRenderFloor() const {
    return false;
}

bool HuafeiSupportPresentation::shouldUseCenteredPartyLayout() const {
    return true;
}

void HuafeiSupportPresentation::ensureBackdropLoaded(SDL_Renderer* renderer) {
    if (renderer == nullptr || backdropTexture_ != nullptr || backdropLoadAttempted_) {
        return;
    }
    backdropLoadAttempted_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolveBackdropPath().c_str());
    if (surface == nullptr) {
        return;
    }

    backdropTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

void HuafeiSupportPresentation::releaseBackdrop() {
    if (backdropTexture_ != nullptr) {
        SDL_DestroyTexture(backdropTexture_);
        backdropTexture_ = nullptr;
    }
    backdropLoadAttempted_ = false;
}

} // namespace battle
