#include "jiafei_boss_presentation.h"

#include "../core/easing.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {

namespace {
constexpr float kIntroDurationSeconds = 0.0f;
constexpr float kFocusOffsetX = 0.0f;
constexpr float kFocusOffsetY = 340.0f;
constexpr float kFocusOffsetZ = -125.0f;
constexpr float kFocusYawDegrees = 180.0f;
constexpr float kFocusPitchDegrees = -1.3f;
constexpr float kFocusFocalLength = 47000.0f;
constexpr float kFocusPosLerp = 0.35f;
constexpr float kFocusRotLerp = 0.30f;
constexpr float kFocusFocalLerp = 0.25f;
constexpr float kCharacterSpriteHeight = 260.0f;
constexpr float kFocusedCharacterScale = 1.13f;
constexpr float kChestAnchorRatio = 0.58f;
constexpr float kHeadSizeRatio = 0.42f;
constexpr float kSideLaneRatio = 0.62f;
constexpr float kHeadEntryDepthRatio = 0.0f;
constexpr float kHeadStartDepthRatio = -0.28f;
constexpr float kHeadPassDepthRatio = 1.20f;
constexpr float kHeadMaxRenderSize = 360.0f;
constexpr float kHeadFadeInDepthRatio = 0.18f;
constexpr float kMinHeadSize = 138.0f;
constexpr float kMaxHeadSize = 220.0f;
constexpr float kDodgeDistanceWorld = 220.0f;
constexpr float kPerfectDodgeTravelSeconds = 0.11f;
constexpr float kDodgeReturnSeconds = 0.06f;
constexpr float kRequiredDodgeCoverageRatio = 0.40f;
constexpr float kPi = 3.14159265f;
constexpr float kHeadImpactRatio =
    (1.0f - kHeadStartDepthRatio) / (kHeadPassDepthRatio - kHeadStartDepthRatio);
}

JiafeiBossPresentation::JiafeiBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ)
{
    phase_ = Phase::Intro;
    currentWave_ = 0;
    successfulDodges_ = 0;
    waveElapsed_ = 0.0f;
    sideFromLeft_ = (std::rand() % 2) == 0;
    damageMultiplier_ = 1.0f;
    inputReceived_ = false;
    inputCorrect_ = false;
    canReceiveInput_ = false;
    dodgeElapsed_ = 0.0f;
    dodgeDirection_ = 0;
    waveImpactResolved_ = false;
    pendingAbilityAudioCues_ = 0;
    pendingHitEvents_ = 0;
    pendingFeedbackEvents_.clear();
    dodgeMotionState_ = DodgeMotionState::Idle;
}

JiafeiBossPresentation::~JiafeiBossPresentation() {
    if (middleHeadTexture_) {
        SDL_DestroyTexture(middleHeadTexture_);
    }
    if (sideHeadTexture_) {
        SDL_DestroyTexture(sideHeadTexture_);
    }
}

void JiafeiBossPresentation::start() {
    elapsedTime_ = 0.0f;
    phase_ = Phase::Intro;
    currentWave_ = 0;
    successfulDodges_ = 0;
    waveElapsed_ = 0.0f;
    sideFromLeft_ = (std::rand() % 2) == 0;
    inputReceived_ = false;
    inputCorrect_ = false;
    canReceiveInput_ = false;
    damageMultiplier_ = 1.0f;
    dodgeElapsed_ = 0.0f;
    dodgeDirection_ = 0;
    waveImpactResolved_ = false;
    pendingAbilityAudioCues_ = 0;
    pendingHitEvents_ = 0;
    dodgeMotionState_ = DodgeMotionState::Idle;
}

void JiafeiBossPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (phase_ == Phase::Intro) {
        if (elapsedTime_ >= kIntroDurationSeconds) {
            phase_ = Phase::Attack;
            waveElapsed_ = 0.0f;
            canReceiveInput_ = true;
            inputReceived_ = false;
            inputCorrect_ = false;
            waveImpactResolved_ = false;
            pendingAbilityAudioCues_ += 2;
        }
        return;
    }

    if (phase_ != Phase::Attack) {
        return;
    }

    waveElapsed_ += deltaTime;
    updateDodgeMotion(deltaTime);

    if (!waveImpactResolved_ && waveElapsed_ >= waveImpactTimeSeconds()) {
        waveImpactResolved_ = true;
        const int landedHeadHits = resolveWaveHeadHits();
        const int dodgedHeads = std::max(0, 2 - landedHeadHits);
        const int totalHeadCount = std::max(1, getDamageLabelHitCount());
        const int projectedSuccessfulDodges = successfulDodges_ + dodgedHeads;
        const float projectedMultiplier = projectedSuccessfulDodges >= totalHeadCount
            ? 0.0f
            : (1.0f - (static_cast<float>(projectedSuccessfulDodges) / static_cast<float>(totalHeadCount)));
        PresentationFeedbackEvent event;
        event.signal = PresentationFeedbackSignal::binary(landedHeadHits == 0);
        event.multiplier = projectedMultiplier;
        event.comboEligible = true;
        pendingFeedbackEvents_.push_back(event);
        pendingHitEvents_ += landedHeadHits;
        successfulDodges_ += dodgedHeads;
        inputCorrect_ = landedHeadHits == 0;
    }

    canReceiveInput_ = !waveImpactResolved_ &&
        !inputReceived_ &&
        dodgeMotionState_ == DodgeMotionState::Idle;

    // Wave end
    if (waveElapsed_ >= waveDuration_) {
        ++currentWave_;
        if (currentWave_ >= totalWaves_) {
            phase_ = Phase::Complete;
            const int totalHeadCount = std::max(1, getDamageLabelHitCount());
            if (successfulDodges_ >= totalHeadCount) {
                damageMultiplier_ = 0.0f;
            } else {
                damageMultiplier_ = 1.0f -
                    static_cast<float>(successfulDodges_) / static_cast<float>(totalHeadCount);
            }
            return;
        }

        // next wave setup
        sideFromLeft_ = !sideFromLeft_;
        waveElapsed_ -= waveDuration_;
        inputReceived_ = false;
        inputCorrect_ = false;
        canReceiveInput_ = true;
        waveImpactResolved_ = false;
        pendingAbilityAudioCues_ += 2;
    }
}

void JiafeiBossPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    ensureTexturesLoaded(renderer);
    (void)screenW;
    (void)screenH;

    renderHeads(renderer, camera, false);
}

void JiafeiBossPresentation::renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    ensureTexturesLoaded(renderer);
    (void)screenW;
    (void)screenH;

    renderHeads(renderer, camera, true);
}

bool JiafeiBossPresentation::isComplete() const {
    if (phase_ == Phase::Complete) {
        std::cout << "[Jiafei] beat presentation complete; dodges=" << successfulDodges_ << " / " << totalWaves_ << ", multiplier=" << damageMultiplier_ << "\n";
    }
    return phase_ == Phase::Complete;
}

bool JiafeiBossPresentation::onKeyPressed(SDL_Keycode key) {
    if (phase_ != Phase::Attack || !canReceiveInput_ || inputReceived_ ||
        dodgeMotionState_ != DodgeMotionState::Idle) {
        return false;
    }

    bool isLeft = (key == SDLK_LEFT);
    bool isRight = (key == SDLK_RIGHT);
    if (!isLeft && !isRight) {
        return false;
    }

    inputReceived_ = true;
    dodgeDirection_ = isRight ? 1 : -1;
    dodgeElapsed_ = 0.0f;
    dodgeMotionState_ = DodgeMotionState::Outbound;
    inputCorrect_ = false;
    return true;
}

bool JiafeiBossPresentation::shouldHideNonCasterCharacters() const {
    return phase_ == Phase::Attack;
}

bool JiafeiBossPresentation::shouldRenderCasterEntity() const {
    return false;
}

bool JiafeiBossPresentation::shouldRenderBossEntity() const {
    return true;
}

bool JiafeiBossPresentation::shouldRenderAboveHud() const {
    return false;
}

bool JiafeiBossPresentation::overridesCamera() const {
    return true;
}

