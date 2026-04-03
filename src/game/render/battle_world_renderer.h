#ifndef BATTLE_WORLD_RENDERER_H
#define BATTLE_WORLD_RENDERER_H

#include <functional>
#include <map>
#include <vector>

#include <SDL2/SDL.h>

#include "battle_stage.h"
#include "battle_scene_types.h"
#include "camera_3d.h"

namespace battle::render {

using WorldShakeOffsetFn = std::function<float(const SceneEntity&)>;

SDL_Texture* createBattleWorldFloorTileTexture(SDL_Renderer* renderer);
SDL_Texture* createBattleWorldFloorTileTexture(SDL_Renderer* renderer,
                                               SDL_Color baseColor,
                                               SDL_Color accentColor);

void renderBattleBackdrop(SDL_Renderer* renderer,
                          int screenWidth,
                          int screenHeight,
                          const Camera3D& camera,
                          const StageRenderData& stage);

void renderBattleFloor(SDL_Renderer* renderer,
                       int screenWidth,
                       int screenHeight,
                       const Camera3D& camera,
                       const StageFloorDefinition& floor,
                       SDL_Texture* floorTileTexture);

void renderBattleProps(SDL_Renderer* renderer,
                       int screenWidth,
                       int screenHeight,
                       const Camera3D& camera,
                       const std::vector<StagePropRenderItem>& props);

void renderBattleEntities(SDL_Renderer* renderer,
                          int screenWidth,
                          int screenHeight,
                          const Camera3D& camera,
                          const std::vector<SceneEntity>& entities,
                          int focusedEntityIndex,
                          const std::map<std::string, SDL_Texture*>& textureByAsset,
                          float frameAccumulator,
                          const WorldShakeOffsetFn& shakeOffsetFn);

void renderBattleWorld(SDL_Renderer* renderer,
                       int screenWidth,
                       int screenHeight,
                       const Camera3D& camera,
                       const StageRenderData& stage,
                       const std::vector<SceneEntity>& entities,
                       int focusedEntityIndex,
                       const std::map<std::string, SDL_Texture*>& textureByAsset,
                       float frameAccumulator,
                       const WorldShakeOffsetFn& shakeOffsetFn);

} // namespace battle::render

#endif
