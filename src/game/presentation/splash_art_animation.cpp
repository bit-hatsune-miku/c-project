#include "splash_art_animation.h"

#include <algorithm>
#include <cmath>
#include <vector>

#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include "../../platform/path_resolution.h"
#include "../../platform/text_fallback.h"
#include "../core/easing.h"

namespace battle {

SplashArtAnimation::SplashArtAnimation(const SplashArtConfig& config)
    : config_(config) {}

SplashArtAnimation::~SplashArtAnimation() {
#ifdef BATTLE_ENABLE_TTF
    if (textTexture_) { SDL_DestroyTexture(textTexture_); textTexture_ = nullptr; }
    if (font_)        { TTF_CloseFont(font_);             font_        = nullptr; }
    if (cjkFont_)     { TTF_CloseFont(cjkFont_);          cjkFont_     = nullptr; }
#endif
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void SplashArtAnimation::start() {
    phase_      = Phase::Enter;
    phaseTimer_ = 0.0f;

#ifdef BATTLE_ENABLE_TTF
    if (!config_.abilityName.empty()) {
        if (textTexture_ != nullptr) {
            SDL_DestroyTexture(textTexture_);
            textTexture_ = nullptr;
        }
        if (font_ != nullptr) {
            TTF_CloseFont(font_);
            font_ = nullptr;
        }
        if (cjkFont_ != nullptr) {
            TTF_CloseFont(cjkFont_);
            cjkFont_ = nullptr;
        }
        for (const std::string& path : platform::path::preferredLatinFontPaths()) {
            font_ = TTF_OpenFont(path.c_str(), 44);
            if (font_ != nullptr) {
                break;
            }
        }
        for (const std::string& path : platform::path::preferredCjkFontPaths()) {
            cjkFont_ = TTF_OpenFont(path.c_str(), 44);
            if (cjkFont_ != nullptr) {
                break;
            }
        }
    }
#endif
}

void SplashArtAnimation::update(float dt) {
    if (phase_ == Phase::Complete) return;

    phaseTimer_ += dt;

    float phaseDuration = 0.0f;
    switch (phase_) {
        case Phase::Enter: phaseDuration = config_.enterDuration; break;
        case Phase::Hold:  phaseDuration = config_.holdDuration;  break;
        case Phase::Exit:  phaseDuration = config_.exitDuration;  break;
        default: return;
    }

    if (phaseTimer_ >= phaseDuration) {
        phaseTimer_ = 0.0f;
        switch (phase_) {
            case Phase::Enter: phase_ = Phase::Hold;     break;
            case Phase::Hold:  phase_ = Phase::Exit;     break;
            case Phase::Exit:  phase_ = Phase::Complete; break;
            default: break;
        }
    }
}

bool SplashArtAnimation::isComplete() const {
    return phase_ == Phase::Complete;
}

void SplashArtAnimation::skip() {
    if (phase_ == Phase::Enter || phase_ == Phase::Hold) {
        phase_      = Phase::Exit;
        phaseTimer_ = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// Private animation helpers
// ---------------------------------------------------------------------------

float SplashArtAnimation::dimAlpha() const {
    switch (phase_) {
        case Phase::Enter: {
            const float t = easing::clamp01(phaseTimer_ / config_.enterDuration);
            return config_.maxDimAlpha * easing::easeOutCubic(t);
        }
        case Phase::Hold: return config_.maxDimAlpha;
        case Phase::Exit: {
            const float t = easing::clamp01(phaseTimer_ / config_.exitDuration);
            return config_.maxDimAlpha * (1.0f - t);
        }
        default: return 0.0f;
    }
}

float SplashArtAnimation::spriteOffsetY(int screenH) const {
    // Negative = shift upward (off-screen top); positive = shift downward.
    // Enter: sprite starts 28% above its final position and eases into place.
    // Exit:  sprite continues downward (easeInQuad) until visually off-screen.
    const float enterSlide = static_cast<float>(screenH) * 0.28f;
    const float exitSlide  = static_cast<float>(screenH) * 0.32f;

    switch (phase_) {
        case Phase::Enter: {
            const float t     = easing::clamp01(phaseTimer_ / config_.enterDuration);
            const float eased = easing::easeOutCubic(t);
            return -enterSlide * (1.0f - eased);
        }
        case Phase::Hold: return 0.0f;
        case Phase::Exit: {
            const float t = easing::clamp01(phaseTimer_ / config_.exitDuration);
            return exitSlide * (t * t);  // easeInQuad
        }
        default: return 0.0f;
    }
}

float SplashArtAnimation::textAlpha() const {
    switch (phase_) {
        case Phase::Enter: {
            // Fades in over the second half of the enter phase.
            const float t     = easing::clamp01(phaseTimer_ / config_.enterDuration);
            const float tText = easing::clamp01((t - 0.55f) / 0.45f);
            return tText;
        }
        case Phase::Hold: return 1.0f;
        case Phase::Exit: {
            // Fades out during the first 60% of exit.
            const float t = easing::clamp01(phaseTimer_ / config_.exitDuration);
            return 1.0f - easing::clamp01(t / 0.6f);
        }
        default: return 0.0f;
    }
}

void SplashArtAnimation::ensureTextTexture(SDL_Renderer* renderer) {
#ifdef BATTLE_ENABLE_TTF
    if (textTexture_ != nullptr || (font_ == nullptr && cjkFont_ == nullptr) || config_.abilityName.empty()) return;

    const SDL_Color white{255, 255, 255, 255};
    TTF_Font* referenceFont = font_ != nullptr ? font_ : cjkFont_;
    const std::vector<platform::text::FontRun> runs = platform::text::buildFontRuns(config_.abilityName, font_, cjkFont_);
    if (runs.empty()) {
        return;
    }

    int totalWidth = 0;
    int maxHeight = 0;
    std::vector<SDL_Surface*> surfaces;
    surfaces.reserve(runs.size());
    for (const platform::text::FontRun& run : runs) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(run.font, run.text.c_str(), white);
        surfaces.push_back(surface);
        if (surface != nullptr) {
            totalWidth += surface->w;
            maxHeight = std::max(maxHeight, surface->h);
        }
    }

    if (totalWidth <= 0 || maxHeight <= 0) {
        for (SDL_Surface* surface : surfaces) {
            if (surface != nullptr) {
                SDL_FreeSurface(surface);
            }
        }
        return;
    }

    SDL_Surface* combined = SDL_CreateRGBSurfaceWithFormat(0, totalWidth, maxHeight, 32, SDL_PIXELFORMAT_RGBA32);
    if (combined == nullptr) {
        for (SDL_Surface* surface : surfaces) {
            if (surface != nullptr) {
                SDL_FreeSurface(surface);
            }
        }
        return;
    }

    SDL_SetSurfaceBlendMode(combined, SDL_BLENDMODE_BLEND);
    SDL_FillRect(combined, nullptr, SDL_MapRGBA(combined->format, 0, 0, 0, 0));

    int cursorX = 0;
    const int referenceAscent = TTF_FontAscent(referenceFont);
    for (size_t i = 0; i < runs.size(); ++i) {
        SDL_Surface* surface = surfaces[i];
        if (surface == nullptr) {
            continue;
        }

        SDL_Rect dst{cursorX, referenceAscent - TTF_FontAscent(runs[i].font), surface->w, surface->h};
        SDL_BlitSurface(surface, nullptr, combined, &dst);
        cursorX += surface->w;
        SDL_FreeSurface(surface);
    }

    textTexture_ = SDL_CreateTextureFromSurface(renderer, combined);
    if (textTexture_ != nullptr) {
        SDL_QueryTexture(textTexture_, nullptr, nullptr, &textW_, &textH_);
    }
    SDL_FreeSurface(combined);
#else
    (void)renderer;
#endif
}

// ---------------------------------------------------------------------------
// renderOverlay — call after world render, before SDL_RenderPresent
// ---------------------------------------------------------------------------

void SplashArtAnimation::renderOverlay(SDL_Renderer* renderer, int screenW, int screenH) {
    if (phase_ == Phase::Complete) return;

    // 1. Background dim -------------------------------------------------------
    const Uint8 dimA = static_cast<Uint8>(dimAlpha() * 255.0f);
    if (dimA > 0) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, dimA);
        const SDL_Rect full{0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &full);
    }

    // 2. Character sprite -------------------------------------------------------
    if (config_.sprite != nullptr) {
        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(config_.sprite, nullptr, nullptr, &texW, &texH);

        // Fit to ~80% of screen height, maintain aspect ratio, centre horizontally.
        const float targetH = static_cast<float>(screenH) * 0.80f;
        const float scale   = (texH > 0) ? targetH / static_cast<float>(texH) : 1.0f;
        const int   dstH    = static_cast<int>(targetH);
        const int   dstW    = static_cast<int>(static_cast<float>(texW) * scale);

        const float offsetY = spriteOffsetY(screenH);
        const int   dstX    = (screenW - dstW) / 2;
        // Place top edge at 8% from the top of the screen (character feet near 88%).
        const int   dstY    = static_cast<int>(static_cast<float>(screenH) * 0.08f + offsetY);

        SDL_SetTextureBlendMode(config_.sprite, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(config_.sprite, 255);
        const SDL_Rect dst{dstX, dstY, dstW, dstH};
        SDL_RenderCopy(renderer, config_.sprite, nullptr, &dst);
    }

    // 3. Ability name text -------------------------------------------------------
#ifdef BATTLE_ENABLE_TTF
    const float tAlpha = textAlpha();
    if (tAlpha > 0.01f && !config_.abilityName.empty()) {
        ensureTextTexture(renderer);
        if (textTexture_ != nullptr) {
            const Uint8 tA = static_cast<Uint8>(tAlpha * 255.0f);
            SDL_SetTextureBlendMode(textTexture_, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(textTexture_, tA);
            const int textX = (screenW - textW_) / 2;
            const int textY = static_cast<int>(static_cast<float>(screenH) * 0.88f);
            const SDL_Rect tDst{textX, textY, textW_, textH_};
            SDL_RenderCopy(renderer, textTexture_, nullptr, &tDst);
        }
    }
#endif
}

} // namespace battle
