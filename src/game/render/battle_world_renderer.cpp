#include "battle_world_renderer.h"

#include <algorithm>

namespace battle::render {
namespace {

constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kSpriteFrameTime = 0.15f;

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
    const int indices[6] = {0, 1, 2, 0, 2, 3};

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
            if (d00 <= 1.0f && d10 <= 1.0f && d11 <= 1.0f && d01 <= 1.0f) {
                continue;
            }

            const SDL_FPoint p00 = camera.worldToScreen(x0, y0, floorZ);
            const SDL_FPoint p10 = camera.worldToScreen(x1, y0, floorZ);
            const SDL_FPoint p11 = camera.worldToScreen(x1, y1, floorZ);
            const SDL_FPoint p01 = camera.worldToScreen(x0, y1, floorZ);

            const float minX = std::min(std::min(p00.x, p10.x), std::min(p11.x, p01.x));
            const float maxX = std::max(std::max(p00.x, p10.x), std::max(p11.x, p01.x));
            const float minY = std::min(std::min(p00.y, p10.y), std::min(p11.y, p01.y));
            const float maxY = std::max(std::max(p00.y, p10.y), std::max(p11.y, p01.y));
            if (maxX < -200.0f || minX > screenWidth + 200.0f || maxY < -200.0f || minY > screenHeight + 200.0f) {
                continue;
            }

            SDL_Vertex verts[4];
            verts[0].position = p00;
            verts[1].position = p10;
            verts[2].position = p11;
            verts[3].position = p01;
            verts[0].color = SDL_Color{255, 255, 255, 255};
            verts[1].color = SDL_Color{255, 255, 255, 255};
            verts[2].color = SDL_Color{255, 255, 255, 255};
            verts[3].color = SDL_Color{255, 255, 255, 255};
            verts[0].tex_coord = SDL_FPoint{0.0f, 0.0f};
            verts[1].tex_coord = SDL_FPoint{1.0f, 0.0f};
            verts[2].tex_coord = SDL_FPoint{1.0f, 1.0f};
            verts[3].tex_coord = SDL_FPoint{0.0f, 1.0f};

            SDL_RenderGeometry(renderer, floorTileTexture, verts, 4, indices, 6);
        }
    }
}

} // namespace

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

        const int drawWidth = static_cast<int>(kSuggestedBaseSpriteWidth * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
        const int drawHeight = static_cast<int>(kSuggestedBaseSpriteHeight * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));

        SDL_Rect dstRect{
            static_cast<int>(drawCall.screen.x + shakeOffsetX) - drawWidth / 2,
            static_cast<int>(drawCall.screen.y) - drawHeight,
            std::max(8, drawWidth),
            std::max(8, drawHeight)
        };

        SDL_Texture* texture = nullptr;
        auto textureIt = textureByAsset.find(entity.assetName);
        if (textureIt != textureByAsset.end()) {
            texture = textureIt->second;
        }

        if (texture != nullptr) {
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
        } else {
            SDL_SetRenderDrawColor(renderer, entity.fallbackColor.r, entity.fallbackColor.g, entity.fallbackColor.b, 255);
            SDL_RenderFillRect(renderer, &dstRect);
            SDL_SetRenderDrawColor(renderer, 16, 16, 20, 255);
            SDL_RenderDrawRect(renderer, &dstRect);
        }

        if (isFocused) {
            SDL_SetRenderDrawColor(renderer, 250, 230, 96, 255);
            SDL_Rect ringRect{dstRect.x - 6, dstRect.y - 6, dstRect.w + 12, dstRect.h + 12};
            SDL_RenderDrawRect(renderer, &ringRect);
        }
    }
}

} // namespace battle::render
