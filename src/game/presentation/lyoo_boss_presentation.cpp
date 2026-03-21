#include "lyoo_boss_presentation.h"

#include "../core/easing.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kLowFps = 16.0f;
constexpr float kLoopFpsBoost = 1.24f;
constexpr float kMaxLoopFps = 72.0f;
constexpr int kLoopCyclesBeforeSwitch = 24;
constexpr float kHoldFrame13Seconds = 0.34f;
constexpr float kZoomDurationSeconds = 1.55f;
constexpr float kPostZoomHoldSeconds = 0.25f;
constexpr float kPulseTravelSeconds = 0.52f;
constexpr float kParryToleranceSeconds = 0.20f;
constexpr float kMaxDamageReduction = 0.50f;

float lerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace

LyooBossPresentation::LyooBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(1.0f) // Force boss sprite above floor regardless of input
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
}

LyooBossPresentation::~LyooBossPresentation() {
    releaseFrames();
}

void LyooBossPresentation::start() {
    elapsedTime_ = 0.0f;
    phase_ = Phase::IntroFrames;
    uiFrameIndex_ = 0;
    uiFrameTimer_ = 0.0f;
    currentLoopFps_ = kLowFps;
    loopCycles_ = 0;
    holdFrame13Timer_ = 0.0f;

    zoomInitialized_ = false;
    zoomElapsed_ = 0.0f;
    postZoomHold_ = 0.0f;
    pulsesSpawned_ = 0;
    pulsesResolved_ = 0;
    pulses_.clear();
    pendingHitDamageMultipliers_.clear();
    parryAccuracy_.fill(0.0f);
    parryRegistered_.fill(false);
    abilityAudioTriggered_ = false;
    pendingAbilityAudioCues_ = 0;
    pendingHitEvents_ = 0;

    frontCamera_ = Camera3D{};
    gameplayCamera_ = Camera3D{};

    // Front-facing camera on Lyoo to support the 2D -> 2.5D illusion handoff.
    frontCamera_.posX = casterX_;
    frontCamera_.posY = casterY_ - 210.0f;
    frontCamera_.posZ = -175.0f;
    frontCamera_.pitchDegrees = -2.5f;
    frontCamera_.yawDegrees = 0.0f;
    frontCamera_.focalLength = 44000.0f;

    // Zoom-out destination keeps the same frontal angle (no yaw/pitch swing)
    // and just opens framing so boss + full lineup are visible.
    gameplayCamera_.posX = casterX_;
    gameplayCamera_.posY = 45.0f;
    gameplayCamera_.posZ = -175.0f;
    gameplayCamera_.pitchDegrees = frontCamera_.pitchDegrees;
    gameplayCamera_.yawDegrees = frontCamera_.yawDegrees;
    gameplayCamera_.focalLength = 32000.0f;
}

void LyooBossPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (phase_ == Phase::IntroFrames || phase_ == Phase::AccelLoop) {
        updateUiFrameStepper(deltaTime);
        return;
    }

    if (phase_ == Phase::HoldFrame13Ui) {
        holdFrame13Timer_ += deltaTime;
        if (holdFrame13Timer_ >= kHoldFrame13Seconds) {
            phase_ = Phase::ZoomOut;
            zoomElapsed_ = 0.0f;
            postZoomHold_ = 0.0f;
        }
        return;
    }

    if (phase_ == Phase::ZoomOut) {
        zoomElapsed_ += deltaTime;

        while (pulsesSpawned_ < static_cast<int>(pulseSpawnTimes_.size()) &&
               (zoomElapsed_ / kZoomDurationSeconds) >= pulseSpawnTimes_[static_cast<size_t>(pulsesSpawned_)]) {
            spawnPulse();
            ++pulsesSpawned_;
        }

        updatePulses(deltaTime);

        while (pulsesResolved_ < static_cast<int>(pulseSpawnTimes_.size()) &&
               zoomElapsed_ >= waveImpactTimeSeconds(static_cast<size_t>(pulsesResolved_))) {
            pendingHitDamageMultipliers_.push_back(waveDamageMultiplier(static_cast<size_t>(pulsesResolved_)));
            ++pendingHitEvents_;
            ++pulsesResolved_;
        }

        if (zoomElapsed_ >= kZoomDurationSeconds) {
            postZoomHold_ += deltaTime;
            if (postZoomHold_ >= kPostZoomHoldSeconds && allPulsesFinished()) {
                phase_ = Phase::Complete;
            }
        }
        return;
    }

    if (phase_ == Phase::Complete) {
        updatePulses(deltaTime);
    }
}

void LyooBossPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    ensureFramesLoaded(renderer);

    if (phase_ == Phase::IntroFrames || phase_ == Phase::AccelLoop || phase_ == Phase::HoldFrame13Ui) {
        renderUiFrame(renderer, screenW, screenH);
        return;
    }

    if (phase_ == Phase::ZoomOut || phase_ == Phase::Complete) {
        if (!zoomInitialized_) {
            enterZoomOutPhase(camera, screenW, screenH);
        }
        renderPulses(renderer, camera);
    }
}

void LyooBossPresentation::renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    ensureFramesLoaded(renderer);

    if (phase_ != Phase::ZoomOut && phase_ != Phase::Complete) {
        return;
    }

    if (!zoomInitialized_) {
        enterZoomOutPhase(camera, screenW, screenH);
    }

    renderWorldBossFrame(renderer, screenW, screenH, camera);
}

bool LyooBossPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

void LyooBossPresentation::onSpacePressed() {
    if (phase_ != Phase::ZoomOut && phase_ != Phase::Complete) {
        return;
    }

    const float pressTime = zoomElapsed_;
    int bestWave = -1;
    float bestDelta = kParryToleranceSeconds + 1.0f;

    for (size_t waveIndex = 0; waveIndex < pulseSpawnTimes_.size(); ++waveIndex) {
        if (parryRegistered_[waveIndex] || static_cast<int>(waveIndex) < pulsesResolved_) {
            continue;
        }

        const float impactTime = waveImpactTimeSeconds(waveIndex);
        if (pressTime > impactTime) {
            continue;
        }

        const float delta = std::fabs(pressTime - impactTime);
        if (delta <= kParryToleranceSeconds && delta < bestDelta) {
            bestDelta = delta;
            bestWave = static_cast<int>(waveIndex);
        }
    }

    if (bestWave < 0) {
        return;
    }

    parryRegistered_[static_cast<size_t>(bestWave)] = true;
    parryAccuracy_[static_cast<size_t>(bestWave)] = std::max(0.0f, 1.0f - (bestDelta / kParryToleranceSeconds));
}

bool LyooBossPresentation::overridesCamera() const {
    return true;
}

void LyooBossPresentation::applyCameraState(Camera3D& camera) const {
    if (phase_ == Phase::IntroFrames || phase_ == Phase::AccelLoop || phase_ == Phase::HoldFrame13Ui) {
        camera = frontCamera_;
        return;
    }

    if (phase_ == Phase::ZoomOut) {
        const float t = easing::clamp01(zoomElapsed_ / std::max(0.001f, kZoomDurationSeconds));
        const float eased = easing::easeOutCubic(t);

        camera.posX = lerpF(frontCamera_.posX, gameplayCamera_.posX, eased);
        camera.posY = lerpF(frontCamera_.posY, gameplayCamera_.posY, eased);
        camera.posZ = lerpF(frontCamera_.posZ, gameplayCamera_.posZ, eased);
        camera.pitchDegrees = lerpF(frontCamera_.pitchDegrees, gameplayCamera_.pitchDegrees, eased);
        camera.yawDegrees = lerpF(frontCamera_.yawDegrees, gameplayCamera_.yawDegrees, eased);
        camera.focalLength = lerpF(frontCamera_.focalLength, gameplayCamera_.focalLength, eased);
        return;
    }

    camera = gameplayCamera_;
}

bool LyooBossPresentation::shouldHideNonCasterCharacters() const {
    return phase_ != Phase::ZoomOut && phase_ != Phase::Complete;
}

bool LyooBossPresentation::shouldRenderCasterEntity() const {
    // Keep the real boss entity hidden for the full presentation.
    // The storyboard uses the frame-based fake boss all the way through the attack.
    return false;
}

bool LyooBossPresentation::shouldRenderAboveHud() const {
    return false;
}

std::string LyooBossPresentation::resolvePath(const std::string& relativePath) {
    const std::array<std::string, 3> candidates = {
        relativePath,
        "../" + relativePath,
        "../../" + relativePath
    };

    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            return p;
        }
    }

    return relativePath;
}

void LyooBossPresentation::ensureFramesLoaded(SDL_Renderer* renderer) {
    if (attemptedLoad_) {
        return;
    }
    attemptedLoad_ = true;

    frames_.assign(14, nullptr);

#ifdef BATTLE_ENABLE_IMAGE
    for (int i = 0; i <= 13; ++i) {
        char frameName[128];
        std::snprintf(frameName, sizeof(frameName), "assets/combat/presentations/lyooBoss/frame%04d.png", i);
        const std::string path = resolvePath(frameName);
        if (!std::filesystem::exists(path)) {
            continue;
        }

        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface == nullptr) {
            continue;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (texture != nullptr) {
            frames_[static_cast<size_t>(i)] = texture;
        }
    }
#else
    (void)renderer;
#endif
}