void JiafeiBossPresentation::applyCameraState(Camera3D& camera) const {
    const bool hasFocus = focusedPartyIndex_ >= 0;
    const float focusX = hasFocus ? targetX_ : (casterX_ + targetX_) * 0.5f;
    const float focusY = hasFocus ? targetY_ : (casterY_ + targetY_) * 0.5f;
    const float focusZ = hasFocus ? targetZ_ : (casterZ_ + targetZ_) * 0.5f;

    const float desiredX = focusX + kFocusOffsetX;
    const float desiredY = focusY + kFocusOffsetY;
    const float desiredZ = focusZ + kFocusOffsetZ;

    const float posLerp = hasFocus ? kFocusPosLerp : 0.25f;
    camera.posX += (desiredX - camera.posX) * posLerp;
    camera.posY += (desiredY - camera.posY) * posLerp;
    camera.posZ += (desiredZ - camera.posZ) * posLerp;

    camera.yawDegrees += (kFocusYawDegrees - camera.yawDegrees) * kFocusRotLerp;
    camera.pitchDegrees += (kFocusPitchDegrees - camera.pitchDegrees) * kFocusRotLerp;
    camera.focalLength += (kFocusFocalLength - camera.focalLength) * kFocusFocalLerp;
}

bool JiafeiBossPresentation::getTargetWorldOverride(float& outX, float& outY, float& outZ) const {
    if (focusedPartyIndex_ < 0 || dodgeDirection_ == 0) {
        return false;
    }

    const float dodgeOffset = currentDodgeOffsetWorld();
    if (dodgeOffset <= 0.001f) {
        return false;
    }

    const float yawRadians = kFocusYawDegrees * (kPi / 180.0f);
    const float rightX = std::cos(yawRadians);
    const float rightY = std::sin(yawRadians);

    outX = targetX_ + rightX * dodgeOffset * static_cast<float>(dodgeDirection_);
    outY = targetY_ + rightY * dodgeOffset * static_cast<float>(dodgeDirection_);
    outZ = targetZ_;
    return true;
}

int JiafeiBossPresentation::getFocusedPartyIndex() const {
    return focusedPartyIndex_;
}

void JiafeiBossPresentation::setTargetPartyIndex(int index) {
    focusedPartyIndex_ = index;
}

void JiafeiBossPresentation::setTargetWorldPosition(float x, float y, float z) {
    targetX_ = x;
    targetY_ = y;
    targetZ_ = z;
}

float JiafeiBossPresentation::getInputMultiplier() const {
    return damageMultiplier_;
}

PresentationFeedbackSignal JiafeiBossPresentation::getFeedbackSignal() const {
    return PresentationFeedbackSignal::binary(damageMultiplier_ <= 0.001f);
}

std::vector<PresentationFeedbackEvent> JiafeiBossPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

float JiafeiBossPresentation::consumeHitDamageMultiplier() {
    return getInputMultiplier();
}

int JiafeiBossPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int JiafeiBossPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

int JiafeiBossPresentation::getDamageLabelHitCount() const {
    return std::max(1, totalWaves_ * 2);
}

std::string JiafeiBossPresentation::getInputResultText() const {
    const int totalHeadCount = std::max(1, getDamageLabelHitCount());
    const int dodgePercent = static_cast<int>(std::lround(
        100.0f * static_cast<float>(successfulDodges_) / static_cast<float>(totalHeadCount)));
    const int reductionPct = static_cast<int>(std::lround((1.0f - damageMultiplier_) * 100.0f));
    return "Dodged " + std::to_string(successfulDodges_) + "/" + std::to_string(totalHeadCount) +
        " heads (" + std::to_string(dodgePercent) + "%), damage reduced " +
        std::to_string(reductionPct) + "%";
}

std::optional<SplashArtConfig> JiafeiBossPresentation::getSplashConfig(SDL_Texture* sprite) const {
    SplashArtConfig cfg;
    cfg.sprite = sprite;
    cfg.enterDuration = 0.42f;
    cfg.holdDuration = 1.95f;
    cfg.exitDuration = 0.38f;
    cfg.maxDimAlpha = 0.68f;
    return cfg;
}

