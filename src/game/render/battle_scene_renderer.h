#pragma once

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "camera_3d.h"

namespace battle::render {

inline constexpr int kSuggestedBaseSpriteWidth = 140;
inline constexpr int kSuggestedBaseSpriteHeight = 260;
inline constexpr float kSpriteFrameTime = 0.15f;

inline constexpr float kBossCharacterDistanceWorld = 420.0f;
inline constexpr float kCharacterGapWorld = 1200.0f;
inline constexpr float kDuelCharacterSlotX = -0.5f * kCharacterGapWorld;
inline constexpr float kDuelBossSlotX = 0.0f;
inline constexpr float kDuelCharacterBaseY = 300.0f;

using SpritePathResolver = std::function<std::string(const std::string&)>;

struct WorldEntity {
    std::string key;
    std::string assetName;
    bool isBoss = false;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    SDL_Color fallbackColor{200, 200, 200, 255};
};

inline SDL_Color colorFromKey(const std::string& key, bool boss) {
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

struct SoftwareSceneRenderer {
    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* floorTileTexture = nullptr;
    std::map<std::string, SDL_Texture*> textureByAsset;
    int width = 0;
    int height = 0;

    ~SoftwareSceneRenderer() {
        destroy();
    }

    void destroy() {
        if (floorTileTexture != nullptr) {
            SDL_DestroyTexture(floorTileTexture);
            floorTileTexture = nullptr;
        }
        for (auto& [_, texture] : textureByAsset) {
            if (texture != nullptr) {
                SDL_DestroyTexture(texture);
            }
        }
        textureByAsset.clear();
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (surface != nullptr) {
            SDL_FreeSurface(surface);
            surface = nullptr;
        }
        width = 0;
        height = 0;
    }

    bool initialize(int newWidth, int newHeight, const std::vector<std::string>& assetNames, const SpritePathResolver& resolveSpritePath);
};

namespace detail {

inline constexpr float kFloorNearClipDepth = 1.0f;
inline constexpr float kClipEpsilon = 0.0001f;

struct FloorVertex {
    float worldX = 0.0f;
    float worldY = 0.0f;
    float depth = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

inline FloorVertex interpolateToNearPlane(const FloorVertex& from, const FloorVertex& to) {
    const float depthDelta = to.depth - from.depth;
    float t = 0.0f;
    if (std::fabs(depthDelta) > kClipEpsilon) {
        t = (kFloorNearClipDepth - from.depth) / depthDelta;
    }
    t = std::clamp(t, 0.0f, 1.0f);

    return FloorVertex{
        from.worldX + (to.worldX - from.worldX) * t,
        from.worldY + (to.worldY - from.worldY) * t,
        kFloorNearClipDepth,
        from.u + (to.u - from.u) * t,
        from.v + (to.v - from.v) * t
    };
}

inline int clipFloorQuadToNearPlane(const std::array<FloorVertex, 4>& input, std::array<FloorVertex, 6>& output) {
    std::array<FloorVertex, 6> working{};
    for (size_t i = 0; i < input.size(); ++i) {
        working[i] = input[i];
    }
    const int workingCount = static_cast<int>(input.size());

    int outCount = 0;
    for (int i = 0; i < workingCount; ++i) {
        const FloorVertex& current = working[static_cast<size_t>(i)];
        const FloorVertex& next = working[static_cast<size_t>((i + 1) % workingCount)];
        const bool currentInside = current.depth > kFloorNearClipDepth;
        const bool nextInside = next.depth > kFloorNearClipDepth;

        if (currentInside && nextInside) {
            output[static_cast<size_t>(outCount++)] = next;
            continue;
        }
        if (currentInside && !nextInside) {
            output[static_cast<size_t>(outCount++)] = interpolateToNearPlane(current, next);
            continue;
        }
        if (!currentInside && nextInside) {
            output[static_cast<size_t>(outCount++)] = interpolateToNearPlane(current, next);
            output[static_cast<size_t>(outCount++)] = next;
        }
    }

    return outCount;
}

inline bool clipLineToNearPlane(float& x0, float& y0, float& d0, float& x1, float& y1, float& d1) {
    if (d0 <= kFloorNearClipDepth && d1 <= kFloorNearClipDepth) {
        return false;
    }
    if (d0 > kFloorNearClipDepth && d1 > kFloorNearClipDepth) {
        return true;
    }

    const float depthDelta = d1 - d0;
    if (std::fabs(depthDelta) <= kClipEpsilon) {
        return false;
    }

    const float t = std::clamp((kFloorNearClipDepth - d0) / depthDelta, 0.0f, 1.0f);
    const float ix = x0 + (x1 - x0) * t;
    const float iy = y0 + (y1 - y0) * t;

    if (d0 <= kFloorNearClipDepth) {
        x0 = ix;
        y0 = iy;
        d0 = kFloorNearClipDepth;
    } else {
        x1 = ix;
        y1 = iy;
        d1 = kFloorNearClipDepth;
    }

    return true;
}

inline SDL_Texture* createFloorTileTexture(SDL_Renderer* renderer) {
    constexpr int texSize = 64;
    constexpr int cell = 16;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, texSize, texSize, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return nullptr;
    }

    const Uint32 c0 = SDL_MapRGBA(surface->format, 46, 49, 60, 255);
    const Uint32 c1 = SDL_MapRGBA(surface->format, 52, 56, 69, 255);

    SDL_Rect r{0, 0, cell, cell};
    for (int y = 0; y < texSize; y += cell) {
        for (int x = 0; x < texSize; x += cell) {
            r.x = x;
            r.y = y;
            const bool alt = ((x / cell) + (y / cell)) % 2 == 0;
            SDL_FillRect(surface, &r, alt ? c0 : c1);
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

inline std::optional<SDL_Texture*> tryLoadTexture(SDL_Renderer* renderer,
                                                  const std::string& assetName,
                                                  const SpritePathResolver& resolveSpritePath) {
#ifdef BATTLE_ENABLE_IMAGE
    const std::string path = resolveSpritePath ? resolveSpritePath(assetName) : std::string();
    if (!path.empty()) {
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface != nullptr) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            if (texture != nullptr) {
                return texture;
            }
        }
    }
#else
    (void)renderer;
    (void)assetName;
    (void)resolveSpritePath;
#endif
    return std::nullopt;
}

inline void drawFloorGridLines(SDL_Renderer* renderer, int screenW, int screenH, const battle::Camera3D& camera) {
    const float floorZ = 0.0f;
    const float centerX = -300.0f;
    const float centerY = 510.0f;
    const float tileSize = 200.0f;
    const int tilesX = 14;
    const int tilesY = 16;

    const float startX = centerX - (tilesX * tileSize * 0.5f);
    const float startY = centerY - (tilesY * tileSize * 0.30f);

    SDL_SetRenderDrawColor(renderer, 62, 70, 90, 160);
    for (int tx = 0; tx <= tilesX; ++tx) {
        float x0 = startX + tx * tileSize;
        float y0 = startY;
        float x1 = x0;
        float y1 = startY + tilesY * tileSize;
        float d0 = camera.getDepth(x0, y0, floorZ);
        float d1 = camera.getDepth(x1, y1, floorZ);
        if (!clipLineToNearPlane(x0, y0, d0, x1, y1, d1)) {
            continue;
        }

        const SDL_FPoint p0 = camera.worldToScreen(x0, y0, floorZ);
        const SDL_FPoint p1 = camera.worldToScreen(x1, y1, floorZ);
        if ((p0.x > -200.0f || p1.x > -200.0f) && (p0.x < screenW + 200.0f || p1.x < screenW + 200.0f) &&
            (p0.y > -200.0f || p1.y > -200.0f) && (p0.y < screenH + 200.0f || p1.y < screenH + 200.0f)) {
            SDL_RenderDrawLineF(renderer, p0.x, p0.y, p1.x, p1.y);
        }
    }
    for (int ty = 0; ty <= tilesY; ++ty) {
        float x0 = startX;
        float y0 = startY + ty * tileSize;
        float x1 = startX + tilesX * tileSize;
        float y1 = y0;
        float d0 = camera.getDepth(x0, y0, floorZ);
        float d1 = camera.getDepth(x1, y1, floorZ);
        if (!clipLineToNearPlane(x0, y0, d0, x1, y1, d1)) {
            continue;
        }

        const SDL_FPoint p0 = camera.worldToScreen(x0, y0, floorZ);
        const SDL_FPoint p1 = camera.worldToScreen(x1, y1, floorZ);
        if ((p0.x > -200.0f || p1.x > -200.0f) && (p0.x < screenW + 200.0f || p1.x < screenW + 200.0f) &&
            (p0.y > -200.0f || p1.y > -200.0f) && (p0.y < screenH + 200.0f || p1.y < screenH + 200.0f)) {
            SDL_RenderDrawLineF(renderer, p0.x, p0.y, p1.x, p1.y);
        }
    }
}

inline void drawFloor(SDL_Renderer* renderer,
                      int screenW,
                      int screenH,
                      const battle::Camera3D& camera,
                      SDL_Texture* floorTileTexture) {
    if (floorTileTexture != nullptr) {
        const float floorZ = 0.0f;
        const float centerX = -300.0f;
        const float centerY = 510.0f;
        const float tileSize = 200.0f;
        const int tilesX = 14;
        const int tilesY = 16;

        const float startX = centerX - (tilesX * tileSize * 0.5f);
        const float startY = centerY - (tilesY * tileSize * 0.30f);

        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                const float x0 = startX + tx * tileSize;
                const float y0 = startY + ty * tileSize;
                const float x1 = x0 + tileSize;
                const float y1 = y0 + tileSize;

                const float d00 = camera.getDepth(x0, y0, floorZ);
                const float d10 = camera.getDepth(x1, y0, floorZ);
                const float d11 = camera.getDepth(x1, y1, floorZ);
                const float d01 = camera.getDepth(x0, y1, floorZ);
                if (d00 <= kFloorNearClipDepth && d10 <= kFloorNearClipDepth &&
                    d11 <= kFloorNearClipDepth && d01 <= kFloorNearClipDepth) {
                    continue;
                }

                const std::array<FloorVertex, 4> tile = {{
                    FloorVertex{x0, y0, d00, 0.0f, 0.0f},
                    FloorVertex{x1, y0, d10, 1.0f, 0.0f},
                    FloorVertex{x1, y1, d11, 1.0f, 1.0f},
                    FloorVertex{x0, y1, d01, 0.0f, 1.0f}
                }};
                std::array<FloorVertex, 6> clippedTile{};
                const int clippedCount = clipFloorQuadToNearPlane(tile, clippedTile);
                if (clippedCount < 3) {
                    continue;
                }

                std::array<SDL_Vertex, 6> verts{};
                float minX = 0.0f;
                float maxX = 0.0f;
                float minY = 0.0f;
                float maxY = 0.0f;
                for (int i = 0; i < clippedCount; ++i) {
                    const FloorVertex& vertex = clippedTile[static_cast<size_t>(i)];
                    const SDL_FPoint projected = camera.worldToScreen(vertex.worldX, vertex.worldY, floorZ);
                    verts[static_cast<size_t>(i)].position = projected;
                    verts[static_cast<size_t>(i)].color = SDL_Color{255, 255, 255, 255};
                    verts[static_cast<size_t>(i)].tex_coord = SDL_FPoint{vertex.u, vertex.v};

                    if (i == 0) {
                        minX = projected.x;
                        maxX = projected.x;
                        minY = projected.y;
                        maxY = projected.y;
                    } else {
                        minX = std::min(minX, projected.x);
                        maxX = std::max(maxX, projected.x);
                        minY = std::min(minY, projected.y);
                        maxY = std::max(maxY, projected.y);
                    }
                }
                if (maxX < -200.0f || minX > screenW + 200.0f || maxY < -200.0f || minY > screenH + 200.0f) {
                    continue;
                }

                std::array<int, 12> indices{};
                int indexCount = 0;
                for (int i = 1; i + 1 < clippedCount; ++i) {
                    indices[static_cast<size_t>(indexCount++)] = 0;
                    indices[static_cast<size_t>(indexCount++)] = i;
                    indices[static_cast<size_t>(indexCount++)] = i + 1;
                }

                SDL_RenderGeometry(
                    renderer,
                    floorTileTexture,
                    verts.data(),
                    clippedCount,
                    indices.data(),
                    indexCount
                );
            }
        }
    }

    drawFloorGridLines(renderer, screenW, screenH, camera);
}

} // namespace detail

inline void renderBattleScene(SoftwareSceneRenderer& sceneRenderer,
                              const battle::Camera3D& camera,
                              const std::vector<WorldEntity>& entities,
                              int focusedEntityIndex,
                              float frameAccumulator) {
    SDL_SetRenderDrawBlendMode(sceneRenderer.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sceneRenderer.renderer, 16, 18, 26, 255);
    SDL_RenderClear(sceneRenderer.renderer);

    detail::drawFloor(sceneRenderer.renderer, sceneRenderer.width, sceneRenderer.height, camera, sceneRenderer.floorTileTexture);

    struct DrawCall {
        size_t index;
        float depth;
        SDL_FPoint screen;
    };

    std::vector<DrawCall> drawList;
    drawList.reserve(entities.size());
    for (size_t i = 0; i < entities.size(); ++i) {
        const WorldEntity& entity = entities[i];
        drawList.push_back(DrawCall{
            i,
            camera.getDepth(entity.worldX, entity.worldY, entity.worldZ),
            camera.worldToScreen(entity.worldX, entity.worldY, entity.worldZ)
        });
    }

    std::sort(drawList.begin(), drawList.end(), [](const DrawCall& a, const DrawCall& b) {
        return a.depth > b.depth;
    });

    const WorldEntity* focusedEntity = entities.empty()
        ? nullptr
        : &entities[std::clamp(focusedEntityIndex, 0, static_cast<int>(entities.size() - 1))];

    for (const DrawCall& call : drawList) {
        const WorldEntity& entity = entities[call.index];
        const float scale = camera.getPerspectiveScale(entity.worldX, entity.worldY, entity.worldZ);
        const bool isFocused = (&entity == focusedEntity);
        const float focusScale = isFocused ? 1.13f : 1.0f;

        const int drawW = static_cast<int>(kSuggestedBaseSpriteWidth * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
        const int drawH = static_cast<int>(kSuggestedBaseSpriteHeight * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
        SDL_Rect dst{
            static_cast<int>(call.screen.x) - drawW / 2,
            static_cast<int>(call.screen.y) - drawH,
            std::max(8, drawW),
            std::max(8, drawH)
        };

        SDL_Texture* texture = nullptr;
        const auto texIt = sceneRenderer.textureByAsset.find(entity.assetName);
        if (texIt != sceneRenderer.textureByAsset.end()) {
            texture = texIt->second;
        }

        if (texture != nullptr) {
            int texW = 0;
            int texH = 0;
            SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);
            const int frameCount = texW / kSuggestedBaseSpriteWidth;
            const bool isAnimated = (frameCount > 1) && (texW % kSuggestedBaseSpriteWidth == 0);
            if (isAnimated) {
                const int currentFrame = static_cast<int>(frameAccumulator / kSpriteFrameTime) % frameCount;
                SDL_Rect srcRect{currentFrame * kSuggestedBaseSpriteWidth, 0, kSuggestedBaseSpriteWidth, texH};
                SDL_RenderCopy(sceneRenderer.renderer, texture, &srcRect, &dst);
            } else {
                SDL_RenderCopy(sceneRenderer.renderer, texture, nullptr, &dst);
            }
        } else {
            SDL_SetRenderDrawColor(sceneRenderer.renderer, entity.fallbackColor.r, entity.fallbackColor.g, entity.fallbackColor.b, 255);
            SDL_RenderFillRect(sceneRenderer.renderer, &dst);
            SDL_SetRenderDrawColor(sceneRenderer.renderer, 16, 16, 20, 255);
            SDL_RenderDrawRect(sceneRenderer.renderer, &dst);
        }

        if (isFocused) {
            SDL_SetRenderDrawColor(sceneRenderer.renderer, 250, 230, 96, 255);
            SDL_Rect ring{dst.x - 6, dst.y - 6, dst.w + 12, dst.h + 12};
            SDL_RenderDrawRect(sceneRenderer.renderer, &ring);
        }
    }

    SDL_RenderPresent(sceneRenderer.renderer);
}

inline bool SoftwareSceneRenderer::initialize(int newWidth,
                                              int newHeight,
                                              const std::vector<std::string>& assetNames,
                                              const SpritePathResolver& resolveSpritePath) {
    destroy();

    width = newWidth;
    height = newHeight;

    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        std::cerr << "Failed to create software surface: " << SDL_GetError() << "\n";
        return false;
    }

    renderer = SDL_CreateSoftwareRenderer(surface);
    if (renderer == nullptr) {
        std::cerr << "Failed to create software renderer: " << SDL_GetError() << "\n";
        destroy();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    floorTileTexture = detail::createFloorTileTexture(renderer);

    for (const std::string& assetName : assetNames) {
        const auto loaded = detail::tryLoadTexture(renderer, assetName, resolveSpritePath);
        textureByAsset[assetName] = loaded.has_value() ? *loaded : nullptr;
    }

    return true;
}

} // namespace battle::render
