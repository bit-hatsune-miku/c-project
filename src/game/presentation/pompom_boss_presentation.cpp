#include "pompom_boss_presentation.h"

#include "../core/easing.h"
#include "../render/battle_asset_loading.h"

#include <algorithm>
#include <cmath>

namespace battle {

namespace {

constexpr float kIntroDurationSeconds = 0.80f;
constexpr float kCompleteDurationSeconds = 0.85f;
constexpr float kBaseDamageMultiplier = 1.0f;
constexpr float kAdditionalHitDamageMultiplier = 0.5f;
constexpr float kFirstCollisionDamageMultiplier = 1.5f;
constexpr float kOkaySignalScore = 0.30f;
constexpr float kGoodSignalScore = 0.70f;
constexpr float kOkayHitDamageMultiplier = 0.65f;
constexpr float kGoodHitDamageMultiplier = 0.28f;

constexpr float kJumpDurationSeconds = 0.46f;
constexpr float kJumpPeakOffsetPixels = 150.0f;

constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kRunnerDrawHeight = 170.0f;
constexpr int kObstacleDrawWidth = 140;
constexpr int kObstacleDrawHeight = 140;

constexpr float kGroundLineWidthInset = 160.0f;

constexpr float kFocusOffsetX = -70.0f;
constexpr float kFocusOffsetY = 325.0f;
constexpr float kFocusOffsetZ = -150.0f;
constexpr float kFocusYawDegrees = 180.0f;
constexpr float kFocusPitchDegrees = -2.2f;
constexpr float kFocusFocalLength = 43000.0f;
constexpr float kFocusPosLerp = 0.34f;
constexpr float kFocusRotLerp = 0.30f;
constexpr float kFocusFocalLerp = 0.24f;

} // namespace

PomPomBossPresentation::PomPomBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) : casterX_(casterWorldX)
  , casterY_(casterWorldY)
  , casterZ_(casterWorldZ)
  , targetX_(targetWorldX)
  , targetY_(targetWorldY)
  , targetZ_(targetWorldZ) {}

PomPomBossPresentation::~PomPomBossPresentation() {
    if (obstacleTexture_ != nullptr) {
        SDL_DestroyTexture(obstacleTexture_);
    }
    if (targetSpriteTexture_ != nullptr) {
        SDL_DestroyTexture(targetSpriteTexture_);
    }
}

void PomPomBossPresentation::start() {
    phase_ = Phase::Intro;
    survivalElapsed_ = 0.0f;
    spawnTimer_ = baseSpawnInterval_ * 0.5f;
    jumpActive_ = false;
    jumpElapsedSeconds_ = 0.0f;
    jumpOffsetPixels_ = 0.0f;
    obstacles_.clear();
    pendingFeedbackEvents_.clear();
    pendingHitDamageMultipliers_.clear();
    damageMultiplier_ = kBaseDamageMultiplier;
    landedHitCount_ = 0;
    runnerCenterScreenX_ = -1.0f;
    lastScreenWidth_ = 0;
}

void PomPomBossPresentation::setTuningProfile(const PresentationTuningProfile& profile) {
    if (const auto it = profile.floatParams.find("survivalDurationSeconds"); it != profile.floatParams.end()) {
        survivalDurationSeconds_ = std::max(1.5f, it->second);
    }
    if (const auto it = profile.floatParams.find("baseObstacleSpeed"); it != profile.floatParams.end()) {
        baseObstacleSpeed_ = std::max(150.0f, it->second);
    }
    if (const auto it = profile.floatParams.find("baseSpawnInterval"); it != profile.floatParams.end()) {
        baseSpawnInterval_ = std::max(0.18f, it->second);
    }
    if (const auto it = profile.floatParams.find("judgementProgress"); it != profile.floatParams.end()) {
        judgementProgress_ = std::clamp(it->second, 0.35f, 0.95f);
    }
    if (const auto it = profile.floatParams.find("okayClearancePixels"); it != profile.floatParams.end()) {
        okayClearancePixels_ = std::max(0.0f, it->second);
    }
    if (const auto it = profile.floatParams.find("goodClearancePixels"); it != profile.floatParams.end()) {
        goodClearancePixels_ = std::max(okayClearancePixels_, it->second);
    }
    if (const auto it = profile.floatParams.find("perfectClearancePixels"); it != profile.floatParams.end()) {
        perfectClearancePixels_ = std::max(goodClearancePixels_, it->second);
    }
}

