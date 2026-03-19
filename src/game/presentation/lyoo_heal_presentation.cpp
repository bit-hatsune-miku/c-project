#include "lyoo_heal_presentation.h"

#include "../core/easing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kInputDurationSeconds = 1.5f;
constexpr float kOutroDurationSeconds = 0.90f;
constexpr float kHealApplyTimeSeconds = 0.32f;
constexpr float kPressFrameHoldSeconds = 0.09f;
constexpr float kHealBonusPerValidKey = 0.015f;
constexpr float kMaxHealMultiplier = 1.40f;

float lerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

SDL_Rect centeredRect(int centerX, int centerY, int width, int height) {
    return SDL_Rect{
        centerX - width / 2,
        centerY - height / 2,
        width,
        height
    };
}

} // namespace

LyooHealPresentation::LyooHealPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ,
    Variant variant
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ)
    , variant_(variant)
    , rng_(std::random_device{}()) {
    totalDuration_ = (variant_ == Variant::Ultimate)
        ? kOutroDurationSeconds
        : (kInputDurationSeconds + kOutroDurationSeconds);
}

LyooHealPresentation::~LyooHealPresentation() {
    releaseTextures();
}

void LyooHealPresentation::start() {
    elapsedTime_ = 0.0f;
    inputElapsed_ = 0.0f;
    outroElapsed_ = 0.0f;
    ambientTime_ = 0.0f;
    phase_ = (variant_ == Variant::Ultimate) ? Phase::Outro : Phase::Input;
    currentFrameIndex_ = 0;
    nextPressFrameIndex_ = 1;
    pressFrameHoldRemaining_ = 0.0f;
    validPressCount_ = 0;
    pendingAbilityAudioCues_ = (variant_ == Variant::Skill) ? 1 : 0;
    pendingHitEvents_ = 0;
    healEventTriggered_ = false;
    recentKeys_.clear();
    hearts_.clear();
    inputWindow_.startTime = 0.0f;
    inputWindow_.endTime = (variant_ == Variant::Skill) ? kInputDurationSeconds : 0.0f;
    inputWindow_.active = variant_ == Variant::Skill;

    inputCamera_ = Camera3D{};
    inputCamera_.posX = -980.0f;
    inputCamera_.posY = 760.0f;
    inputCamera_.posZ = -125.0f;
    inputCamera_.pitchDegrees = -1.3f;
    inputCamera_.yawDegrees = 180.0f;
    inputCamera_.focalLength = 47000.0f;

    outroCamera_ = Camera3D{};
    outroCamera_.posX = -987.0f;
    outroCamera_.posY = 801.0f;
    outroCamera_.posZ = -129.0f;
    outroCamera_.pitchDegrees = -1.0f;
    outroCamera_.yawDegrees = 180.0f;
    outroCamera_.focalLength = 44885.0f;
}

void LyooHealPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
    ambientTime_ += deltaTime;

    if (pressFrameHoldRemaining_ > 0.0f) {
        pressFrameHoldRemaining_ = std::max(0.0f, pressFrameHoldRemaining_ - deltaTime);
        if (pressFrameHoldRemaining_ <= 0.0f && phase_ == Phase::Input) {
            currentFrameIndex_ = 0;
        }
    }

    updateHearts(deltaTime);

    if (phase_ == Phase::Input) {
        inputElapsed_ += deltaTime;
        if (inputElapsed_ >= kInputDurationSeconds) {
            phase_ = Phase::Outro;
            outroElapsed_ = 0.0f;
            inputWindow_.active = false;
            currentFrameIndex_ = 0;
        }
        return;
    }

    if (phase_ == Phase::Outro) {
        outroElapsed_ += deltaTime;
        if (!healEventTriggered_ && outroElapsed_ >= kHealApplyTimeSeconds) {
            healEventTriggered_ = true;
            ++pendingHitEvents_;
        }
        if (outroElapsed_ >= kOutroDurationSeconds && hearts_.empty()) {
            phase_ = Phase::Complete;
        }
        return;
    }
}

void LyooHealPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)camera;
    if (variant_ == Variant::Ultimate) {
        return;
    }
    ensureTexturesLoaded(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    renderUiSprite(renderer, screenW, screenH);
    renderHearts(renderer, screenW, screenH);
}

bool LyooHealPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

void LyooHealPresentation::onKeyPressed(SDL_Keycode key) {
    if (phase_ != Phase::Input || !inputWindow_.active) {
        return;
    }

    const bool isRecent = std::find(recentKeys_.begin(), recentKeys_.end(), key) != recentKeys_.end();

    recentKeys_.push_back(key);
    while (recentKeys_.size() > 5) {
        recentKeys_.pop_front();
    }

    if (isRecent) {
        return;
    }

    ++validPressCount_;
    currentFrameIndex_ = nextPressFrameIndex_;
    nextPressFrameIndex_ = (nextPressFrameIndex_ == 1) ? 2 : 1;
    pressFrameHoldRemaining_ = kPressFrameHoldSeconds;

    spawnHeartsForValidPress();
}

