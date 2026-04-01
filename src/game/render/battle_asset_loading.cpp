#include "battle_asset_loading.h"

#include <algorithm>
#include <filesystem>
#include <vector>

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

std::optional<SDL_Texture*> tryLoadTextureFromPath(SDL_Renderer* renderer, const std::string& path) {
#ifdef BATTLE_ENABLE_IMAGE
    if (renderer == nullptr || path.empty()) {
        return std::nullopt;
    }

    const std::string resolvedPath = platform::path::resolvePath(path);
    if (!std::filesystem::exists(resolvedPath)) {
        return std::nullopt;
    }

    SDL_Surface* surface = IMG_Load(resolvedPath.c_str());
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
    (void)path;
    return std::nullopt;
#endif
}

std::optional<SDL_Texture*> tryLoadCombatSpriteTexture(SDL_Renderer* renderer, const std::string& assetName) {
    const std::vector<std::string> candidates = {
        "assets/combat/sprites/" + assetName + ".png",
        "assets/combat/sprites/" + assetName + ".webp"
    };
    for (const std::string& candidate : candidates) {
        if (const auto texture = tryLoadTextureFromPath(renderer, candidate); texture.has_value()) {
            return texture;
        }
    }
    (void)assetName;
    return std::nullopt;
}

std::optional<SDL_Texture*> tryLoadCombatIconTexture(SDL_Renderer* renderer, const std::string& assetName) {
    const std::vector<std::string> candidates = {
        "assets/combat/icons/" + assetName + ".png",
        "assets/combat/icons/" + assetName + ".webp"
    };
    for (const std::string& candidate : candidates) {
        if (const auto texture = tryLoadTextureFromPath(renderer, candidate); texture.has_value()) {
            return texture;
        }
    }
    (void)assetName;
    return std::nullopt;
}

} // namespace battle::render