void PomPomBossPresentation::update(float deltaTime) {
    updateJump(deltaTime);

    if (phase_ == Phase::Intro) {
        survivalElapsed_ += deltaTime;
        if (survivalElapsed_ >= kIntroDurationSeconds) {
            phase_ = Phase::Attack;
            survivalElapsed_ = 0.0f;
        }
        return;
    }

    if (phase_ == Phase::Attack) {
        survivalElapsed_ += deltaTime;

        if (survivalElapsed_ < survivalDurationSeconds_) {
            spawnTimer_ += deltaTime;
            if (spawnTimer_ >= baseSpawnInterval_) {
                spawnTimer_ = 0.0f;
                spawnObstacle();
            }
        }

        const float survivalRatio = easing::clamp01(survivalElapsed_ / std::max(0.001f, survivalDurationSeconds_));
        const float speedMultiplier = 1.0f + survivalRatio * 0.55f;

        for (auto it = obstacles_.begin(); it != obstacles_.end();) {
            const float speed = baseObstacleSpeed_ * speedMultiplier * deltaTime;
            const float progressDelta = speed / 1420.0f;
            it->progress += progressDelta;

            float judgementProgress = judgementProgress_;
            if (lastScreenWidth_ > 0 && runnerCenterScreenX_ >= 0.0f) {
                judgementProgress = std::clamp(
                    (static_cast<float>(lastScreenWidth_) + kObstacleDrawWidth * 0.5f - runnerCenterScreenX_) /
                        (static_cast<float>(lastScreenWidth_) + static_cast<float>(kObstacleDrawWidth)),
                    0.05f,
                    0.98f
                );
            }

            if (!it->resolved && it->progress >= judgementProgress) {
                resolveObstacleJudgement(*it);
            }

            if (it->progress > 1.0f) {
                if (!it->resolved) {
                    resolveObstacleJudgement(*it);
                }
                it = obstacles_.erase(it);
                continue;
            }

            ++it;
        }

        if (survivalElapsed_ >= survivalDurationSeconds_ && obstacles_.empty()) {
            phase_ = Phase::Complete;
            survivalElapsed_ = 0.0f;
            if (landedHitCount_ == 0) {
                damageMultiplier_ = kBaseDamageMultiplier;
                queueDamageHit(kBaseDamageMultiplier);
            }
        }
        return;
    }

    if (phase_ == Phase::Complete) {
        survivalElapsed_ += deltaTime;
    }
}

void PomPomBossPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    ensureTexturesLoaded(renderer);

    if (phase_ != Phase::Attack) {
        return;
    }

    const SDL_FPoint targetFeet = camera.worldToScreen(targetX_, targetY_, targetZ_);
    const float runnerDrawHeight = kRunnerDrawHeight;

    float runnerDrawWidth = runnerDrawHeight * (static_cast<float>(kSuggestedBaseSpriteWidth) /
                                                static_cast<float>(kSuggestedBaseSpriteHeight));
    int spriteTexW = 0;
    int spriteTexH = 0;
    if (targetSpriteTexture_ != nullptr && SDL_QueryTexture(targetSpriteTexture_, nullptr, nullptr, &spriteTexW, &spriteTexH) == 0) {
        const int frameWidth =
            (spriteTexW > kSuggestedBaseSpriteWidth && spriteTexW % kSuggestedBaseSpriteWidth == 0)
                ? kSuggestedBaseSpriteWidth
                : spriteTexW;
        if (frameWidth > 0 && spriteTexH > 0) {
            runnerDrawWidth = runnerDrawHeight * (static_cast<float>(frameWidth) / static_cast<float>(spriteTexH));
        }
    }

    const float groundY = std::min(static_cast<float>(screenH) - 120.0f, targetFeet.y + 18.0f);
    const float runnerCenterX = targetFeet.x;
    const float runnerTopY = groundY - runnerDrawHeight - jumpOffsetPixels_;
    runnerCenterScreenX_ = runnerCenterX;
    lastScreenWidth_ = screenW;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 210);
    SDL_Rect groundLineRect{
        static_cast<int>(kGroundLineWidthInset),
        static_cast<int>(groundY + 2.0f),
        std::max(0, screenW - static_cast<int>(kGroundLineWidthInset * 2.0f)),
        4
    };
    SDL_RenderFillRect(renderer, &groundLineRect);

    if (targetSpriteTexture_ != nullptr) {
        int texW = 0;
        int texH = 0;
        if (SDL_QueryTexture(targetSpriteTexture_, nullptr, nullptr, &texW, &texH) == 0 && texW > 0 && texH > 0) {
            const int frameWidth =
                (texW > kSuggestedBaseSpriteWidth && texW % kSuggestedBaseSpriteWidth == 0)
                    ? kSuggestedBaseSpriteWidth
                    : texW;
            const SDL_Rect srcRect{0, 0, frameWidth, texH};
            const SDL_Rect dstRect{
                static_cast<int>(std::lround(runnerCenterX - runnerDrawWidth * 0.5f)),
                static_cast<int>(std::lround(runnerTopY)),
                std::max(8, static_cast<int>(std::lround(runnerDrawWidth))),
                std::max(8, static_cast<int>(std::lround(runnerDrawHeight)))
            };
            SDL_SetTextureBlendMode(targetSpriteTexture_, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(targetSpriteTexture_, 255, 255, 255);
            SDL_SetTextureAlphaMod(targetSpriteTexture_, 255);
            SDL_RenderCopy(renderer, targetSpriteTexture_, &srcRect, &dstRect);
        }
    }

    for (const Obstacle& obstacle : obstacles_) {
        const float obstacleCenterX =
            static_cast<float>(screenW + kObstacleDrawWidth / 2) -
            obstacle.progress * static_cast<float>(screenW + kObstacleDrawWidth);
        const SDL_Rect obstacleRect{
            static_cast<int>(std::lround(obstacleCenterX - kObstacleDrawWidth * 0.5f)),
            static_cast<int>(std::lround(groundY - static_cast<float>(kObstacleDrawHeight))),
            kObstacleDrawWidth,
            kObstacleDrawHeight
        };

        if (obstacleTexture_ != nullptr) {
            SDL_SetTextureBlendMode(obstacleTexture_, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(obstacleTexture_, 255, 255, 255);
            SDL_SetTextureAlphaMod(obstacleTexture_, 255);
            SDL_RenderCopy(renderer, obstacleTexture_, nullptr, &obstacleRect);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 215, 72, 255);
            SDL_RenderFillRect(renderer, &obstacleRect);
        }
    }
}

