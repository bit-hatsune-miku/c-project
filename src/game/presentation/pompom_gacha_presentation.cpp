#include "pompom_gacha_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr int kFrameCount = 161;
constexpr int kFrameWindowRadius = 6;
constexpr float kIntroDurationSeconds = 16.192f;
constexpr float kWhiteFlashDurationSeconds = 0.12f;
constexpr float kPullRevealIntervalSeconds = 0.18f;
constexpr float kFinalHoldDurationSeconds = 0.9f;

constexpr SDL_Color kDimBackdropColor{6, 11, 24, 178};
constexpr SDL_Color kRowGlowColor{255, 247, 205, 56};

std::string resolveFramePath(int frameNumber) {
    char relativePath[256];
    std::snprintf(
        relativePath,
        sizeof(relativePath),
        "assets/combat/presentations/pompom/output_%04d.png",
        frameNumber
    );
    return platform::path::resolvePath(relativePath);
}

std::string resolveVoicePath() {
    return platform::path::resolveAudioPath("assets/combat/voices/pompom/ability.opus")
        .value_or(platform::path::resolvePath("assets/combat/voices/pompom/ability.opus"));
}

std::string resolveStarPath() {
    return platform::path::resolvePath("assets/combat/presentations/lyooPlotTwist/star.png");
}

int clampedRarity(int rarity) {
    return std::clamp(rarity, 3, 5);
}

PresentationFeedbackSignal signalForRarity(int rarity) {
    switch (rarity) {
        case 5:
            return PresentationFeedbackSignal::forcedPerfect();
        case 4:
            return PresentationFeedbackSignal::graded(0.70f);
        case 3:
        default:
            return PresentationFeedbackSignal::graded(0.30f);
    }
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

PomPomGachaPresentation::PomPomGachaPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ,
    Variant variant
) : variant_(variant) {
    (void)casterWorldX;
    (void)casterWorldY;
    (void)casterWorldZ;
    (void)targetWorldX;
    (void)targetWorldY;
    (void)targetWorldZ;
    recalculateTotalDuration();
}

PomPomGachaPresentation::~PomPomGachaPresentation() {
    releaseFrames();
    if (starTexture_ != nullptr) {
        SDL_DestroyTexture(starTexture_);
        starTexture_ = nullptr;
    }
}

void PomPomGachaPresentation::start() {
    elapsedTime_ = 0.0f;
    revealedPullCount_ = 0;
    pendingFeedbackEvents_.clear();
    pendingAudioCommands_.clear();
    pendingAudioCommands_.push_back(PresentationAudioCommand{
        PresentationAudioCommandType::PlayVoiceOneShot,
        resolveVoicePath(),
        1.0f
    });
}

void PomPomGachaPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
}

void PomPomGachaPresentation::preload(SDL_Renderer* renderer) {
    ensureFrameLoaded(renderer, 0);
}

void PomPomGachaPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)camera;
    if (renderer == nullptr) {
        return;
    }

    if (elapsedTime_ < kIntroDurationSeconds) {
        const float frameDurationSeconds = kIntroDurationSeconds / static_cast<float>(kFrameCount);
        const int frameIndex = std::clamp(
            static_cast<int>(elapsedTime_ / std::max(0.001f, frameDurationSeconds)),
            0,
            kFrameCount - 1
        );
        maintainFrameWindow(renderer, frameIndex);
        const FrameSlot& frame = frames_[static_cast<size_t>(frameIndex)];
        if (frame.texture != nullptr) {
            int textureWidth = 0;
            int textureHeight = 0;
            SDL_QueryTexture(frame.texture, nullptr, nullptr, &textureWidth, &textureHeight);
            SDL_FRect destination{
                0.0f,
                0.0f,
                static_cast<float>(screenW),
                static_cast<float>(screenH)
            };
            if (textureWidth > 0 && textureHeight > 0) {
                const float aspect = static_cast<float>(textureWidth) / static_cast<float>(textureHeight);
                float drawWidth = static_cast<float>(screenW);
                float drawHeight = drawWidth / aspect;
                if (drawHeight < static_cast<float>(screenH)) {
                    drawHeight = static_cast<float>(screenH);
                    drawWidth = drawHeight * aspect;
                }
                destination = SDL_FRect{
                    (static_cast<float>(screenW) - drawWidth) * 0.5f,
                    (static_cast<float>(screenH) - drawHeight) * 0.5f,
                    drawWidth,
                    drawHeight
                };
            }
            SDL_RenderCopyF(renderer, frame.texture, nullptr, &destination);
        }
        return;
    }

    const float revealElapsed = elapsedTime_ - kIntroDurationSeconds;
    const float flashProgress = clamp01(revealElapsed / kWhiteFlashDurationSeconds);
    const float revealStartTime = kWhiteFlashDurationSeconds;
    const int targetRevealCount = revealElapsed < revealStartTime
        ? 0
        : std::min(
            static_cast<int>(resolvedRolls_.size()),
            1 + static_cast<int>((revealElapsed - revealStartTime) / kPullRevealIntervalSeconds)
        );
    while (revealedPullCount_ < targetRevealCount) {
        revealNextPull(renderer);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(
        renderer,
        kDimBackdropColor.r,
        kDimBackdropColor.g,
        kDimBackdropColor.b,
        kDimBackdropColor.a
    );
    SDL_Rect backdrop{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &backdrop);

    ensureStarLoaded(renderer);
    for (int index = 0; index < revealedPullCount_ && index < static_cast<int>(resolvedRolls_.size()); ++index) {
        const float rowRevealTime = revealElapsed - revealStartTime - (static_cast<float>(index) * kPullRevealIntervalSeconds);
        const float rowRevealProgress = clamp01(rowRevealTime / 0.14f);
        drawStarRow(
            renderer,
            screenW,
            screenH,
            index,
            clampedRarity(resolvedRolls_[static_cast<size_t>(index)]),
            rowRevealProgress
        );
    }

    if (flashProgress < 1.0f) {
        const float whiteAlpha = 1.0f - easing::easeOutCubic(flashProgress);
        const Uint8 overlayAlpha = static_cast<Uint8>(std::lround(whiteAlpha * 255.0f));
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, overlayAlpha);
        SDL_Rect overlay{0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &overlay);
    }
}

bool PomPomGachaPresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

void PomPomGachaPresentation::setResolvedRolls(const std::vector<int>& rolls) {
    resolvedRolls_ = rolls;
    recalculateTotalDuration();
}

std::vector<PresentationFeedbackEvent> PomPomGachaPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

std::vector<PresentationAudioCommand> PomPomGachaPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

PresentationFeedbackSignal PomPomGachaPresentation::getFeedbackSignal() const {
    return {};
}

bool PomPomGachaPresentation::shouldRenderAboveHud() const {
    return false;
}

void PomPomGachaPresentation::recalculateTotalDuration() {
    const int revealCount = std::max(0, static_cast<int>(resolvedRolls_.size()));
    const float revealSpan = revealCount > 0
        ? static_cast<float>(revealCount - 1) * kPullRevealIntervalSeconds
        : 0.0f;
    totalDuration_ = kIntroDurationSeconds + kWhiteFlashDurationSeconds + revealSpan + kFinalHoldDurationSeconds;
}

void PomPomGachaPresentation::releaseFrames() {
    for (FrameSlot& slot : frames_) {
        if (slot.texture != nullptr) {
            SDL_DestroyTexture(slot.texture);
            slot.texture = nullptr;
        }
        slot.attemptedLoad = false;
    }
}

void PomPomGachaPresentation::maintainFrameWindow(SDL_Renderer* renderer, int frameIndex) {
    if (frames_.empty()) {
        frames_.resize(static_cast<size_t>(kFrameCount));
    }

    const int minFrame = std::max(0, frameIndex - kFrameWindowRadius);
    const int maxFrame = std::min(kFrameCount - 1, frameIndex + kFrameWindowRadius);
    for (int index = minFrame; index <= maxFrame; ++index) {
        ensureFrameLoaded(renderer, index);
    }

    for (int index = 0; index < kFrameCount; ++index) {
        if (index >= minFrame && index <= maxFrame) {
            continue;
        }
        FrameSlot& slot = frames_[static_cast<size_t>(index)];
        if (slot.texture != nullptr) {
            SDL_DestroyTexture(slot.texture);
            slot.texture = nullptr;
        }
        slot.attemptedLoad = false;
    }
}

