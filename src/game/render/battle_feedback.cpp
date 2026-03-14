#include "battle_feedback.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

namespace battle::render {
namespace {

constexpr float kDamagePopupLifetimeSeconds = 0.95f;
constexpr float kDamagePopupSlidePixels = 26.0f;
constexpr float kDamagePopupBaseYOffset = -14.0f;
constexpr int kDamagePopupFontSize = 30;
constexpr float kDamageShakeDurationSeconds = 0.065f;
constexpr float kDamageShakeAmplitudePixels = 2.8f;
constexpr int kSuggestedBaseSpriteHeight = 260;

std::mt19937& popupRng() {
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

#ifdef BATTLE_ENABLE_TTF
TTF_Font* openBestAvailablePopupFont(int ptSize) {
    const std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "assets/rmlui/DejaVuSans.ttf",
        "../assets/rmlui/DejaVuSans.ttf",
        "../../assets/rmlui/DejaVuSans.ttf"
    };

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
    popups_.clear();

    const BattleState& state = manager.getBattleState();
    lastCharacterHp_.assign(state.party.size(), 0);
    for (size_t i = 0; i < state.party.size(); ++i) {
        lastCharacterHp_[i] = manager.getCharacterCurrentHp(static_cast<int>(i));
    }
    lastBossHp_ = manager.getBossCurrentHp();
    suppressBossNextDeltaPopup_ = false;

    bossShakeState_ = DamageShakeState{};
    characterShakeStates_.assign(state.party.size(), DamageShakeState{});
    suppressCharacterNextDeltaPopup_.assign(state.party.size(), false);

#ifdef BATTLE_ENABLE_TTF
    if (font_ == nullptr) {
        font_ = openBestAvailablePopupFont(kDamagePopupFontSize);
    }
#endif
}

void BattleFeedbackSystem::shutdown() {
    popups_.clear();
    lastCharacterHp_.clear();
    lastBossHp_ = -1;
    suppressBossNextDeltaPopup_ = false;
    suppressCharacterNextDeltaPopup_.clear();
    bossShakeState_ = DamageShakeState{};
    characterShakeStates_.clear();

#ifdef BATTLE_ENABLE_TTF
    if (font_ != nullptr) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
#endif
}

void BattleFeedbackSystem::spawnDamagePopup(bool onBoss, int partyIndex, int damage) {
    if (damage <= 0) {
        return;
    }

    std::uniform_real_distribution<float> xDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> yDist(-7.0f, 7.0f);

    DamagePopup popup;
    popup.onBoss = onBoss;
    popup.partyIndex = partyIndex;
    popup.damage = damage;
    popup.elapsed = 0.0f;
    popup.lifetime = kDamagePopupLifetimeSeconds;
    popup.jitterX = xDist(popupRng());
    popup.jitterY = yDist(popupRng());
    popups_.push_back(popup);
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
    if (lastBossHp_ >= 0 && bossHpNow < lastBossHp_) {
        if (!suppressBossNextDeltaPopup_) {
            spawnDamagePopup(true, -1, lastBossHp_ - bossHpNow);
        }
        if (!presentationPlaybackActive && !suppressBossNextDeltaPopup_) {
            queueDamageShake(true, -1, 1);
        }
        suppressBossNextDeltaPopup_ = false;
    }
    lastBossHp_ = bossHpNow;

    const BattleState& state = manager.getBattleState();
    if (lastCharacterHp_.size() != state.party.size()) {
        lastCharacterHp_.assign(state.party.size(), 0);
        characterShakeStates_.assign(state.party.size(), DamageShakeState{});
        suppressCharacterNextDeltaPopup_.assign(state.party.size(), false);
        for (size_t i = 0; i < state.party.size(); ++i) {
            lastCharacterHp_[i] = manager.getCharacterCurrentHp(static_cast<int>(i));
        }
        return;
    }

    for (size_t i = 0; i < state.party.size(); ++i) {
        const int hpNow = manager.getCharacterCurrentHp(static_cast<int>(i));
        if (hpNow < lastCharacterHp_[i]) {
            const bool suppress = i < suppressCharacterNextDeltaPopup_.size() &&
                                  suppressCharacterNextDeltaPopup_[i];
            if (!suppress) {
                spawnDamagePopup(false, static_cast<int>(i), lastCharacterHp_[i] - hpNow);
            }
            if (!presentationPlaybackActive && !suppress) {
                queueDamageShake(false, static_cast<int>(i), 1);
            }
            if (i < suppressCharacterNextDeltaPopup_.size()) {
                suppressCharacterNextDeltaPopup_[i] = false;
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

    if (manager.getBossCurrentHp() > 0) {
        queueDamageShake(true, -1, hitEvents);
    }
}

void BattleFeedbackSystem::queuePresentationHitFeedback(bool isBossCaster,
                                                        int hitEvents,
                                                        int perHitDamage,
                                                        const BattleManager& manager) {
    if (hitEvents <= 0 || perHitDamage <= 0) {
        return;
    }

    if (isBossCaster) {
        const BattleState& state = manager.getBattleState();
        if (suppressCharacterNextDeltaPopup_.size() != state.party.size()) {
            suppressCharacterNextDeltaPopup_.assign(state.party.size(), false);
        }

        for (size_t i = 0; i < state.party.size(); ++i) {
            if (manager.getCharacterCurrentHp(static_cast<int>(i)) <= 0) {
                continue;
            }

            for (int h = 0; h < hitEvents; ++h) {
                spawnDamagePopup(false, static_cast<int>(i), perHitDamage);
            }
            queueDamageShake(false, static_cast<int>(i), hitEvents);
            suppressCharacterNextDeltaPopup_[i] = true;
        }
        return;
    }

    if (manager.getBossCurrentHp() <= 0) {
        return;
    }

    for (int h = 0; h < hitEvents; ++h) {
        spawnDamagePopup(true, -1, perHitDamage);
    }
    queueDamageShake(true, -1, hitEvents);
    suppressBossNextDeltaPopup_ = true;
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
    if (state.elapsed >= kDamageShakeDurationSeconds) {
        state.active = false;
        state.elapsed = 0.0f;
    }
}

void BattleFeedbackSystem::update(float deltaSeconds) {
    for (DamagePopup& popup : popups_) {
        popup.elapsed += deltaSeconds;
    }

    popups_.erase(
        std::remove_if(popups_.begin(), popups_.end(), [](const DamagePopup& popup) {
            return popup.elapsed >= popup.lifetime;
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

    const float t = std::clamp(state->elapsed / std::max(0.0001f, kDamageShakeDurationSeconds), 0.0f, 1.0f);
    const float wave = std::sin(t * 6.2831853f);
    return wave * kDamageShakeAmplitudePixels * static_cast<float>(state->directionSign);
}

void BattleFeedbackSystem::render(SDL_Renderer* renderer,
                                  const Camera3D& camera,
                                  const std::vector<FeedbackEntityAnchor>& anchors) {
#ifdef BATTLE_ENABLE_TTF
    if (renderer == nullptr || font_ == nullptr) {
        return;
    }

    for (const DamagePopup& popup : popups_) {
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
        const float slide = kDamagePopupSlidePixels * t;
        const float alphaNorm = 1.0f - t;

        const std::string text = std::to_string(popup.damage);
        int textW = 0;
        int textH = 0;
        if (TTF_SizeUTF8(font_, text.c_str(), &textW, &textH) != 0) {
            continue;
        }

        const int drawX = static_cast<int>(std::lround(baseScreen.x + popup.jitterX)) - textW / 2;
        const int drawY = static_cast<int>(std::lround(baseScreen.y - approxSpriteHeight * 0.95f +
                                                       kDamagePopupBaseYOffset + popup.jitterY - slide)) - textH / 2;

        SDL_Color outlineColor{0, 0, 0, static_cast<Uint8>(220.0f * alphaNorm)};
        SDL_Color mainColor{255, 255, 255, static_cast<Uint8>(255.0f * alphaNorm)};

        SDL_Surface* shadowSurface = TTF_RenderUTF8_Blended(font_, text.c_str(), outlineColor);
        SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font_, text.c_str(), mainColor);
        if (shadowSurface == nullptr || textSurface == nullptr) {
            if (shadowSurface != nullptr) {
                SDL_FreeSurface(shadowSurface);
            }
            if (textSurface != nullptr) {
                SDL_FreeSurface(textSurface);
            }
            continue;
        }

        SDL_Texture* shadowTex = SDL_CreateTextureFromSurface(renderer, shadowSurface);
        SDL_Texture* textTex = SDL_CreateTextureFromSurface(renderer, textSurface);
        SDL_FreeSurface(shadowSurface);
        SDL_FreeSurface(textSurface);
        if (shadowTex == nullptr || textTex == nullptr) {
            if (shadowTex != nullptr) {
                SDL_DestroyTexture(shadowTex);
            }
            if (textTex != nullptr) {
                SDL_DestroyTexture(textTex);
            }
            continue;
        }

        SDL_Rect textDst{drawX, drawY, textW, textH};

        const int outlineOffsets[8][2] = {
            {-1, -1}, {0, -1}, {1, -1},
            {-1,  0},          {1,  0},
            {-1,  1}, {0,  1}, {1,  1}
        };
        for (const auto& offset : outlineOffsets) {
            SDL_Rect shadowDst{drawX + offset[0], drawY + offset[1], textW, textH};
            SDL_RenderCopy(renderer, shadowTex, nullptr, &shadowDst);
        }
        SDL_RenderCopy(renderer, textTex, nullptr, &textDst);

        SDL_DestroyTexture(shadowTex);
        SDL_DestroyTexture(textTex);
    }
#else
    (void)renderer;
    (void)camera;
    (void)anchors;
#endif
}

} // namespace battle::render