float LyooHealPresentation::getInputMultiplier() const {
    if (variant_ == Variant::Ultimate) {
        return 1.0f;
    }
    return std::min(kMaxHealMultiplier, 1.0f + (static_cast<float>(validPressCount_) * kHealBonusPerValidKey));
}

std::string LyooHealPresentation::getInputResultText() const {
    if (variant_ == Variant::Ultimate) {
        return {};
    }
    const int bonusPercent = std::max(0, static_cast<int>(std::lround((getInputMultiplier() - 1.0f) * 100.0f)));
    char buffer[160];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "Valid keys: %d, healing increased by %d%%",
        validPressCount_,
        bonusPercent
    );
    return buffer;
}

int LyooHealPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int LyooHealPresentation::consumeHitEvents() {
    const int events = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return events;
}

bool LyooHealPresentation::overridesCamera() const {
    return true;
}

void LyooHealPresentation::applyCameraState(Camera3D& camera) const {
    if (phase_ == Phase::Input) {
        camera = inputCamera_;
        camera.posX += std::sin(ambientTime_ * 0.55f) * 8.0f;
        camera.posZ += std::sin(ambientTime_ * 0.85f) * 3.0f;
        camera.pitchDegrees += std::sin(ambientTime_ * 0.70f) * 0.18f;
        camera.focalLength += std::sin(ambientTime_ * 0.65f) * 280.0f;
        return;
    }

    if (phase_ == Phase::Outro) {
        const float t = easing::clamp01(outroElapsed_ / std::max(0.001f, kOutroDurationSeconds));
        const float eased = easing::easeOutQuint(t);

        camera.posX = lerpF(inputCamera_.posX, outroCamera_.posX, eased);
        camera.posY = lerpF(inputCamera_.posY, outroCamera_.posY, eased);
        camera.posZ = lerpF(inputCamera_.posZ, outroCamera_.posZ, eased);
        camera.pitchDegrees = lerpF(inputCamera_.pitchDegrees, outroCamera_.pitchDegrees, eased);
        camera.yawDegrees = lerpF(inputCamera_.yawDegrees, outroCamera_.yawDegrees, eased);
        camera.focalLength = lerpF(inputCamera_.focalLength, outroCamera_.focalLength, eased);
        return;
    }

    camera = outroCamera_;
}

bool LyooHealPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool LyooHealPresentation::shouldRenderCasterEntity() const {
    if (variant_ == Variant::Ultimate) {
        return true;
    }
    return phase_ != Phase::Input;
}

bool LyooHealPresentation::shouldRenderBossEntity() const {
    return false;
}

bool LyooHealPresentation::shouldRenderAboveHud() const {
    return false;
}

std::string LyooHealPresentation::resolveAssetPath(const std::string& relativePath) {
    const std::array<std::string, 8> candidates = {
        relativePath,
        "../" + relativePath,
        "../../" + relativePath,
        "assets/combat/presentation/lyoo/" + std::filesystem::path(relativePath).filename().string(),
        "../assets/combat/presentation/lyoo/" + std::filesystem::path(relativePath).filename().string(),
        "../../assets/combat/presentation/lyoo/" + std::filesystem::path(relativePath).filename().string(),
        "assets/combat/presentation/lyoo/hearts/" + std::filesystem::path(relativePath).filename().string(),
        "../../assets/combat/presentation/lyoo/hearts/" + std::filesystem::path(relativePath).filename().string()
    };

    for (const std::string& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return relativePath;
}

void LyooHealPresentation::ensureTexturesLoaded(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_) {
        return;
    }
    attemptedTextureLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
        char framePath[160];
        std::snprintf(
            framePath,
            sizeof(framePath),
            "assets/combat/presentations/lyoo/frame%04d.png",
            frameIndex
        );
        const std::string resolved = resolveAssetPath(framePath);
        if (!std::filesystem::exists(resolved)) {
            continue;
        }

        SDL_Surface* surface = IMG_Load(resolved.c_str());
        if (surface == nullptr) {
            continue;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (texture != nullptr) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            frameTextures_[static_cast<size_t>(frameIndex)] = texture;
        }
    }

    {
        const std::string resolved = resolveAssetPath("assets/combat/presentations/lyoo/hearts/heart.png");
        if (std::filesystem::exists(resolved)) {
            SDL_Surface* surface = IMG_Load(resolved.c_str());
            if (surface != nullptr) {
                heartTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
                if (heartTexture_ != nullptr) {
                    SDL_SetTextureBlendMode(heartTexture_, SDL_BLENDMODE_BLEND);
                }
            }
        }
    }
