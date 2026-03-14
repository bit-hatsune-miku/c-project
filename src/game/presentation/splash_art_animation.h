#pragma once

#include <string>

#include <SDL2/SDL.h>

#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

namespace battle {

struct SplashArtConfig {
    std::string abilityName;
    SDL_Texture* sprite = nullptr;
    float enterDuration = 0.40f;
    float holdDuration = 1.50f;
    float exitDuration = 0.35f;
    float maxDimAlpha = 0.62f;
};

class SplashArtAnimation {
public:
    explicit SplashArtAnimation(const SplashArtConfig& config);
    ~SplashArtAnimation();

    void start();
    void update(float deltaSeconds);
    void renderOverlay(SDL_Renderer* renderer, int screenW, int screenH);
    bool isComplete() const;
    void skip();

private:
    enum class Phase { Enter, Hold, Exit, Complete };

    float dimAlpha() const;
    float spriteOffsetY(int screenH) const;
    float textAlpha() const;
    void ensureTextTexture(SDL_Renderer* renderer);

    SplashArtConfig config_;
    Phase phase_ = Phase::Enter;
    float phaseTimer_ = 0.0f;

#ifdef BATTLE_ENABLE_TTF
    TTF_Font* font_ = nullptr;
    SDL_Texture* textTexture_ = nullptr;
    int textW_ = 0;
    int textH_ = 0;
#endif
};

} // namespace battle