void PomPomBossPresentation::renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool PomPomBossPresentation::isComplete() const {
    return phase_ == Phase::Complete &&
           survivalElapsed_ >= kCompleteDurationSeconds &&
           pendingHitDamageMultipliers_.empty();
}

void PomPomBossPresentation::onSpacePressed() {
    if (phase_ != Phase::Attack || jumpActive_) {
        return;
    }

    jumpActive_ = true;
    jumpElapsedSeconds_ = 0.0f;
}

bool PomPomBossPresentation::shouldHideNonCasterCharacters() const {
    return true;
}

bool PomPomBossPresentation::shouldRenderCasterEntity() const {
    return true;
}

bool PomPomBossPresentation::shouldRenderBossEntity() const {
    return true;
}

bool PomPomBossPresentation::shouldRenderFocusedTargetEntity() const {
    return phase_ != Phase::Attack;
}

bool PomPomBossPresentation::shouldRenderAboveHud() const {
    return false;
}

bool PomPomBossPresentation::overridesCamera() const {
    return true;
}

void PomPomBossPresentation::applyCameraState(Camera3D& camera) const {
    const float focusX = (casterX_ + targetX_) * 0.5f;
    const float focusY = (casterY_ + targetY_) * 0.5f;
    const float focusZ = (casterZ_ + targetZ_) * 0.5f;

    const float desiredX = focusX + kFocusOffsetX;
    const float desiredY = focusY + kFocusOffsetY;
    const float desiredZ = focusZ + kFocusOffsetZ;

    camera.posX += (desiredX - camera.posX) * kFocusPosLerp;
    camera.posY += (desiredY - camera.posY) * kFocusPosLerp;
    camera.posZ += (desiredZ - camera.posZ) * kFocusPosLerp;

    camera.yawDegrees += (kFocusYawDegrees - camera.yawDegrees) * kFocusRotLerp;
    camera.pitchDegrees += (kFocusPitchDegrees - camera.pitchDegrees) * kFocusRotLerp;
    camera.focalLength += (kFocusFocalLength - camera.focalLength) * kFocusFocalLerp;
}

int PomPomBossPresentation::getFocusedPartyIndex() const {
    return focusedPartyIndex_;
}

void PomPomBossPresentation::setTargetPartyIndex(int index) {
    focusedPartyIndex_ = index;
}

void PomPomBossPresentation::setTargetWorldPosition(float x, float y, float z) {
    targetX_ = x;
    targetY_ = y;
    targetZ_ = z;
}

void PomPomBossPresentation::setPartyAssetNames(const std::vector<std::string>& assetNames) {
    if (focusedPartyIndex_ >= 0 && focusedPartyIndex_ < static_cast<int>(assetNames.size())) {
        targetAssetName_ = assetNames[static_cast<size_t>(focusedPartyIndex_)];
    } else {
        targetAssetName_.clear();
    }
}

float PomPomBossPresentation::getInputMultiplier() const {
    return damageMultiplier_;
}

PresentationFeedbackSignal PomPomBossPresentation::getFeedbackSignal() const {
    return {};
}

