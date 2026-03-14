#ifndef BATTLE_SCENE_TYPES_H
#define BATTLE_SCENE_TYPES_H

#include <string>

#include <SDL2/SDL.h>

namespace battle::render {

struct SceneEntity {
    std::string key;
    std::string assetName;
    bool isBoss = false;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    SDL_Color fallbackColor{200, 200, 200, 255};
    int partyIndex = -1;
    bool visible = true;
};

} // namespace battle::render

#endif
