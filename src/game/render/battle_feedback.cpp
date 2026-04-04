#include "battle_feedback.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include "../../platform/path_resolution.h"

namespace battle::render {
namespace {

constexpr float kFeedbackPopupLifetimeSeconds = 0.95f;
constexpr float kFeedbackPopupSlidePixels = 26.0f;
constexpr float kFeedbackPopupBaseYOffset = -14.0f;
constexpr int kFeedbackPopupFontSize = 30;
constexpr float kPopupShakeDurationSeconds = 0.065f;
constexpr float kPopupShakeAmplitudePixels = 2.8f;
constexpr int kSuggestedBaseSpriteHeight = 260;

std::mt19937& popupRng() {
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

#ifdef BATTLE_ENABLE_TTF
TTF_Font* openBestAvailablePopupFont(int ptSize) {
    const std::vector<std::string> candidates = platform::path::preferredLatinFontPaths();
    for (const auto& path : candidates) {
        TTF_Font* font = TTF_OpenFont(path.c_str(), ptSize);
        if (font != nullptr) {
            return font;
        }
    }
    return nullptr;
}
#endif

} // namespace

void BattleFeedbackSystem::reset(const BattleManager& manager) {
    for (FeedbackPopup& popup : popups_) {
        destroyPopupTextures(popup);
    }
    popups_.clear();

    const BattleState& state = manager.getBattleState();
    lastCharacterHp_.assign(state.party.size(), 0);
    for (size_t i = 0; i < state.party.size(); ++i) {
        lastCharacterHp_[i] = manager.getCharacterCurrentHp(static_cast<int>(i));
    }
    lastBossHp_ = manager.getBossCurrentHp();
    suppressBossNextDamagePopup_ = false;
    suppressBossNextHealingPopup_ = false;

    bossShakeState_ = DamageShakeState{};
    characterShakeStates_.assign(state.party.size(), DamageShakeState{});
    suppressCharacterNextDamagePopup_.assign(state.party.size(), false);
    suppressCharacterNextHealingPopup_.assign(state.party.size(), false);

#ifdef BATTLE_ENABLE_TTF
    if (font_ == nullptr) {
        font_ = openBestAvailablePopupFont(kFeedbackPopupFontSize);
    }
#endif
}

void BattleFeedbackSystem::shutdown() {
    for (FeedbackPopup& popup : popups_) {
        destroyPopupTextures(popup);
    }
    popups_.clear();
    lastCharacterHp_.clear();
    lastBossHp_ = -1;
    suppressBossNextDamagePopup_ = false;
    suppressBossNextHealingPopup_ = false;
    suppressCharacterNextDamagePopup_.clear();
    suppressCharacterNextHealingPopup_.clear();
    bossShakeState_ = DamageShakeState{};
    characterShakeStates_.clear();

#ifdef BATTLE_ENABLE_TTF
    if (font_ != nullptr) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
#endif
}

void BattleFeedbackSystem::spawnFeedbackPopup(bool onBoss, int partyIndex, int amount, bool healing) {
    if (amount <= 0) {
        return;
    }

    std::uniform_real_distribution<float> xDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> yDist(-7.0f, 7.0f);

    FeedbackPopup popup;
    popup.onBoss = onBoss;
    popup.partyIndex = partyIndex;
    popup.amount = amount;
    popup.healing = healing;
    popup.elapsed = 0.0f;
    popup.lifetime = kFeedbackPopupLifetimeSeconds;
    popup.jitterX = xDist(popupRng());
    popup.jitterY = yDist(popupRng());
    popup.text = (healing ? "+" : "") + std::to_string(amount);
    popups_.push_back(popup);
}

void BattleFeedbackSystem::spawnDamagePopup(bool onBoss, int partyIndex, int damage) {
    spawnFeedbackPopup(onBoss, partyIndex, damage, false);
}

void BattleFeedbackSystem::spawnHealingPopup(bool onBoss, int partyIndex, int amount) {
    spawnFeedbackPopup(onBoss, partyIndex, amount, true);
}

void BattleFeedbackSystem::queueDamageShake(bool onBoss, int partyIndex, int hitCount) {
    if (hitCount <= 0) {
        return;
    }

    if (onBoss) {
        bossShakeState_.queuedHits += hitCount;
        return;
    }

    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characterShakeStates_.size()) {
        return;
    }

