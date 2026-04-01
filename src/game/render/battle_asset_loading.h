#ifndef BATTLE_ASSET_LOADING_H
#define BATTLE_ASSET_LOADING_H

#include <optional>
#include <string>

#include <SDL2/SDL.h>

namespace battle::render {

SDL_Color colorFromKey(const std::string& key, bool boss);

std::optional<SDL_Texture*> tryLoadTextureFromPath(SDL_Renderer* renderer, const std::string& path);
std::optional<SDL_Texture*> tryLoadCombatSpriteTexture(SDL_Renderer* renderer, const std::string& assetName);
std::optional<SDL_Texture*> tryLoadCombatIconTexture(SDL_Renderer* renderer, const std::string& assetName);

} // namespace battle::render

#endif
