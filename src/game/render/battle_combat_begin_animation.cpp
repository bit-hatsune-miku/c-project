#include "battle_combat_begin_animation.h"

#include <algorithm>

#include "../core/easing.h"

namespace battle::render {
namespace {

constexpr float kEnterDurationSeconds = 0.72f;
constexpr float kHoldDurationSeconds = 1.35f;
constexpr float kExitDurationSeconds = 0.48f;

constexpr float kSpriteHeightRatio = 0.74f;
constexpr float kLeftAnchorRatio = 0.24f;
constexpr float kRightAnchorRatio = 0.76f;
constexpr int kOffscreenMargin = 56;

} // namespace

void BattleCombatBeginAnimation::reset(SDL_Texture* mikuSprite, SDL_Texture* bossSprite) {
    mikuSprite_ = mikuSprite;
    bossSprite_ = bossSprite;
    phase_ = Phase::Enter;
    phaseTimer_ = 0.0f;
}

void BattleCombatBeginAnimation::update(float deltaSeconds) {
    if (phase_ == Phase::Complete) {
        return;
    }

    phaseTimer_ += deltaSeconds;

    const float phaseDuration =
        (phase_ == Phase::Enter) ? kEnterDurationSeconds :
        (phase_ == Phase::Hold) ? kHoldDurationSeconds :
        (phase_ == Phase::Exit) ? kExitDurationSeconds :
        0.0f;

    if (phaseDuration <= 0.0f || phaseTimer_ < phaseDuration) {
        return;
    }

    phaseTimer_ = 0.0f;
    switch (phase_) {
        case Phase::Enter: phase_ = Phase::Hold; break;
        case Phase::Hold: phase_ = Phase::Exit; break;
        case Phase::Exit: phase_ = Phase::Complete; break;
        case Phase::Complete: break;
    }
}

void BattleCombatBeginAnimation::render(SDL_Renderer* renderer, int screenWidth, int screenHeight) const {
    SDL_SetRenderDrawColor(renderer, 8, 8, 12, 255);
    SDL_RenderClear(renderer);

    const int targetHeight = static_cast<int>(screenHeight * kSpriteHeightRatio);

    int mikuTexW = 0;
    int mikuTexH = 0;
    if (mikuSprite_ != nullptr) {
        SDL_QueryTexture(mikuSprite_, nullptr, nullptr, &mikuTexW, &mikuTexH);
    }

    int bossTexW = 0;
    int bossTexH = 0;
    if (bossSprite_ != nullptr) {
        SDL_QueryTexture(bossSprite_, nullptr, nullptr, &bossTexW, &bossTexH);
    }

    const int mikuWidth = (mikuTexH > 0) ? (mikuTexW * targetHeight / mikuTexH) : static_cast<int>(targetHeight * 0.65f);
    const int bossWidth = (bossTexH > 0) ? (bossTexW * targetHeight / bossTexH) : static_cast<int>(targetHeight * 0.65f);

    const int targetY = (screenHeight - targetHeight) / 2;

    const int mikuBaseX = static_cast<int>(screenWidth * kLeftAnchorRatio) - (mikuWidth / 2);
    const int bossBaseX = static_cast<int>(screenWidth * kRightAnchorRatio) - (bossWidth / 2);

    int mikuX = mikuBaseX;
    int bossX = bossBaseX;
    int mikuY = targetY;
    int bossY = targetY;

    if (phase_ == Phase::Enter) {
        const float t = phaseT(kEnterDurationSeconds);
        const float eased = easing::easeOutBounce(t);
        mikuY = static_cast<int>(easing::lerp(static_cast<float>(-targetHeight - kOffscreenMargin),
                                              static_cast<float>(targetY), eased));
        bossY = static_cast<int>(easing::lerp(static_cast<float>(screenHeight + kOffscreenMargin),
                                              static_cast<float>(targetY), eased));
    } else if (phase_ == Phase::Exit) {
        const float t = phaseT(kExitDurationSeconds);
        const float eased = easing::easeOutCubic(t);
        mikuX = static_cast<int>(easing::lerp(static_cast<float>(mikuBaseX),
                                              static_cast<float>(-mikuWidth - kOffscreenMargin),
                                              eased));
        bossX = static_cast<int>(easing::lerp(static_cast<float>(bossBaseX),
                                              static_cast<float>(screenWidth + kOffscreenMargin),
                                              eased));
    }

    const SDL_Rect mikuDst{mikuX, mikuY, mikuWidth, targetHeight};
    const SDL_Rect bossDst{bossX, bossY, bossWidth, targetHeight};

    drawSpriteOrFallback(renderer, mikuSprite_, mikuDst, SDL_Color{93, 202, 224, 255});
    drawSpriteOrFallback(renderer, bossSprite_, bossDst, SDL_Color{208, 106, 162, 255});
}

bool BattleCombatBeginAnimation::isActive() const {
    return phase_ != Phase::Complete;
}

void BattleCombatBeginAnimation::drawSpriteOrFallback(SDL_Renderer* renderer,
                                                      SDL_Texture* texture,
                                                      const SDL_Rect& dst,
                                                      const SDL_Color& fallbackColor) {
    if (texture != nullptr) {
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        return;
    }

    SDL_SetRenderDrawColor(renderer, fallbackColor.r, fallbackColor.g, fallbackColor.b, fallbackColor.a);
    SDL_RenderFillRect(renderer, &dst);
}

float BattleCombatBeginAnimation::phaseT(float duration) const {
    if (duration <= 0.0f) {
        return 1.0f;
    }
    return easing::clamp01(phaseTimer_ / duration);
}

} // namespace battle::render
