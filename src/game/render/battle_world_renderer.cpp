#include "battle_world_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "battle_backdrop_projection.h"

namespace battle::render {
namespace {

constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kSpriteFrameTime = 0.15f;
constexpr float kFloorNearClipDepth = 1.0f;
constexpr float kClipEpsilon = 0.0001f;
constexpr int kMaxFloorTilesX = 20;
constexpr int kMaxFloorTilesY = 20;

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

bool isOffscreen(const SDL_Rect& rect, int screenWidth, int screenHeight, int margin = 200) {
    return rect.x + rect.w < -margin ||
           rect.x > screenWidth + margin ||
           rect.y + rect.h < -margin ||
           rect.y > screenHeight + margin;
}

void renderBackdropQuad(SDL_Renderer* renderer, SDL_Texture* texture, const BackdropQuad& quad) {
    if (renderer == nullptr || texture == nullptr) {
        return;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    const std::array<SDL_Vertex, 4> verts{{
        SDL_Vertex{SDL_FPoint{quad.x0, quad.y0}, SDL_Color{255, 255, 255, 255}, SDL_FPoint{quad.u0, quad.v0}},
        SDL_Vertex{SDL_FPoint{quad.x1, quad.y0}, SDL_Color{255, 255, 255, 255}, SDL_FPoint{quad.u1, quad.v0}},
        SDL_Vertex{SDL_FPoint{quad.x1, quad.y1}, SDL_Color{255, 255, 255, 255}, SDL_FPoint{quad.u1, quad.v1}},
        SDL_Vertex{SDL_FPoint{quad.x0, quad.y1}, SDL_Color{255, 255, 255, 255}, SDL_FPoint{quad.u0, quad.v1}}
    }};
    constexpr std::array<int, 6> indices{{0, 1, 2, 0, 2, 3}};
    SDL_RenderGeometry(renderer,
                       texture,
                       verts.data(),
                       static_cast<int>(verts.size()),
                       indices.data(),
                       static_cast<int>(indices.size()));
}

void renderSkyboxBackdrop(SDL_Renderer* renderer,
                          int screenWidth,
                          int screenHeight,
                          const Camera3D& camera,
                          const StageRenderData& stage) {
    if (renderer == nullptr || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    const SkyboxBackdropBatches skyboxBatches = buildSkyboxBackdropBatches(camera, screenWidth, screenHeight);
    for (size_t faceIndex = 0; faceIndex < skyboxBatches.size(); ++faceIndex) {
        SDL_Texture* texture = stage.skyboxTextures[faceIndex];
        const auto& vertices = skyboxBatches[faceIndex];
        if (texture == nullptr || vertices.empty()) {
            continue;
        }

        std::vector<SDL_Vertex> sdlVertices;
        sdlVertices.reserve(vertices.size());
        for (const SkyboxBackdropVertex& vertex : vertices) {
            sdlVertices.push_back(SDL_Vertex{
                SDL_FPoint{vertex.x, vertex.y},
                SDL_Color{255, 255, 255, 255},
                SDL_FPoint{vertex.u, vertex.v}
            });
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderGeometry(renderer, texture, sdlVertices.data(), static_cast<int>(sdlVertices.size()), nullptr, 0);
    }
}

} // namespace

void renderBattleBackdrop(SDL_Renderer* renderer,
                          int screenWidth,
                          int screenHeight,
                          const Camera3D& camera,
                          const StageRenderData& stage) {
    if (renderer == nullptr || screenWidth <= 0 || screenHeight <= 0) {
        return;
    }

    const StageBackdropDefinition& backdrop = stage.definition.backdrop;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const std::array<SDL_Vertex, 4> verts{{
        SDL_Vertex{SDL_FPoint{0.0f, 0.0f}, backdrop.gradientTopColor, SDL_FPoint{0.0f, 0.0f}},
        SDL_Vertex{SDL_FPoint{static_cast<float>(screenWidth), 0.0f}, backdrop.gradientTopColor, SDL_FPoint{1.0f, 0.0f}},
        SDL_Vertex{SDL_FPoint{static_cast<float>(screenWidth), static_cast<float>(screenHeight)}, backdrop.gradientBottomColor, SDL_FPoint{1.0f, 1.0f}},
        SDL_Vertex{SDL_FPoint{0.0f, static_cast<float>(screenHeight)}, backdrop.gradientBottomColor, SDL_FPoint{0.0f, 1.0f}}
    }};
    constexpr std::array<int, 6> indices{{0, 1, 2, 0, 2, 3}};
    SDL_RenderGeometry(renderer, nullptr, verts.data(), static_cast<int>(verts.size()), indices.data(), static_cast<int>(indices.size()));

    switch (backdrop.mode) {
    case StageBackdropMode::Screen:
        renderBackdropQuad(renderer, stage.backdropTexture, makeFullscreenBackdropQuad(screenWidth, screenHeight));
        break;
    case StageBackdropMode::Parallax:
        renderBackdropQuad(renderer,
                           stage.backdropTexture,
                           computeParallaxBackdropQuad(camera,
                                                       screenWidth,
                                                       screenHeight,
                                                       backdrop.parallaxStrengthX,
                                                       backdrop.parallaxStrengthY));
        break;
    case StageBackdropMode::Panorama: {
        const std::vector<BackdropQuad> quads = buildPanoramaBackdropQuads(camera, screenWidth, screenHeight);
        for (const BackdropQuad& quad : quads) {
            renderBackdropQuad(renderer, stage.backdropTexture, quad);
        }
        break;
    }
    case StageBackdropMode::Skybox:
        renderSkyboxBackdrop(renderer, screenWidth, screenHeight, camera, stage);
        break;
    }
}

void renderBattleFloor(SDL_Renderer* renderer,
                       int screenWidth,
                       int screenHeight,
                       const Camera3D& camera,
                       const StageFloorDefinition& floor,
                       SDL_Texture* floorTileTexture) {
    if (floorTileTexture == nullptr) {
        return;
    }

    const float floorZ = 0.0f;
    const float floorWidth = std::max(64.0f, floor.width);
    const float floorDepth = std::max(64.0f, floor.depth);
    const float requestedTileSize = std::max(16.0f, floor.tileSize);
    const int tilesX = std::clamp(static_cast<int>(std::ceil(floorWidth / requestedTileSize)), 1, kMaxFloorTilesX);
    const int tilesY = std::clamp(static_cast<int>(std::ceil(floorDepth / requestedTileSize)), 1, kMaxFloorTilesY);
    const float tileWidth = floorWidth / static_cast<float>(tilesX);
    const float tileDepth = floorDepth / static_cast<float>(tilesY);
    const float startX = floor.centerX - floorWidth * 0.5f;
    const float startY = floor.centerY - floorDepth * 0.5f;

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            const float x0 = startX + tx * tileWidth;
            const float y0 = startY + ty * tileDepth;
            const float x1 = x0 + tileWidth;
            const float y1 = y0 + tileDepth;

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

SDL_Texture* createBattleWorldFloorTileTexture(SDL_Renderer* renderer,
                                               SDL_Color baseColor,
                                               SDL_Color accentColor) {
    constexpr int texSize = 64;
    constexpr int cell = 16;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, texSize, texSize, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return nullptr;
    }

    const Uint32 c0 = SDL_MapRGBA(surface->format, baseColor.r, baseColor.g, baseColor.b, baseColor.a);
    const Uint32 c1 = SDL_MapRGBA(surface->format, accentColor.r, accentColor.g, accentColor.b, accentColor.a);

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

SDL_Texture* createBattleWorldFloorTileTexture(SDL_Renderer* renderer) {
    return createBattleWorldFloorTileTexture(renderer, SDL_Color{46, 49, 60, 255}, SDL_Color{52, 56, 69, 255});
}

void renderBattleProps(SDL_Renderer* renderer,
                       int screenWidth,
                       int screenHeight,
                       const Camera3D& camera,
                       const std::vector<StagePropRenderItem>& props) {
    struct DrawCall {
        const StagePropRenderItem* prop = nullptr;
        float depth = 0.0f;
        SDL_FPoint screen{};
    };

    std::vector<DrawCall> drawCalls;
    drawCalls.reserve(props.size());
    for (const StagePropRenderItem& prop : props) {
        if (prop.texture == nullptr) {
            continue;
        }
        const float depth = camera.getDepth(prop.definition.worldX, prop.definition.worldY, prop.definition.worldZ);
        if (depth <= kFloorNearClipDepth) {
            continue;
        }
        drawCalls.push_back(DrawCall{
            &prop,
            depth,
            camera.worldToScreen(prop.definition.worldX, prop.definition.worldY, prop.definition.worldZ)
        });
    }

    std::sort(drawCalls.begin(), drawCalls.end(), [](const DrawCall& lhs, const DrawCall& rhs) {
        return lhs.depth > rhs.depth;
    });

    for (const DrawCall& drawCall : drawCalls) {
        const float scale = camera.getPerspectiveScale(drawCall.prop->definition.worldX,
                                                       drawCall.prop->definition.worldY,
                                                       drawCall.prop->definition.worldZ);
        const int drawWidth = std::max(1, static_cast<int>(std::lround(static_cast<float>(drawCall.prop->definition.pixelWidth) * scale)));
        const int drawHeight = std::max(1, static_cast<int>(std::lround(static_cast<float>(drawCall.prop->definition.pixelHeight) * scale)));
        SDL_Rect dstRect{
            static_cast<int>(std::lround(drawCall.screen.x)) - drawWidth / 2,
            static_cast<int>(std::lround(drawCall.screen.y)) - drawHeight,
            drawWidth,
            drawHeight
        };
        if (isOffscreen(dstRect, screenWidth, screenHeight, 240)) {
            continue;
        }

        SDL_SetTextureBlendMode(drawCall.prop->texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(drawCall.prop->texture,
                               drawCall.prop->definition.tint.r,
                               drawCall.prop->definition.tint.g,
                               drawCall.prop->definition.tint.b);
        SDL_SetTextureAlphaMod(drawCall.prop->texture,
                               static_cast<Uint8>(std::clamp(drawCall.prop->definition.alpha, 0.0f, 1.0f) * 255.0f));
        SDL_RenderCopy(renderer, drawCall.prop->texture, nullptr, &dstRect);
        SDL_SetTextureColorMod(drawCall.prop->texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(drawCall.prop->texture, 255);
    }
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
                       const StageRenderData& stage,
                       const std::vector<SceneEntity>& entities,
                       int focusedEntityIndex,
                       const std::map<std::string, SDL_Texture*>& textureByAsset,
                       float frameAccumulator,
                       const WorldShakeOffsetFn& shakeOffsetFn) {
    renderBattleBackdrop(renderer, screenWidth, screenHeight, camera, stage);
    renderBattleFloor(renderer, screenWidth, screenHeight, camera, stage.definition.floor, stage.floorTexture);
    renderBattleProps(renderer, screenWidth, screenHeight, camera, stage.props);
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