std::vector<PresentationFeedbackEvent> PomPomBossPresentation::consumeFeedbackEvents() {
    auto events = pendingFeedbackEvents_;
    pendingFeedbackEvents_.clear();
    return events;
}

float PomPomBossPresentation::consumeHitDamageMultiplier() {
    if (pendingHitDamageMultipliers_.empty()) {
        return damageMultiplier_;
    }

    const float multiplier = pendingHitDamageMultipliers_.front();
    pendingHitDamageMultipliers_.erase(pendingHitDamageMultipliers_.begin());
    return multiplier;
}

int PomPomBossPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int PomPomBossPresentation::consumeHitEvents() {
    return pendingHitDamageMultipliers_.empty() ? 0 : 1;
}

int PomPomBossPresentation::getDamageLabelHitCount() const {
    return 1;
}

std::string PomPomBossPresentation::getInputResultText() const {
    return "";
}

std::optional<SplashArtConfig> PomPomBossPresentation::getSplashConfig(SDL_Texture* sprite) const {
    (void)sprite;
    return std::nullopt;
}

void PomPomBossPresentation::updateJump(float deltaTime) {
    if (!jumpActive_) {
        jumpOffsetPixels_ = 0.0f;
        return;
    }

    jumpElapsedSeconds_ += deltaTime;
    const float clampedTime = std::min(jumpElapsedSeconds_, kJumpDurationSeconds);
    const float ascentDuration = kJumpDurationSeconds * 0.42f;

    if (clampedTime <= ascentDuration) {
        const float t = easing::clamp01(clampedTime / std::max(0.001f, ascentDuration));
        jumpOffsetPixels_ = easing::easeOutCubic(t) * kJumpPeakOffsetPixels;
    } else {
        const float t = easing::clamp01((clampedTime - ascentDuration) /
                                        std::max(0.001f, kJumpDurationSeconds - ascentDuration));
        jumpOffsetPixels_ = (1.0f - easing::easeInCubic(t)) * kJumpPeakOffsetPixels;
    }

    if (jumpElapsedSeconds_ >= kJumpDurationSeconds) {
        jumpActive_ = false;
        jumpElapsedSeconds_ = 0.0f;
        jumpOffsetPixels_ = 0.0f;
    }
}

void PomPomBossPresentation::ensureTexturesLoaded(SDL_Renderer* renderer) {
    if (texturesLoaded_) {
        return;
    }

    if (const auto loaded = render::tryLoadTextureFromPath(renderer, "assets/combat/presentations/pompom/pompom.png");
        loaded.has_value()) {
        obstacleTexture_ = *loaded;
    }

    if (!targetAssetName_.empty()) {
        if (const auto loaded = render::tryLoadCombatSpriteTexture(renderer, targetAssetName_); loaded.has_value()) {
            targetSpriteTexture_ = *loaded;
        }
    }

    texturesLoaded_ = true;
}

void PomPomBossPresentation::spawnObstacle() {
    obstacles_.push_back(Obstacle{});
}

void PomPomBossPresentation::queueDamageHit(float multiplier) {
    damageMultiplier_ = std::max(0.0f, multiplier);
    pendingHitDamageMultipliers_.push_back(damageMultiplier_);
}

void PomPomBossPresentation::resolveObstacleJudgement(Obstacle& obstacle) {
    if (obstacle.resolved) {
        return;
    }
    obstacle.resolved = true;

    PresentationFeedbackEvent event;
    event.comboEligible = true;

    if (jumpOffsetPixels_ >= perfectClearancePixels_) {
        event.signal = PresentationFeedbackSignal::forcedPerfect();
        pendingFeedbackEvents_.push_back(event);
        return;
    }

    if (jumpOffsetPixels_ >= goodClearancePixels_) {
        event.signal = PresentationFeedbackSignal::graded(kGoodSignalScore);
        pendingFeedbackEvents_.push_back(event);
        queueDamageHit(kGoodHitDamageMultiplier);
        return;
    }

    if (jumpOffsetPixels_ >= okayClearancePixels_) {
        event.signal = PresentationFeedbackSignal::graded(kOkaySignalScore);
        pendingFeedbackEvents_.push_back(event);
        queueDamageHit(kOkayHitDamageMultiplier);
        return;
    }

    ++landedHitCount_;
    damageMultiplier_ =
        kBaseDamageMultiplier +
        static_cast<float>(landedHitCount_) * kAdditionalHitDamageMultiplier;
    queueDamageHit(landedHitCount_ == 1 ? kFirstCollisionDamageMultiplier : kAdditionalHitDamageMultiplier);
    event.signal = PresentationFeedbackSignal::binary(false);
    pendingFeedbackEvents_.push_back(event);
}

} // namespace battle
