#ifndef BATTLE_WORLD_RENDERER_H
#define BATTLE_WORLD_RENDERER_H

#include <functional>
#include <map>
#include <vector>

#include <SDL2/SDL.h>

#include "battle_scene_types.h"
#include "camera_3d.h"

namespace battle::render {

using WorldShakeOffsetFn = std::function<float(const SceneEntity&)>;

SDL_Texture* createBattleWorldFloorTileTexture(SDL_Renderer* renderer);

void renderBattleWorld(SDL_Renderer* renderer,
                       int screenWidth,
                       int screenHeight,
                       const Camera3D& camera,
                       SDL_Texture* floorTileTexture,
                       const std::vector<SceneEntity>& entities,
                       int focusedEntityIndex,
                       const std::map<std::string, SDL_Texture*>& textureByAsset,
                       float frameAccumulator,
                       const WorldShakeOffsetFn& shakeOffsetFn);

} // namespace battle::render

#endif
