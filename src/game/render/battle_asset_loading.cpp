#include "battle_asset_loading.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
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
        std::cerr << "[Battle] IMG_Load failed for '" << path << "': " << IMG_GetError() << "\n";
        return std::nullopt;
    }

    SDL_RendererInfo rendererInfo{};
    if (SDL_GetRendererInfo(renderer, &rendererInfo) == 0 && rendererInfo.max_texture_width > 0
        && rendererInfo.max_texture_height > 0) {
        if (surface->w > rendererInfo.max_texture_width || surface->h > rendererInfo.max_texture_height) {
            std::cerr << "[Battle] Texture too large for renderer: '" << path << "' (" << surface->w << "x" << surface->h
                      << "), max is " << rendererInfo.max_texture_width << "x" << rendererInfo.max_texture_height
                      << ". Resize the image and try again.\n";
            SDL_FreeSurface(surface);
            return std::nullopt;
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr) {
        std::cerr << "[Battle] SDL_CreateTextureFromSurface failed for '" << path << "': " << SDL_GetError() << "\n";
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

#ifdef BATTLE_ENABLE_IMAGE
    if (renderer == nullptr || assetName.empty()) {
        return std::nullopt;
    }

    const std::vector<std::string> spriteCandidates = {
        "assets/combat/sprites/" + assetName + ".png",
        "assets/combat/sprites/" + assetName + ".webp"
    };
    for (const std::string& candidate : spriteCandidates) {
        const std::string resolvedPath = platform::path::resolvePath(candidate);
        if (!std::filesystem::exists(resolvedPath)) {
            continue;
        }

        SDL_Surface* source = IMG_Load(resolvedPath.c_str());
        if (source == nullptr) {
            continue;
        }

        const int side = std::max(1, std::min(source->w, source->h));
        SDL_Surface* cropped = SDL_CreateRGBSurfaceWithFormat(0, side, side, 32, SDL_PIXELFORMAT_RGBA32);
        if (cropped == nullptr) {
            SDL_FreeSurface(source);
            continue;
        }

        SDL_FillRect(cropped, nullptr, SDL_MapRGBA(cropped->format, 0, 0, 0, 0));
        const SDL_Rect srcRect{
            std::max(0, (source->w - side) / 2),
            0,
            side,
            side
        };
        SDL_BlitSurface(source, &srcRect, cropped, nullptr);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, cropped);
        SDL_FreeSurface(cropped);
        SDL_FreeSurface(source);
        if (texture != nullptr) {
            return texture;
        }
    }
#else
    (void)renderer;
    (void)assetName;
#endif
    return std::nullopt;
}

} // namespace battle::render
