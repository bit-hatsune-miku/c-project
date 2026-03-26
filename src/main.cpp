#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#ifdef VN_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif
#ifdef VN_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "GameMenu/menu_shared.h"
#include "Settings/settings.h"
#include "window.h"
#include "game/core/battle_loader.h"
#include "game/demo_battle_session.h"
#include "game/save/save.h"
#include "game/vn/vn_system.h"
#ifdef APP_ENABLE_RMLUI
#include "graphics/front_ui_session.h"
#endif
#include "platform/path_resolution.h"
#include "platform/text_fallback.h"

#ifdef VN_SCRIPT_PATH
constexpr const char* kChapterScriptPath = VN_SCRIPT_PATH;
#else
constexpr const char* kChapterScriptPath = "assets/vn/json/ch0.json";
#endif
constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr float kMenuCanvasScale = 2.0f;

std::string resolvePath(const std::string& relativePath) {
    const std::array<std::string, 3> candidates = {
        relativePath,
        "../" + relativePath,
        "../../" + relativePath
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return relativePath;
}

SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path) {
    SDL_Surface* surface = nullptr;
#ifdef VN_ENABLE_IMAGE
    surface = IMG_Load(path.c_str());
#else
    surface = SDL_LoadBMP(path.c_str());
#endif
    if (surface == nullptr) {
        std::cerr << "Failed to load texture: " << path << " ("
#ifdef VN_ENABLE_IMAGE
                  << IMG_GetError()
#else
                  << SDL_GetError()
#endif
                  << ")\n";
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

SDL_Texture* createRenderTarget(SDL_Renderer* renderer, int width, int height, float scale) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        static_cast<int>(std::lround(static_cast<float>(width) * scale)),
        static_cast<int>(std::lround(static_cast<float>(height) * scale))
    );
    if (texture != nullptr) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}

#ifdef VN_ENABLE_TTF
TTF_Font* openBestAvailableFont(const std::vector<std::string>& preferredPaths, int ptSize) {
    std::vector<std::string> candidates = preferredPaths;
    candidates.emplace_back(resolvePath("assets/fonts/SpaceMono-Bold.ttf"));
    candidates.emplace_back(resolvePath("assets/fonts/SpaceMono-Regular.ttf"));
    candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans-Bold.ttf");
    candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    candidates.emplace_back(resolvePath("assets/rmlui/DejaVuSans-Bold.ttf"));
    candidates.emplace_back(resolvePath("assets/rmlui/DejaVuSans.ttf"));

    for (const auto& path : candidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }

        TTF_Font* font = TTF_OpenFont(path.c_str(), ptSize);
        if (font != nullptr) {
            return font;
        }
    }

    return nullptr;
}

void drawTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                    const SDL_Color& color, const SDL_FRect& rect, bool centerX) {
    if (font == nullptr || text.empty()) {
        return;
    }

    TTF_Font* cjkFont = platform::text::fallbackCjkFontFor(font);
    const std::vector<platform::text::FontRun> runs = platform::text::buildFontRuns(text, font, cjkFont);
    if (runs.empty()) {
        return;
    }

    int totalWidth = 0;
    int maxHeight = 0;
    for (const platform::text::FontRun& run : runs) {
        int runW = 0;
        int runH = 0;
        if (TTF_SizeUTF8(run.font, run.text.c_str(), &runW, &runH) == 0) {
            totalWidth += runW;
            maxHeight = std::max(maxHeight, runH);
        }
    }

    int cursorX = static_cast<int>(std::lround(centerX ? rect.x + (rect.w - static_cast<float>(totalWidth)) * 0.5f : rect.x));
    const int baselineY = static_cast<int>(std::lround(rect.y + (rect.h - static_cast<float>(maxHeight)) * 0.5f)) +
                          TTF_FontAscent(font);

    for (const platform::text::FontRun& run : runs) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(run.font, run.text.c_str(), color);
        if (surface == nullptr) {
            continue;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture != nullptr) {
            SDL_Rect dst{
                cursorX,
                baselineY - TTF_FontAscent(run.font),
                surface->w,
                surface->h
            };
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }
        cursorX += surface->w;
        SDL_FreeSurface(surface);
    }
}

void drawWrappedTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                           const SDL_Color& color, const SDL_FRect& rect, bool centerX) {
    if (font == nullptr || text.empty()) {
        return;
    }

    TTF_Font* cjkFont = platform::text::fallbackCjkFontFor(font);
    const std::vector<platform::text::FontRun> runs = platform::text::buildWrapRuns(text, font, cjkFont);
    if (runs.empty()) {
        return;
    }

    std::vector<std::vector<platform::text::FontRun>> lines(1);
    float currentWidth = 0.0f;
    const float maxWidth = rect.w;

    for (const platform::text::FontRun& run : runs) {
        if (run.text == "\n") {
            lines.emplace_back();
            currentWidth = 0.0f;
            continue;
        }

        int runW = 0;
        int runH = 0;
        if (TTF_SizeUTF8(run.font, run.text.c_str(), &runW, &runH) != 0) {
            continue;
        }

        if (currentWidth > 0.0f && currentWidth + static_cast<float>(runW) > maxWidth) {
            lines.emplace_back();
            currentWidth = 0.0f;
        }

        lines.back().push_back(run);
        currentWidth += static_cast<float>(runW);
    }

    const int lineSkip = TTF_FontLineSkip(font);
    const float totalHeight = static_cast<float>(std::max(1, static_cast<int>(lines.size())) * lineSkip);
    float cursorY = rect.y + (rect.h - totalHeight) * 0.5f;

    for (const auto& line : lines) {
        int lineWidth = 0;
        int lineHeight = 0;
        for (const platform::text::FontRun& run : line) {
            int runW = 0;
            int runH = 0;
            if (TTF_SizeUTF8(run.font, run.text.c_str(), &runW, &runH) == 0) {
                lineWidth += runW;
                lineHeight = std::max(lineHeight, runH);
            }
        }

        int cursorX = static_cast<int>(std::lround(centerX ? rect.x + (rect.w - static_cast<float>(lineWidth)) * 0.5f : rect.x));
        const int baselineY = static_cast<int>(std::lround(cursorY + (static_cast<float>(lineSkip - lineHeight) * 0.5f))) +
                              TTF_FontAscent(font);

        for (const platform::text::FontRun& run : line) {
            SDL_Surface* surface = TTF_RenderUTF8_Blended(run.font, run.text.c_str(), color);
            if (surface == nullptr) {
                continue;
            }

            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture != nullptr) {
                SDL_Rect dst{
                    cursorX,
                    baselineY - TTF_FontAscent(run.font),
                    surface->w,
                    surface->h
                };
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            cursorX += surface->w;
            SDL_FreeSurface(surface);
        }

        cursorY += static_cast<float>(lineSkip);
    }
}

void drawShadowedTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                            const SDL_Color& color, const SDL_FRect& rect, bool centerX) {
    drawTextInRect(renderer, font, text, SDL_Color{8, 12, 22, 150},
                   SDL_FRect{rect.x + 2.0f, rect.y + 2.0f, rect.w, rect.h}, centerX);
    drawTextInRect(renderer, font, text, color, rect, centerX);
}

void drawShadowedWrappedTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                                   const SDL_Color& color, const SDL_FRect& rect, bool centerX) {
    drawWrappedTextInRect(renderer, font, text, SDL_Color{8, 12, 22, 150},
                          SDL_FRect{rect.x + 2.0f, rect.y + 2.0f, rect.w, rect.h}, centerX);
    drawWrappedTextInRect(renderer, font, text, color, rect, centerX);
}

#endif

bool pointInRect(float x, float y, const SDL_FRect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float easeOutBack(float value) {
    const float t = clamp01(value);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float shifted = t - 1.0f;
    return 1.0f + c3 * shifted * shifted * shifted + c1 * shifted * shifted;
}

float smoothstep01(float value) {
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

SDL_FRect offsetRect(const SDL_FRect& rect, float dx, float dy) {
    return SDL_FRect{rect.x + dx, rect.y + dy, rect.w, rect.h};
}

SDL_FRect insetRect(const SDL_FRect& rect, float dx, float dy) {
    return SDL_FRect{
        rect.x + dx,
        rect.y + dy,
        std::max(0.0f, rect.w - dx * 2.0f),
        std::max(0.0f, rect.h - dy * 2.0f)
    };
}

void drawPolygon(SDL_Renderer* renderer, const std::vector<SDL_FPoint>& points, const SDL_Color& color) {
    if (points.size() < 3) {
        return;
    }

    float centerX = 0.0f;
    float centerY = 0.0f;
    for (const SDL_FPoint& point : points) {
        centerX += point.x;
        centerY += point.y;
    }
    centerX /= static_cast<float>(points.size());
    centerY /= static_cast<float>(points.size());

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(points.size() + 1);
    vertices.push_back(SDL_Vertex{SDL_FPoint{centerX, centerY}, color, SDL_FPoint{0.0f, 0.0f}});
    for (const SDL_FPoint& point : points) {
        vertices.push_back(SDL_Vertex{point, color, SDL_FPoint{0.0f, 0.0f}});
    }

    std::vector<int> indices;
    indices.reserve(points.size() * 3);
    for (size_t i = 1; i < vertices.size(); ++i) {
        const size_t next = (i + 1 < vertices.size()) ? (i + 1) : 1;
        indices.push_back(0);
        indices.push_back(static_cast<int>(i));
        indices.push_back(static_cast<int>(next));
    }

    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void buildSlantedPoints(const SDL_FRect& rect, float slant, SDL_FPoint* points) {
    points[0] = SDL_FPoint{rect.x + slant, rect.y};
    points[1] = SDL_FPoint{rect.x + rect.w, rect.y};
    points[2] = SDL_FPoint{rect.x + rect.w - slant, rect.y + rect.h};
    points[3] = SDL_FPoint{rect.x, rect.y + rect.h};
}

void drawQuad(SDL_Renderer* renderer, const SDL_FPoint* points, const SDL_Color& color) {
    SDL_Vertex vertices[4];
    for (int i = 0; i < 4; ++i) {
        vertices[i].position = points[i];
        vertices[i].color = color;
        vertices[i].tex_coord = SDL_FPoint{0.0f, 0.0f};
    }

    const int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, nullptr, vertices, 4, indices, 6);
}

void drawSlantedPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float slant, const SDL_Color& color) {
    SDL_FPoint points[4];
    buildSlantedPoints(rect, slant, points);
    drawQuad(renderer, points, color);
}

std::vector<SDL_FPoint> buildCyberPanelPoints(const SDL_FRect& rect, float slant, float bevel) {
    const float clampedSlant = std::min({slant, rect.w * 0.32f, rect.h * 0.48f});
    const float clampedBevel = std::min({bevel, rect.w * 0.18f, rect.h * 0.34f});
    const float topBevelY = rect.y + rect.h * 0.24f;
    const float bottomRiseY = rect.y + rect.h * 0.72f;

    return {
        SDL_FPoint{rect.x + clampedSlant, rect.y},
        SDL_FPoint{rect.x + rect.w - clampedBevel, rect.y},
        SDL_FPoint{rect.x + rect.w, topBevelY},
        SDL_FPoint{rect.x + rect.w, rect.y + rect.h - clampedBevel},
        SDL_FPoint{rect.x + rect.w - clampedSlant, rect.y + rect.h},
        SDL_FPoint{rect.x + clampedBevel, rect.y + rect.h},
        SDL_FPoint{rect.x, bottomRiseY},
        SDL_FPoint{rect.x, rect.y + clampedBevel}
    };
}

std::vector<SDL_FPoint> buildJaggedButtonPoints(const SDL_FRect& rect, float leadCut, float tailCut, float notchDepth) {
    const float clampedLeadCut = std::min({leadCut, rect.w * 0.24f, rect.h * 0.58f});
    const float clampedTailCut = std::min({tailCut, rect.w * 0.24f, rect.h * 0.58f});
    const float clampedNotchDepth = std::min({notchDepth, rect.w * 0.14f, rect.h * 0.32f});
    const float midTopY = rect.y + rect.h * 0.26f;
    const float midBottomY = rect.y + rect.h * 0.72f;

    return {
        SDL_FPoint{rect.x + clampedLeadCut, rect.y},
        SDL_FPoint{rect.x + rect.w - clampedTailCut, rect.y},
        SDL_FPoint{rect.x + rect.w, midTopY},
        SDL_FPoint{rect.x + rect.w - clampedNotchDepth, rect.y + rect.h * 0.5f},
        SDL_FPoint{rect.x + rect.w - clampedTailCut * 0.6f, rect.y + rect.h},
        SDL_FPoint{rect.x + clampedLeadCut * 0.35f, rect.y + rect.h},
        SDL_FPoint{rect.x, midBottomY},
        SDL_FPoint{rect.x + clampedNotchDepth * 0.42f, rect.y + rect.h * 0.34f}
    };
}

void drawPointLoopOutline(SDL_Renderer* renderer, const std::vector<SDL_FPoint>& points, const SDL_Color& color) {
    if (points.size() < 2) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (size_t i = 0; i < points.size(); ++i) {
        const SDL_FPoint& a = points[i];
        const SDL_FPoint& b = points[(i + 1) % points.size()];
        SDL_RenderDrawLine(
            renderer,
            static_cast<int>(std::lround(a.x)),
            static_cast<int>(std::lround(a.y)),
            static_cast<int>(std::lround(b.x)),
            static_cast<int>(std::lround(b.y))
        );
    }
}

void drawCyberPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float slant, float bevel,
                    const SDL_Color& fillColor, const SDL_Color& outlineColor,
                    const SDL_Color& highlightColor, const SDL_Color& shadowColor) {
    drawPolygon(renderer, buildCyberPanelPoints(offsetRect(rect, 8.0f, 10.0f), slant, bevel), shadowColor);
    drawPolygon(renderer, buildCyberPanelPoints(rect, slant, bevel), fillColor);

    const SDL_FRect innerRect = insetRect(rect, 3.0f, 3.0f);
    drawPolygon(renderer, buildCyberPanelPoints(innerRect, std::max(0.0f, slant - 3.0f), std::max(0.0f, bevel - 2.0f)),
                highlightColor);
    drawPointLoopOutline(renderer, buildCyberPanelPoints(rect, slant, bevel), outlineColor);
}

void drawJaggedButtonPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float leadCut, float tailCut, float notchDepth,
                           const SDL_Color& fillColor, const SDL_Color& outlineColor,
                           const SDL_Color& highlightColor, const SDL_Color& shadowColor) {
    drawPolygon(renderer, buildJaggedButtonPoints(offsetRect(rect, 8.0f, 10.0f), leadCut, tailCut, notchDepth), shadowColor);
    drawPolygon(renderer, buildJaggedButtonPoints(rect, leadCut, tailCut, notchDepth), fillColor);

    const SDL_FRect innerRect = insetRect(rect, 3.0f, 3.0f);
    drawPolygon(renderer, buildJaggedButtonPoints(innerRect, std::max(0.0f, leadCut - 3.0f), std::max(0.0f, tailCut - 2.0f),
                                                  std::max(0.0f, notchDepth - 2.0f)),
                highlightColor);
    drawPointLoopOutline(renderer, buildJaggedButtonPoints(rect, leadCut, tailCut, notchDepth), outlineColor);
}

void drawNeonLine(SDL_Renderer* renderer, const SDL_FPoint& a, const SDL_FPoint& b, const SDL_Color& glowColor,
                  const SDL_Color& coreColor) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, glowColor.r, glowColor.g, glowColor.b, glowColor.a);
    for (int offset = -2; offset <= 2; ++offset) {
        SDL_RenderDrawLine(
            renderer,
            static_cast<int>(std::lround(a.x)),
            static_cast<int>(std::lround(a.y + static_cast<float>(offset))),
            static_cast<int>(std::lround(b.x)),
            static_cast<int>(std::lround(b.y + static_cast<float>(offset)))
        );
    }

    SDL_SetRenderDrawColor(renderer, coreColor.r, coreColor.g, coreColor.b, coreColor.a);
    SDL_RenderDrawLine(
        renderer,
        static_cast<int>(std::lround(a.x)),
        static_cast<int>(std::lround(a.y)),
        static_cast<int>(std::lround(b.x)),
        static_cast<int>(std::lround(b.y))
    );
}

void renderBackgroundCover(SDL_Renderer* renderer, SDL_Texture* texture, int windowWidth, int windowHeight) {
    if (texture == nullptr) {
        return;
    }

    int texW = 0;
    int texH = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH) != 0 || texW <= 0 || texH <= 0) {
        return;
    }

    const float scale = std::max(
        static_cast<float>(windowWidth) / static_cast<float>(texW),
        static_cast<float>(windowHeight) / static_cast<float>(texH)
    );

    const int srcW = static_cast<int>(std::lround(static_cast<float>(windowWidth) / scale));
    const int srcH = static_cast<int>(std::lround(static_cast<float>(windowHeight) / scale));
    const SDL_Rect src{
        std::max(0, (texW - srcW) / 2),
        std::max(0, (texH - srcH) / 2),
        std::min(texW, srcW),
        std::min(texH, srcH)
    };
    const SDL_Rect dst{0, 0, windowWidth, windowHeight};
    SDL_RenderCopy(renderer, texture, &src, &dst);
}

void renderTextureContain(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& bounds,
                          Uint8 alpha, float alignX, float alignY) {
    if (texture == nullptr) {
        return;
    }

    int texW = 0;
    int texH = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH) != 0 || texW <= 0 || texH <= 0) {
        return;
    }

    const float scale = std::min(bounds.w / static_cast<float>(texW), bounds.h / static_cast<float>(texH));
    const float dstW = static_cast<float>(texW) * scale;
    const float dstH = static_cast<float>(texH) * scale;
    const SDL_Rect dst{
        static_cast<int>(std::lround(bounds.x + (bounds.w - dstW) * std::clamp(alignX, 0.0f, 1.0f))),
        static_cast<int>(std::lround(bounds.y + (bounds.h - dstH) * std::clamp(alignY, 0.0f, 1.0f))),
        static_cast<int>(std::lround(dstW)),
        static_cast<int>(std::lround(dstH))
    };

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_SetTextureAlphaMod(texture, 255);
}