void PomPomGachaPresentation::ensureFrameLoaded(SDL_Renderer* renderer, int frameIndex) {
    if (renderer == nullptr || frameIndex < 0 || frameIndex >= kFrameCount) {
        return;
    }
    if (frames_.empty()) {
        frames_.resize(static_cast<size_t>(kFrameCount));
    }

    FrameSlot& slot = frames_[static_cast<size_t>(frameIndex)];
    if (slot.texture != nullptr || slot.attemptedLoad) {
        return;
    }
    slot.attemptedLoad = true;

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolveFramePath(frameIndex + 1).c_str());
    if (surface == nullptr) {
        return;
    }
    slot.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

void PomPomGachaPresentation::ensureStarLoaded(SDL_Renderer* renderer) {
    if (renderer == nullptr || starTexture_ != nullptr) {
        return;
    }
#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolveStarPath().c_str());
    if (surface == nullptr) {
        return;
    }
    starTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

void PomPomGachaPresentation::revealNextPull(SDL_Renderer* renderer) {
    (void)renderer;
    if (revealedPullCount_ >= static_cast<int>(resolvedRolls_.size())) {
        return;
    }

    const int rarity = clampedRarity(resolvedRolls_[static_cast<size_t>(revealedPullCount_)]);
    ++revealedPullCount_;

    PresentationFeedbackEvent event;
    event.signal = signalForRarity(rarity);
    event.comboEligible = true;
    pendingFeedbackEvents_.push_back(std::move(event));
}

void PomPomGachaPresentation::drawStarRow(SDL_Renderer* renderer,
                                          int screenW,
                                          int screenH,
                                          int rowIndex,
                                          int rarity,
                                          float revealProgress) const {
    if (renderer == nullptr || starTexture_ == nullptr || rarity <= 0) {
        return;
    }

    const float t = easing::easeOutBack(clamp01(revealProgress));
    const float alpha = clamp01(revealProgress);
    const int totalRows = std::max(1, static_cast<int>(resolvedRolls_.size()));
    const float availableHeight = std::max(220.0f, static_cast<float>(screenH) - 180.0f);
    const float rowGap = std::clamp(availableHeight / static_cast<float>(totalRows), 18.0f, 42.0f);
    const float starSize = std::clamp(rowGap * 0.78f, 16.0f, 34.0f);
    const float starGap = starSize * 0.86f;
    const float listHeight = rowGap * static_cast<float>(std::max(0, totalRows - 1));
    const float baseX = static_cast<float>(screenW) * 0.22f;
    const float baseY = (static_cast<float>(screenH) * 0.5f) - (listHeight * 0.5f);
    const float rowY = baseY + (static_cast<float>(rowIndex) * rowGap);
    const float width = (static_cast<float>(rarity - 1) * starGap) + starSize;
    const float rowX = baseX;

    const SDL_FRect glowRect{
        rowX - starSize * 0.42f,
        rowY - starSize * 0.22f,
        width + starSize * 0.92f,
        starSize * 1.12f
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(
        renderer,
        kRowGlowColor.r,
        kRowGlowColor.g,
        kRowGlowColor.b,
        static_cast<Uint8>(std::lround(alpha * static_cast<float>(kRowGlowColor.a)))
    );
    SDL_RenderFillRectF(renderer, &glowRect);

    SDL_SetTextureBlendMode(starTexture_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(starTexture_, static_cast<Uint8>(std::lround(alpha * 255.0f)));
    for (int index = 0; index < rarity; ++index) {
        const float lift = (1.0f - t) * 18.0f;
        const float scale = 0.82f + (0.18f * t);
        const float drawSize = starSize * scale;
        const SDL_FRect dstRect{
            rowX + (static_cast<float>(index) * starGap),
            rowY + lift,
            drawSize,
            drawSize
        };
        SDL_RenderCopyF(renderer, starTexture_, nullptr, &dstRect);
    }
    SDL_SetTextureAlphaMod(starTexture_, 255);
}

} // namespace battle
