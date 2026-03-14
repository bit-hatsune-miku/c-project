#ifndef BATTLE_COMBAT_BEGIN_ANIMATION_H
#define BATTLE_COMBAT_BEGIN_ANIMATION_H

#include <SDL2/SDL.h>

namespace battle::render {

class BattleCombatBeginAnimation {
public:
    void reset(SDL_Texture* mikuSprite, SDL_Texture* bossSprite);
    void update(float deltaSeconds);
    void render(SDL_Renderer* renderer, int screenWidth, int screenHeight) const;

    bool isActive() const;

private:
    enum class Phase {
        Enter,
        Hold,
        Exit,
        Complete
    };

    static void drawSpriteOrFallback(SDL_Renderer* renderer,
                                     SDL_Texture* texture,
                                     const SDL_Rect& dst,
                                     const SDL_Color& fallbackColor);

    float phaseT(float duration) const;

    SDL_Texture* mikuSprite_ = nullptr;
    SDL_Texture* bossSprite_ = nullptr;

    Phase phase_ = Phase::Complete;
    float phaseTimer_ = 0.0f;
};

} // namespace battle::render

#endif
