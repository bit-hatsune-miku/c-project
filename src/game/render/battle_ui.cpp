#include "battle_ui.h"

#include "../core/easing.h"
#include "../core/turn_system.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

namespace battle::ui {
#ifdef BATTLE_ENABLE_TTF
struct BattleHudFontCache {
    std::map<int, TTF_Font*> fonts;

    ~BattleHudFontCache() {
        for (auto& [_, font] : fonts) {
            if (font != nullptr) {
                TTF_CloseFont(font);
            }
        }
    }

    TTF_Font* openBestAvailableFont(int ptSize) {
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

    TTF_Font* get(int ptSize) {
        auto it = fonts.find(ptSize);
        if (it != fonts.end()) {
            return it->second;
        }

        TTF_Font* loaded = openBestAvailableFont(ptSize);
        fonts[ptSize] = loaded;
        return loaded;
    }
};
#endif

namespace {

void drawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

void drawCroppedTexture(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_Rect& dstRect) {
    if (renderer == nullptr || texture == nullptr) {
        return;
    }

    int texW = 0;
    int texH = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);
    if (texW <= 0 || texH <= 0) {
        return;
    }

    const float aspectDst = static_cast<float>(dstRect.w) / std::max(1, dstRect.h);
    const float aspectSrc = static_cast<float>(texW) / texH;

    SDL_Rect srcRect{};
    if (aspectSrc > aspectDst) {
        const int croppedW = static_cast<int>(texH * aspectDst);
        srcRect.x = (texW - croppedW) / 2;
        srcRect.y = 0;
        srcRect.w = croppedW;
        srcRect.h = texH;
    } else {
        const int croppedH = static_cast<int>(texW / aspectDst);
        srcRect.x = 0;
        srcRect.y = (texH - croppedH) / 2;
        srcRect.w = texW;
        srcRect.h = croppedH;
    }

