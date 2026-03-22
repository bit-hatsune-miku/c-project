#include "battle_world_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace battle::render {
namespace {

constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kSpriteFrameTime = 0.15f;
constexpr float kFloorNearClipDepth = 1.0f;
constexpr float kClipEpsilon = 0.0001f;

struct FloorVertex {
    float worldX = 0.0f;
    float worldY = 0.0f;
    float depth = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

FloorVertex interpolateToNearPlane(const FloorVertex& from, const FloorVertex& to) {
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

int clipFloorQuadToNearPlane(const std::array<FloorVertex, 4>& input, std::array<FloorVertex, 6>& output) {
    std::array<FloorVertex, 6> working{};
    for (size_t i = 0; i < input.size(); ++i) {
        working[i] = input[i];
    }
    int workingCount = static_cast<int>(input.size());

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

} // namespace

void renderBattleFloor(SDL_Renderer* renderer,
                       int screenWidth,
                       int screenHeight,
                       const Camera3D& camera,
                       SDL_Texture* floorTileTexture) {
    if (floorTileTexture == nullptr) {
        return;
    }

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
            if (maxX < -200.0f || minX > screenWidth + 200.0f || maxY < -200.0f || minY > screenHeight + 200.0f) {
                continue;
            }

            std::array<int, 12> indices{};
            int indexCount = 0;
            for (int i = 1; i + 1 < clippedCount; ++i) {
                indices[static_cast<size_t>(indexCount++)] = 0;
                indices[static_cast<size_t>(indexCount++)] = i;
                indices[static_cast<size_t>(indexCount++)] = i + 1;
            }

            SDL_RenderGeometry(renderer, floorTileTexture, verts.data(), clippedCount, indices.data(), indexCount);
        }
    }
}

SDL_Texture* createBattleWorldFloorTileTexture(SDL_Renderer* renderer) {
    constexpr int texSize = 64;
    constexpr int cell = 16;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, texSize, texSize, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return nullptr;
    }

    const Uint32 c0 = SDL_MapRGBA(surface->format, 46, 49, 60, 255);
    const Uint32 c1 = SDL_MapRGBA(surface->format, 52, 56, 69, 255);

    SDL_Rect rect{0, 0, cell, cell};
    for (int y = 0; y < texSize; y += cell) {
        for (int x = 0; x < texSize; x += cell) {
            rect.x = x;
            rect.y = y;
            const bool alt = ((x / cell) + (y / cell)) % 2 == 0;
            SDL_FillRect(surface, &rect, alt ? c0 : c1);
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void renderBattleEntities(SDL_Renderer* renderer,
                          int screenWidth,
                          int screenHeight,
                          const Camera3D& camera,
                          const std::vector<SceneEntity>& entities,
                          int focusedEntityIndex,
                          const std::map<std::string, SDL_Texture*>& textureByAsset,
                          float frameAccumulator,
                          const WorldShakeOffsetFn& shakeOffsetFn) {
    struct DrawCall {
        size_t index;
        float depth;
        SDL_FPoint screen;
    };

    std::vector<DrawCall> drawList;
    drawList.reserve(entities.size());
    for (size_t i = 0; i < entities.size(); ++i) {
        const SceneEntity& entity = entities[i];
        if (!entity.visible) {
            continue;
        }
        drawList.push_back(DrawCall{
            i,
            camera.getDepth(entity.worldX, entity.worldY, entity.worldZ),
            camera.worldToScreen(entity.worldX, entity.worldY, entity.worldZ)
        });
    }

    std::sort(drawList.begin(), drawList.end(), [](const DrawCall& lhs, const DrawCall& rhs) {
        return lhs.depth > rhs.depth;
    });

    for (const DrawCall& drawCall : drawList) {
        const SceneEntity& entity = entities[drawCall.index];
        const float scale = camera.getPerspectiveScale(entity.worldX, entity.worldY, entity.worldZ);
        const bool isFocused = drawCall.index == static_cast<size_t>(focusedEntityIndex);
        const float focusScale = isFocused ? 1.13f : 1.0f;
        const float shakeOffsetX = shakeOffsetFn ? shakeOffsetFn(entity) : 0.0f;
        const Uint8 alpha = static_cast<Uint8>(std::clamp(entity.spriteAlpha, 0.0f, 1.0f) * 255.0f);

        const int drawWidth = static_cast<int>(kSuggestedBaseSpriteWidth * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
        const int drawHeight = static_cast<int>(kSuggestedBaseSpriteHeight * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));

        SDL_Rect dstRect{
            static_cast<int>(drawCall.screen.x + shakeOffsetX) - drawWidth / 2,
            static_cast<int>(drawCall.screen.y + entity.spriteOffsetYPx) - drawHeight,
            std::max(8, drawWidth),
            std::max(8, drawHeight)
        };

        SDL_Texture* texture = nullptr;
        auto textureIt = textureByAsset.find(entity.assetName);
        if (textureIt != textureByAsset.end()) {
            texture = textureIt->second;
        }

        if (texture != nullptr) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(texture, alpha);
            int texWidth = 0;
            int texHeight = 0;
            SDL_QueryTexture(texture, nullptr, nullptr, &texWidth, &texHeight);

            const int frameCount = texWidth / kSuggestedBaseSpriteWidth;
            const bool isAnimated = frameCount > 1 && (texWidth % kSuggestedBaseSpriteWidth == 0);
            if (isAnimated) {
                const int currentFrame = static_cast<int>(frameAccumulator / kSpriteFrameTime) % frameCount;
                SDL_Rect srcRect{
                    currentFrame * kSuggestedBaseSpriteWidth,
                    0,
                    kSuggestedBaseSpriteWidth,
                    texHeight
                };
                SDL_RenderCopy(renderer, texture, &srcRect, &dstRect);
            } else {
                SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
            }
            SDL_SetTextureAlphaMod(texture, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, entity.fallbackColor.r, entity.fallbackColor.g, entity.fallbackColor.b, alpha);
            SDL_RenderFillRect(renderer, &dstRect);
            SDL_SetRenderDrawColor(renderer, 16, 16, 20, alpha);
            SDL_RenderDrawRect(renderer, &dstRect);
        }

        if (isFocused) {
            SDL_SetRenderDrawColor(renderer, 250, 230, 96, alpha);
            SDL_Rect ringRect{dstRect.x - 6, dstRect.y - 6, dstRect.w + 12, dstRect.h + 12};
            SDL_RenderDrawRect(renderer, &ringRect);
        }
    }
}

void renderBattleWorld(SDL_Renderer* renderer,
                       int screenWidth,
                       int screenHeight,
                       const Camera3D& camera,
                       SDL_Texture* floorTileTexture,
                       const std::vector<SceneEntity>& entities,
                       int focusedEntityIndex,
                       const std::map<std::string, SDL_Texture*>& textureByAsset,
                       float frameAccumulator,
                       const WorldShakeOffsetFn& shakeOffsetFn) {
    renderBattleFloor(renderer, screenWidth, screenHeight, camera, floorTileTexture);
    renderBattleEntities(
        renderer,
        screenWidth,
        screenHeight,
        camera,
        entities,
        focusedEntityIndex,
        textureByAsset,
        frameAccumulator,
        shakeOffsetFn
    );
}

} // namespace battle::render
