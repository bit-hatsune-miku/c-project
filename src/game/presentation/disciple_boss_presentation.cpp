#include "disciple_boss_presentation.h"

#include "../core/easing.h"

#include <algorithm>
#include <cmath>

namespace battle {
namespace {

constexpr float kIntroDurationSeconds = 0.40f;
constexpr float kStrikeDurationSeconds = 0.34f;
constexpr float kStrikeHitTimeSeconds = 0.14f;
constexpr float kOutroDurationSeconds = 0.24f;
constexpr float kTargetAnchorZ = -120.0f;

float alphaForIntro(float elapsed) {
    return easing::easeOutCubic(
        easing::clamp01(elapsed / std::max(0.001f, kIntroDurationSeconds))
    );
}

float alphaForOutro(float elapsed) {
    return 1.0f - easing::easeInCubic(
        easing::clamp01(elapsed / std::max(0.001f, kOutroDurationSeconds))
    );
}

} // namespace

DiscipleBossPresentation::DiscipleBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = kIntroDurationSeconds + kOutroDurationSeconds;
}

void DiscipleBossPresentation::start() {
    elapsedTime_ = 0.0f;
    phaseElapsed_ = 0.0f;
    currentStrikeIndex_ = 0;
    currentTargetIndex_ = -1;
    hitQueued_ = false;
    pendingHitEvents_ = 0;
    pendingHitTargetIndices_.clear();
    phase_ = targetSequence_.empty() ? Phase::Outro : Phase::Intro;
}

void DiscipleBossPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
    phaseElapsed_ += deltaTime;

    switch (phase_) {
        case Phase::Intro:
            if (phaseElapsed_ >= kIntroDurationSeconds) {
                beginStrike();
            }
            break;
        case Phase::Strike:
            if (!hitQueued_ && phaseElapsed_ >= kStrikeHitTimeSeconds && currentTargetIndex_ >= 0) {
                hitQueued_ = true;
                ++pendingHitEvents_;
                pendingHitTargetIndices_.push_back(currentTargetIndex_);
            }
            if (phaseElapsed_ >= kStrikeDurationSeconds) {
                ++currentStrikeIndex_;
                if (currentStrikeIndex_ >= static_cast<int>(targetSequence_.size())) {
                    phase_ = Phase::Outro;
                    phaseElapsed_ = 0.0f;
                    currentTargetIndex_ = -1;
                } else {
                    beginStrike();
                }
            }
            break;
        case Phase::Outro:
            if (phaseElapsed_ >= kOutroDurationSeconds) {
                phase_ = Phase::Complete;
            }
            break;
        case Phase::Complete:
            break;
    }
}

void DiscipleBossPresentation::render(SDL_Renderer* renderer,
                                      int screenW,
                                      int screenH,
                                      const Camera3D& camera) {
    if (renderer == nullptr) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    float overlayAlpha = 1.0f;
    if (phase_ == Phase::Intro) {
        overlayAlpha = alphaForIntro(phaseElapsed_);
    } else if (phase_ == Phase::Outro) {
        overlayAlpha = alphaForOutro(phaseElapsed_);
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(255.0f * overlayAlpha));
    SDL_FRect fullRect{0.0f, 0.0f, static_cast<float>(screenW), static_cast<float>(screenH)};
    SDL_RenderFillRectF(renderer, &fullRect);

    if (phase_ != Phase::Strike || currentTargetIndex_ < 0) {
        return;
    }

    const float strikeT = easing::clamp01(phaseElapsed_ / std::max(0.001f, kStrikeDurationSeconds));
    const float flash = 1.0f - std::abs((strikeT * 2.0f) - 1.0f);
    const SDL_FPoint targetScreen = camera.worldToScreen(targetX_, targetY_, targetZ_ + kTargetAnchorZ);
    if (targetScreen.x <= -500000.0f || targetScreen.y <= -500000.0f) {
        return;
    }

    const float slashLength = 210.0f + (flash * 70.0f);
    const float slashOffset = 48.0f;
    SDL_SetRenderDrawColor(renderer, 255, 64, 64, static_cast<Uint8>(220.0f * flash));
    SDL_RenderDrawLineF(renderer,
                        targetScreen.x - slashLength * 0.55f,
                        targetScreen.y - slashOffset,
                        targetScreen.x + slashLength * 0.45f,
                        targetScreen.y + slashOffset);
    SDL_RenderDrawLineF(renderer,
                        targetScreen.x - slashLength * 0.45f,
                        targetScreen.y + slashOffset,
                        targetScreen.x + slashLength * 0.55f,
                        targetScreen.y - slashOffset);

    SDL_SetRenderDrawColor(renderer, 255, 220, 220, static_cast<Uint8>(180.0f * flash));
    SDL_FRect impactRect{
        targetScreen.x - 56.0f - (flash * 14.0f),
        targetScreen.y - 56.0f - (flash * 14.0f),
        112.0f + (flash * 28.0f),
        112.0f + (flash * 28.0f)
    };
    SDL_RenderDrawRectF(renderer, &impactRect);
}

bool DiscipleBossPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

int DiscipleBossPresentation::consumeHitEvents() {
    const int hitEvents = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hitEvents;
}

std::vector<int> DiscipleBossPresentation::consumeHitTargetIndices() {
    std::vector<int> targets;
    targets.swap(pendingHitTargetIndices_);
    return targets;
}

void DiscipleBossPresentation::setResolvedTargetIndices(const std::vector<int>& targetIndices) {
    targetSequence_ = targetIndices;
    totalDuration_ =
        kIntroDurationSeconds +
        (static_cast<float>(targetSequence_.size()) * kStrikeDurationSeconds) +
        kOutroDurationSeconds;
}

int DiscipleBossPresentation::getFocusedPartyIndex() const {
    return currentTargetIndex_;
}

void DiscipleBossPresentation::setTargetWorldPosition(float x, float y, float z) {
    targetX_ = x;
    targetY_ = y;
    targetZ_ = z;
}

bool DiscipleBossPresentation::shouldBlackoutWorld() const {
    return true;
}

bool DiscipleBossPresentation::shouldRenderAboveHud() const {
    return false;
}

bool DiscipleBossPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

void DiscipleBossPresentation::beginStrike() {
    phase_ = Phase::Strike;
    phaseElapsed_ = 0.0f;
    hitQueued_ = false;
    currentTargetIndex_ =
        currentStrikeIndex_ >= 0 && currentStrikeIndex_ < static_cast<int>(targetSequence_.size())
            ? targetSequence_[static_cast<size_t>(currentStrikeIndex_)]
            : -1;
}

} // namespace battle
