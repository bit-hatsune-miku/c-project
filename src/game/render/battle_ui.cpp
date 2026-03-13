#include "battle_ui.h"

#include "../core/easing.h"
#include "../core/turn_system.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <vector>

#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

namespace battle::ui {
namespace {

struct HpTransition {
    bool active = false;
    float elapsed = 0.0f;
    float duration = 0.5f;
    int oldHp = 0;
    int newHp = 0;
    int maxHp = 1;
    Uint32 lastTickMs = 0;
};

struct CharacterHpTransitions {
    std::map<int, HpTransition> transitions;
};

struct DamageFlashState {
    bool active = false;
    float elapsed = 0.0f;
    float duration = 1.0f;
    Uint32 lastTickMs = 0;
};

struct CharacterDamageFlashes {
    std::map<int, DamageFlashState> flashes;
};

CharacterHpTransitions gCharacterHpTransitions;
HpTransition gBossHpTransition;
CharacterDamageFlashes gCharacterDamageFlashes;

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

#ifdef BATTLE_ENABLE_TTF
std::map<int, TTF_Font*> gBattleFonts;

TTF_Font* openBestAvailableBattleFont(int ptSize) {
    const std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf"
    };

    for (const auto& path : candidates) {
        TTF_Font* font = TTF_OpenFont(path.c_str(), ptSize);
        if (font != nullptr) {
            return font;
        }
    }
    return nullptr;
}

TTF_Font* getBattleFont(int ptSize) {
    auto it = gBattleFonts.find(ptSize);
    if (it != gBattleFonts.end()) {
        return it->second;
    }

    TTF_Font* loaded = openBestAvailableBattleFont(ptSize);
    gBattleFonts[ptSize] = loaded;
    return loaded;
}

void drawTextTtf(SDL_Renderer* renderer, const std::string& text, int centerX, int y, SDL_Color color, int fontSize) {
    TTF_Font* font = getBattleFont(fontSize);
    if (font == nullptr || text.empty()) {
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

    SDL_Rect dst{centerX - surface->w / 2, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surface);
}

void drawTextTtfAt(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color, int fontSize) {
    TTF_Font* font = getBattleFont(fontSize);
    if (font == nullptr || text.empty()) {
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

    SDL_Rect dst{x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surface);
}
#endif

} // namespace

void drawTurnOrderUI(SDL_Renderer* renderer,
                     const TurnState& turnState,
                     const std::map<std::string, SDL_Texture*>& iconByAsset,
                     int activeActorIndex) {
    constexpr int cardWidth = 100;
    constexpr int cardHeight = 50;
    constexpr int cardSpacing = 4;
    constexpr int startX = 16;
    constexpr int startY = 16;
    constexpr int maxCards = 10;

    std::vector<int> sortedActorIndices(turnState.actors.size());
    std::iota(sortedActorIndices.begin(), sortedActorIndices.end(), 0);
    std::sort(sortedActorIndices.begin(), sortedActorIndices.end(), [&](int lhs, int rhs) {
        constexpr float eps = 0.0001f;
        const TurnActor& a = turnState.actors[static_cast<size_t>(lhs)];
        const TurnActor& b = turnState.actors[static_cast<size_t>(rhs)];
        if (std::fabs(a.currentActionValue - b.currentActionValue) > eps) {
            return a.currentActionValue < b.currentActionValue;
        }
        return turn::turnPriorityLess(a, b);
    });

    const int cardCount = std::min(static_cast<int>(sortedActorIndices.size()), maxCards);
    const float animTime = SDL_GetTicks() / 1000.0f;
    constexpr float cycleDuration = 1.0f;
    const float phase = std::fmod(animTime, cycleDuration) / cycleDuration;

    for (int cardIndex = 0; cardIndex < cardCount; ++cardIndex) {
        const int actorIndex = sortedActorIndices[static_cast<size_t>(cardIndex)];
        const TurnActor& actor = turnState.actors[static_cast<size_t>(actorIndex)];
        int cardX = startX;
        if (actorIndex == activeActorIndex) {
            // Smooth single bounce: move right, then settle back with slight overshoot.
            float offset = 0.0f;
            if (phase < 0.35f) {
                const float t = phase / 0.35f;
                offset = battle::easing::lerp(0.0f, 10.0f, battle::easing::easeOutCubic(t));
            } else if (phase < 0.75f) {
                const float t = (phase - 0.35f) / 0.40f;
                offset = battle::easing::lerp(10.0f, 0.0f, battle::easing::easeOutBack(t));
            } else {
                offset = 0.0f;
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
            int texW = 0;
            int texH = 0;
            SDL_QueryTexture(iconIt->second, nullptr, nullptr, &texW, &texH);

            const SDL_Rect dstRect{cardX, cardY, cardWidth, cardHeight};
            const float aspectDst = static_cast<float>(cardWidth) / cardHeight;
            const float aspectSrc = static_cast<float>(texW) / texH;

            SDL_Rect srcRect;
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

            SDL_RenderCopy(renderer, iconIt->second, &srcRect, &dstRect);
        }

        const int av = static_cast<int>(actor.currentActionValue);
        const std::string avText = std::to_string(av);
        const int bgWidth = static_cast<int>(avText.size()) * 10 + 8;
        const int textX = cardX + cardWidth - bgWidth;
        const int textY = cardY + cardHeight - 16;
        SDL_Rect avBg{textX - 2, textY - 1, bgWidth, 10};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(renderer, &avBg);

#ifdef BATTLE_ENABLE_TTF
        drawTextTtfAt(renderer, avText, textX, textY - 1, SDL_Color{230, 230, 230, 255}, 12);
#endif
    }
}

void drawCharacterStatusUI(SDL_Renderer* renderer,
                           int screenW,
                           int screenH,
                           const BattleManager& manager,
                           const BattleState& battleState,
                           const std::map<std::string, SDL_Texture*>& iconByAsset) {
    if (battleState.party.empty()) {
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

    int baseIconX = marginLeft;
    const int baseIconY = screenH - iconHeight - marginBottom;

    for (int idx = 0; idx < static_cast<int>(battleState.party.size()); ++idx) {
        const CharacterDefinition& character = battleState.party[idx];

        const int iconX = baseIconX + (idx * (iconWidth + charSpacing));
        const int iconY = baseIconY;

        // Apply shake effect if character is damaged
        int finalIconX = iconX;
        float damageAlpha = 0.0f;
        auto damageIt = gCharacterDamageFlashes.flashes.find(idx);
        if (damageIt != gCharacterDamageFlashes.flashes.end() && damageIt->second.active) {
            DamageFlashState& flash = damageIt->second;
            const Uint32 nowMs = SDL_GetTicks();
            const float dt = (nowMs - flash.lastTickMs) / 1000.0f;
            flash.elapsed += std::max(0.0f, dt);
            flash.lastTickMs = nowMs;

            const float t = easing::clamp01(flash.elapsed / flash.duration);
            
            // Shake effect - oscillate horizontally
            const float shakeFrequency = 8.0f; // oscillations per second
            const float shakeAmount = 6.0f; // pixels of movement
            const float shakePhase = t * shakeFrequency * 3.14159f * 2.0f;
            finalIconX = iconX + static_cast<int>(std::sin(shakePhase) * shakeAmount);
            
            // Red tint intensity - much more transparent, max 25% opacity
            damageAlpha = (1.0f - t) * 0.25f;
            
            if (t >= 1.0f) {
                flash.active = false;
            }
        }

        const std::string iconKey = character.key;
        auto iconIt = iconByAsset.find(iconKey);
        if (iconIt != iconByAsset.end() && iconIt->second != nullptr) {
            int texW = 0;
            int texH = 0;
            SDL_QueryTexture(iconIt->second, nullptr, nullptr, &texW, &texH);

            const SDL_Rect dstRect{finalIconX, iconY, iconWidth, iconHeight};
            const float aspectDst = static_cast<float>(iconWidth) / iconHeight;
            const float aspectSrc = static_cast<float>(texW) / texH;

            SDL_Rect srcRect;
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

            SDL_RenderCopy(renderer, iconIt->second, &srcRect, &dstRect);
            
            // Apply red damage tint by rendering the icon again with color modulation
            // This only affects non-transparent pixels
            if (damageAlpha > 0.0f) {
                // Calculate blended color: mix white with red based on damage intensity
                const Uint8 redComponent = static_cast<Uint8>(255 - (damageAlpha * 100));
                const Uint8 greenBlueComponent = static_cast<Uint8>(255 - (damageAlpha * 175));
                
                SDL_SetTextureColorMod(iconIt->second, redComponent, greenBlueComponent, greenBlueComponent);
                SDL_RenderCopy(renderer, iconIt->second, &srcRect, &dstRect);
                SDL_SetTextureColorMod(iconIt->second, 255, 255, 255);  // Reset to normal
            }
        }

        const int hpBarY = iconY + iconHeight - hpBarHeight;
        const int currentHp = manager.getCharacterCurrentHp(idx);
        const int maxHp = std::max(1, manager.getCharacterMaxHp(idx));

        // Background
        SDL_Rect hpBarBg{iconX, hpBarY, iconWidth, hpBarHeight};
        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 200);
        SDL_RenderFillRect(renderer, &hpBarBg);
        SDL_SetRenderDrawColor(renderer, 40, 45, 60, 255);
        SDL_RenderDrawRect(renderer, &hpBarBg);

        // Check for HP transition
        auto transIt = gCharacterHpTransitions.transitions.find(idx);
        if (transIt != gCharacterHpTransitions.transitions.end() && transIt->second.active) {
            HpTransition& trans = transIt->second;
            const Uint32 nowMs = SDL_GetTicks();
            const float dt = (nowMs - trans.lastTickMs) / 1000.0f;
            trans.elapsed += std::max(0.0f, dt);
            trans.lastTickMs = nowMs;

            const float t = easing::clamp01(trans.elapsed / trans.duration);
            const float eased = easing::easeOutCubic(t);

            // Interpolate HP
            const float interpHp = easing::lerp(static_cast<float>(trans.oldHp), static_cast<float>(trans.newHp), eased);
            const int displayHp = static_cast<int>(std::round(interpHp));

            // Draw damage layer (darker red) showing lost HP
            if (trans.oldHp > trans.newHp) {
                const int oldPercent = std::clamp((trans.oldHp * 100) / trans.maxHp, 0, 100);
                const int oldWidth = (oldPercent * iconWidth) / 100;
                SDL_Rect damageLayer{iconX, hpBarY, oldWidth, hpBarHeight};
                SDL_SetRenderDrawColor(renderer, 120, 40, 40, 180);
                SDL_RenderFillRect(renderer, &damageLayer);
            }

            // Draw current HP (animating)
            const int hpPercent = std::clamp((displayHp * 100) / trans.maxHp, 0, 100);
            const int hpBarWidth = (hpPercent * iconWidth) / 100;
            SDL_Rect hpBarFg{iconX, hpBarY, hpBarWidth, hpBarHeight};
            SDL_SetRenderDrawColor(renderer, 76, 175, 80, 220);
            SDL_RenderFillRect(renderer, &hpBarFg);

            if (t >= 1.0f) {
                trans.active = false;
                trans.oldHp = trans.newHp;
            }
        } else {
            // No transition, draw normally
            const int hpPercent = std::clamp((currentHp * 100) / maxHp, 0, 100);
            const int hpBarWidth = (hpPercent * iconWidth) / 100;
            SDL_Rect hpBarFg{iconX, hpBarY, hpBarWidth, hpBarHeight};
            SDL_SetRenderDrawColor(renderer, 76, 175, 80, 220);
            SDL_RenderFillRect(renderer, &hpBarFg);
        }

        const int circlesStartY = iconY + iconHeight + 12;
        const int maxUltimatePoints = std::max(1, manager.getCharacterUltimateRequired(idx));
        const int currentUltimateCharge = std::clamp(manager.getCharacterUltimateCharge(idx), 0, maxUltimatePoints);

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

void drawBossHeaderUI(SDL_Renderer* renderer,
                      int screenW,
                      const BattleState& battleState,
                      int bossCurrentHp,
                      int bossMaxHp) {
    constexpr int topPadding = 16;
    constexpr int barTop = 62;
    constexpr int barHeight = 28;

    const int barWidth = std::min(screenW - 120, 900);
    const int barX = (screenW - barWidth) / 2;

    const int safeMaxHp = std::max(1, bossMaxHp);
    const int safeCurrentHp = std::clamp(bossCurrentHp, 0, safeMaxHp);

#ifdef BATTLE_ENABLE_TTF
    drawTextTtf(renderer, battleState.boss.title, screenW / 2, topPadding, SDL_Color{245, 245, 245, 255}, 32);
#endif

    SDL_Rect barBg{barX, barTop, barWidth, barHeight};
    SDL_SetRenderDrawColor(renderer, 28, 20, 24, 220);
    SDL_RenderFillRect(renderer, &barBg);

    const Uint32 nowMs = SDL_GetTicks();
    int displayHp = safeCurrentHp;

    // Check for boss HP transition
    if (gBossHpTransition.active) {
        const float dt = (nowMs - gBossHpTransition.lastTickMs) / 1000.0f;
        gBossHpTransition.elapsed += std::max(0.0f, dt);
        gBossHpTransition.lastTickMs = nowMs;

        const float t = easing::clamp01(gBossHpTransition.elapsed / gBossHpTransition.duration);
        const float eased = easing::easeOutCubic(t);

        // Interpolate HP
        const float interpHp = easing::lerp(static_cast<float>(gBossHpTransition.oldHp), static_cast<float>(gBossHpTransition.newHp), eased);
        displayHp = static_cast<int>(std::round(interpHp));

        // Draw damage layer (darker red) showing lost HP
        if (gBossHpTransition.oldHp > gBossHpTransition.newHp) {
            const int oldPercent = static_cast<int>(std::round((100.0f * gBossHpTransition.oldHp) / gBossHpTransition.maxHp));
            const int oldFillWidth = (barWidth * oldPercent) / 100;
            SDL_Rect damageLayer{barX + 2, barTop + 2, std::max(0, oldFillWidth - 4), barHeight - 4};
            SDL_SetRenderDrawColor(renderer, 100, 20, 20, 200);
            SDL_RenderFillRect(renderer, &damageLayer);
        }

        if (t >= 1.0f) {
            gBossHpTransition.active = false;
            gBossHpTransition.oldHp = gBossHpTransition.newHp;
        }
    }

    const int hpPercent = static_cast<int>(std::round((100.0f * displayHp) / safeMaxHp));
    const int fillWidth = (barWidth * hpPercent) / 100;

    SDL_Rect barFill{barX + 2, barTop + 2, std::max(0, fillWidth - 4), barHeight - 4};
    SDL_SetRenderDrawColor(renderer, 196, 54, 54, 240);
    SDL_RenderFillRect(renderer, &barFill);

    SDL_SetRenderDrawColor(renderer, 230, 230, 235, 255);
    SDL_RenderDrawRect(renderer, &barBg);

#ifdef BATTLE_ENABLE_TTF
    const std::string pctText = std::to_string(hpPercent) + "%";
    drawTextTtf(renderer, pctText, screenW / 2, barTop + 3, SDL_Color{255, 255, 255, 255}, 22);
#endif
}

void updateCharacterHp(int charIndex, int newHp, int maxHp) {
    auto& trans = gCharacterHpTransitions.transitions[charIndex];
    
    // On first call or when not animating, set the baseline
    if (!trans.active && trans.oldHp == 0 && trans.newHp == 0) {
        trans.oldHp = newHp;
        trans.newHp = newHp;
        trans.maxHp = maxHp;
        return;
    }
    
    // Check if HP actually changed
    int currentDisplayHp = trans.active ? trans.newHp : trans.oldHp;
    if (currentDisplayHp != newHp) {
        trans.active = true;
        trans.elapsed = 0.0f;
        trans.duration = 0.5f;
        trans.oldHp = currentDisplayHp;
        trans.newHp = newHp;
        trans.maxHp = maxHp;
        trans.lastTickMs = SDL_GetTicks();
    }
}

void updateBossHp(int newHp, int maxHp) {
    // On first call or when not animating, set the baseline
    if (!gBossHpTransition.active && gBossHpTransition.oldHp == 0 && gBossHpTransition.newHp == 0) {
        gBossHpTransition.oldHp = newHp;
        gBossHpTransition.newHp = newHp;
        gBossHpTransition.maxHp = maxHp;
        return;
    }
    
    // Check if HP actually changed
    int currentDisplayHp = gBossHpTransition.active ? gBossHpTransition.newHp : gBossHpTransition.oldHp;
    if (currentDisplayHp != newHp) {
        gBossHpTransition.active = true;
        gBossHpTransition.elapsed = 0.0f;
        gBossHpTransition.duration = 0.5f;
        gBossHpTransition.oldHp = currentDisplayHp;
        gBossHpTransition.newHp = newHp;
        gBossHpTransition.maxHp = maxHp;
        gBossHpTransition.lastTickMs = SDL_GetTicks();
    }
}

void triggerCharacterDamageFlash(int charIndex) {
    auto& flash = gCharacterDamageFlashes.flashes[charIndex];
    flash.active = true;
    flash.elapsed = 0.0f;
    flash.duration = 1.0f;
    flash.lastTickMs = SDL_GetTicks();
}

void shutdownFonts() {
#ifdef BATTLE_ENABLE_TTF
    for (auto& [_, font] : gBattleFonts) {
        if (font != nullptr) {
            TTF_CloseFont(font);
        }
    }
    gBattleFonts.clear();
#endif
}

} // namespace battle::ui