void LyooBossPresentation::releaseFrames() {
    for (SDL_Texture*& texture : frames_) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }
    frames_.clear();
}

void LyooBossPresentation::updateUiFrameStepper(float deltaTime) {
    const float fps = (phase_ == Phase::IntroFrames) ? kLowFps : currentLoopFps_;
    const float frameDuration = 1.0f / std::max(1.0f, fps);

    uiFrameTimer_ += deltaTime;
    while (uiFrameTimer_ >= frameDuration) {
        uiFrameTimer_ -= frameDuration;

        if (phase_ == Phase::IntroFrames) {
            if (uiFrameIndex_ < 9) {
                ++uiFrameIndex_;
            }
            if (!abilityAudioTriggered_ && uiFrameIndex_ >= 2) {
                abilityAudioTriggered_ = true;
                ++pendingAbilityAudioCues_;
            }
            if (uiFrameIndex_ >= 9) {
                uiFrameIndex_ = 9;
                phase_ = Phase::AccelLoop;
            }
            continue;
        }

        // Accelerating loop: 0009 -> 0012
        ++uiFrameIndex_;
        if (uiFrameIndex_ > 12) {
            uiFrameIndex_ = 9;
            ++loopCycles_;
            currentLoopFps_ = std::min(kMaxLoopFps, currentLoopFps_ * kLoopFpsBoost);
            if (loopCycles_ >= kLoopCyclesBeforeSwitch || currentLoopFps_ >= kMaxLoopFps - 0.1f) {
                phase_ = Phase::HoldFrame13Ui;
                uiFrameIndex_ = 13;
                holdFrame13Timer_ = 0.0f;
                break;
            }
        }
    }
}

void LyooBossPresentation::enterZoomOutPhase(const Camera3D& referenceCamera, int screenW, int screenH) {
    (void)referenceCamera;
    zoomInitialized_ = true;

    // Match perceived size at handoff by deriving world height from desired on-screen height.
    const float depth = std::max(1.0f, frontCamera_.getDepth(casterX_, casterY_, casterZ_));
    const float scale = std::max(0.0001f, frontCamera_.focalLength * 0.01f / depth);
    const float desiredScreenHeight = static_cast<float>(screenH) * 0.88f;
    worldSpriteHeightUnits_ = desiredScreenHeight / scale;
}

void LyooBossPresentation::spawnPulse() {
    static constexpr std::array<float, 4> kTargetX = {-390.0f, -130.0f, 130.0f, 390.0f};

    for (float targetX : kTargetX) {
        RedPulse pulse;
        pulse.startX = casterX_;
        pulse.startY = casterY_;
        pulse.startZ = casterZ_ - 160.0f;
        pulse.targetX = targetX;
        pulse.targetY = targetY_ + 100.0f;
        pulse.targetZ = targetZ_ - 110.0f;
        pulse.duration = 0.52f;
        pulse.elapsed = 0.0f;
        pulse.active = true;
        pulses_.push_back(pulse);
    }
    // Hit event is fired at impact time (waveImpactTimeSeconds) by the resolved loop,
    // not at spawn time, so that damage and audio land when the pulse actually arrives.
}

int LyooBossPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

float LyooBossPresentation::getInputMultiplier() const {
    return 1.0f - (kMaxDamageReduction * averageAccuracy());
}

float LyooBossPresentation::consumeHitDamageMultiplier() {
    if (pendingHitDamageMultipliers_.empty()) {
        return getInputMultiplier();
    }

    const float multiplier = pendingHitDamageMultipliers_.front();
    pendingHitDamageMultipliers_.erase(pendingHitDamageMultipliers_.begin());
    return multiplier;
}

std::string LyooBossPresentation::getInputResultText() const {
    const int accuracyPercent = std::max(0, static_cast<int>(std::lround(averageAccuracy() * 100.0f)));
    const int reductionPercent = std::max(0, static_cast<int>(std::lround((1.0f - getInputMultiplier()) * 100.0f)));
    return "Average Accuracy: " + std::to_string(accuracyPercent) + "%, reduced damage taken by " +
           std::to_string(reductionPercent) + "%";
}

int LyooBossPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

int LyooBossPresentation::getDamageLabelHitCount() const {
    return std::max(1, static_cast<int>(pulseSpawnTimes_.size()));
}

void LyooBossPresentation::updatePulses(float deltaTime) {
    for (RedPulse& pulse : pulses_) {
        if (!pulse.active) {
            continue;
        }
        pulse.elapsed += deltaTime;
        if (pulse.elapsed >= pulse.duration) {
            pulse.active = false;
        }
    }
}

bool LyooBossPresentation::allPulsesFinished() const {
    for (const RedPulse& pulse : pulses_) {
        if (pulse.active) {
            return false;
        }
    }
    return true;
}

