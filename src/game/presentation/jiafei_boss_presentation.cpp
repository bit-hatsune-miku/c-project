#include "jiafei_boss_presentation.h"

#include "../core/easing.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

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
    chooseNextSafeLane();
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
    chooseNextSafeLane();
    inputReceived_ = false;
    inputCorrect_ = false;
    canReceiveInput_ = false;
    damageMultiplier_ = 1.0f;
    dodgeElapsed_ = 0.0f;
    dodgeDirection_ = 0;
    waveImpactResolved_ = false;
    pendingAbilityAudioCues_ = 0;
    pendingHitEvents_ = 0;
    pendingFeedbackEvents_.clear();
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
        chooseNextSafeLane();
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
    return phase_ == Phase::Complete;
}

void JiafeiBossPresentation::setTuningProfile(const PresentationTuningProfile& profile) {
    if (const auto it = profile.intParams.find("totalWaves"); it != profile.intParams.end()) {
        totalWaves_ = std::max(1, it->second);
    }
    if (const auto it = profile.intParams.find("patternMode"); it != profile.intParams.end()) {
        switch (it->second) {
            case 1:
                patternMode_ = PatternMode::RandomSideSafe;
                break;
            case 2:
                patternMode_ = PatternMode::RandomAnySafe;
                break;
            case 0:
            default:
                patternMode_ = PatternMode::AlternateSideSafe;
                break;
        }
    }
    if (const auto it = profile.floatParams.find("waveDurationSeconds"); it != profile.floatParams.end()) {
        waveDuration_ = std::max(0.15f, it->second);
    }
    if (const auto it = profile.floatParams.find("dodgeTravelSeconds"); it != profile.floatParams.end()) {
        dodgeTravelSeconds_ = std::max(0.01f, it->second);
    }
    if (const auto it = profile.floatParams.find("dodgeReturnSeconds"); it != profile.floatParams.end()) {
        dodgeReturnSeconds_ = std::max(0.01f, it->second);
    }
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
        }
    }

    if (std::filesystem::exists(sidePath)) {
        SDL_Surface* surf = IMG_Load(sidePath.c_str());
        if (surf) {
            sideHeadTexture_ = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
        }
    }
#endif

    texturesLoaded_ = true;
}

float JiafeiBossPresentation::interp(float a, float b, float t) const {
    return a + (b - a) * t;
}

float JiafeiBossPresentation::waveImpactTimeSeconds() const {
    return waveDuration_ * kHeadImpactRatio;
}

bool JiafeiBossPresentation::isCorrectDodgeDirection(int direction) const {
    return safeLane_ != 0 && direction == safeLane_;
}

void JiafeiBossPresentation::chooseNextSafeLane() {
    switch (patternMode_) {
    case PatternMode::AlternateSideSafe:
        safeLane_ = sideFromLeft_ ? -1 : 1;
        sideFromLeft_ = !sideFromLeft_;
        return;
    case PatternMode::RandomSideSafe:
        safeLane_ = (std::rand() % 2 == 0) ? -1 : 1;
        return;
    case PatternMode::RandomAnySafe: {
        constexpr int kSafeLanes[3] = {-1, 0, 1};
        safeLane_ = kSafeLanes[std::rand() % 3];
        return;
    }
    }
}

int JiafeiBossPresentation::resolvePlayerLane() const {
    const float dodgeCoverage = std::clamp(
        currentDodgeOffsetWorld() / std::max(0.001f, kDodgeDistanceWorld),
        0.0f,
        1.0f
    );
    if (dodgeCoverage < kRequiredDodgeCoverageRatio) {
        return 0;
    }
    return dodgeDirection_;
}

int JiafeiBossPresentation::resolveWaveHeadHits() const {
    const int playerLane = resolvePlayerLane();
    if (safeLane_ == 0) {
        return playerLane == 0 ? 0 : 1;
    }

    int landedHits = 0;
    if (playerLane == 0) {
        ++landedHits;
    }
    if (playerLane != safeLane_) {
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
        if (dodgeElapsed_ >= dodgeTravelSeconds_) {
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
        if (dodgeElapsed_ >= dodgeReturnSeconds_) {
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
        const float t = easing::clamp01(dodgeElapsed_ / std::max(0.001f, dodgeTravelSeconds_));
        return kDodgeDistanceWorld * easing::easeOutQuint(t);
    }
    case DodgeMotionState::Hold:
        return kDodgeDistanceWorld;
    case DodgeMotionState::Return: {
        const float t = easing::clamp01(dodgeElapsed_ / std::max(0.001f, dodgeReturnSeconds_));
        return kDodgeDistanceWorld * (1.0f - easing::easeOutQuint(t));
    }
    }

    return 0.0f;
}

bool JiafeiBossPresentation::computeHeadRenderState(const Camera3D& camera,
                                                    bool& outBehindTarget,
                                                    SDL_FPoint& outLeftScreen,
                                                    SDL_FPoint& outMidScreen,
                                                    SDL_FPoint& outRightScreen,
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
    outLeftScreen = SDL_FPoint{targetFeet.x - sideLaneOffset, chestY};
    outMidScreen = SDL_FPoint{targetFeet.x, chestY};
    outRightScreen = SDL_FPoint{targetFeet.x + sideLaneOffset, chestY};
    outAlpha = static_cast<Uint8>(std::clamp(alphaT, 0.0f, 1.0f) * 255.0f);
    return outAlpha > 0;
}

void JiafeiBossPresentation::renderHeads(SDL_Renderer* renderer, const Camera3D& camera, bool behindTargetPass) {
    bool behindTarget = false;
    SDL_FPoint leftScreen{};
    SDL_FPoint midScreen{};
    SDL_FPoint rightScreen{};
    float headSize = 0.0f;
    Uint8 alpha = 255;
    if (!computeHeadRenderState(camera, behindTarget, leftScreen, midScreen, rightScreen, headSize, alpha) ||
        behindTarget != behindTargetPass) {
        return;
    }

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

    auto makeRect = [&](const SDL_FPoint& point) {
        return SDL_Rect{
            static_cast<int>(point.x - headSize * 0.5f),
            static_cast<int>(point.y - headSize * 0.5f),
            static_cast<int>(headSize),
            static_cast<int>(headSize)
        };
    };

    const bool attackLeft = safeLane_ != -1;
    const bool attackMid = safeLane_ != 0;
    const bool attackRight = safeLane_ != 1;
    if (attackLeft) {
        drawHead(sideHeadTexture_, makeRect(leftScreen), SDL_Color{255, 100, 100, 255});
    }
    if (attackMid) {
        drawHead(middleHeadTexture_, makeRect(midScreen), SDL_Color{255, 180, 180, 255});
    }
    if (attackRight) {
        drawHead(sideHeadTexture_, makeRect(rightScreen), SDL_Color{255, 100, 100, 255});
    }
}

} // namespace battle