    SDL_RenderCopy(renderer, texture, &srcRect, &dstRect);
}

#ifdef BATTLE_ENABLE_TTF
void drawTextInternal(BattleHudFontCache* fontCache,
                      SDL_Renderer* renderer,
                      const std::string& text,
                      int x,
                      int y,
                      SDL_Color color,
                      int fontSize,
                      bool centered) {
    if (fontCache == nullptr || text.empty()) {
        return;
    }

    TTF_Font* font = fontCache->get(fontSize);
    if (font == nullptr) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (tex == nullptr) {
        SDL_FreeSurface(surface);
        return;
    }

    const int finalX = centered ? x - surface->w / 2 : x;
    SDL_Rect dst{finalX, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surface);
}

void drawTextCentered(BattleHudFontCache* fontCache,
                      SDL_Renderer* renderer,
                      const std::string& text,
                      int centerX,
                      int y,
                      SDL_Color color,
                      int fontSize) {
    drawTextInternal(fontCache, renderer, text, centerX, y, color, fontSize, true);
}

void drawTextAt(BattleHudFontCache* fontCache,
                SDL_Renderer* renderer,
                const std::string& text,
                int x,
                int y,
                SDL_Color color,
                int fontSize) {
    drawTextInternal(fontCache, renderer, text, x, y, color, fontSize, false);
}
#endif

} // namespace

BattleHud::BattleHud() {
#ifdef BATTLE_ENABLE_TTF
    fontCache_ = std::make_unique<BattleHudFontCache>();
#endif
}

BattleHud::~BattleHud() = default;

void BattleHud::reset() {
    model_ = BattleHudModel{};
    initialized_ = false;
    bossHpTransition_ = HpTransition{};
    characterHpTransitions_.clear();
    characterDamageFlashes_.clear();
}

void BattleHud::syncFromManager(const BattleManager& manager) {
    const BattleState& battleState = manager.getBattleState();

    BattleHudModel nextModel;
    nextModel.bossTitle = battleState.boss.title;
    nextModel.bossCurrentHp = manager.getBossCurrentHp();
    nextModel.bossMaxHp = manager.getBossMaxHp();
    nextModel.turnState = manager.getTurnState();
    nextModel.activeActorIndex = manager.getPreviewNextActorIndex();
    nextModel.characters.reserve(battleState.party.size());

    for (size_t i = 0; i < battleState.party.size(); ++i) {
        const CharacterDefinition& character = battleState.party[i];
        HudCharacterModel hudCharacter;
        hudCharacter.key = character.key;
        hudCharacter.title = character.title;
        hudCharacter.currentHp = manager.getCharacterCurrentHp(static_cast<int>(i));
        hudCharacter.maxHp = manager.getCharacterMaxHp(static_cast<int>(i));
        hudCharacter.ultimateCharge = manager.getCharacterUltimateCharge(static_cast<int>(i));
        hudCharacter.ultimateRequired = manager.getCharacterUltimateRequired(static_cast<int>(i));
        nextModel.characters.push_back(std::move(hudCharacter));
    }

    characterHpTransitions_.resize(nextModel.characters.size());
    characterDamageFlashes_.resize(nextModel.characters.size());

    if (!initialized_) {
        updateBossTransition(nextModel.bossCurrentHp, nextModel.bossMaxHp);
        for (size_t i = 0; i < nextModel.characters.size(); ++i) {
            updateCharacterTransition(static_cast<int>(i),
                                      nextModel.characters[i].currentHp,
                                      nextModel.characters[i].maxHp);
        }
    } else {
        if (model_.bossCurrentHp != nextModel.bossCurrentHp || model_.bossMaxHp != nextModel.bossMaxHp) {
            updateBossTransition(nextModel.bossCurrentHp, nextModel.bossMaxHp);
        }

        for (size_t i = 0; i < nextModel.characters.size(); ++i) {
            const int previousHp = i < model_.characters.size() ? model_.characters[i].currentHp : nextModel.characters[i].currentHp;
            const int previousMaxHp = i < model_.characters.size() ? model_.characters[i].maxHp : nextModel.characters[i].maxHp;

            if (previousHp != nextModel.characters[i].currentHp || previousMaxHp != nextModel.characters[i].maxHp) {
                updateCharacterTransition(static_cast<int>(i),
                                          nextModel.characters[i].currentHp,
                                          nextModel.characters[i].maxHp);
                if (nextModel.characters[i].currentHp < previousHp) {
                    triggerCharacterDamageFlash(static_cast<int>(i));
                }
            }
        }
    }

    model_ = std::move(nextModel);
    initialized_ = true;
}

void BattleHud::draw(SDL_Renderer* renderer,
                     int screenW,
                     int screenH,
                     const std::map<std::string, SDL_Texture*>& iconByAsset) {
    if (!initialized_ || renderer == nullptr) {
        return;
    }

    drawTurnOrder(renderer, iconByAsset);
    drawBossHeader(renderer, screenW);
    drawCharacterStatus(renderer, screenW, screenH, iconByAsset);
}

void BattleHud::updateBossTransition(int newHp, int maxHp) {
    if (!initialized_ && bossHpTransition_.fromHp == 0 && bossHpTransition_.toHp == 0) {
        bossHpTransition_.fromHp = newHp;
        bossHpTransition_.toHp = newHp;
        bossHpTransition_.maxHp = std::max(1, maxHp);
        return;
    }

    if (bossHpTransition_.toHp == newHp && bossHpTransition_.maxHp == std::max(1, maxHp)) {
        return;
    }

    bossHpTransition_.active = true;
    bossHpTransition_.elapsed = 0.0f;
    bossHpTransition_.duration = 0.5f;
    bossHpTransition_.fromHp = bossHpTransition_.toHp;
    bossHpTransition_.toHp = newHp;
    bossHpTransition_.maxHp = std::max(1, maxHp);
    bossHpTransition_.lastTickMs = SDL_GetTicks64();
}

void BattleHud::updateCharacterTransition(int charIndex, int newHp, int maxHp) {
    if (charIndex < 0 || static_cast<size_t>(charIndex) >= characterHpTransitions_.size()) {
        return;
    }

    HpTransition& transition = characterHpTransitions_[static_cast<size_t>(charIndex)];
    if (!initialized_ && transition.fromHp == 0 && transition.toHp == 0) {
        transition.fromHp = newHp;
        transition.toHp = newHp;
        transition.maxHp = std::max(1, maxHp);
        return;
    }

    if (transition.toHp == newHp && transition.maxHp == std::max(1, maxHp)) {
        return;
    }

    transition.active = true;
    transition.elapsed = 0.0f;
    transition.duration = 0.5f;
    transition.fromHp = transition.toHp;
    transition.toHp = newHp;
    transition.maxHp = std::max(1, maxHp);
    transition.lastTickMs = SDL_GetTicks64();
}

void BattleHud::triggerCharacterDamageFlash(int charIndex) {
    if (charIndex < 0 || static_cast<size_t>(charIndex) >= characterDamageFlashes_.size()) {
        return;
    }

    DamageFlashState& flash = characterDamageFlashes_[static_cast<size_t>(charIndex)];
    flash.active = true;
    flash.elapsed = 0.0f;
    flash.duration = 1.0f;
    flash.lastTickMs = SDL_GetTicks64();
}

int BattleHud::getDisplayedHp(HpTransition& transition, int fallbackCurrentHp) {
    if (!transition.active) {
        return fallbackCurrentHp;
    }

    const Uint64 nowMs = SDL_GetTicks64();
    const float dt = (nowMs - transition.lastTickMs) / 1000.0f;
    transition.elapsed += std::max(0.0f, dt);
    transition.lastTickMs = nowMs;

    const float t = easing::clamp01(transition.elapsed / std::max(0.001f, transition.duration));
    const float eased = easing::easeOutCubic(t);
    const float interpolatedHp = easing::lerp(static_cast<float>(transition.fromHp),
                                              static_cast<float>(transition.toHp),
                                              eased);

    if (t >= 1.0f) {
        transition.active = false;
        transition.fromHp = transition.toHp;
    }

    return static_cast<int>(std::round(interpolatedHp));
}

float BattleHud::getDamageFlashAlpha(int charIndex, int& shakeOffsetX) {
    shakeOffsetX = 0;
    if (charIndex < 0 || static_cast<size_t>(charIndex) >= characterDamageFlashes_.size()) {
        return 0.0f;
    }

    DamageFlashState& flash = characterDamageFlashes_[static_cast<size_t>(charIndex)];
    if (!flash.active) {
        return 0.0f;
    }

    const Uint64 nowMs = SDL_GetTicks64();
    const float dt = (nowMs - flash.lastTickMs) / 1000.0f;
    flash.elapsed += std::max(0.0f, dt);
    flash.lastTickMs = nowMs;

    const float t = easing::clamp01(flash.elapsed / std::max(0.001f, flash.duration));
    const float shakeFrequency = 8.0f;
    const float shakeAmount = 6.0f;
    const float shakePhase = t * shakeFrequency * 3.14159f * 2.0f;
    shakeOffsetX = static_cast<int>(std::sin(shakePhase) * shakeAmount);

    if (t >= 1.0f) {
        flash.active = false;
    }

    return (1.0f - t) * 0.25f;
}

void BattleHud::drawTurnOrder(SDL_Renderer* renderer, const std::map<std::string, SDL_Texture*>& iconByAsset) {
    constexpr int cardWidth = 100;
    constexpr int cardHeight = 50;
    constexpr int cardSpacing = 4;
    constexpr int startX = 16;
    constexpr int startY = 16;
    constexpr int maxCards = 10;

    std::vector<int> sortedActorIndices(model_.turnState.actors.size());
    std::iota(sortedActorIndices.begin(), sortedActorIndices.end(), 0);
    std::sort(sortedActorIndices.begin(), sortedActorIndices.end(), [&](int lhs, int rhs) {
        constexpr float eps = 0.0001f;
        const TurnActor& a = model_.turnState.actors[static_cast<size_t>(lhs)];
        const TurnActor& b = model_.turnState.actors[static_cast<size_t>(rhs)];
        if (std::fabs(a.currentActionValue - b.currentActionValue) > eps) {
            return a.currentActionValue < b.currentActionValue;
        }
        return turn::turnPriorityLess(a, b);
    });

    const int cardCount = std::min(static_cast<int>(sortedActorIndices.size()), maxCards);
    const float animTime = SDL_GetTicks() / 1000.0f;
    const float phase = std::fmod(animTime, 1.0f);

    for (int cardIndex = 0; cardIndex < cardCount; ++cardIndex) {
        const int actorIndex = sortedActorIndices[static_cast<size_t>(cardIndex)];
        const TurnActor& actor = model_.turnState.actors[static_cast<size_t>(actorIndex)];

        int cardX = startX;
        if (actorIndex == model_.activeActorIndex) {
            float offset = 0.0f;
            if (phase < 0.35f) {
                const float t = phase / 0.35f;
                offset = easing::lerp(0.0f, 10.0f, easing::easeOutCubic(t));
            } else if (phase < 0.75f) {
                const float t = (phase - 0.35f) / 0.40f;
                offset = easing::lerp(10.0f, 0.0f, easing::easeOutBack(t));
            }
            cardX = startX + static_cast<int>(std::round(offset));
        }

        const int cardY = startY + cardIndex * (cardHeight + cardSpacing);
        SDL_Rect cardRect{cardX, cardY, cardWidth, cardHeight};
        SDL_SetRenderDrawColor(renderer, 30, 32, 40, 220);
        SDL_RenderFillRect(renderer, &cardRect);
        SDL_SetRenderDrawColor(renderer, 60, 65, 80, 255);
        SDL_RenderDrawRect(renderer, &cardRect);

        const std::string iconKey = (actor.type == ParticipantType::Boss) ? "boss_" + actor.key : actor.key;
        auto iconIt = iconByAsset.find(iconKey);
        if (iconIt != iconByAsset.end() && iconIt->second != nullptr) {
            drawCroppedTexture(renderer, iconIt->second, cardRect);
        }

        const std::string avText = std::to_string(static_cast<int>(actor.currentActionValue));
        const int bgWidth = static_cast<int>(avText.size()) * 10 + 8;
        const int textX = cardX + cardWidth - bgWidth;
        const int textY = cardY + cardHeight - 16;
        SDL_Rect avBg{textX - 2, textY - 1, bgWidth, 10};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(renderer, &avBg);

#ifdef BATTLE_ENABLE_TTF
        drawTextAt(fontCache_.get(), renderer, avText, textX, textY - 1, SDL_Color{230, 230, 230, 255}, 12);
#endif
    }
}

void BattleHud::drawBossHeader(SDL_Renderer* renderer, int screenW) {
    constexpr int topPadding = 16;
    constexpr int barTop = 62;
    constexpr int barHeight = 28;

    const int barWidth = std::min(screenW - 120, 900);
    const int barX = (screenW - barWidth) / 2;

    const int safeMaxHp = std::max(1, model_.bossMaxHp);
    const int safeCurrentHp = std::clamp(model_.bossCurrentHp, 0, safeMaxHp);
    const int displayHp = getDisplayedHp(bossHpTransition_, safeCurrentHp);

#ifdef BATTLE_ENABLE_TTF
    drawTextCentered(fontCache_.get(), renderer, model_.bossTitle, screenW / 2, topPadding, SDL_Color{245, 245, 245, 255}, 32);
#endif

    SDL_Rect barBg{barX, barTop, barWidth, barHeight};
    SDL_SetRenderDrawColor(renderer, 28, 20, 24, 220);
    SDL_RenderFillRect(renderer, &barBg);

    if (bossHpTransition_.active && bossHpTransition_.fromHp > bossHpTransition_.toHp) {
        const int oldPercent = static_cast<int>(std::round((100.0f * bossHpTransition_.fromHp) / std::max(1, bossHpTransition_.maxHp)));
        const int oldFillWidth = (barWidth * oldPercent) / 100;
        SDL_Rect damageLayer{barX + 2, barTop + 2, std::max(0, oldFillWidth - 4), barHeight - 4};
        SDL_SetRenderDrawColor(renderer, 100, 20, 20, 200);
        SDL_RenderFillRect(renderer, &damageLayer);
    }

    const int hpPercent = static_cast<int>(std::round((100.0f * displayHp) / safeMaxHp));
    const int fillWidth = (barWidth * hpPercent) / 100;
    SDL_Rect barFill{barX + 2, barTop + 2, std::max(0, fillWidth - 4), barHeight - 4};
    SDL_SetRenderDrawColor(renderer, 196, 54, 54, 240);
    SDL_RenderFillRect(renderer, &barFill);

    SDL_SetRenderDrawColor(renderer, 230, 230, 235, 255);
    SDL_RenderDrawRect(renderer, &barBg);

#ifdef BATTLE_ENABLE_TTF
    drawTextCentered(fontCache_.get(), renderer, std::to_string(hpPercent) + "%", screenW / 2, barTop + 3, SDL_Color{255, 255, 255, 255}, 22);
#endif
}

void BattleHud::drawCharacterStatus(SDL_Renderer* renderer,
                                    int screenW,
                                    int screenH,
                                    const std::map<std::string, SDL_Texture*>& iconByAsset) {
    (void)screenW;
    if (model_.characters.empty()) {
        return;
    }

    constexpr int iconWidth = 140;
    constexpr int iconHeight = 140;
    constexpr int hpBarHeight = 14;
    constexpr int ultimateRadius = 10;
    constexpr int ultimateSpacing = 24;
    constexpr int marginLeft = 20;
    constexpr int marginBottom = 30;
    constexpr int charSpacing = 30;

    const int baseIconY = screenH - iconHeight - marginBottom;

    for (int idx = 0; idx < static_cast<int>(model_.characters.size()); ++idx) {
        const HudCharacterModel& character = model_.characters[static_cast<size_t>(idx)];
        const int iconX = marginLeft + (idx * (iconWidth + charSpacing));
        const int iconY = baseIconY;

        int shakeOffsetX = 0;
        const float damageAlpha = getDamageFlashAlpha(idx, shakeOffsetX);
        const int finalIconX = iconX + shakeOffsetX;

        auto iconIt = iconByAsset.find(character.key);
        if (iconIt != iconByAsset.end() && iconIt->second != nullptr) {
            const SDL_Rect dstRect{finalIconX, iconY, iconWidth, iconHeight};
            drawCroppedTexture(renderer, iconIt->second, dstRect);

            if (damageAlpha > 0.0f) {
                const Uint8 redComponent = static_cast<Uint8>(255 - (damageAlpha * 100));
                const Uint8 greenBlueComponent = static_cast<Uint8>(255 - (damageAlpha * 175));
                SDL_SetTextureColorMod(iconIt->second, redComponent, greenBlueComponent, greenBlueComponent);
                drawCroppedTexture(renderer, iconIt->second, dstRect);
                SDL_SetTextureColorMod(iconIt->second, 255, 255, 255);
            }
        }

        const int hpBarY = iconY + iconHeight - hpBarHeight;
        const int currentHp = std::clamp(character.currentHp, 0, std::max(1, character.maxHp));
        const int maxHp = std::max(1, character.maxHp);
        HpTransition& transition = characterHpTransitions_[static_cast<size_t>(idx)];
        const int displayHp = getDisplayedHp(transition, currentHp);

        SDL_Rect hpBarBg{iconX, hpBarY, iconWidth, hpBarHeight};
        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 200);
        SDL_RenderFillRect(renderer, &hpBarBg);
        SDL_SetRenderDrawColor(renderer, 40, 45, 60, 255);
        SDL_RenderDrawRect(renderer, &hpBarBg);

        if (transition.active && transition.fromHp > transition.toHp) {
            const int oldPercent = std::clamp((transition.fromHp * 100) / std::max(1, transition.maxHp), 0, 100);
            const int oldWidth = (oldPercent * iconWidth) / 100;
            SDL_Rect damageLayer{iconX, hpBarY, oldWidth, hpBarHeight};
            SDL_SetRenderDrawColor(renderer, 120, 40, 40, 180);
            SDL_RenderFillRect(renderer, &damageLayer);
        }

        const int hpPercent = std::clamp((displayHp * 100) / maxHp, 0, 100);
        const int hpBarWidth = (hpPercent * iconWidth) / 100;
        SDL_Rect hpBarFg{iconX, hpBarY, hpBarWidth, hpBarHeight};
        SDL_SetRenderDrawColor(renderer, 76, 175, 80, 220);
        SDL_RenderFillRect(renderer, &hpBarFg);

        const int circlesStartY = iconY + iconHeight + 12;
        const int maxUltimatePoints = std::max(1, character.ultimateRequired);
        const int currentUltimateCharge = std::clamp(character.ultimateCharge, 0, maxUltimatePoints);

        for (int i = 0; i < maxUltimatePoints; ++i) {
            const int circleX = iconX + (iconWidth / 2) - ((maxUltimatePoints - 1) * ultimateSpacing / 2) + i * ultimateSpacing;
            const int circleY = circlesStartY;

            if (i < currentUltimateCharge) {
                drawFilledCircle(renderer, circleX, circleY, ultimateRadius - 2, SDL_Color{100, 190, 255, 220});
            }

            SDL_SetRenderDrawColor(renderer, 100, 120, 200, 180);
            SDL_Rect circleRect{circleX - ultimateRadius, circleY - ultimateRadius, ultimateRadius * 2, ultimateRadius * 2};
            SDL_RenderDrawRect(renderer, &circleRect);
        }
    }
}

} // namespace battle::ui