#else
    (void)renderer;
#endif
}

void LyooHealPresentation::releaseTextures() {
    for (SDL_Texture*& texture : frameTextures_) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }

    if (heartTexture_ != nullptr) {
        SDL_DestroyTexture(heartTexture_);
        heartTexture_ = nullptr;
    }
}

void LyooHealPresentation::updateHearts(float deltaTime) {
    for (HeartParticle& heart : hearts_) {
        if (!heart.active) {
            continue;
        }

        heart.elapsed += deltaTime;
        const float totalLifetime = heart.riseDuration + heart.holdDuration + heart.fadeDuration;
        if (heart.elapsed >= totalLifetime) {
            heart.active = false;
        }
    }

    hearts_.erase(
        std::remove_if(hearts_.begin(), hearts_.end(), [](const HeartParticle& heart) {
            return !heart.active;
        }),
        hearts_.end()
    );
}

void LyooHealPresentation::spawnHeartsForValidPress() {
    const int growthTier = std::min(3, std::max(0, validPressCount_ / 8));
    std::uniform_int_distribution<int> countDist(1 + growthTier, 3 + growthTier);
    std::uniform_real_distribution<float> xDist(0.03f, 0.97f);
    std::uniform_real_distribution<float> yDist(0.34f, 0.63f);
    std::uniform_real_distribution<float> sizeDist(20.0f, 38.0f);
    std::uniform_real_distribution<float> riseDist(0.40f, 0.60f);
    std::uniform_real_distribution<float> holdDist(0.06f, 0.12f);
    std::uniform_real_distribution<float> fadeDist(0.14f, 0.22f);

    const int spawnCount = countDist(rng_);
    hearts_.reserve(hearts_.size() + static_cast<size_t>(spawnCount));

    for (int index = 0; index < spawnCount; ++index) {
        HeartParticle heart;
        heart.xNorm = xDist(rng_);
        heart.startYNorm = 1.08f;
        heart.targetYNorm = yDist(rng_);
        heart.sizePx = sizeDist(rng_);
        heart.riseDuration = riseDist(rng_);
        heart.holdDuration = holdDist(rng_);
        heart.fadeDuration = fadeDist(rng_);
        hearts_.push_back(heart);
    }
}

void LyooHealPresentation::renderHearts(SDL_Renderer* renderer, int screenW, int screenH) const {
    for (const HeartParticle& heart : hearts_) {
        if (!heart.active) {
            continue;
        }

        const float riseEnd = heart.riseDuration;
        const float holdEnd = riseEnd + heart.holdDuration;
        const float totalEnd = holdEnd + heart.fadeDuration;

        float yNorm = heart.targetYNorm;
        float alpha = 255.0f;
        if (heart.elapsed < riseEnd) {
            const float t = easing::clamp01(heart.elapsed / std::max(0.001f, heart.riseDuration));
            yNorm = lerpF(heart.startYNorm, heart.targetYNorm, easing::easeOutCubic(t));
        } else if (heart.elapsed > holdEnd) {
            const float fadeT = easing::clamp01((heart.elapsed - holdEnd) / std::max(0.001f, heart.fadeDuration));
            yNorm = heart.targetYNorm - fadeT * 0.02f;
            alpha = 255.0f * (1.0f - fadeT);
        }

        if (heart.elapsed >= totalEnd || alpha <= 0.0f) {
            continue;
        }

        const float x = heart.xNorm * static_cast<float>(screenW);
        const float y = yNorm * static_cast<float>(screenH);
        SDL_FRect dst{
            x - heart.sizePx * 0.5f,
            y - heart.sizePx * 0.5f,
            heart.sizePx,
            heart.sizePx
        };

        if (heartTexture_ != nullptr) {
            SDL_SetTextureAlphaMod(heartTexture_, static_cast<Uint8>(alpha));
            SDL_RenderCopyF(renderer, heartTexture_, nullptr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, 232, 70, 84, static_cast<Uint8>(alpha));
            SDL_RenderFillRectF(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 255, 214, 220, static_cast<Uint8>(alpha));
            SDL_RenderDrawRectF(renderer, &dst);
        }
    }
}

