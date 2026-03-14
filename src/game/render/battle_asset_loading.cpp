#include "battle_asset_loading.h"

#include <algorithm>
#include <filesystem>

#include "../../platform/path_resolution.h"

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle::render {

SDL_Color colorFromKey(const std::string& key, bool boss) {
    unsigned hash = 2166136261u;
    for (char c : key) {
        hash ^= static_cast<unsigned>(static_cast<unsigned char>(c));
        hash *= 16777619u;
    }
    const Uint8 r = static_cast<Uint8>(80 + (hash & 0x7F));
    const Uint8 g = static_cast<Uint8>(80 + ((hash >> 8) & 0x7F));
    const Uint8 b = static_cast<Uint8>(80 + ((hash >> 16) & 0x7F));
    return boss ? SDL_Color{static_cast<Uint8>(std::min(255, r + 30)), 90, 90, 255} : SDL_Color{r, g, b, 255};
}

std::optional<SDL_Texture*> tryLoadCombatSpriteTexture(SDL_Renderer* renderer, const std::string& assetName) {
#ifdef BATTLE_ENABLE_IMAGE
    const std::string pngPath = platform::path::resolvePath("assets/combat/sprites/" + assetName + ".png");
    if (!std::filesystem::exists(pngPath)) {
        return std::nullopt;
    }
    SDL_Surface* surface = IMG_Load(pngPath.c_str());
    if (surface == nullptr) {
        return std::nullopt;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr) {
        return std::nullopt;
    }
    return texture;
#else
    (void)renderer;
    (void)assetName;
    return std::nullopt;
#endif
}

std::optional<SDL_Texture*> tryLoadCombatIconTexture(SDL_Renderer* renderer, const std::string& assetName) {
#ifdef BATTLE_ENABLE_IMAGE
    const std::string pngPath = platform::path::resolvePath("assets/combat/icons/" + assetName + ".png");
    if (!std::filesystem::exists(pngPath)) {
        return std::nullopt;
    }
    SDL_Surface* surface = IMG_Load(pngPath.c_str());
    if (surface == nullptr) {
        return std::nullopt;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr) {
        return std::nullopt;
    }
    return texture;
#else
    (void)renderer;
    (void)assetName;
    return std::nullopt;
#endif
}

} // namespace battle::render