bool beginMenuCanvas(SDL_Renderer* renderer, SDL_Texture* canvas) {
    if (canvas == nullptr) {
        return false;
    }

    if (SDL_SetRenderTarget(renderer, canvas) != 0) {
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_RenderSetScale(renderer, kMenuCanvasScale, kMenuCanvasScale);
    return true;
}

SDL_FRect uiDestinationRect(int windowWidth, int windowHeight) {
    const float scale = std::min(
        static_cast<float>(windowWidth) / static_cast<float>(kReferenceWidth),
        static_cast<float>(windowHeight) / static_cast<float>(kReferenceHeight)
    );
    const float dstW = static_cast<float>(kReferenceWidth) * scale;
    const float dstH = static_cast<float>(kReferenceHeight) * scale;
    return SDL_FRect{
        (static_cast<float>(windowWidth) - dstW) * 0.5f,
        (static_cast<float>(windowHeight) - dstH) * 0.5f,
        dstW,
        dstH
    };
}

bool mapWindowPointToReference(float windowX, float windowY, int windowWidth, int windowHeight, SDL_FPoint& outPoint) {
    const SDL_FRect dst = uiDestinationRect(windowWidth, windowHeight);
    if (!pointInRect(windowX, windowY, dst)) {
        return false;
    }

    outPoint.x = (windowX - dst.x) * static_cast<float>(kReferenceWidth) / dst.w;
    outPoint.y = (windowY - dst.y) * static_cast<float>(kReferenceHeight) / dst.h;
    return true;
}

void endMenuCanvas(SDL_Renderer* renderer, SDL_Texture* canvas, int windowWidth, int windowHeight) {
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(renderer, nullptr);
    const SDL_FRect overlayRect = uiDestinationRect(windowWidth, windowHeight);
    const SDL_Rect dst{
        static_cast<int>(std::lround(overlayRect.x)),
        static_cast<int>(std::lround(overlayRect.y)),
        static_cast<int>(std::lround(overlayRect.w)),
        static_cast<int>(std::lround(overlayRect.h))
    };
    SDL_RenderCopy(renderer, canvas, nullptr, &dst);
}

bool beginReferenceLayout(SDL_Renderer* renderer, int windowWidth, int windowHeight) {
    const SDL_FRect overlayRect = uiDestinationRect(windowWidth, windowHeight);
    const SDL_Rect viewport{
        static_cast<int>(std::lround(overlayRect.x)),
        static_cast<int>(std::lround(overlayRect.y)),
        static_cast<int>(std::lround(overlayRect.w)),
        static_cast<int>(std::lround(overlayRect.h))
    };
    if (viewport.w <= 0 || viewport.h <= 0) {
        return false;
    }

    if (SDL_RenderSetViewport(renderer, &viewport) != 0) {
        return false;
    }

    const float scale = static_cast<float>(viewport.w) / static_cast<float>(kReferenceWidth);
    SDL_RenderSetScale(renderer, scale, scale);
    SDL_RenderSetClipRect(renderer, nullptr);
    return true;
}

void endReferenceLayout(SDL_Renderer* renderer) {
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetClipRect(renderer, nullptr);
}

std::string effectiveStoryBackground(const StorySession& story, std::size_t entryIndex) {
    if (story.script.entries.empty()) {
        return std::string();
    }

    const std::size_t clampedIndex = std::min(entryIndex, story.script.entries.size() - 1);
    for (std::size_t i = clampedIndex + 1; i > 0; --i) {
        const auto& candidate = story.script.entries[i - 1];
        if (!candidate.background.empty()) {
            return candidate.background;
        }
    }

    return std::string();
}

std::string storyScriptPathFromReference(const std::string& scriptRef) {
    if (scriptRef.empty()) {
        return resolvePath(kChapterScriptPath);
    }

    const bool hasDirectorySeparators =
        scriptRef.find('/') != std::string::npos || scriptRef.find('\\') != std::string::npos;
    if (hasDirectorySeparators) {
        return resolvePath(scriptRef);
    }

    const bool hasJsonExtension =
        scriptRef.size() >= 5 &&
        scriptRef.substr(scriptRef.size() - 5) == ".json";
    const std::string normalized = hasJsonExtension ? scriptRef : (scriptRef + ".json");
    return resolvePath("assets/vn/json/" + normalized);
}

void applyCurrentEntry(const StorySession& story, const GameSettings& settings) {
    if (story.script.entries.empty() || story.entryIndex >= story.script.entries.size()) {
        return;
    }

    vn::setVoiceVolume(settings.voiceVolume);
    vn::setTypewriterSpeed(settings.textSpeed);

    const auto& entry = story.script.entries[story.entryIndex];
    const std::string iconPath = entry.icon.empty() ? std::string{} : platform::path::resolvePath(entry.icon);
    const std::string voicePath = entry.voice.empty() ? std::string{} : platform::path::resolvePath(entry.voice);
    const std::string fontPath = entry.fontPath.empty() ? std::string{} : platform::path::resolvePath(entry.fontPath);
    const std::string backgroundRef = entry.background.empty()
        ? effectiveStoryBackground(story, story.entryIndex)
        : entry.background;
    const std::string backgroundPath = backgroundRef.empty() ? std::string{} : platform::path::resolvePath(backgroundRef);

    vn::showLine(
        entry.text,
        vn::getDisplaySpeakerName(entry),
        iconPath,
        voicePath,
        fontPath,
        entry.autoAdvanceOnVoiceEnd,
        entry.iconFrameCount,
        entry.iconFps,
        backgroundPath
    );
}

bool loadStoryScript(StorySession& story, const std::string& scriptRef) {
    vn::Script script;
    if (!vn::loadScript(storyScriptPathFromReference(scriptRef), script)) {
        return false;
    }

    story.script = std::move(script);
    story.loaded = true;
    story.entryIndex = 0;
    return true;
}

bool ensureStoryLoaded(StorySession& story) {
    if (story.loaded) {
        return true;
    }

    return loadStoryScript(story, kChapterScriptPath);
}

save::SaveGame buildStorySaveGame(const AppState& state) {
    save::SaveGame saveGame;
    saveGame.chapter = save::chapterIdFromScript(state.story.script);
    saveGame.entryIndex = static_cast<int>(state.story.entryIndex);
    saveGame.label = save::generateLabel(state.story.script, state.story.entryIndex);
    saveGame.settings.fullscreen = state.settings.fullscreen;
    saveGame.settings.voiceVolume = static_cast<int>(std::lround(std::clamp(state.settings.voiceVolume, 0.0f, 1.0f) * 100.0f));
    saveGame.settings.textSpeed = static_cast<int>(std::lround(std::max(1.0f, state.settings.textSpeed)));
    return saveGame;
}

bool tryStartDuplicateSaveOverwritePrompt(AppState& state, const save::SaveGame& saveGame) {
    const std::optional<std::filesystem::path> existingPath = save::findMatchingManualSavePath(saveGame);
    if (!existingPath.has_value()) {
        return false;
    }

    state.pendingOverwriteSavePath = existingPath->string();
    state.confirmSelection = ConfirmAction::Cancel;
    state.screen = ScreenState::PauseConfirmOverwriteSave;
    return true;
}

bool canSaveCurrentStoryState(const AppState& state, std::string& outReason) {
    if (!state.story.loaded || state.story.script.entries.empty()) {
        outReason = "No story progress to save.";
        return false;
    }
    if (state.story.entryIndex >= state.story.script.entries.size()) {
        outReason = "No story progress to save.";
        return false;
    }
    if (!vn::isLineFinished()) {
        outReason = "Wait for dialogue to finish.";
        return false;
    }

    const vn::ScriptEntry& entry = state.story.script.entries[state.story.entryIndex];
    if (entry.autoAdvanceOnVoiceEnd && vn::isVoicePlaying()) {
        outReason = "Wait for voice playback to finish.";
        return false;
    }

    return true;
}

bool restoreStorySave(AppState& state, const save::SaveGame& saveGame) {
    vn::Script script;
    const std::string scriptPath = resolvePath(save::chapterScriptPathFromId(saveGame.chapter));
    if (!vn::loadScript(scriptPath, script)) {
        return false;
    }
    if (script.entries.empty()) {
        return false;
    }

    state.settings.fullscreen = saveGame.settings.fullscreen;
    state.settings.voiceVolume = std::clamp(static_cast<float>(saveGame.settings.voiceVolume) / 100.0f, 0.0f, 1.0f);
    state.settings.textSpeed = static_cast<float>(std::max(1, saveGame.settings.textSpeed));

    state.story.script = std::move(script);
    state.story.loaded = true;
    state.story.entryIndex = static_cast<std::size_t>(std::clamp(saveGame.entryIndex, 0, static_cast<int>(state.story.script.entries.size() - 1)));

    vn::reset();
    vn::setVoiceVolume(state.settings.voiceVolume);
    vn::setTypewriterSpeed(state.settings.textSpeed);

    state.pauseSelection = PauseAction::Continue;
    state.pauseContext = PauseContext::Story;
    state.confirmSelection = ConfirmAction::Cancel;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::Playing;

    applyCurrentEntry(state.story, state.settings);
    return true;
}

void beginStory(AppState& state) {
    if (!ensureStoryLoaded(state.story)) {
        state.noticeText = "Chapter 0 failed to load.";
        state.noticeTimer = 2.6f;
        state.screen = ScreenState::MainMenu;
        return;
    }

    if (state.story.script.entries.empty()) {
        state.noticeText = "Chapter 0 has no dialogue entries.";
        state.noticeTimer = 2.6f;
        state.screen = ScreenState::MainMenu;
        return;
    }

    state.story.entryIndex = 0;
    state.pauseSelection = PauseAction::Continue;
    state.pauseContext = PauseContext::Story;
    state.confirmSelection = ConfirmAction::Cancel;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::Playing;
    state.pendingBattleKey.clear();
    state.pendingBattleLaunchedFromStory = false;
    state.pendingBattleWinScript.clear();
    state.pendingBattleLoseScript.clear();
    vn::reset();
    vn::setVoiceVolume(state.settings.voiceVolume);
    vn::setTypewriterSpeed(state.settings.textSpeed);
    applyCurrentEntry(state.story, state.settings);
    (void)save::autosave(buildStorySaveGame(state));
}

void beginBattleDemo(AppState& state) {
    state.pauseSelection = PauseAction::Continue;
    state.pauseContext = PauseContext::Battle;
    state.confirmSelection = ConfirmAction::Cancel;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::BattleDemo;
}

void beginBattle(AppState& state, const std::string& battleKey) {
    if (battleKey.empty()) {
        state.pendingBattleKey.clear();
        state.pendingBattleLaunchedFromStory = false;
        state.pendingBattleWinScript.clear();
        state.pendingBattleLoseScript.clear();
        state.noticeText = "Battle key missing.";
        state.noticeTimer = 2.6f;
        state.screen = ScreenState::MainMenu;
        return;
    }

    vn::setBackground("");
    battle::BattleDefinition battleDefinition;
    if (!battle::loader::loadBattleDefinition(battleKey, battleDefinition)) {
        state.pendingBattleKey.clear();
        state.pendingBattleLaunchedFromStory = false;
        state.pendingBattleWinScript.clear();
        state.pendingBattleLoseScript.clear();
        state.noticeText = "Battle not found: " + battleKey;
        state.noticeTimer = 2.6f;
        state.screen = ScreenState::MainMenu;
        return;
    }

    state.pendingBattleKey = battleDefinition.key;
    beginBattleDemo(state);
}

// Dispatches to the correct battle session based on the ID defined in assets/combat/battles.json.
void beginBattle(AppState& state, int battleId) {
    battle::BattleDefinition battleDefinition;
    if (!battle::loader::loadBattleDefinitionById(battleId, battleDefinition)) {
        state.pendingBattleKey.clear();
        state.pendingBattleLaunchedFromStory = false;
        state.pendingBattleWinScript.clear();
        state.pendingBattleLoseScript.clear();
        state.noticeText = "Battle not found: " + std::to_string(battleId);
        state.noticeTimer = 2.6f;
        state.screen = ScreenState::MainMenu;
        return;
    }

    beginBattle(state, battleDefinition.key);
}

std::string shellQuote(const std::string& value) {
    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}

int launchDefaultBattleMode(int argc, char** argv) {
    std::filesystem::path executablePath = std::filesystem::absolute(argv[0]);
    const std::string siblingName =
#ifdef _WIN32
        "battle_testing.exe";
#else
        "battle_testing";
#endif
    const std::filesystem::path battlePath = executablePath.parent_path() / siblingName;
    if (!std::filesystem::exists(battlePath)) {
        std::cerr << "Default battle executable not found: " << battlePath << "\n";
        return 1;
    }

    std::string command = shellQuote(battlePath.string());
    for (int i = 3; i < argc; ++i) {
        command += " " + shellQuote(argv[i]);
    }
    return std::system(command.c_str());
}

void destroyMenuResources(MenuResources& resources) {
    if (resources.background != nullptr) {
        SDL_DestroyTexture(resources.background);
        resources.background = nullptr;
    }
    if (resources.menuCanvas != nullptr) {
        SDL_DestroyTexture(resources.menuCanvas);
        resources.menuCanvas = nullptr;
    }
    if (resources.titleLogo != nullptr) {
        SDL_DestroyTexture(resources.titleLogo);
        resources.titleLogo = nullptr;
    }

#ifdef VN_ENABLE_TTF
    if (resources.titleFont != nullptr) {
        TTF_CloseFont(resources.titleFont);
        resources.titleFont = nullptr;
    }
    if (resources.subtitleFont != nullptr) {
        TTF_CloseFont(resources.subtitleFont);
        resources.subtitleFont = nullptr;
    }
    if (resources.itemFont != nullptr) {
        TTF_CloseFont(resources.itemFont);
        resources.itemFont = nullptr;
    }
    if (resources.smallFont != nullptr) {
        TTF_CloseFont(resources.smallFont);
        resources.smallFont = nullptr;
    }
    if (resources.tinyFont != nullptr) {
        TTF_CloseFont(resources.tinyFont);
        resources.tinyFont = nullptr;
    }

    platform::text::releaseFallbackCjkFonts();
#endif
}

void loadMenuResources(MenuResources& resources, SDL_Renderer* renderer) {
    resources.background = loadTexture(renderer, resolvePath(kMainMenuBackgroundArtPath));
    resources.menuCanvas = createRenderTarget(renderer, kReferenceWidth, kReferenceHeight, kMenuCanvasScale);
    resources.titleLogo = loadTexture(renderer, resolvePath(kMainMenuTitleLogoPath));
#ifdef VN_ENABLE_TTF
    resources.titleFont = openBestAvailableFont({}, 52);
    resources.subtitleFont = openBestAvailableFont({}, 28);
    resources.itemFont = openBestAvailableFont({}, 30);
    resources.smallFont = openBestAvailableFont({}, 18);
    resources.tinyFont = openBestAvailableFont({}, 14);
    if (resources.titleFont == nullptr || resources.itemFont == nullptr || resources.smallFont == nullptr) {
        std::cerr << "Menu font loading failed; UI text may be missing.\n";
    }
#endif
}

bool restoreRendererUi(Window& window, MenuResources& menuResources, const GameSettings& settings) {
    if (!window.enableRenderer()) {
        return false;
    }
    SDL_SetWindowResizable(window.getNativeWindow(), SDL_FALSE);
    if (!vn::initialize(window.getRenderer(), window.getWidth(), window.getHeight())) {
        return false;
    }
    vn::setVoiceVolume(settings.voiceVolume);
    vn::setTypewriterSpeed(settings.textSpeed);
    loadMenuResources(menuResources, window.getRenderer());
    return true;
}

#ifdef APP_ENABLE_RMLUI
bool shouldUseFrontUiScreen(const AppState& state) {
    if (state.screen == ScreenState::MainMenu || state.screen == ScreenState::Playing) {
        return true;
    }

    if ((state.screen == ScreenState::PauseMenu ||
         state.screen == ScreenState::PauseConfirmExit ||
         state.screen == ScreenState::PauseConfirmOverwriteSave) &&
        state.pauseContext == PauseContext::Story) {
        return true;
    }

    if ((state.screen == ScreenState::LoadMenu ||
         state.screen == ScreenState::LoadConfirmDelete) &&
        (state.loadReturnScreen == ScreenState::MainMenu ||
         (state.loadReturnScreen == ScreenState::PauseMenu &&
          state.pauseContext == PauseContext::Story))) {
        return true;
    }

    return state.screen == ScreenState::Settings &&
           (state.settingsReturnScreen == ScreenState::MainMenu ||
            (state.settingsReturnScreen == ScreenState::PauseMenu &&
             state.pauseContext == PauseContext::Story));
}

bool restoreFrontMenuUi(Window& window,
                        graphics::frontui::Session& frontUi,
                        MenuResources& menuResources,
                        bool& rendererUiReady,
                        const AppState& state) {
    if (rendererUiReady) {
        destroyMenuResources(menuResources);
        rendererUiReady = false;
    }

    if (frontUi.isInitialized()) {
        return frontUi.showForState(state);
    }

    if (!window.enableOpenGL()) {
        return false;
    }
    SDL_SetWindowResizable(window.getNativeWindow(), SDL_FALSE);
    if (!vn::initialize(nullptr, window.getWidth(), window.getHeight())) {
        return false;
    }
    return frontUi.initialize(window, state);
}

bool restoreNonMainMenuUi(Window& window,
                          graphics::frontui::Session& frontUi,
                          MenuResources& menuResources,
                          bool& rendererUiReady,
                          const GameSettings& settings) {
    if (frontUi.isInitialized()) {
        frontUi.shutdown();
    }

    if (rendererUiReady) {
        return true;
    }

    if (!restoreRendererUi(window, menuResources, settings)) {
        return false;
    }

    rendererUiReady = true;
    return true;
}

bool applyFrontMenuAction(AppState& state,
                          Window& window,
                          graphics::frontui::Session& frontUi,
                          MenuResources& menuResources,
                          bool& rendererUiReady,
                          MainMenuAction action) {
    state.mainSelection = action;

    if (action == MainMenuAction::Exit) {
        applyMainMenuAction(state, window, action);
        return true;
    }

    if (action == MainMenuAction::Settings) {
        applyMainMenuAction(state, window, action);
        return restoreFrontMenuUi(window, frontUi, menuResources, rendererUiReady, state);
    }

    if (action == MainMenuAction::Battle) {
        if (!restoreNonMainMenuUi(window, frontUi, menuResources, rendererUiReady, state.settings)) {
            return false;
        }
    }

    applyMainMenuAction(state, window, action);
    return true;
}
#endif

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "battle" && std::string(argv[2]) == "mode") {
        return launchDefaultBattleMode(argc, argv);
    }

    Window window("Hatsune Miku: Our Underground BIT Idol", 1280, 720);
    if (!window.isOpen()) {
        std::cerr << "Failed to initialize window\n";
        return 1;
    }
    SDL_SetWindowResizable(window.getNativeWindow(), SDL_FALSE);

    window.setEscapeToQuitEnabled(false);

    save::init();

    AppState state;
    state.settings.fullscreen = window.isFullscreen();

    MenuResources menuResources;
    SettingsMenuController settingsMenu;
    std::unique_ptr<battle::demo::Session> battleSession;
    bool rendererUiReady = false;
