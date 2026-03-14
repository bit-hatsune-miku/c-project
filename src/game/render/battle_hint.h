#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include <SDL2/SDL.h>

namespace battle::ui {

// A transient hint banner rendered just below the boss HP bar.
// Fades in when set() is called and fades out when clear() is called.
// Drawing and tick() are handled by BattleHud.
struct BattleHint {
    std::string text;

    static constexpr float kFadeInMs  = 220.0f;
    static constexpr float kFadeOutMs = 320.0f;

    void set(const std::string& msg, Uint32 displayMs = 0) {
        text    = msg;
        showMs_ = SDL_GetTicks64();
        hideMs_ = 0;
        autoHideAtMs_ = (displayMs > 0) ? (showMs_ + displayMs) : 0;
    }

    // Start fade-out; text is preserved until the animation finishes.
    void clear() {
        if (!text.empty() && hideMs_ == 0) {
            hideMs_ = SDL_GetTicks64();
            autoHideAtMs_ = 0;
        }
    }

    // True while text is visible or still fading out.
    bool active() const {
        if (text.empty()) return false;
        if (hideMs_ == 0) return true;
        const float elapsed = static_cast<float>(SDL_GetTicks64() - hideMs_);
        return elapsed < kFadeOutMs;
    }

    // 0-255 alpha for the current frame.
    Uint8 currentAlpha() const {
        if (text.empty() || showMs_ == 0) return 0;
        const Uint64 now = SDL_GetTicks64();
        if (hideMs_ != 0) {
            const float t = std::clamp(
                static_cast<float>(now - hideMs_) / kFadeOutMs, 0.0f, 1.0f);
            return static_cast<Uint8>(std::lround((1.0f - t) * 255.0f));
        }
        const float t = std::clamp(
            static_cast<float>(now - showMs_) / kFadeInMs, 0.0f, 1.0f);
        return static_cast<Uint8>(std::lround(t * 255.0f));
    }

    // Call once per frame; cleans up after the fade-out finishes.
    void tick() {
        if (autoHideAtMs_ != 0 && hideMs_ == 0 && SDL_GetTicks64() >= autoHideAtMs_) {
            clear();
        }
        if (hideMs_ != 0 && !active()) {
            text.clear();
            showMs_ = 0;
            hideMs_ = 0;
            autoHideAtMs_ = 0;
        }
    }

private:
    Uint64 showMs_ = 0;
    Uint64 hideMs_ = 0;
    Uint64 autoHideAtMs_ = 0;
};

} // namespace battle::ui