float LyooBossPresentation::waveImpactTimeSeconds(size_t waveIndex) const {
    return (pulseSpawnTimes_[waveIndex] * kZoomDurationSeconds) + kPulseTravelSeconds;
}

float LyooBossPresentation::waveDamageMultiplier(size_t waveIndex) const {
    return 1.0f - (kMaxDamageReduction * parryAccuracy_[waveIndex]);
}

float LyooBossPresentation::averageAccuracy() const {
    float sum = 0.0f;
    for (float accuracy : parryAccuracy_) {
        sum += accuracy;
    }
    return sum / static_cast<float>(parryAccuracy_.size());
}

void LyooBossPresentation::renderUiFrame(SDL_Renderer* renderer, int screenW, int screenH) {
    if (uiFrameIndex_ < 0 || uiFrameIndex_ >= static_cast<int>(frames_.size())) {
        return;
    }

    SDL_Texture* texture = frames_[static_cast<size_t>(uiFrameIndex_)];
    if (texture != nullptr) {
        SDL_Rect dst{0, 0, screenW, screenH};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        return;
    }

    // Fallback if textures are unavailable.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 160, 25, 25, 210);
    SDL_Rect full{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &full);
}

void LyooBossPresentation::renderWorldBossFrame(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    float depth = camera.getDepth(casterX_, casterY_, casterZ_);
    if (depth <= 1.0f) {
        return;
    }

    const SDL_FPoint center = camera.worldToScreen(casterX_, casterY_, casterZ_);
    const float scale = camera.getPerspectiveScale(casterX_, casterY_, casterZ_);

    float aspect = 1.0f;
    SDL_Texture* frame13 = (frames_.size() > 13) ? frames_[13] : nullptr;
    if (frame13 != nullptr) {
        int w = 1;
        int h = 1;
        SDL_QueryTexture(frame13, nullptr, nullptr, &w, &h);
        aspect = static_cast<float>(std::max(1, w)) / static_cast<float>(std::max(1, h));
    }

    const float drawH = std::max(24.0f, worldSpriteHeightUnits_ * scale);
    const float drawW = drawH * aspect;

    SDL_FRect dst;
    dst.w = drawW;
    dst.h = drawH;
    dst.x = center.x - drawW * 0.5f;
    dst.y = center.y - drawH;

    if (frame13 != nullptr) {
        SDL_SetTextureAlphaMod(frame13, 255);
        SDL_RenderCopyF(renderer, frame13, nullptr, &dst);
    } else {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 185, 35, 35, 230);
        SDL_RenderFillRectF(renderer, &dst);
        SDL_SetRenderDrawColor(renderer, 255, 210, 210, 255);
        SDL_RenderDrawRectF(renderer, &dst);
    }

    (void)screenW;
    (void)screenH;
}

void LyooBossPresentation::renderPulses(SDL_Renderer* renderer, const Camera3D& camera) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const RedPulse& pulse : pulses_) {
        if (!pulse.active) {
            continue;
        }

        const float t = easing::clamp01(pulse.elapsed / std::max(0.0001f, pulse.duration));
        const float eased = easing::easeOutCubic(t);

        const float x = lerpF(pulse.startX, pulse.targetX, eased);
        const float y = lerpF(pulse.startY, pulse.targetY, eased);
        const float arc = std::sin(t * 3.14159265f) * 130.0f;
        const float z = lerpF(pulse.startZ, pulse.targetZ, eased) - arc;

        if (camera.getDepth(x, y, z) <= 1.0f) {
            continue;
        }

        const SDL_FPoint p = camera.worldToScreen(x, y, z);
        const float radius = std::max(6.0f, camera.getPerspectiveScale(x, y, z) * 22.0f);
        const float alpha = (t > 0.85f) ? (1.0f - (t - 0.85f) / 0.15f) : 1.0f;
        const Uint8 a = static_cast<Uint8>(std::clamp(alpha, 0.0f, 1.0f) * 220.0f);

        SDL_FRect rect{p.x - radius * 0.5f, p.y - radius * 0.5f, radius, radius};
        SDL_SetRenderDrawColor(renderer, 240, 35, 35, a);
        SDL_RenderFillRectF(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 255, 170, 170, a);
        SDL_RenderDrawRectF(renderer, &rect);
    }
}

std::optional<SplashArtConfig> LyooBossPresentation::getSplashConfig(SDL_Texture* sprite) const {
    SplashArtConfig cfg;
    cfg.sprite        = sprite;
    cfg.enterDuration = 0.42f;
    cfg.holdDuration  = 2.05f; // Boss presentations intentionally linger longer.
    cfg.exitDuration  = 0.40f;
    cfg.maxDimAlpha   = 0.68f;
    return cfg;
}

} // namespace battle