#ifdef APP_ENABLE_RMLUI
    graphics::frontui::Session frontUi;
    if (!restoreFrontMenuUi(window, frontUi, menuResources, rendererUiReady, state)) {
        std::cerr << "Failed to initialize RmlUi main menu\n";
        return 1;
    }
#else
    if (!restoreRendererUi(window, menuResources, state.settings)) {
        std::cerr << "Failed to initialize VN system\n";
        return 1;
    }
    rendererUiReady = true;
    vn::setVoiceVolume(state.settings.voiceVolume);
    vn::setTypewriterSpeed(state.settings.textSpeed);
#endif

#ifdef VN_AUTO_START_STORY
    beginStory(state);
#endif

    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (window.isOpen()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            window.handleEvent(event);

            if (window.getRenderer() != nullptr &&
                event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESIZED)) {
                vn::setViewportSize(window.getWidth(), window.getHeight());
            }

            if (event.type == SDL_KEYDOWN && event.key.repeat != 0) {
                continue;
            }

            switch (state.screen) {
                case ScreenState::MainMenu:
#ifdef APP_ENABLE_RMLUI
                    frontUi.handleEvent(event, state);
#else
                    handleMainMenuEvent(state, window, event, window.getWidth(), window.getHeight());
#endif
                    break;

                case ScreenState::Settings:
#ifdef APP_ENABLE_RMLUI
                    if (shouldUseFrontUiScreen(state)) {
                        frontUi.handleEvent(event, state);
                    } else {
                        settingsMenu.handleEvent(state, window, event, window.getWidth(), window.getHeight());
                    }
#else
                    settingsMenu.handleEvent(state, window, event, window.getWidth(), window.getHeight());
#endif
                    break;

                case ScreenState::LoadMenu:
                case ScreenState::LoadConfirmDelete:
#ifdef APP_ENABLE_RMLUI
                    if (shouldUseFrontUiScreen(state)) {
                        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else {
                            frontUi.handleEvent(event, state);
                        }
                    } else {
                        handleLoadMenuEvent(state, window, event, window.getWidth(), window.getHeight());
                    }
#else
                    handleLoadMenuEvent(state, window, event, window.getWidth(), window.getHeight());
#endif
                    break;

                case ScreenState::BattleDemo:
                    if (battleSession != nullptr) {
                        battleSession->handleEvent(event);
                    }
                    break;

                case ScreenState::Playing:
#ifdef APP_ENABLE_RMLUI
                    if (shouldUseFrontUiScreen(state)) {
                        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else {
                            frontUi.handleEvent(event, state);
                        }
                    } else if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            openPauseMenu(state);
                        } else if (event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else if (event.key.keysym.sym == SDLK_SPACE) {
                            vn::onSpacePressed();
                        }
                    }