void LyooHealPresentation::renderUiSprite(SDL_Renderer* renderer, int screenW, int screenH) const {
    if (phase_ != Phase::Input) {
        return;
    }

    const Uint8 alpha = currentOverlayAlpha();
    if (alpha == 0) {
        return;
    }

    const int centerX = static_cast<int>(screenW * 0.60f);
    const int centerY = static_cast<int>(screenH * 0.70f);
    const int width = static_cast<int>(screenW * 0.27f);
    const int height = static_cast<int>(screenH * 0.54f);

    SDL_Texture* frameTexture = frameTextures_[static_cast<size_t>(std::clamp(currentFrameIndex_, 0, 2))];
    if (frameTexture != nullptr) {
        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(frameTexture, nullptr, nullptr, &texW, &texH);

        const float aspect = (texH > 0) ? (static_cast<float>(texW) / static_cast<float>(texH)) : (16.0f / 9.0f);
        float drawH = static_cast<float>(screenH) * 0.90f;
        float drawW = drawH * aspect;
        const float maxWidth = static_cast<float>(screenW) * 0.95f;
        if (drawW > maxWidth) {
            drawW = maxWidth;
            drawH = drawW / std::max(0.01f, aspect);
        }

        SDL_Rect dstRect{
            static_cast<int>((static_cast<float>(screenW) - drawW) * 0.5f),
            screenH - static_cast<int>(drawH),
            static_cast<int>(drawW),
            static_cast<int>(drawH)
        };
        SDL_SetTextureAlphaMod(frameTexture, alpha);
        SDL_RenderCopy(renderer, frameTexture, nullptr, &dstRect);
        return;
    }

    const Uint8 fillR = currentFrameIndex_ == 1 ? 255 : (currentFrameIndex_ == 2 ? 248 : 236);
    const Uint8 fillG = currentFrameIndex_ == 1 ? 180 : (currentFrameIndex_ == 2 ? 154 : 196);
    const Uint8 fillB = currentFrameIndex_ == 1 ? 196 : (currentFrameIndex_ == 2 ? 216 : 224);

    SDL_Rect bodyRect = centeredRect(centerX, centerY, width, height);
    SDL_SetRenderDrawColor(renderer, fillR, fillG, fillB, static_cast<Uint8>(alpha * 0.95f));
    SDL_RenderFillRect(renderer, &bodyRect);

    SDL_SetRenderDrawColor(renderer, 255, 252, 255, alpha);
    SDL_RenderDrawRect(renderer, &bodyRect);

    SDL_Rect faceRect = centeredRect(centerX, centerY - height / 6, static_cast<int>(width * 0.56f), static_cast<int>(height * 0.32f));
    SDL_SetRenderDrawColor(renderer, 255, 243, 248, alpha);
    SDL_RenderFillRect(renderer, &faceRect);

    SDL_SetRenderDrawColor(renderer, 255, 120, 165, alpha);
    SDL_Rect leftEye{
        faceRect.x + faceRect.w / 5,
        faceRect.y + faceRect.h / 3,
        static_cast<int>(width * 0.07f),
        static_cast<int>(height * 0.035f)
    };
    SDL_Rect rightEye = leftEye;
    rightEye.x = faceRect.x + faceRect.w - rightEye.w - faceRect.w / 5;
    SDL_RenderFillRect(renderer, &leftEye);
    SDL_RenderFillRect(renderer, &rightEye);

    SDL_Rect smile{
        faceRect.x + faceRect.w / 3,
        faceRect.y + static_cast<int>(faceRect.h * 0.62f),
        faceRect.w / 3,
        static_cast<int>(height * 0.03f)
    };
    SDL_RenderFillRect(renderer, &smile);

    SDL_SetRenderDrawColor(renderer, 255, 214, 228, alpha);
    SDL_Rect accentRect = centeredRect(centerX, centerY + height / 5, static_cast<int>(width * 0.72f), static_cast<int>(height * 0.24f));
    SDL_RenderFillRect(renderer, &accentRect);

    SDL_SetRenderDrawColor(renderer, 255, 94, 140, alpha);
    if (currentFrameIndex_ == 1) {
        SDL_Rect pulse = centeredRect(centerX - width / 6, centerY - height / 4, static_cast<int>(width * 0.20f), static_cast<int>(height * 0.08f));
        SDL_RenderFillRect(renderer, &pulse);
    } else if (currentFrameIndex_ == 2) {
        SDL_Rect pulse = centeredRect(centerX + width / 6, centerY - height / 4, static_cast<int>(width * 0.20f), static_cast<int>(height * 0.08f));
        SDL_RenderFillRect(renderer, &pulse);
    } else {
        SDL_Rect pulse = centeredRect(centerX, centerY - height / 4, static_cast<int>(width * 0.14f), static_cast<int>(height * 0.06f));
        SDL_RenderFillRect(renderer, &pulse);
    }
}

Uint8 LyooHealPresentation::currentOverlayAlpha() const {
    if (phase_ != Phase::Input) {
        return 0;
    }

    return 255;
}

} // namespace battle