void JiafeiBossPresentation::ensureTexturesLoaded(SDL_Renderer* renderer) {
    if (texturesLoaded_) return;

    // actual assets live under `presentations/jiafei` and are named head0/head1
    std::string midPath = "assets/combat/presentations/jiafei/head0.png";
    std::string sidePath = "assets/combat/presentations/jiafei/head1.png";

#ifdef BATTLE_ENABLE_IMAGE
    if (std::filesystem::exists(midPath)) {
        SDL_Surface* surf = IMG_Load(midPath.c_str());
        if (surf) {
            middleHeadTexture_ = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            std::cout << "[Jiafei] Loaded middle head texture: " << midPath << "\n";
        } else {
            std::cout << "[Jiafei] IMG_Load failed for " << midPath << "\n";
        }
    } else {
        std::cout << "[Jiafei] Missing file: " << midPath << "\n";
    }

    if (std::filesystem::exists(sidePath)) {
        SDL_Surface* surf = IMG_Load(sidePath.c_str());
        if (surf) {
            sideHeadTexture_ = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            std::cout << "[Jiafei] Loaded side head texture: " << sidePath << "\n";
        } else {
            std::cout << "[Jiafei] IMG_Load failed for " << sidePath << "\n";
        }
    } else {
        std::cout << "[Jiafei] Missing file: " << sidePath << "\n";
    }
#endif

    // If image support was disabled at compile-time or loading failed, log fallback use.
    if (!middleHeadTexture_ || !sideHeadTexture_) {
        std::cout << "[Jiafei] head textures not available; using fallback rectangles."
                  << " middleTex=" << (middleHeadTexture_?"OK":"NULL")
                  << " sideTex=" << (sideHeadTexture_?"OK":"NULL")
                  << " paths=(" << midPath << ", " << sidePath << ")\n";
    }

    texturesLoaded_ = true;
}

float JiafeiBossPresentation::interp(float a, float b, float t) const {
    return a + (b - a) * t;
}

float JiafeiBossPresentation::waveImpactTimeSeconds() const {
    return waveDuration_ * kHeadImpactRatio;
}

bool JiafeiBossPresentation::isCorrectDodgeDirection(int direction) const {
    const int safeDirection = sideFromLeft_ ? 1 : -1;
    return direction == safeDirection;
}

int JiafeiBossPresentation::resolveWaveHeadHits() const {
    const float dodgeCoverage = std::clamp(
        currentDodgeOffsetWorld() / std::max(0.001f, kDodgeDistanceWorld),
        0.0f,
        1.0f
    );
    const bool dodgedMiddleHead = dodgeCoverage >= kRequiredDodgeCoverageRatio;
    const bool dodgedSideHead = dodgedMiddleHead && inputReceived_ && isCorrectDodgeDirection(dodgeDirection_);

    int landedHits = 0;
    if (!dodgedMiddleHead) {
        ++landedHits;
    }
    if (!dodgedSideHead) {
        ++landedHits;
    }
    return landedHits;
}

void JiafeiBossPresentation::updateDodgeMotion(float deltaTime) {
    switch (dodgeMotionState_) {
    case DodgeMotionState::Idle:
        return;
    case DodgeMotionState::Outbound:
        dodgeElapsed_ += deltaTime;
        if (dodgeElapsed_ >= kPerfectDodgeTravelSeconds) {
            dodgeElapsed_ = 0.0f;
            dodgeMotionState_ = waveElapsed_ >= waveImpactTimeSeconds()
                ? DodgeMotionState::Return
                : DodgeMotionState::Hold;
        }
        return;
    case DodgeMotionState::Hold:
        if (waveElapsed_ >= waveImpactTimeSeconds()) {
            dodgeElapsed_ = 0.0f;
            dodgeMotionState_ = DodgeMotionState::Return;
        }
        return;
    case DodgeMotionState::Return:
        dodgeElapsed_ += deltaTime;
        if (dodgeElapsed_ >= kDodgeReturnSeconds) {
            dodgeElapsed_ = 0.0f;
            dodgeDirection_ = 0;
            dodgeMotionState_ = DodgeMotionState::Idle;
        }
        return;
    }
}

float JiafeiBossPresentation::currentDodgeOffsetWorld() const {
    switch (dodgeMotionState_) {
    case DodgeMotionState::Idle:
        return 0.0f;
    case DodgeMotionState::Outbound: {
        const float t = easing::clamp01(dodgeElapsed_ / std::max(0.001f, kPerfectDodgeTravelSeconds));
        return kDodgeDistanceWorld * easing::easeOutQuint(t);
    }
    case DodgeMotionState::Hold:
        return kDodgeDistanceWorld;
    case DodgeMotionState::Return: {
        const float t = easing::clamp01(dodgeElapsed_ / std::max(0.001f, kDodgeReturnSeconds));
        return kDodgeDistanceWorld * (1.0f - easing::easeOutQuint(t));
    }
    }

    return 0.0f;
}