#else
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            openPauseMenu(state);
                        } else if (event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else if (event.key.keysym.sym == SDLK_SPACE) {
                            vn::onSpacePressed();
                        }
                    }
#endif
                    break;

                case ScreenState::PauseMenu:
#ifdef APP_ENABLE_RMLUI
                    if (shouldUseFrontUiScreen(state)) {
                        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else {
                            frontUi.handleEvent(event, state);
                        }
                    } else {
                        handlePauseMenuEvent(state, window, event, window.getWidth(), window.getHeight());
                    }
#else
                    handlePauseMenuEvent(state, window, event, window.getWidth(), window.getHeight());
#endif
                    break;

                case ScreenState::PauseConfirmExit:
                case ScreenState::PauseConfirmOverwriteSave:
#ifdef APP_ENABLE_RMLUI
                    if (shouldUseFrontUiScreen(state)) {
                        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else {
                            frontUi.handleEvent(event, state);
                        }
                    } else {
                        handlePauseConfirmEvent(state, window, event, window.getWidth(), window.getHeight());
                    }
#else
                    handlePauseConfirmEvent(state, window, event, window.getWidth(), window.getHeight());
#endif
                    break;
            }
        }

#ifdef APP_ENABLE_RMLUI
        if (const std::optional<graphics::frontui::Command> command = frontUi.consumeCommand(); command.has_value()) {
            switch (command->type) {
                case graphics::frontui::CommandType::ActivateMainMenuAction:
                    if (!applyFrontMenuAction(state, window, frontUi, menuResources, rendererUiReady,
                                              command->mainMenuAction)) {
                        std::cerr << "Failed to apply main menu action\n";
                        return 1;
                    }
                    break;

                case graphics::frontui::CommandType::ApplyDisplayMode:
                    SettingsMenuController::applyDisplayMode(window, state.settings, command->displayModeFullscreen);
                    break;

                case graphics::frontui::CommandType::ReturnFromSettings:
                    state.screen = state.settingsReturnScreen;
                    break;
            }
        }

        if (shouldUseFrontUiScreen(state)) {
            if (!restoreFrontMenuUi(window, frontUi, menuResources, rendererUiReady, state)) {
                std::cerr << "Failed to restore RmlUi front-ui screen\n";
                return 1;
            }
        } else if (!restoreNonMainMenuUi(window, frontUi, menuResources, rendererUiReady, state.settings)) {
            std::cerr << "Failed to restore renderer UI\n";
            return 1;
        }