    characterShakeStates_[static_cast<size_t>(partyIndex)].queuedHits += hitCount;
}

void BattleFeedbackSystem::syncFromManager(const BattleManager& manager, bool presentationPlaybackActive) {
    const int bossHpNow = manager.getBossCurrentHp();
    if (lastBossHp_ >= 0) {
        if (bossHpNow < lastBossHp_) {
            if (!suppressBossNextDamagePopup_) {
                spawnDamagePopup(true, -1, lastBossHp_ - bossHpNow);
            }
            if (!presentationPlaybackActive && !suppressBossNextDamagePopup_) {
                queueDamageShake(true, -1, 1);
            }
            suppressBossNextDamagePopup_ = false;
        } else if (bossHpNow > lastBossHp_) {
            if (!suppressBossNextHealingPopup_) {
                spawnHealingPopup(true, -1, bossHpNow - lastBossHp_);
            }
            suppressBossNextHealingPopup_ = false;
        }
    }
    lastBossHp_ = bossHpNow;

    const BattleState& state = manager.getBattleState();
    if (lastCharacterHp_.size() != state.party.size()) {
        lastCharacterHp_.assign(state.party.size(), 0);
        characterShakeStates_.assign(state.party.size(), DamageShakeState{});
        suppressCharacterNextDamagePopup_.assign(state.party.size(), false);
        suppressCharacterNextHealingPopup_.assign(state.party.size(), false);
        for (size_t i = 0; i < state.party.size(); ++i) {
            lastCharacterHp_[i] = manager.getCharacterCurrentHp(static_cast<int>(i));
        }
        return;
    }

    for (size_t i = 0; i < state.party.size(); ++i) {
        const int hpNow = manager.getCharacterCurrentHp(static_cast<int>(i));
        if (hpNow < lastCharacterHp_[i]) {
            const bool suppress = i < suppressCharacterNextDamagePopup_.size() &&
                                  suppressCharacterNextDamagePopup_[i];
            if (!suppress) {
                spawnDamagePopup(false, static_cast<int>(i), lastCharacterHp_[i] - hpNow);
            }
            if (!presentationPlaybackActive && !suppress) {
                queueDamageShake(false, static_cast<int>(i), 1);
            }
            if (i < suppressCharacterNextDamagePopup_.size()) {
                suppressCharacterNextDamagePopup_[i] = false;
            }
        } else if (hpNow > lastCharacterHp_[i]) {
            const bool suppress = i < suppressCharacterNextHealingPopup_.size() &&
                                  suppressCharacterNextHealingPopup_[i];
            if (!suppress) {
                spawnHealingPopup(false, static_cast<int>(i), hpNow - lastCharacterHp_[i]);
            }
            if (i < suppressCharacterNextHealingPopup_.size()) {
                suppressCharacterNextHealingPopup_[i] = false;
            }
        }
        lastCharacterHp_[i] = hpNow;
    }
}

void BattleFeedbackSystem::queuePresentationHitShakes(bool isBossCaster, int hitEvents, const BattleManager& manager) {
    if (hitEvents <= 0) {
        return;
    }

    if (isBossCaster) {
        const BattleState& state = manager.getBattleState();
        for (size_t i = 0; i < state.party.size(); ++i) {
            if (manager.getCharacterCurrentHp(static_cast<int>(i)) > 0) {
                queueDamageShake(false, static_cast<int>(i), hitEvents);
            }
        }
        return;
    }

    if (manager.playerDamageHealsBoss()) {
        return;
    }

    if (manager.getBossCurrentHp() > 0) {
        queueDamageShake(true, -1, hitEvents);
    }
}

int BattleFeedbackSystem::queuePresentationHitFeedback(bool isBossCaster,
                                                       int hitEvents,
                                                       int perHitDamage,
                                                       const BattleManager& manager,
                                                       int targetPartyIndex) {
    if (hitEvents <= 0 || perHitDamage <= 0) {
        return 0;
    }

    if (isBossCaster) {
        const BattleState& state = manager.getBattleState();
        if (suppressCharacterNextDamagePopup_.size() != state.party.size()) {
            suppressCharacterNextDamagePopup_.assign(state.party.size(), false);
        }

        if (targetPartyIndex >= 0 && static_cast<size_t>(targetPartyIndex) < state.party.size()) {
            if (manager.getCharacterCurrentHp(targetPartyIndex) > 0) {
                for (int h = 0; h < hitEvents; ++h) {
                    spawnDamagePopup(false, targetPartyIndex, perHitDamage);
                }
                queueDamageShake(false, targetPartyIndex, hitEvents);
                suppressCharacterNextDamagePopup_[static_cast<size_t>(targetPartyIndex)] = true;
                return hitEvents;
            }
            return 0;
        }

        int spawnedDamagePopups = 0;
        for (size_t i = 0; i < state.party.size(); ++i) {
            if (manager.getCharacterCurrentHp(static_cast<int>(i)) <= 0) {
                continue;
            }

            for (int h = 0; h < hitEvents; ++h) {
                spawnDamagePopup(false, static_cast<int>(i), perHitDamage);
            }
            queueDamageShake(false, static_cast<int>(i), hitEvents);
            suppressCharacterNextDamagePopup_[i] = true;
            spawnedDamagePopups += hitEvents;
        }
        return spawnedDamagePopups;
    }

    if (manager.getBossCurrentHp() <= 0) {
        return 0;
    }

    if (manager.playerDamageHealsBoss()) {
        for (int h = 0; h < hitEvents; ++h) {
            spawnHealingPopup(true, -1, perHitDamage);
        }
        suppressBossNextHealingPopup_ = true;
        return 0;
    }

    for (int h = 0; h < hitEvents; ++h) {
        spawnDamagePopup(true, -1, perHitDamage);
    }
    queueDamageShake(true, -1, hitEvents);
    suppressBossNextDamagePopup_ = true;
    return hitEvents;
}

void BattleFeedbackSystem::queuePresentationHealFeedback(bool isBossCaster,
                                                        int hitEvents,
                                                        int perHitHealing,
                                                        const BattleManager& manager) {
    if (hitEvents <= 0 || perHitHealing <= 0) {
        return;
    }

    if (isBossCaster) {
        if (manager.getBossCurrentHp() <= 0) {
            return;
        }
        for (int h = 0; h < hitEvents; ++h) {
            spawnHealingPopup(true, -1, perHitHealing);
        }
        suppressBossNextHealingPopup_ = true;
        return;
    }

    const BattleState& state = manager.getBattleState();
    if (suppressCharacterNextHealingPopup_.size() != state.party.size()) {
        suppressCharacterNextHealingPopup_.assign(state.party.size(), false);
    }

    for (size_t i = 0; i < state.party.size(); ++i) {
        if (manager.getCharacterCurrentHp(static_cast<int>(i)) <= 0) {
            continue;
        }

        for (int h = 0; h < hitEvents; ++h) {
            spawnHealingPopup(false, static_cast<int>(i), perHitHealing);
        }
        suppressCharacterNextHealingPopup_[i] = true;
    }
}

void BattleFeedbackSystem::updateOneDamageShakeState(DamageShakeState& state, float deltaSeconds) {
    if (!state.active && state.queuedHits > 0) {
        state.active = true;
        state.elapsed = 0.0f;
        state.directionSign *= -1;
        --state.queuedHits;
    }

    if (!state.active) {
        return;
    }

    state.elapsed += deltaSeconds;
    if (state.elapsed >= kPopupShakeDurationSeconds) {
        state.active = false;
        state.elapsed = 0.0f;
    }
}

void BattleFeedbackSystem::update(float deltaSeconds) {
    for (FeedbackPopup& popup : popups_) {
        popup.elapsed += deltaSeconds;
    }

    popups_.erase(
        std::remove_if(popups_.begin(), popups_.end(), [this](FeedbackPopup& popup) {
            if (popup.elapsed < popup.lifetime) {
                return false;
            }
            destroyPopupTextures(popup);
            return true;
        }),
        popups_.end()
    );

    updateOneDamageShakeState(bossShakeState_, deltaSeconds);
    for (DamageShakeState& state : characterShakeStates_) {
        updateOneDamageShakeState(state, deltaSeconds);
    }
}

float BattleFeedbackSystem::getShakeOffsetX(bool isBoss, int partyIndex) const {
    const DamageShakeState* state = nullptr;
    if (isBoss) {
        state = &bossShakeState_;
    } else if (partyIndex >= 0 && static_cast<size_t>(partyIndex) < characterShakeStates_.size()) {
        state = &characterShakeStates_[static_cast<size_t>(partyIndex)];
    }

    if (state == nullptr || !state->active) {
        return 0.0f;
    }

    const float t = std::clamp(state->elapsed / std::max(0.0001f, kPopupShakeDurationSeconds), 0.0f, 1.0f);
    const float wave = std::sin(t * 6.2831853f);
    return wave * kPopupShakeAmplitudePixels * static_cast<float>(state->directionSign);
}

bool BattleFeedbackSystem::hasVisiblePopups() const {
    return !popups_.empty();
}

void BattleFeedbackSystem::render(SDL_Renderer* renderer,
                                  const Camera3D& camera,
                                  const std::vector<FeedbackEntityAnchor>& anchors) {
#ifdef BATTLE_ENABLE_TTF
    if (renderer == nullptr || font_ == nullptr) {
        return;
    }

    for (FeedbackPopup& popup : popups_) {
        const FeedbackEntityAnchor* anchor = nullptr;
        for (const FeedbackEntityAnchor& candidate : anchors) {
            if (popup.onBoss && candidate.isBoss) {
                anchor = &candidate;
                break;
            }
            if (!popup.onBoss && !candidate.isBoss && candidate.partyIndex == popup.partyIndex) {
                anchor = &candidate;
                break;
            }
        }

        if (anchor == nullptr) {
            continue;
        }

        const SDL_FPoint baseScreen = camera.worldToScreen(anchor->worldX, anchor->worldY, anchor->worldZ);
        const float popupScale = camera.getPerspectiveScale(anchor->worldX, anchor->worldY, anchor->worldZ);
        const float spriteScale = anchor->isBoss ? 1.28f : 1.0f;
        const float approxSpriteHeight = kSuggestedBaseSpriteHeight * popupScale * spriteScale;
        const float t = std::clamp(popup.elapsed / std::max(0.001f, popup.lifetime), 0.0f, 1.0f);
        const float slide = kFeedbackPopupSlidePixels * t;
        const float alphaNorm = 1.0f - t;

        if (!ensurePopupTextures(renderer, popup)) {
            continue;
        }

        const int drawX = static_cast<int>(std::lround(baseScreen.x + popup.jitterX)) - popup.textWidth / 2;
        const int drawY = static_cast<int>(std::lround(baseScreen.y - approxSpriteHeight * 0.95f +
                                                       kFeedbackPopupBaseYOffset + popup.jitterY - slide)) - popup.textHeight / 2;

        const Uint8 outlineAlpha = static_cast<Uint8>(220.0f * alphaNorm);
        const Uint8 mainAlpha = static_cast<Uint8>(255.0f * alphaNorm);
        SDL_SetTextureAlphaMod(popup.shadowTexture, outlineAlpha);
        SDL_SetTextureAlphaMod(popup.textTexture, mainAlpha);

        SDL_Rect textDst{drawX, drawY, popup.textWidth, popup.textHeight};

        const int outlineOffsets[8][2] = {
            {-1, -1}, {0, -1}, {1, -1},
            {-1,  0},          {1,  0},
            {-1,  1}, {0,  1}, {1,  1}
        };
        for (const auto& offset : outlineOffsets) {
            SDL_Rect shadowDst{drawX + offset[0], drawY + offset[1], popup.textWidth, popup.textHeight};
            SDL_RenderCopy(renderer, popup.shadowTexture, nullptr, &shadowDst);
        }
        SDL_RenderCopy(renderer, popup.textTexture, nullptr, &textDst);
    }
#else
    (void)renderer;
    (void)camera;
    (void)anchors;
#endif
}

void BattleFeedbackSystem::destroyPopupTextures(FeedbackPopup& popup) {
    if (popup.shadowTexture != nullptr) {
        SDL_DestroyTexture(popup.shadowTexture);
        popup.shadowTexture = nullptr;
    }
    if (popup.textTexture != nullptr) {
        SDL_DestroyTexture(popup.textTexture);
        popup.textTexture = nullptr;
    }
    popup.cachedRenderer = nullptr;
    popup.textWidth = 0;
    popup.textHeight = 0;
}

bool BattleFeedbackSystem::ensurePopupTextures(SDL_Renderer* renderer, FeedbackPopup& popup) {
#ifdef BATTLE_ENABLE_TTF
    if (renderer == nullptr || font_ == nullptr || popup.text.empty()) {
        return false;
    }

    if (popup.cachedRenderer == renderer &&
        popup.shadowTexture != nullptr &&
        popup.textTexture != nullptr &&
        popup.textWidth > 0 &&
        popup.textHeight > 0) {
        return true;
    }

    destroyPopupTextures(popup);

    if (TTF_SizeUTF8(font_, popup.text.c_str(), &popup.textWidth, &popup.textHeight) != 0) {
        popup.textWidth = 0;
        popup.textHeight = 0;
        return false;
    }

    const SDL_Color outlineColor{0, 0, 0, 220};
    const SDL_Color mainColor = popup.healing
        ? SDL_Color{114, 255, 163, 255}
        : SDL_Color{255, 255, 255, 255};

    SDL_Surface* shadowSurface = TTF_RenderUTF8_Blended(font_, popup.text.c_str(), outlineColor);
    SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font_, popup.text.c_str(), mainColor);
    if (shadowSurface == nullptr || textSurface == nullptr) {
        if (shadowSurface != nullptr) {
            SDL_FreeSurface(shadowSurface);
        }
        if (textSurface != nullptr) {
            SDL_FreeSurface(textSurface);
        }
        popup.textWidth = 0;
        popup.textHeight = 0;
        return false;
    }

    popup.shadowTexture = SDL_CreateTextureFromSurface(renderer, shadowSurface);
    popup.textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(shadowSurface);
    SDL_FreeSurface(textSurface);
    if (popup.shadowTexture == nullptr || popup.textTexture == nullptr) {
        destroyPopupTextures(popup);
        return false;
    }

    popup.cachedRenderer = renderer;
    return true;
#else
    (void)renderer;
    (void)popup;
    return false;
#endif
}

} // namespace battle::render