bool JiafeiBossPresentation::computeHeadRenderState(const Camera3D& camera,
                                                    bool& outBehindTarget,
                                                    SDL_FPoint& outMidScreen,
                                                    SDL_FPoint& outSideScreen,
                                                    float& outHeadSize,
                                                    Uint8& outAlpha) const {
    if (phase_ != Phase::Attack && phase_ != Phase::Complete) {
        return false;
    }

    const float t = std::clamp(waveElapsed_ / std::max(0.0001f, waveDuration_), 0.0f, 1.0f);

    const SDL_FPoint targetFeet = camera.worldToScreen(targetX_, targetY_, targetZ_);
    const float targetScale = std::max(0.0001f, camera.getPerspectiveScale(targetX_, targetY_, targetZ_));
    const float targetDepth = std::max(1.0f, camera.getDepth(targetX_, targetY_, targetZ_));
    const float focusedCharacterHeight = kCharacterSpriteHeight * targetScale * kFocusedCharacterScale;
    const float chestY = targetFeet.y - focusedCharacterHeight * kChestAnchorRatio;

    const float targetHeadSize = std::clamp(
        focusedCharacterHeight * kHeadSizeRatio,
        kMinHeadSize,
        kMaxHeadSize
    );
    const float sideLaneOffset = std::max(targetHeadSize * 1.35f, focusedCharacterHeight * kSideLaneRatio);

    const float entryDepth = targetDepth * kHeadEntryDepthRatio;
    const float startDepth = targetDepth * kHeadStartDepthRatio;
    const float endDepth = targetDepth * kHeadPassDepthRatio;
    const float currentDepth = interp(startDepth, endDepth, t);

    if (currentDepth < entryDepth) {
        return false;
    }

    const float fadeInEndDepth = entryDepth + targetDepth * kHeadFadeInDepthRatio;
    float alphaT = 1.0f;
    if (currentDepth < fadeInEndDepth) {
        const float fadeInT = std::clamp(
            (currentDepth - entryDepth) / std::max(0.001f, fadeInEndDepth - entryDepth),
            0.0f,
            1.0f
        );
        alphaT = easing::easeOutCubic(fadeInT);
    }

    const float sizeDepth = std::max(entryDepth, currentDepth);
    outHeadSize = std::clamp(
        targetHeadSize * (targetDepth / sizeDepth),
        targetHeadSize,
        kHeadMaxRenderSize
    );
    outBehindTarget = currentDepth > targetDepth;
    outMidScreen = SDL_FPoint{targetFeet.x, chestY};
    outSideScreen = SDL_FPoint{
        targetFeet.x + (sideFromLeft_ ? -sideLaneOffset : sideLaneOffset),
        chestY
    };
    outAlpha = static_cast<Uint8>(std::clamp(alphaT, 0.0f, 1.0f) * 255.0f);
    return outAlpha > 0;
}

void JiafeiBossPresentation::renderHeads(SDL_Renderer* renderer, const Camera3D& camera, bool behindTargetPass) {
    bool behindTarget = false;
    SDL_FPoint midScreen{};
    SDL_FPoint sideScreen{};
    float headSize = 0.0f;
    Uint8 alpha = 255;
    if (!computeHeadRenderState(camera, behindTarget, midScreen, sideScreen, headSize, alpha) ||
        behindTarget != behindTargetPass) {
        return;
    }

    SDL_Rect midRect = {
        static_cast<int>(midScreen.x - headSize * 0.5f),
        static_cast<int>(midScreen.y - headSize * 0.5f),
        static_cast<int>(headSize),
        static_cast<int>(headSize)
    };
    SDL_Rect sideRect = {
        static_cast<int>(sideScreen.x - headSize * 0.5f),
        static_cast<int>(sideScreen.y - headSize * 0.5f),
        static_cast<int>(headSize),
        static_cast<int>(headSize)
    };

    auto drawHead = [&](SDL_Texture* texture, const SDL_Rect& rect, SDL_Color fallbackColor) {
        if (texture != nullptr) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(texture, alpha);
            SDL_RenderCopy(renderer, texture, nullptr, &rect);
            SDL_SetTextureAlphaMod(texture, 255);
            return;
        }

        SDL_SetRenderDrawColor(renderer, fallbackColor.r, fallbackColor.g, fallbackColor.b, alpha);
        SDL_RenderFillRect(renderer, &rect);
    };

    drawHead(middleHeadTexture_, midRect, SDL_Color{255, 180, 180, 255});
    drawHead(sideHeadTexture_, sideRect, SDL_Color{255, 100, 100, 255});
}

} // namespace battle