#endif

        if (state.requestStoryManualSave) {
            state.requestStoryManualSave = false;

            std::string reason;
            if (!canSaveCurrentStoryState(state, reason)) {
                state.noticeText = std::move(reason);
                state.noticeTimer = 2.0f;
            } else {
                const save::SaveGame saveGame = buildStorySaveGame(state);
                if (tryStartDuplicateSaveOverwritePrompt(state, saveGame)) {
                    // confirmation modal opened
                } else if (save::manualSave(saveGame)) {
                    state.noticeText = "Game saved.";
                    state.noticeTimer = 2.0f;
                } else {
                    state.noticeText = "Save failed.";
                    state.noticeTimer = 2.0f;
                }
            }
        }

        if (state.requestStoryOverwriteSave) {
            state.requestStoryOverwriteSave = false;

            std::string reason;
            if (!canSaveCurrentStoryState(state, reason)) {
                state.noticeText = std::move(reason);
                state.noticeTimer = 2.0f;
                state.pendingOverwriteSavePath.clear();
            } else if (!state.pendingOverwriteSavePath.empty() &&
                       save::writeToPath(state.pendingOverwriteSavePath, buildStorySaveGame(state))) {
                state.noticeText = "Game saved.";
                state.noticeTimer = 2.0f;
                state.pendingOverwriteSavePath.clear();
            } else {
                state.noticeText = "Save failed.";
                state.noticeTimer = 2.0f;
                state.pendingOverwriteSavePath.clear();
            }
        }

        if (!state.pendingLoadPath.empty()) {
            const std::string pendingLoadPath = state.pendingLoadPath;
            state.pendingLoadPath.clear();

            const std::optional<save::SaveGame> saveGame = save::load(pendingLoadPath);
            if (!saveGame.has_value()) {
                state.noticeText = "Save file corrupted.";
                state.noticeTimer = 2.4f;
            } else {
                if (restoreStorySave(state, *saveGame)) {
                    SettingsMenuController::applyDisplayMode(window, state.settings, saveGame->settings.fullscreen);
                    state.noticeText = "Game loaded.";
                    state.noticeTimer = 2.0f;
                } else {
                    state.noticeText = "Load failed.";
                    state.noticeTimer = 2.4f;
                }
            }
        }

        if (state.screen == ScreenState::BattleDemo && battleSession == nullptr) {
            destroyMenuResources(menuResources);
            vn::stopVoicePlayback();  // stop story audio; keep TTF alive (demo session uses VN internally)
            battleSession = std::make_unique<battle::demo::Session>();
            const std::string battleKey = state.pendingBattleKey.empty() ? "tutorial_vs_lyoo" : state.pendingBattleKey;
            if (!battleSession->initialize(window.getRenderer(), battleKey)) {
                battleSession.reset();
                state.screen = ScreenState::MainMenu;
                state.pauseContext = PauseContext::Story;
                state.mainSelection = MainMenuAction::Battle;
                state.pendingBattleKey.clear();
                state.pendingBattleLaunchedFromStory = false;
                state.pendingBattleWinScript.clear();
                state.pendingBattleLoseScript.clear();
                state.noticeText = "Battle demo failed to load.";
                state.noticeTimer = 2.8f;
            }
        }

        if (state.screen == ScreenState::MainMenu && battleSession != nullptr) {
            battleSession->shutdown();
            battleSession.reset();
            state.pauseContext = PauseContext::Story;
            state.pendingBattleKey.clear();
            state.pendingBattleLaunchedFromStory = false;
            state.pendingBattleWinScript.clear();
            state.pendingBattleLoseScript.clear();
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(now - lastCounter) /
            static_cast<float>(SDL_GetPerformanceFrequency());
        lastCounter = now;

        if (state.noticeTimer > 0.0f) {
            state.noticeTimer = std::max(0.0f, state.noticeTimer - deltaSeconds);
            if (state.noticeTimer <= 0.0f) {
                state.noticeText.clear();
            }
        }

        if (state.screen == ScreenState::MainMenu && state.menuIntroTime < kMenuIntroMaxTime) {
            state.menuIntroTime = std::min(kMenuIntroMaxTime, state.menuIntroTime + deltaSeconds);
        }
#ifdef APP_ENABLE_RMLUI
        if (shouldUseFrontUiScreen(state)) {
            frontUi.update(state, deltaSeconds);
        }
#endif

        if ((state.screen == ScreenState::PauseMenu ||
             state.screen == ScreenState::PauseConfirmExit ||
             state.screen == ScreenState::PauseConfirmOverwriteSave) &&
            state.pauseIntroTime < kPauseIntroMaxTime) {
            state.pauseIntroTime = std::min(kPauseIntroMaxTime, state.pauseIntroTime + deltaSeconds);
        }

        if (state.screen == ScreenState::BattleDemo) {
            if (battleSession != nullptr) {
                battleSession->update(deltaSeconds);
                if (battleSession->isFinished()) {
                    const battle::demo::BattleOutcome outcome = battleSession->outcome();
                    battleSession->shutdown();
                    battleSession.reset();
                    if (!restoreRendererUi(window, menuResources, state.settings)) {
                        std::cerr << "Failed to restore renderer UI\n";
                        return 1;
                    }

                    const bool launchedFromStory = state.pendingBattleLaunchedFromStory;
                    const std::string battleWinScript = state.pendingBattleWinScript;
                    state.pendingBattleKey.clear();
                    state.pendingBattleLaunchedFromStory = false;
                    state.pendingBattleWinScript.clear();
                    state.pendingBattleLoseScript.clear();

                    if (launchedFromStory && outcome == battle::demo::BattleOutcome::Victory) {
                        if (!battleWinScript.empty()) {
                            if (!loadStoryScript(state.story, battleWinScript)) {
                                state.screen = ScreenState::MainMenu;
                                state.pauseContext = PauseContext::Story;
                                state.mainSelection = MainMenuAction::Start;
                                state.noticeText = "Post-battle story failed to load.";
                                state.noticeTimer = 2.8f;
                            } else {
                                vn::reset();
                                vn::setVoiceVolume(state.settings.voiceVolume);
                                vn::setTypewriterSpeed(state.settings.textSpeed);
                                state.pauseSelection = PauseAction::Continue;
                                state.pauseContext = PauseContext::Story;
                                state.confirmSelection = ConfirmAction::Cancel;
                                state.pauseIntroTime = 0.0f;
                                state.screen = ScreenState::Playing;
                                applyCurrentEntry(state.story, state.settings);
                            }
                        } else if (state.story.loaded &&
                                   state.story.entryIndex < state.story.script.entries.size()) {
                            state.pauseSelection = PauseAction::Continue;
                            state.pauseContext = PauseContext::Story;
                            state.confirmSelection = ConfirmAction::Cancel;
                            state.pauseIntroTime = 0.0f;
                            state.screen = ScreenState::Playing;
                            applyCurrentEntry(state.story, state.settings);
                        } else {
                            state.screen = ScreenState::MainMenu;
                            state.pauseContext = PauseContext::Story;
                            state.mainSelection = MainMenuAction::Start;
                            state.noticeText = "End of story.";
                            state.noticeTimer = 2.2f;
                        }
                    } else {
                        state.screen = ScreenState::MainMenu;
                        state.pauseContext = PauseContext::Story;
                        state.mainSelection = MainMenuAction::Battle;
                    }
                }
            }
        } else if (state.screen == ScreenState::Playing) {
            vn::update(deltaSeconds);

            if (vn::consumeAdvanceRequest()) {
                const std::string previousBackground = effectiveStoryBackground(state.story, state.story.entryIndex);
                const vn::ScriptEntry currentEntry =
                    state.story.entryIndex < state.story.script.entries.size()
                    ? state.story.script.entries[state.story.entryIndex]
                    : vn::ScriptEntry{};
                const std::string pendingBattleKey = currentEntry.battleKey;
                const int pendingBattleId = currentEntry.battleId;
                state.story.entryIndex++;
                if (!pendingBattleKey.empty()) {
                    state.pendingBattleLaunchedFromStory = true;
                    state.pendingBattleWinScript = currentEntry.battleWinScript;
                    state.pendingBattleLoseScript = currentEntry.battleLoseScript;
                    beginBattle(state, pendingBattleKey);
                } else if (pendingBattleId >= 0) {
                    state.pendingBattleLaunchedFromStory = true;
                    state.pendingBattleWinScript = currentEntry.battleWinScript;
                    state.pendingBattleLoseScript = currentEntry.battleLoseScript;
                    beginBattle(state, pendingBattleId);
                } else if (state.story.entryIndex >= state.story.script.entries.size()) {
                    state.screen = ScreenState::MainMenu;
                    state.mainSelection = MainMenuAction::Start;
                    state.noticeText = "End of chapter 0.";
                    state.noticeTimer = 2.2f;
                } else {
                    const auto& entry = state.story.script.entries[state.story.entryIndex];
                    if (!entry.background.empty() && entry.background != previousBackground) {
                        (void)save::autosave(buildStorySaveGame(state));
                    }
                    applyCurrentEntry(state.story, state.settings);
                }
            }
        }

#ifdef APP_ENABLE_RMLUI
        if (shouldUseFrontUiScreen(state)) {
            if (!restoreFrontMenuUi(window, frontUi, menuResources, rendererUiReady, state)) {
                std::cerr << "Failed to restore RmlUi front-ui screen\n";
                return 1;
            }
        } else if (!restoreNonMainMenuUi(window, frontUi, menuResources, rendererUiReady, state.settings)) {
            std::cerr << "Failed to restore renderer UI\n";
            return 1;
        }
#endif

        if (window.getRenderer() != nullptr) {
            window.clear(14, 18, 30, 255);
        }

        const bool renderBattleBackdrop =
            battleSession != nullptr &&
            (((state.screen == ScreenState::PauseMenu ||
               state.screen == ScreenState::PauseConfirmExit ||
               state.screen == ScreenState::PauseConfirmOverwriteSave) &&
              state.pauseContext == PauseContext::Battle) ||
             (state.screen == ScreenState::Settings &&
              state.settingsReturnScreen == ScreenState::PauseMenu &&
              state.pauseContext == PauseContext::Battle));

        if (shouldUseFrontUiScreen(state)) {
#ifdef APP_ENABLE_RMLUI
            frontUi.render();
#else
            renderMainMenu(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
#endif
        } else if (renderBattleBackdrop) {
            battleSession->render(window.getRenderer(), window.getWidth(), window.getHeight());
            if (state.screen == ScreenState::PauseMenu ||
                state.screen == ScreenState::PauseConfirmExit ||
                state.screen == ScreenState::PauseConfirmOverwriteSave) {
                renderPauseScreen(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
            } else if (state.screen == ScreenState::Settings && state.settingsReturnScreen == ScreenState::PauseMenu) {
                settingsMenu.render(window.getRenderer(), menuResources, state,
                                    window.getWidth(), window.getHeight(), true);
            }
        } else if (state.screen == ScreenState::Settings) {
            if (state.settingsReturnScreen == ScreenState::MainMenu) {
#ifdef APP_ENABLE_RMLUI
                frontUi.render();
#else
                settingsMenu.render(window.getRenderer(), menuResources, state,
                                    window.getWidth(), window.getHeight(), false);
#endif
            } else {
                settingsMenu.render(window.getRenderer(), menuResources, state,
                                    window.getWidth(), window.getHeight(), false);
            }
        } else if (state.screen == ScreenState::LoadMenu ||
                   state.screen == ScreenState::LoadConfirmDelete) {
            renderLoadScreen(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
        } else if (state.screen == ScreenState::BattleDemo && battleSession != nullptr) {
            battleSession->render(window.getRenderer(), window.getWidth(), window.getHeight());
        } else {
#ifdef APP_ENABLE_RMLUI
            frontUi.render();
#else
            renderMainMenu(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
#endif
        }

        window.present();
    }

    if (battleSession != nullptr) {
        battleSession->shutdown();
        battleSession.reset();
    }
#ifdef APP_ENABLE_RMLUI
    frontUi.shutdown();
#endif
    destroyMenuResources(menuResources);
    vn::shutdown();
    return 0;
}
