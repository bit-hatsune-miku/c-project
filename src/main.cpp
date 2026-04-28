#define GL_GLEXT_PROTOTYPES

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
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
#include "game/core/tutorial_scenarios.h"
#ifdef RMLUI_SDL_VERSION_MAJOR
#include "game/boss_selector_session.h"
#include "game/party_loader_session.h"
#include "game/post_battle_session.h"
#endif
#include "game/audio/ui_music_controller.h"
#include "game/app_battle_session.h"
#include "game/render/gl_function_loader.h"
#include "game/save/save.h"
#include "game/vn/vn_script_catalog.h"
#include "game/vn/vn_system.h"
#ifdef APP_ENABLE_RMLUI
#include "graphics/front_ui_session.h"
#endif
#include "platform/path_resolution.h"
#include "platform/text_fallback.h"

#ifdef VN_SCRIPT_PATH
constexpr const char* kInitialStoryScriptRef = VN_SCRIPT_PATH;
#else
constexpr const char* kInitialStoryScriptRef = "ch0001";
#endif
constexpr int kStartupWindowWidth = 1280;
constexpr int kStartupWindowHeight = 720;
constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr float kMenuCanvasScale = 2.0f;
constexpr float kLoadingFadeInSeconds = 0.42f;
constexpr float kLoadingLogoHoldSeconds = 3.0f;
constexpr float kLoadingFadeOutSeconds = 0.44f;
constexpr float kLoadingLogoBounceAmplitude = 8.0f;
constexpr float kLoadingLogoBounceSpeed = 2.4f;
constexpr float kPi = 3.14159265358979323846f;
constexpr const char* kUnlockDrillBattleKey = "test_ground";

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
    const std::string resolvedPath = vn::resolveScriptPath(
        scriptRef.empty() ? std::string(kInitialStoryScriptRef) : scriptRef);
    return resolvedPath.empty() ? resolvePath(scriptRef) : resolvedPath;
}

ScreenState resolveStoryEndReturnScreen(const vn::Script& script, ScreenState fallback) {
    if (script.endReturnScreen == "selector" || script.endReturnScreen == "boss_selector") {
        return ScreenState::BossSelector;
    }
    if (script.endReturnScreen == "main_menu" || script.endReturnScreen == "menu") {
        return ScreenState::MainMenu;
    }
    return fallback;
}

std::string resolvePendingStoryNextScript(const vn::Script& script) {
    const std::string scriptId = save::chapterIdFromScript(script);
    if (scriptId.empty()) {
        return std::string();
    }

    std::vector<battle::BattleDefinition> battleDefinitions;
    if (!battle::loader::loadAllBattleDefinitions(battleDefinitions)) {
        return std::string();
    }

    const auto it = std::find_if(
        battleDefinitions.begin(),
        battleDefinitions.end(),
        [&scriptId](const battle::BattleDefinition& battleDefinition) {
            return battleDefinition.victoryStoryScript == scriptId;
        });
    if (it == battleDefinitions.end()) {
        for (const vn::ScriptCatalogEntry& catalogEntry : vn::scriptCatalog()) {
            if (catalogEntry.category == "battle") {
                continue;
            }

            vn::Script candidateScript;
            if (!vn::loadScript(vn::resolveScriptPath(catalogEntry.scriptId), candidateScript)) {
                continue;
            }

            for (const vn::ScriptEntry& scriptEntry : candidateScript.entries) {
                if (scriptEntry.battleWinScript != scriptId) {
                    continue;
                }

                battle::BattleDefinition linkedBattleDefinition;
                bool loadedBattleDefinition = false;
                if (!scriptEntry.battleKey.empty()) {
                    loadedBattleDefinition =
                        battle::loader::loadBattleDefinition(scriptEntry.battleKey, linkedBattleDefinition);
                } else if (scriptEntry.battleId >= 0) {
                    loadedBattleDefinition =
                        battle::loader::loadBattleDefinitionById(scriptEntry.battleId, linkedBattleDefinition);
                }

                if (loadedBattleDefinition && !linkedBattleDefinition.nextStoryScript.empty()) {
                    return linkedBattleDefinition.nextStoryScript;
                }
            }
        }

        return std::string();
    }

    return it->nextStoryScript;
}

save::SaveContext saveContextForStoryFlow(StoryFlowMode flowMode) {
    return flowMode == StoryFlowMode::PracticeReplay
        ? save::SaveContext::Practice
        : save::SaveContext::Campaign;
}

StoryFlowMode storyFlowModeForSaveContext(save::SaveContext saveContext) {
    return saveContext == save::SaveContext::Practice
        ? StoryFlowMode::PracticeReplay
        : StoryFlowMode::Campaign;
}

StoryFlowMode storyFlowModeForBattleFlow(BattleFlowMode flowMode) {
    return flowMode == BattleFlowMode::PracticeReplayStory
        ? StoryFlowMode::PracticeReplay
        : StoryFlowMode::Campaign;
}

/**
 * @brief Apply the currently indexed script entry to the VN runtime.
 *
 * If the story has no entries or the entry index is out of range, this function does nothing.
 * Otherwise it updates VN runtime audio and text speed from settings, resolves any per-entry
 * asset references (icon, voice, bgm, font, and background — falling back to the nearest
 * previous non-empty background when the entry's background is empty), and displays the
 * entry's line with the resolved assets and entry playback flags.
 *
 * @param story Current story session containing the script and entry index.
 * @param settings Runtime settings used to set music volume, voice volume, and typewriter speed.
 */
void applyCurrentEntry(const StorySession& story, const GameSettings& settings) {
    if (story.script.entries.empty() || story.entryIndex >= story.script.entries.size()) {
        return;
    }

    vn::setMusicVolume(settings.musicVolume);
    vn::setVoiceVolume(settings.voiceVolume);
    vn::setTypewriterSpeed(settings.textSpeed);

    const auto& entry = story.script.entries[story.entryIndex];
    const std::string iconPath = entry.icon.empty() ? std::string{} : platform::path::resolvePath(entry.icon);
    const std::string voicePath = entry.voice.empty()
        ? std::string{}
        : platform::path::resolveAudioPath(entry.voice).value_or(platform::path::resolvePath(entry.voice));
    const std::string bgmPath = entry.bgm.empty()
        ? std::string{}
        : platform::path::resolveAudioPath(entry.bgm).value_or(platform::path::resolvePath(entry.bgm));
    const std::string fontPath = entry.fontPath.empty() ? std::string{} : platform::path::resolvePath(entry.fontPath);
    const std::string backgroundRef = entry.background.empty()
        ? (entry.clearBackground ? std::string{} : effectiveStoryBackground(story, story.entryIndex))
        : entry.background;
    const std::string backgroundPath = backgroundRef.empty()
        ? std::string{}
        : (vn::isHexColorString(backgroundRef) ? backgroundRef : platform::path::resolvePath(backgroundRef));
    const std::string accentColor = vn::isHexColorString(entry.color) ? entry.color : std::string{};

    if (entry.clearBackground) {
        vn::setBackground("");
    }

    vn::showLine(
        entry.text,
        vn::getDisplaySpeakerName(entry),
        iconPath,
        voicePath,
        fontPath,
        entry.autoAdvanceOnVoiceEnd,
        entry.iconFrameCount,
        entry.iconFps,
        backgroundPath,
        bgmPath,
        entry.bgmVolume,
        entry.bgmStop,
        entry.bgmPause,
        accentColor
    );
}

void debugSkipStoryToLastEntry(AppState& state) {
    if (!state.story.loaded || state.story.script.entries.empty()) {
        return;
    }

    const std::size_t lastIndex = state.story.script.entries.size() - 1;
    if (state.story.entryIndex >= lastIndex) {
        vn::onSpacePressed();
        return;
    }

    state.story.entryIndex = lastIndex;
    applyCurrentEntry(state.story, state.settings);
    if (!vn::isLineFinished()) {
        vn::onSpacePressed();
    }
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

        return loadStoryScript(story, kInitialStoryScriptRef);
}

struct StoryModeCandidate {
    save::SlotInfo slot;
    save::SaveGame saveGame;
    int chapterRank = std::numeric_limits<int>::min();
};

int storyModeChapterRank(const save::SaveGame& saveGame) {
    int numericRank = 0;
    bool hasDigits = false;
    for (const unsigned char c : saveGame.chapter) {
        if (!std::isdigit(c)) {
            continue;
        }
        hasDigits = true;
        numericRank = numericRank * 10 + static_cast<int>(c - '0');
    }

    if (saveGame.chapter.rfind("finale", 0) == 0) {
        return 1'000'000 + numericRank;
    }

    return hasDigits ? numericRank : std::numeric_limits<int>::min();
}

std::vector<StoryModeCandidate> collectStoryModeCandidates() {
    std::vector<StoryModeCandidate> candidates;
    for (const save::SlotInfo& slot : save::listSlots()) {
        const std::optional<save::SaveGame> saveGame = save::load(slot.path);
        if (!saveGame.has_value() || saveGame->saveContext != save::SaveContext::Campaign) {
            continue;
        }

        StoryModeCandidate candidate;
        candidate.slot = slot;
        candidate.chapterRank = storyModeChapterRank(*saveGame);
        candidate.saveGame = *saveGame;
        candidates.push_back(std::move(candidate));
    }

    std::sort(candidates.begin(), candidates.end(), [](const StoryModeCandidate& lhs, const StoryModeCandidate& rhs) {
        if (lhs.saveGame.timestamp != rhs.saveGame.timestamp) {
            return lhs.saveGame.timestamp > rhs.saveGame.timestamp;
        }
        if (lhs.slot.isAutosave != rhs.slot.isAutosave) {
            return lhs.slot.isAutosave;
        }
        if (lhs.chapterRank != rhs.chapterRank) {
            return lhs.chapterRank > rhs.chapterRank;
        }
        if (lhs.saveGame.entryIndex != rhs.saveGame.entryIndex) {
            return lhs.saveGame.entryIndex > rhs.saveGame.entryIndex;
        }
        return lhs.slot.path.generic_string() < rhs.slot.path.generic_string();
    });

    return candidates;
}

/**
 * @brief Builds a SaveGame snapshot capturing the current story position and relevant player settings.
 *
 * Populates a save::SaveGame with the active chapter id, current entry index and generated label,
 * serialized display/audio/text settings, and the current progression state.
 *
 * @param state The current application state used as the source of truth for story position, settings, and progression.
 * @return save::SaveGame A save object containing:
 *  - `chapter`: chapter id derived from the current script,
 *  - `entryIndex`: index of the current story entry,
 *  - `label`: generated label for the save position,
 *  - `settings.fullscreen`: fullscreen flag,
 *  - `settings.musicVolume`: music volume as an integer 0–100,
 *  - `settings.voiceVolume`: voice volume as an integer 0–100,
 *  - `settings.textSpeed`: text speed as an integer >= 1,
 *  - `progression`: copied progression data,
 *  - `hasProgressionData`: set to `true`.
 */
save::SaveGame buildStorySaveGame(const AppState& state) {
    save::SaveGame saveGame;
    saveGame.chapter = save::chapterIdFromScript(state.story.script);
    saveGame.scriptId = saveGame.chapter;
    saveGame.saveContext = saveContextForStoryFlow(state.storyFlowMode);
    saveGame.entryIndex = static_cast<int>(state.story.entryIndex);
    saveGame.label = save::generateLabel(state.story.script, state.story.entryIndex);
    saveGame.settings.fullscreen = state.settings.fullscreen;
    saveGame.settings.musicVolume = static_cast<int>(std::lround(std::clamp(state.settings.musicVolume, 0.0f, 1.0f) * 100.0f));
    saveGame.settings.voiceVolume = static_cast<int>(std::lround(std::clamp(state.settings.voiceVolume, 0.0f, 1.0f) * 100.0f));
    saveGame.settings.textSpeed = static_cast<int>(std::lround(std::max(1.0f, state.settings.textSpeed)));
    saveGame.progression = state.progression;
    saveGame.hasProgressionData = true;
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

/**
 * @brief Restores application state from a saved story snapshot and prepares playback.
 *
 * Loads the saved chapter script, validates it, restores settings (fullscreen, music/voice volumes, text speed),
 * player progression, and VN runtime state, then positions the story at the saved entry and applies that entry
 * so playback resumes immediately.
 *
 * @param state Mutable application state to update with restored data.
 * @param saveGame Saved game data to restore from.
 * @return true if the save was successfully restored and the story is ready to play; `false` if the script
 *         could not be loaded or the loaded script contains no entries.
 */
bool restoreStorySave(AppState& state, const save::SaveGame& saveGame) {
    vn::Script script;
    const std::string scriptPath = save::chapterScriptPathFromId(
        saveGame.scriptId.empty() ? saveGame.chapter : saveGame.scriptId);
    if (!vn::loadScript(scriptPath, script)) {
        return false;
    }
    if (script.entries.empty()) {
        return false;
    }

    state.settings.fullscreen = saveGame.settings.fullscreen;
    state.settings.musicVolume = std::clamp(static_cast<float>(saveGame.settings.musicVolume) / 100.0f, 0.0f, 1.0f);
    state.settings.voiceVolume = std::clamp(static_cast<float>(saveGame.settings.voiceVolume) / 100.0f, 0.0f, 1.0f);
    state.settings.textSpeed = static_cast<float>(std::max(1, saveGame.settings.textSpeed));
    state.progression = saveGame.progression;
    battle::normalizePlayerProgression(state.progression, battle::ProgressionFallbackPolicy::StarterRoster);
    (void)save::writeProfileProgression(state.progression);

    state.story.script = std::move(script);
    state.story.loaded = true;
    state.storyFlowMode = storyFlowModeForSaveContext(saveGame.saveContext);
    state.story.entryIndex = static_cast<std::size_t>(std::clamp(saveGame.entryIndex, 0, static_cast<int>(state.story.script.entries.size() - 1)));
    state.storyEndReturnScreen = resolveStoryEndReturnScreen(state.story.script, ScreenState::MainMenu);
    state.requestStoryReturnToBossSelector = false;
    state.pendingBattleKey.clear();
    state.pendingBattlePartyLineup.clear();
    state.pendingBattleFlowMode = BattleFlowMode::Direct;
    state.pendingBattleReturnScreen = ScreenState::MainMenu;
    state.pendingBattleWinScript.clear();
    state.pendingBattleLoseScript.clear();
    state.pendingBattleNextStoryScript.clear();
    state.pendingStoryNextScript =
        state.storyFlowMode == StoryFlowMode::Campaign
        ? resolvePendingStoryNextScript(state.story.script)
        : std::string();

    vn::reset();
    vn::setMusicVolume(state.settings.musicVolume);
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

/**
 * @brief Initialize and begin playback of a story script.
 *
 * Loads the script identified by `scriptRef` (or the default when empty), validates it has dialogue
 * entries, and transitions the application into the Playing screen. On load failure or an empty
 * script the function sets an explanatory notice and returns to the main menu. When successful,
 * the function initializes story playback state (entry index, end-return target, pause/confirm
 * defaults), clears any pending battle data, resets the VN runtime, applies runtime audio and
 * typewriter settings, displays the first entry, and performs an autosave of the story state.
 *
 * @param state Application state to update (modified in-place; selects screen and story fields).
 * @param scriptRef Reference to the script to load; empty means use the default chapter script.
 * @param endReturnScreen Screen to use if the script does not specify its own end-return target.
 */
void clearPendingBattleState(AppState& state,
                             ScreenState returnScreen = ScreenState::MainMenu) {
    state.pendingBattleKey.clear();
    state.pendingBattlePartyLineup.clear();
    state.pendingBattleFlowMode = BattleFlowMode::Direct;
    state.pendingBattleReturnScreen = returnScreen;
    state.pendingBattleWinScript.clear();
    state.pendingBattleLoseScript.clear();
    state.pendingBattleNextStoryScript.clear();
    state.pendingStoryNextScript.clear();
}

bool shouldLaunchCreditsAfterStory(const StorySession& story) {
    return story.loaded && (story.script.credits || story.script.scriptId == "finale02");
}

void beginCredits(AppState& state) {
#ifdef APP_ENABLE_RMLUI
    vn::stopVoicePlayback();
    state.pauseSelection = PauseAction::Continue;
    state.pauseContext = PauseContext::Story;
    state.confirmSelection = ConfirmAction::Cancel;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::Credits;
#else
    state.screen = ScreenState::MainMenu;
    state.mainSelection = MainMenuAction::Start;
    state.noticeText = "Credits require an RmlUi-enabled build.";
    state.noticeTimer = 2.6f;
#endif
}

void debugJumpToCredits(AppState& state) {
    beginCredits(state);
}

bool startLoadedStoryPlayback(AppState& state,
                              ScreenState endReturnScreen,
                              bool autosaveOnStart = true,
                              StoryFlowMode flowMode = StoryFlowMode::Campaign) {
    if (state.story.script.entries.empty()) {
        return false;
    }

    const std::string resumedNextStoryScript =
        flowMode == StoryFlowMode::Campaign
        ? resolvePendingStoryNextScript(state.story.script)
        : std::string();

    state.storyFlowMode = flowMode;
    state.storyEndReturnScreen = endReturnScreen;
    state.requestStoryReturnToBossSelector = false;
    state.pauseSelection = PauseAction::Continue;
    state.pauseContext = PauseContext::Story;
    state.confirmSelection = ConfirmAction::Cancel;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::Playing;
    clearPendingBattleState(state);
    state.pendingStoryNextScript = resumedNextStoryScript;
    vn::reset();
    vn::setMusicVolume(state.settings.musicVolume);
    vn::setVoiceVolume(state.settings.voiceVolume);
    vn::setTypewriterSpeed(state.settings.textSpeed);
    applyCurrentEntry(state.story, state.settings);
    if (autosaveOnStart) {
        (void)save::autosave(buildStorySaveGame(state));
    }
    return true;
}

void beginStory(AppState& state,
                const std::string& scriptRef,
                ScreenState endReturnScreen,
                bool autosaveOnStart,
                StoryFlowMode flowMode) {
    state.pendingStoryNextScript.clear();
    const bool loaded = scriptRef.empty()
        ? ensureStoryLoaded(state.story)
        : loadStoryScript(state.story, scriptRef);
    if (!loaded) {
        state.noticeText = scriptRef.empty()
            ? "Chapter 0 failed to load."
            : "Story failed to load.";
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
    const ScreenState resolvedEndReturnScreen =
        resolveStoryEndReturnScreen(state.story.script, endReturnScreen);
    if (!startLoadedStoryPlayback(state, resolvedEndReturnScreen, autosaveOnStart, flowMode)) {
        state.noticeText = scriptRef.empty()
            ? "Chapter 0 has no dialogue entries."
            : "Story has no dialogue entries.";
        state.noticeTimer = 2.6f;
        state.screen = ScreenState::MainMenu;
    }
}

bool beginStoryMode(AppState& state, Window& window) {
    for (const StoryModeCandidate& candidate : collectStoryModeCandidates()) {
        if (!restoreStorySave(state, candidate.saveGame)) {
            continue;
        }

        SettingsMenuController::applyDisplayMode(window, state.settings, candidate.saveGame.settings.fullscreen);
        return true;
    }

    beginStory(state);
    return state.screen == ScreenState::Playing;
}

/**
 * @brief Transition the application into the boss selector screen.
 *
 * When built with RmlUi support, configures and clears relevant AppState fields to enter
 * the BossSelector screen (pause context, selection defaults, pending-battle fields,
 * and stops story BGM). When RmlUi is not available, sets a short notice and returns to
 * the main menu.
 *
 * @param state Mutable application state updated to reflect the new screen and related defaults.
 */
void beginBossSelector(AppState& state) {
#ifdef RMLUI_SDL_VERSION_MAJOR
    vn::stopBgmPlayback();
    state.requestStoryReturnToBossSelector = false;
    state.pauseSelection = PauseAction::Continue;
    state.pauseContext = PauseContext::Story;
    state.confirmSelection = ConfirmAction::Cancel;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::BossSelector;
    clearPendingBattleState(state, ScreenState::BossSelector);
#else
    state.noticeText = "Boss selector requires RmlUi-enabled build.";
    state.noticeTimer = 2.6f;
    state.screen = ScreenState::MainMenu;
#endif
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
        clearPendingBattleState(state);
        state.noticeText = "Battle key missing.";
        state.noticeTimer = 2.6f;
        state.screen = ScreenState::MainMenu;
        return;
    }

    vn::stopBgmPlayback();
    vn::setBackground("");
    battle::BattleDefinition battleDefinition;
    if (!battle::loader::loadBattleDefinition(battleKey, battleDefinition)) {
        clearPendingBattleState(state);
        state.noticeText = "Battle not found: " + battleKey;
        state.noticeTimer = 2.6f;
        state.screen = ScreenState::MainMenu;
        return;
    }

    state.pendingBattleKey = battleDefinition.key;
    state.pendingBattlePartyLineup.clear();
    state.pendingBattleNextStoryScript = state.pendingBattleFlowMode == BattleFlowMode::CampaignStory
        ? battleDefinition.nextStoryScript
        : std::string();
    if (state.pendingBattleWinScript.empty() &&
        state.pendingBattleFlowMode != BattleFlowMode::Direct) {
        state.pendingBattleWinScript = battleDefinition.victoryStoryScript;
    }
    if (state.pendingBattleLoseScript.empty() &&
        state.pendingBattleFlowMode == BattleFlowMode::CampaignStory) {
        state.pendingBattleLoseScript = battleDefinition.defeatStoryScript;
    }

#ifdef RMLUI_SDL_VERSION_MAJOR
    if (!battleDefinition.isLineupFixed) {
        state.pauseSelection = PauseAction::Continue;
        state.pauseContext = PauseContext::Story;
        state.confirmSelection = ConfirmAction::Cancel;
        state.pauseIntroTime = 0.0f;
        state.screen = ScreenState::PartyLoader;
        return;
    }
#endif

    beginBattleDemo(state);
}

// Dispatches to the correct battle session based on the ID defined in assets/combat/battles.json.
void beginBattle(AppState& state, int battleId) {
    battle::BattleDefinition battleDefinition;
    if (!battle::loader::loadBattleDefinitionById(battleId, battleDefinition)) {
        clearPendingBattleState(state);
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

struct LoadingOverlayGlState {
    SDL_GLContext context = nullptr;
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint viewportUniform = -1;
    GLint alphaUniform = -1;
};

enum class LoadingTransitionPresentation {
    RendererFallback,
    RmlUi
};

struct LoadingTransitionState {
    bool active = false;
    bool holdPrepared = false;
    bool committed = false;
    float elapsedSeconds = 0.0f;
    LoadingTransitionPresentation presentation = LoadingTransitionPresentation::RendererFallback;
    std::function<bool()> prepareHold;
    std::function<bool()> commit;
};

float totalLoadingTransitionSeconds() {
    return kLoadingFadeInSeconds + kLoadingLogoHoldSeconds + kLoadingFadeOutSeconds;
}

float loadingOverlayAlpha(const LoadingTransitionState& transition) {
    if (!transition.active) {
        return 0.0f;
    }

    if (transition.elapsedSeconds < kLoadingFadeInSeconds) {
        return std::clamp(transition.elapsedSeconds / kLoadingFadeInSeconds, 0.0f, 1.0f);
    }

    if (transition.elapsedSeconds < kLoadingFadeInSeconds + kLoadingLogoHoldSeconds) {
        return 1.0f;
    }

    const float fadeOutElapsed =
        transition.elapsedSeconds - (kLoadingFadeInSeconds + kLoadingLogoHoldSeconds);
    return std::clamp(1.0f - fadeOutElapsed / kLoadingFadeOutSeconds, 0.0f, 1.0f);
}

bool loadingTransitionInHold(const LoadingTransitionState& transition) {
    return transition.active &&
           transition.elapsedSeconds >= kLoadingFadeInSeconds &&
           transition.elapsedSeconds < kLoadingFadeInSeconds + kLoadingLogoHoldSeconds;
}

bool loadingTransitionInFadeOut(const LoadingTransitionState& transition) {
    return transition.active &&
           transition.elapsedSeconds >= kLoadingFadeInSeconds + kLoadingLogoHoldSeconds &&
           transition.elapsedSeconds < totalLoadingTransitionSeconds();
}

bool loadingTransitionUsesRmlUi(const LoadingTransitionState& transition) {
    return transition.active && transition.presentation == LoadingTransitionPresentation::RmlUi;
}

#if defined(APP_ENABLE_RMLUI) || defined(RMLUI_SDL_VERSION_MAJOR)
graphics::RmlUiLoadingOverlayState buildRmlLoadingOverlayState(const LoadingTransitionState& transition) {
    graphics::RmlUiLoadingOverlayState state;
    if (!loadingTransitionUsesRmlUi(transition)) {
        return state;
    }

    state.visible = true;
    state.opacity = loadingOverlayAlpha(transition);
    state.showLogo = loadingTransitionInHold(transition);
    if (state.showLogo) {
        const float holdElapsed = std::max(0.0f, transition.elapsedSeconds - kLoadingFadeInSeconds);
        state.logoBounceOffsetY =
            std::sin(holdElapsed * kLoadingLogoBounceSpeed * 2.0f * kPi) *
            kLoadingLogoBounceAmplitude;
    }
    return state;
}
#endif

void clearLoadingTransition(LoadingTransitionState& transition) {
    transition.active = false;
    transition.holdPrepared = false;
    transition.committed = false;
    transition.elapsedSeconds = 0.0f;
    transition.presentation = LoadingTransitionPresentation::RendererFallback;
    transition.prepareHold = nullptr;
    transition.commit = nullptr;
}

void startLoadingTransition(LoadingTransitionState& transition,
                            LoadingTransitionPresentation presentation,
                            std::function<bool()> prepareHold,
                            std::function<bool()> commit) {
    transition.active = true;
    transition.holdPrepared = false;
    transition.committed = false;
    transition.elapsedSeconds = 0.0f;
    transition.presentation = presentation;
    transition.prepareHold = std::move(prepareHold);
    transition.commit = std::move(commit);
}

void destroyLoadingOverlayGlState(LoadingOverlayGlState& state) {
    if (state.context != nullptr && SDL_GL_GetCurrentContext() == nullptr) {
        state = LoadingOverlayGlState{};
        return;
    }

    const auto& gl = battle::render::gl::get();
    if (state.vbo != 0) {
        gl.deleteBuffers(1, &state.vbo);
        state.vbo = 0;
    }
    if (state.vao != 0) {
        gl.deleteVertexArrays(1, &state.vao);
        state.vao = 0;
    }
    if (state.program != 0) {
        gl.deleteProgram(state.program);
        state.program = 0;
    }
    state.viewportUniform = -1;
    state.alphaUniform = -1;
    state.context = nullptr;
}

GLuint compileLoadingOverlayShader(GLenum type, const char* source) {
    const auto& gl = battle::render::gl::get();
    const GLuint shader = gl.createShader(type);
    gl.shaderSource(shader, 1, &source, nullptr);
    gl.compileShader(shader);

    GLint success = GL_FALSE;
    gl.getShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) {
        return shader;
    }

    char logBuffer[512] = {};
    GLsizei logLength = 0;
    gl.getShaderInfoLog(shader, static_cast<GLsizei>(sizeof(logBuffer)), &logLength, logBuffer);
    std::cerr << "Loading overlay shader compile failed: "
              << std::string(logBuffer, static_cast<std::size_t>(std::max<GLsizei>(0, logLength))) << "\n";
    gl.deleteShader(shader);
    return 0;
}

bool ensureLoadingOverlayGlState(LoadingOverlayGlState& state, SDL_GLContext context) {
    if (context == nullptr) {
        return false;
    }
    if (state.context == context && state.program != 0 && state.vao != 0 && state.vbo != 0) {
        return true;
    }
    if (!battle::render::gl::ensureLoaded()) {
        return false;
    }

    const auto& gl = battle::render::gl::get();

    destroyLoadingOverlayGlState(state);

    static constexpr const char* kVertexShaderSource = R"GLSL(
        #version 330 core
        layout (location = 0) in vec2 a_position;
        uniform vec2 u_viewport;
        void main() {
            vec2 ndc = vec2(
                (a_position.x / u_viewport.x) * 2.0 - 1.0,
                1.0 - (a_position.y / u_viewport.y) * 2.0
            );
            gl_Position = vec4(ndc, 0.0, 1.0);
        }
    )GLSL";

    static constexpr const char* kFragmentShaderSource = R"GLSL(
        #version 330 core
        uniform float u_alpha;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(0.0, 0.0, 0.0, u_alpha);
        }
    )GLSL";

    const GLuint vertexShader = compileLoadingOverlayShader(GL_VERTEX_SHADER, kVertexShaderSource);
    const GLuint fragmentShader = compileLoadingOverlayShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            gl.deleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            gl.deleteShader(fragmentShader);
        }
        return false;
    }

    state.program = gl.createProgram();
    gl.attachShader(state.program, vertexShader);
    gl.attachShader(state.program, fragmentShader);
    gl.linkProgram(state.program);
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    gl.getProgramiv(state.program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char logBuffer[512] = {};
        GLsizei logLength = 0;
        gl.getProgramInfoLog(state.program, static_cast<GLsizei>(sizeof(logBuffer)), &logLength, logBuffer);
        std::cerr << "Loading overlay program link failed: "
                  << std::string(logBuffer, static_cast<std::size_t>(std::max<GLsizei>(0, logLength))) << "\n";
        destroyLoadingOverlayGlState(state);
        return false;
    }

    gl.genVertexArrays(1, &state.vao);
    gl.genBuffers(1, &state.vbo);
    gl.bindVertexArray(state.vao);
    gl.bindBuffer(GL_ARRAY_BUFFER, state.vbo);
    gl.bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(float) * 12), nullptr, GL_DYNAMIC_DRAW);
    gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(float) * 2), nullptr);
    gl.enableVertexAttribArray(0);
    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    gl.bindVertexArray(0);

    state.viewportUniform = gl.getUniformLocation(state.program, "u_viewport");
    state.alphaUniform = gl.getUniformLocation(state.program, "u_alpha");
    state.context = context;
    return true;
}

void renderLoadingOverlayGl(LoadingOverlayGlState& state,
                            SDL_GLContext context,
                            int width,
                            int height,
                            float alpha) {
    if (alpha <= 0.0f || !ensureLoadingOverlayGlState(state, context)) {
        return;
    }

    const auto& gl = battle::render::gl::get();
    const float vertices[] = {
        0.0f, 0.0f,
        static_cast<float>(width), 0.0f,
        static_cast<float>(width), static_cast<float>(height),
        0.0f, 0.0f,
        static_cast<float>(width), static_cast<float>(height),
        0.0f, static_cast<float>(height),
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.useProgram(state.program);
    gl.uniform2f(state.viewportUniform, static_cast<float>(width), static_cast<float>(height));
    gl.uniform1f(state.alphaUniform, std::clamp(alpha, 0.0f, 1.0f));
    gl.bindVertexArray(state.vao);
    gl.bindBuffer(GL_ARRAY_BUFFER, state.vbo);
    gl.bufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(vertices)), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    gl.bindVertexArray(0);
    gl.useProgram(0);
    glDisable(GL_BLEND);
}

void renderLoadingHoldRenderer(Window& window, SDL_Texture* logoTexture, float elapsedSeconds) {
    SDL_Renderer* renderer = window.getRenderer();
    if (renderer == nullptr) {
        return;
    }

    window.clear(0, 0, 0, 255);
    if (logoTexture == nullptr) {
        return;
    }

    int textureWidth = 0;
    int textureHeight = 0;
    SDL_QueryTexture(logoTexture, nullptr, nullptr, &textureWidth, &textureHeight);
    if (textureWidth <= 0 || textureHeight <= 0) {
        return;
    }

    const float maxWidth = std::min(window.getWidth() * 0.34f, 320.0f);
    const float scale = maxWidth / static_cast<float>(textureWidth);
    const float width = static_cast<float>(textureWidth) * scale;
    const float height = static_cast<float>(textureHeight) * scale;
    const float bounceOffset =
        std::sin(elapsedSeconds * kLoadingLogoBounceSpeed * 2.0f * kPi) *
        kLoadingLogoBounceAmplitude;

    const SDL_FRect destination{
        36.0f,
        static_cast<float>(window.getHeight()) - height - 34.0f + bounceOffset,
        width,
        height,
    };
    SDL_RenderCopyF(renderer, logoTexture, nullptr, &destination);
}

void releaseRendererUi(MenuResources& menuResources, bool& rendererUiReady) {
    if (!rendererUiReady) {
        return;
    }

    destroyMenuResources(menuResources);
    rendererUiReady = false;
}

/**
 * @brief Restores the renderer-based UI and initializes VN state for the given window.
 *
 * Initializes the VN renderer and applies runtime UI resources and audio/text settings so the
 * application's renderer-based front-end can be used again.
 *
 * @param window Window instance whose renderer and native window are used.
 * @param menuResources Storage for menu textures and fonts that will be (re)loaded.
 * @param settings Runtime display and audio settings to apply (music volume, voice volume, text speed).
 * @return true if the renderer UI and VN subsystem were successfully initialized and resources loaded, false otherwise.
 */
bool restoreRendererUi(Window& window, MenuResources& menuResources, const GameSettings& settings) {
    if (!window.enableRenderer()) {
        return false;
    }
    SDL_SetWindowResizable(window.getNativeWindow(), SDL_FALSE);
    if (!vn::initialize(window.getRenderer(), window.getWidth(), window.getHeight())) {
        return false;
    }
    vn::setMusicVolume(settings.musicVolume);
    vn::setVoiceVolume(settings.voiceVolume);
    vn::setTypewriterSpeed(settings.textSpeed);
    loadMenuResources(menuResources, window.getRenderer());
    return true;
}

bool shouldUseFrontUiScreen(const AppState& state) {
    if (state.screen == ScreenState::MainMenu ||
        state.screen == ScreenState::Playing ||
        state.screen == ScreenState::Credits) {
        return true;
    }

    if ((state.screen == ScreenState::PauseMenu ||
         state.screen == ScreenState::PauseConfirmExit ||
         state.screen == ScreenState::PauseConfirmOverwriteSave) &&
        state.pauseContext == PauseContext::Story) {
        return true;
    }

    if ((state.screen == ScreenState::LoadGameMenu ||
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

bool shouldDeferBackendRestore(const AppState& state) {
    return state.screen == ScreenState::BossSelector ||
           state.screen == ScreenState::PartyLoader ||
           state.screen == ScreenState::PostBattle ||
           state.screen == ScreenState::BattleDemo;
}

bool shouldKeepStoryBgmPlaying(const AppState& state) {
    if (state.screen == ScreenState::Playing || state.screen == ScreenState::Credits) {
        return true;
    }

    if ((state.screen == ScreenState::PauseMenu ||
         state.screen == ScreenState::PauseConfirmExit ||
         state.screen == ScreenState::PauseConfirmOverwriteSave) &&
        state.pauseContext == PauseContext::Story) {
        return true;
    }

    if (state.screen == ScreenState::Settings) {
        return state.settingsReturnScreen == ScreenState::Playing ||
               (state.settingsReturnScreen == ScreenState::PauseMenu &&
                state.pauseContext == PauseContext::Story);
    }

    if (state.screen == ScreenState::LoadGameMenu || state.screen == ScreenState::LoadConfirmDelete) {
        return state.loadReturnScreen == ScreenState::Playing ||
               (state.loadReturnScreen == ScreenState::PauseMenu &&
                state.pauseContext == PauseContext::Story);
    }

    return false;
}

#ifdef APP_ENABLE_RMLUI
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

    applyMainMenuAction(state, window, action);
    return true;
}
#endif

/**
 * @brief Program entry point that initializes subsystems, runs the main event/update/render loop, and performs cleanup.
 *
 * The application parses optional command-line startup modes (for example: "battle mode", "selector",
 * "story <ref>", or "battle <key>"), initializes windowing, audio, UI, and VN subsystems, and enters
 * the main loop that processes input, updates VN and battle sessions, drives loading transitions,
 * handles saves/loads, and renders the active UI or gameplay view. On exit all subsystems and
 * resources are shut down and released.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument vector.
 * @return int 0 on normal exit, non-zero on initialization or fatal runtime failure.
 */
int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "battle" && std::string(argv[2]) == "mode") {
        return launchDefaultBattleMode(argc, argv);
    }

    std::optional<std::string> startupStoryRef;
    std::optional<std::string> startupBattleKey;
    bool startupBossSelector = false;
    if (argc >= 2) {
        const std::string command = argv[1];
        if (command == "selector") {
            startupBossSelector = true;
        } else if (command == "story" && argc >= 3) {
            startupStoryRef = std::string(argv[2]);
        } else if (command == "battle" && argc >= 3) {
            startupBattleKey = std::string(argv[2]);
        }
    }

    Window window("Hatsune Miku: Our Underground BIT Idol", kStartupWindowWidth, kStartupWindowHeight, false);
    if (!window.isOpen()) {
        std::cerr << "Failed to initialize window\n";
        return 1;
    }
    SDL_SetWindowResizable(window.getNativeWindow(), SDL_FALSE);

    window.setEscapeToQuitEnabled(false);

    save::init();

    AppState state;
    state.progression = save::loadCurrentProgression();
    state.settings.fullscreen = window.isFullscreen();

    MenuResources menuResources;
    SettingsMenuController settingsMenu;
    game::audio::UiMusicController uiMusic;
    std::unique_ptr<battle::app::Session> battleSession;
#ifdef RMLUI_SDL_VERSION_MAJOR
    std::unique_ptr<battle::selector::Session> bossSelectorSession;
    std::unique_ptr<battle::partyloader::Session> partyLoaderSession;
    std::unique_ptr<battle::postbattle::Session> postBattleSession;
#endif
    struct PendingPostBattleContext {
        battle::BattleDefinition battleDefinition;
        battle::postbattle::Summary summary;
        battle::app::BattleOutcome outcome = battle::app::BattleOutcome::Defeat;
        std::vector<std::string> currentPartyLineup;
        BattleFlowMode battleFlowMode = BattleFlowMode::Direct;
        ScreenState battleReturnScreen = ScreenState::MainMenu;
        std::string battleWinScript;
        std::string battleLoseScript;
        std::string battleNextStoryScript;
        std::string battleSourceStoryScript;
    };
    std::optional<PendingPostBattleContext> pendingPostBattleContext;
    struct PendingTutorialDrillContext {
        PendingPostBattleContext resumeContext;
    };
    std::optional<PendingTutorialDrillContext> pendingTutorialDrillContext;
    struct PendingBossPreviewContext {
        std::string battleKey;
        std::vector<std::string> partyLineup;
        BattleFlowMode battleFlowMode = BattleFlowMode::Direct;
        ScreenState battleReturnScreen = ScreenState::MainMenu;
        std::string battleWinScript;
        std::string battleLoseScript;
        std::string battleNextStoryScript;
    };
    std::optional<PendingBossPreviewContext> pendingBossPreviewContext;

    bool rendererUiReady = false;
    (void)uiMusic.initialize();
    GameSettings appliedRuntimeSettings = state.settings;
    const auto applyRuntimeSettings = [&](bool force = false) {
        const auto differs = [&](float lhs, float rhs) {
            return std::fabs(lhs - rhs) > 0.0001f;
        };

        if (force || differs(appliedRuntimeSettings.musicVolume, state.settings.musicVolume)) {
            vn::setMusicVolume(state.settings.musicVolume);
        }
        if (force || differs(appliedRuntimeSettings.voiceVolume, state.settings.voiceVolume)) {
            vn::setVoiceVolume(state.settings.voiceVolume);
        }
        if (force || differs(appliedRuntimeSettings.textSpeed, state.settings.textSpeed)) {
            vn::setTypewriterSpeed(state.settings.textSpeed);
        }
        appliedRuntimeSettings = state.settings;
    };
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
#endif
    applyRuntimeSettings(true);

#ifdef VN_AUTO_START_STORY
    beginStory(state);
#elif !defined(VN_AUTO_START_STORY)
    if (startupBossSelector) {
        beginBossSelector(state);
    } else if (startupStoryRef.has_value()) {
        beginStory(state, *startupStoryRef);
    } else if (startupBattleKey.has_value()) {
        state.pendingBattleReturnScreen = ScreenState::MainMenu;
        beginBattle(state, *startupBattleKey);
    }
#endif

    LoadingTransitionState loadingTransition;
    LoadingOverlayGlState loadingOverlayGl;
    SDL_Texture* loadingLogoTexture = nullptr;

    const auto destroyLoadingLogoTexture = [&]() {
        if (loadingLogoTexture != nullptr) {
            SDL_DestroyTexture(loadingLogoTexture);
            loadingLogoTexture = nullptr;
        }
    };

    const auto prepareRendererLoadingHold = [&]() -> bool {
#ifdef RMLUI_SDL_VERSION_MAJOR
        if (bossSelectorSession != nullptr) {
            bossSelectorSession->shutdown();
            bossSelectorSession.reset();
        }
        if (partyLoaderSession != nullptr) {
            partyLoaderSession->shutdown();
            partyLoaderSession.reset();
        }
#endif
#ifdef APP_ENABLE_RMLUI
        if (frontUi.isInitialized()) {
            frontUi.shutdown();
        }
#endif
        if (rendererUiReady) {
            destroyMenuResources(menuResources);
            rendererUiReady = false;
        }

        vn::stopVoicePlayback();
        vn::shutdown();

        destroyLoadingLogoTexture();
        if (!window.enableRenderer()) {
            state.noticeText = "Loading transition failed.";
            state.noticeTimer = 2.6f;
            return false;
        }

        SDL_SetWindowResizable(window.getNativeWindow(), SDL_FALSE);
        loadingLogoTexture = loadTexture(window.getRenderer(), resolvePath(kMainMenuTitleLogoPath));
        if (loadingLogoTexture != nullptr) {
            SDL_SetTextureBlendMode(loadingLogoTexture, SDL_BLENDMODE_BLEND);
        }
        return true;
    };

    const auto commitStoryExitToMainMenu = [&]() -> bool {
        state.requestStoryExitToMainMenu = false;
        state.requestStoryReturnToBossSelector = false;
        vn::setPaused(false);
        vn::reset();
        state.story.entryIndex = 0;
        state.pauseSelection = PauseAction::Continue;
        state.pauseContext = PauseContext::Story;
        state.confirmSelection = ConfirmAction::Cancel;
        state.settingsReturnScreen = ScreenState::MainMenu;
        state.screen = ScreenState::MainMenu;
        state.mainSelection = MainMenuAction::Start;
        clearPendingBattleState(state);
        state.noticeText = "Current progress was discarded.";
        state.noticeTimer = 2.6f;
        return true;
    };

    const auto commitStoryReturnToBossSelector = [&]() -> bool {
        state.requestStoryReturnToBossSelector = false;
        vn::setPaused(false);
        vn::reset();
        state.story.entryIndex = 0;
        state.pauseSelection = PauseAction::Continue;
        state.pauseContext = PauseContext::Story;
        state.confirmSelection = ConfirmAction::Cancel;
        state.pauseIntroTime = 0.0f;
        beginBossSelector(state);
        return true;
    };

    const auto buildTutorialDrillLineup = [&](const std::string& focusCharacterKey,
                                              const std::vector<std::string>& baseLineup) {
        battle::TutorialScenarioDefinition scenario;
        if (battle::tutorial::loadUnlockDrillScenario(focusCharacterKey, scenario) &&
            !scenario.lineup.empty()) {
            return scenario.lineup;
        }

        std::vector<std::string> lineup;
        lineup.reserve(std::max<std::size_t>(1, baseLineup.size() + 1));

        const auto appendUnique = [](std::vector<std::string>& out, const std::string& key) {
            if (key.empty() || std::find(out.begin(), out.end(), key) != out.end()) {
                return;
            }
            out.push_back(key);
        };

        appendUnique(lineup, focusCharacterKey);
        for (const std::string& key : baseLineup) {
            appendUnique(lineup, key);
        }
        for (const std::string& key : state.progression.currentPartyLineup) {
            appendUnique(lineup, key);
        }
        if (lineup.empty()) {
            appendUnique(lineup, "miku");
        }

        return lineup;
    };

    const auto buildBossPreviewLineup = [&](const battle::BattleDefinition& battleDefinition) {
        battle::TutorialScenarioDefinition scenario;
        if (battle::tutorial::loadBossPreviewScenario(battleDefinition.key, scenario) &&
            scenario.enabled &&
            !scenario.lineup.empty()) {
            return scenario.lineup;
        }

        return std::vector<std::string>{"miku"};
    };

    const auto shouldLaunchBossPreview = [&](const std::string& battleKey) {
        return state.pendingBattleFlowMode == BattleFlowMode::CampaignStory &&
               !battleKey.empty() &&
               battleKey != "tutorial_vs_lyoo" &&
               battle::tutorial::hasEnabledBossPreviewScenario(battleKey) &&
               !battle::hasCompletedTutorial(state.progression, "boss_preview_" + battleKey);
    };

    const auto restoreBossPreviewBattleContext = [&](const PendingBossPreviewContext& context) {
        state.pendingBattleKey = context.battleKey;
        state.pendingBattlePartyLineup = context.partyLineup;
        state.pendingBattleFlowMode = context.battleFlowMode;
        state.pendingBattleReturnScreen = context.battleReturnScreen;
        state.pendingBattleWinScript = context.battleWinScript;
        state.pendingBattleLoseScript = context.battleLoseScript;
        state.pendingBattleNextStoryScript = context.battleNextStoryScript;
    };

    const auto beginBossPreview = [&](const battle::BattleDefinition& battleDefinition) {
        pendingBossPreviewContext = PendingBossPreviewContext{
            battleDefinition.key,
            {},
            state.pendingBattleFlowMode,
            state.pendingBattleReturnScreen,
            state.pendingBattleWinScript,
            state.pendingBattleLoseScript,
            state.pendingBattleNextStoryScript,
        };
        state.pendingBattleKey = battleDefinition.key;
        state.pendingBattlePartyLineup = buildBossPreviewLineup(battleDefinition);
        state.pendingBattleFlowMode = BattleFlowMode::BossPreview;
        state.pendingBattleWinScript.clear();
        state.pendingBattleLoseScript.clear();
        state.pendingBattleNextStoryScript.clear();
        beginBattleDemo(state);
    };

    const auto closeResolvedBattleUi = [&]() {
        if (battleSession != nullptr) {
            battleSession->shutdown();
            battleSession.reset();
        }
#ifdef RMLUI_SDL_VERSION_MAJOR
        if (postBattleSession != nullptr) {
            postBattleSession->shutdown();
            postBattleSession.reset();
        }
#endif
        clearPendingBattleState(state);
    };

    const auto countsAsBattleClear = [&](const PendingPostBattleContext& context) {
        if (context.outcome == battle::app::BattleOutcome::Victory) {
            return true;
        }

        return context.battleDefinition.key == "miku_plot_twist" &&
               context.outcome == battle::app::BattleOutcome::Defeat;
    };

    const auto routeResolvedBattleOutcomeContinuation = [&](const PendingPostBattleContext& context) -> bool {
        closeResolvedBattleUi();

        pendingTutorialDrillContext.reset();
        pendingBossPreviewContext.reset();

        const auto enterLoadedStory = [&](const std::string& scriptRef,
                                          const char* failureNotice,
                                          ScreenState endReturnScreen,
                                          StoryFlowMode flowMode,
                                          const std::string& chainedStoryScript = std::string()) {
            if (!loadStoryScript(state.story, scriptRef)) {
                state.screen = ScreenState::MainMenu;
                state.pauseContext = PauseContext::Story;
                state.mainSelection = MainMenuAction::Start;
                state.noticeText = failureNotice;
                state.noticeTimer = 2.8f;
                return;
            }

            state.story.entryIndex = 0;
            const ScreenState resolvedEndReturnScreen =
                resolveStoryEndReturnScreen(state.story.script, endReturnScreen);
            if (!startLoadedStoryPlayback(state, resolvedEndReturnScreen, true, flowMode)) {
                state.screen = ScreenState::MainMenu;
                state.pauseContext = PauseContext::Story;
                state.mainSelection = MainMenuAction::Start;
                state.noticeText = failureNotice;
                state.noticeTimer = 2.8f;
                return;
            }

            state.pendingStoryNextScript = chainedStoryScript;
        };

        if (context.outcome == battle::app::BattleOutcome::Victory) {
            if (context.battleFlowMode == BattleFlowMode::CampaignStory ||
                context.battleFlowMode == BattleFlowMode::PracticeReplayStory) {
                const StoryFlowMode resumedStoryFlow = storyFlowModeForBattleFlow(context.battleFlowMode);
                const std::string chainedStoryScript =
                    context.battleFlowMode == BattleFlowMode::CampaignStory
                    ? context.battleNextStoryScript
                    : std::string();
                if (!context.battleWinScript.empty()) {
                    enterLoadedStory(context.battleWinScript,
                                     "Post-battle story failed to load.",
                                     context.battleReturnScreen,
                                     resumedStoryFlow,
                                     chainedStoryScript);
                } else if (context.battleFlowMode == BattleFlowMode::CampaignStory &&
                           !context.battleNextStoryScript.empty()) {
                    beginStory(state, context.battleNextStoryScript, context.battleReturnScreen);
                } else if (state.story.loaded &&
                           state.story.entryIndex < state.story.script.entries.size()) {
                    state.storyFlowMode = resumedStoryFlow;
                    state.pauseSelection = PauseAction::Continue;
                    state.pauseContext = PauseContext::Story;
                    state.confirmSelection = ConfirmAction::Cancel;
                    state.pauseIntroTime = 0.0f;
                    state.screen = ScreenState::Playing;
                    applyCurrentEntry(state.story, state.settings);
                    (void)save::autosave(buildStorySaveGame(state));
                } else if (context.battleReturnScreen == ScreenState::BossSelector) {
                    beginBossSelector(state);
                } else {
                    state.screen = ScreenState::MainMenu;
                    state.pauseContext = PauseContext::Story;
                    state.mainSelection = MainMenuAction::Start;
                    state.noticeText = "End of story.";
                    state.noticeTimer = 2.2f;
                }
            } else if (context.battleReturnScreen == ScreenState::BossSelector) {
                beginBossSelector(state);
            } else {
                state.screen = ScreenState::MainMenu;
                state.pauseContext = PauseContext::Story;
                state.mainSelection = MainMenuAction::Battle;
            }
        } else if (context.battleDefinition.key == "miku_plot_twist" &&
                   context.outcome == battle::app::BattleOutcome::Defeat &&
                   !context.battleWinScript.empty() &&
                   (context.battleFlowMode == BattleFlowMode::CampaignStory ||
                    context.battleFlowMode == BattleFlowMode::PracticeReplayStory)) {
            const StoryFlowMode resumedStoryFlow =
                storyFlowModeForBattleFlow(context.battleFlowMode);
            beginStory(state,
                       context.battleWinScript,
                       context.battleReturnScreen,
                       true,
                       resumedStoryFlow);
        } else if ((context.battleFlowMode == BattleFlowMode::CampaignStory ||
                    context.battleFlowMode == BattleFlowMode::PracticeReplayStory) &&
                   !context.battleLoseScript.empty()) {
            const StoryFlowMode resumedStoryFlow =
                storyFlowModeForBattleFlow(context.battleFlowMode);
            beginStory(state,
                       context.battleLoseScript,
                       context.battleReturnScreen,
                       true,
                       resumedStoryFlow);
        } else if (context.battleFlowMode == BattleFlowMode::CampaignStory &&
                   !context.battleSourceStoryScript.empty()) {
            beginStory(state, context.battleSourceStoryScript, context.battleReturnScreen);
        } else if (context.battleReturnScreen == ScreenState::BossSelector) {
            beginBossSelector(state);
        } else {
            state.screen = ScreenState::MainMenu;
            state.pauseContext = PauseContext::Story;
            state.mainSelection = MainMenuAction::Battle;
        }

        return true;
    };

    const auto routeResolvedBattleOutcome = [&](const PendingPostBattleContext& context) -> bool {
        std::string tutorialDrillCharacterKey;
        if (countsAsBattleClear(context) &&
            context.battleFlowMode == BattleFlowMode::CampaignStory &&
            !context.battleDefinition.key.empty() &&
            !battle::hasClearedBattle(state.progression, context.battleDefinition.key)) {
            tutorialDrillCharacterKey =
                battle::resolveUnlockCharacterKeyForBattle(context.battleDefinition.key);
            if (!tutorialDrillCharacterKey.empty() &&
                battle::hasCompletedTutorial(
                    state.progression,
                    "unlock_drill_" + tutorialDrillCharacterKey)) {
                tutorialDrillCharacterKey.clear();
            }
        }

        if (!context.currentPartyLineup.empty()) {
            state.progression.currentPartyLineup = context.currentPartyLineup;
            battle::normalizePlayerProgression(state.progression);
        }
        if (countsAsBattleClear(context) && !context.battleDefinition.key.empty()) {
            state.progression.clearedBattleKeys.push_back(context.battleDefinition.key);
            battle::normalizePlayerProgression(state.progression);
        }
        (void)save::writeProfileProgression(state.progression);

        if (!tutorialDrillCharacterKey.empty()) {
            closeResolvedBattleUi();

            pendingTutorialDrillContext = PendingTutorialDrillContext{context};
            state.pendingBattleKey = kUnlockDrillBattleKey;
            state.pendingBattlePartyLineup =
                buildTutorialDrillLineup(tutorialDrillCharacterKey, context.currentPartyLineup);
            state.pendingBattleFlowMode = BattleFlowMode::UnlockCharacterDrill;
            state.pendingBattleReturnScreen = context.battleReturnScreen;
            state.pendingBattleWinScript.clear();
            state.pendingBattleLoseScript.clear();
            state.pendingBattleNextStoryScript.clear();
            beginBattleDemo(state);
            return true;
        }

        return routeResolvedBattleOutcomeContinuation(context);
    };

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

            if (loadingTransition.active) {
                continue;
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

                case ScreenState::BossSelector:
#ifdef RMLUI_SDL_VERSION_MAJOR
                    if (bossSelectorSession != nullptr) {
                        bossSelectorSession->handleEvent(event);
                    }
#endif
                    break;

                case ScreenState::PartyLoader:
#ifdef RMLUI_SDL_VERSION_MAJOR
                    if (partyLoaderSession != nullptr) {
                        partyLoaderSession->handleEvent(event);
                    }
#endif
                    break;

                case ScreenState::PostBattle:
#ifdef RMLUI_SDL_VERSION_MAJOR
                    if (postBattleSession != nullptr) {
                        postBattleSession->handleEvent(event);
                    }
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

                case ScreenState::LoadGameMenu:
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
                case ScreenState::Credits:
#ifdef APP_ENABLE_RMLUI
                    if (event.type == SDL_KEYDOWN &&
                        state.screen == ScreenState::Playing &&
                        event.key.keysym.sym == SDLK_s) {
                        debugSkipStoryToLastEntry(state);
                    } else if (event.type == SDL_KEYDOWN &&
                        event.key.keysym.sym == SDLK_j &&
                        (event.key.keysym.mod & KMOD_CTRL) != 0) {
                        debugJumpToCredits(state);
                    } else if (shouldUseFrontUiScreen(state)) {
                        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else {
                            frontUi.handleEvent(event, state);
                        }
                    } else if (state.screen == ScreenState::Playing && event.type == SDL_KEYDOWN) {
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
                        if (state.screen == ScreenState::Playing && event.key.keysym.sym == SDLK_s) {
                            debugSkipStoryToLastEntry(state);
                        } else if (event.key.keysym.sym == SDLK_j && (event.key.keysym.mod & KMOD_CTRL) != 0) {
                            debugJumpToCredits(state);
                        } else if (state.screen == ScreenState::Playing && event.key.keysym.sym == SDLK_ESCAPE) {
                            openPauseMenu(state);
                        } else if (state.screen == ScreenState::Playing && event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else if (state.screen == ScreenState::Playing && event.key.keysym.sym == SDLK_SPACE) {
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
        if (!loadingTransition.active) {
            if (const std::optional<graphics::frontui::Command> command = frontUi.consumeCommand(); command.has_value()) {
                switch (command->type) {
                    case graphics::frontui::CommandType::ActivateMainMenuAction:
                        if (command->mainMenuAction == MainMenuAction::Start) {
                            state.mainSelection = MainMenuAction::Start;
                            startLoadingTransition(
                                loadingTransition,
                                LoadingTransitionPresentation::RmlUi,
                                std::function<bool()>{},
                                [&]() -> bool {
                                    return beginStoryMode(state, window);
                                });
                        } else if (command->mainMenuAction == MainMenuAction::Battle) {
                            state.mainSelection = MainMenuAction::Battle;
                            if (!::battle::hasUnlockedCharacter(state.progression, "lyoo")) {
                                state.noticeText = kPracticeModeLockedNotice;
                                state.noticeTimer = 2.6f;
                                break;
                            }
                            startLoadingTransition(
                                loadingTransition,
                                LoadingTransitionPresentation::RmlUi,
                                std::function<bool()>{},
                                [&]() -> bool {
                                    beginBossSelector(state);
                                    return state.screen == ScreenState::BossSelector;
                                });
                        } else if (!applyFrontMenuAction(state, window, frontUi, menuResources, rendererUiReady,
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
        }

        if (!loadingTransition.active && state.requestStoryExitToMainMenu) {
            startLoadingTransition(
                loadingTransition,
                LoadingTransitionPresentation::RmlUi,
                std::function<bool()>{},
                commitStoryExitToMainMenu);
        }

        if (!loadingTransition.active && state.requestStoryReturnToBossSelector) {
            startLoadingTransition(
                loadingTransition,
                LoadingTransitionPresentation::RmlUi,
                std::function<bool()>{},
                commitStoryReturnToBossSelector);
        }

        if (shouldUseFrontUiScreen(state)) {
            if (!restoreFrontMenuUi(window, frontUi, menuResources, rendererUiReady, state)) {
                std::cerr << "Failed to restore RmlUi front-ui screen\n";
                return 1;
            }
        } else if (!shouldDeferBackendRestore(state) &&
                   !restoreNonMainMenuUi(window, frontUi, menuResources, rendererUiReady, state.settings)) {
            std::cerr << "Failed to restore renderer UI\n";
            return 1;
        }
#endif

#ifdef RMLUI_SDL_VERSION_MAJOR
        if (!loadingTransition.active &&
            state.screen == ScreenState::PartyLoader &&
            partyLoaderSession != nullptr) {
            if (const auto request = partyLoaderSession->consumeRequest(); request.has_value()) {
                const battle::partyloader::Request partyLoaderRequest = *request;
                if (partyLoaderRequest.type == battle::partyloader::Request::Type::ConfirmBattle) {
                    startLoadingTransition(
                        loadingTransition,
                        LoadingTransitionPresentation::RmlUi,
                        std::function<bool()>{},
                        [&, partyLoaderRequest]() -> bool {
                            if (partyLoaderSession != nullptr) {
                                partyLoaderSession->shutdown();
                                partyLoaderSession.reset();
                            }
                            battle::BattleDefinition battleDefinition;
                            const std::string battleKey =
                                state.pendingBattleKey.empty() ? "tutorial_vs_lyoo" : state.pendingBattleKey;
                            if (!battle::loader::loadBattleDefinition(battleKey, battleDefinition)) {
                                clearPendingBattleState(state);
                                pendingBossPreviewContext.reset();
                                state.screen = ScreenState::MainMenu;
                                state.mainSelection = MainMenuAction::Battle;
                                state.noticeText = "Battle not found: " + battleKey;
                                state.noticeTimer = 2.6f;
                                return false;
                            }

                            pendingBossPreviewContext.reset();
                            state.pendingBattlePartyLineup = partyLoaderRequest.partyKeys;
                            beginBattleDemo(state);
                            return true;
                        });
                } else {
                    startLoadingTransition(
                        loadingTransition,
                        LoadingTransitionPresentation::RmlUi,
                        std::function<bool()>{},
                        [&]() -> bool {
                            clearPendingBattleState(state);
                            state.requestStoryExitToMainMenu = false;
                            state.requestStoryReturnToBossSelector = false;
                            vn::setPaused(false);
                            vn::reset();
                            state.story.entryIndex = 0;
                            state.pauseSelection = PauseAction::Continue;
                            state.pauseContext = PauseContext::Story;
                            state.confirmSelection = ConfirmAction::Cancel;
                            state.screen = ScreenState::MainMenu;
                            state.mainSelection = MainMenuAction::Start;
                            return true;
                        });
                }
            }
        }

        if (!loadingTransition.active &&
            state.screen == ScreenState::BossSelector &&
            bossSelectorSession != nullptr) {
            if (const auto request = bossSelectorSession->consumeLaunchRequest(); request.has_value()) {
                const battle::selector::LaunchRequest launchRequest = *request;
                LoadingTransitionPresentation presentation = LoadingTransitionPresentation::RmlUi;
                if (launchRequest.mode == battle::selector::LaunchRequest::Mode::PracticeStraightToBattle) {
                    battle::BattleDefinition battleDefinition;
                    if (!battle::loader::loadBattleDefinition(launchRequest.reference, battleDefinition)) {
                        state.noticeText = "Battle not found: " + launchRequest.reference;
                        state.noticeTimer = 2.6f;
                        continue;
                    }
                }

                startLoadingTransition(
                    loadingTransition,
                    presentation,
                    std::function<bool()>{},
                    [&, launchRequest]() -> bool {
                        if (launchRequest.mode == battle::selector::LaunchRequest::Mode::PracticeReplayStory) {
#ifdef APP_ENABLE_RMLUI
                            if (bossSelectorSession != nullptr) {
                                bossSelectorSession->shutdown();
                                bossSelectorSession.reset();
                            }
                            state.screen = ScreenState::MainMenu;
                            state.mainSelection = MainMenuAction::Battle;
                            if (!restoreFrontMenuUi(window, frontUi, menuResources, rendererUiReady, state)) {
                                state.noticeText = "Story failed to load.";
                                state.noticeTimer = 2.6f;
                                return false;
                            }
                            beginStory(state,
                                       launchRequest.reference,
                                       ScreenState::BossSelector,
                                       true,
                                       StoryFlowMode::PracticeReplay);
                            return true;
#else
                            state.noticeText = "Story launch requires RmlUi front UI.";
                            state.noticeTimer = 2.6f;
                            return false;
#endif
                        }

                        destroyLoadingLogoTexture();
                        clearPendingBattleState(state, ScreenState::BossSelector);
                        pendingBossPreviewContext.reset();
                        state.pendingBattleFlowMode = BattleFlowMode::Direct;
                        beginBattle(state, launchRequest.reference);
                        return true;
                    });
            }
        }
#endif

        const Uint64 now = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(now - lastCounter) /
            static_cast<float>(SDL_GetPerformanceFrequency());
        lastCounter = now;

        if (loadingTransition.active) {
            loadingTransition.elapsedSeconds += deltaSeconds;

            if (!loadingTransition.holdPrepared &&
                loadingTransition.elapsedSeconds >= kLoadingFadeInSeconds) {
                loadingTransition.holdPrepared = true;
                if (loadingTransition.prepareHold && !loadingTransition.prepareHold()) {
                    clearLoadingTransition(loadingTransition);
                }
            }

            if (loadingTransition.active &&
                !loadingTransition.committed &&
                loadingTransition.elapsedSeconds >= kLoadingFadeInSeconds + kLoadingLogoHoldSeconds) {
                if (!loadingTransitionUsesRmlUi(loadingTransition)) {
                    destroyLoadingLogoTexture();
                }
                loadingTransition.committed = true;
                if (loadingTransition.commit && !loadingTransition.commit()) {
                    clearLoadingTransition(loadingTransition);
                }
            }
        }

        if (!loadingTransitionUsesRmlUi(loadingTransition) &&
            loadingTransitionInHold(loadingTransition)) {
            renderLoadingHoldRenderer(
                window,
                loadingLogoTexture,
                std::max(0.0f, loadingTransition.elapsedSeconds - kLoadingFadeInSeconds));
            window.present();
            if (loadingTransition.elapsedSeconds >= totalLoadingTransitionSeconds()) {
                clearLoadingTransition(loadingTransition);
            }
            continue;
        }

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
                    state.noticeText = "File Saved";
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
                state.noticeText = "File Saved";
                state.noticeTimer = 2.0f;
                state.pendingOverwriteSavePath.clear();
            } else {
                state.noticeText = "Save failed.";
                state.noticeTimer = 2.0f;
                state.pendingOverwriteSavePath.clear();
            }
        }

        if (!state.pendingLoadPath.empty() && !loadingTransition.active) {
            const std::string pendingLoadPath = state.pendingLoadPath;
            state.pendingLoadPath.clear();

            const LoadingTransitionPresentation presentation = shouldUseFrontUiScreen(state)
                ? LoadingTransitionPresentation::RmlUi
                : LoadingTransitionPresentation::RendererFallback;
            startLoadingTransition(
                loadingTransition,
                presentation,
                presentation == LoadingTransitionPresentation::RendererFallback
                    ? std::function<bool()>(prepareRendererLoadingHold)
                    : std::function<bool()>{},
                [&, pendingLoadPath]() -> bool {
                    const std::optional<save::SaveGame> saveGame = save::load(pendingLoadPath);
                    if (!saveGame.has_value()) {
                        state.noticeText = "Save file corrupted.";
                        state.noticeTimer = 2.4f;
                        return true;
                    }

                    if (!restoreStorySave(state, *saveGame)) {
                        state.noticeText = "Load failed.";
                        state.noticeTimer = 2.4f;
                        return true;
                    }

                    SettingsMenuController::applyDisplayMode(window, state.settings, saveGame->settings.fullscreen);
                    state.noticeText = "Game loaded.";
                    state.noticeTimer = 2.0f;
                    return true;
                });
        }

        if (state.screen == ScreenState::BattleDemo && battleSession == nullptr) {
            releaseRendererUi(menuResources, rendererUiReady);
#ifdef APP_ENABLE_RMLUI
            if (frontUi.isInitialized()) {
                frontUi.shutdown();
            }
#endif
#ifdef RMLUI_SDL_VERSION_MAJOR
            if (bossSelectorSession != nullptr) {
                bossSelectorSession->shutdown();
                bossSelectorSession.reset();
            }
            if (partyLoaderSession != nullptr) {
                partyLoaderSession->shutdown();
                partyLoaderSession.reset();
            }
#endif
            vn::stopVoicePlayback();
            vn::shutdown();
            if (!window.enableOpenGL()) {
                state.screen = ScreenState::MainMenu;
                state.pauseContext = PauseContext::Story;
                state.mainSelection = MainMenuAction::Battle;
                clearPendingBattleState(state);
                state.noticeText = "Battle failed to open.";
                state.noticeTimer = 2.8f;
                continue;
            }

            battleSession = std::make_unique<battle::app::Session>();
            const std::string battleKey = state.pendingBattleKey.empty() ? "tutorial_vs_lyoo" : state.pendingBattleKey;
            std::optional<std::vector<std::string>> initialPartyLineup;
            if (!state.pendingBattlePartyLineup.empty()) {
                initialPartyLineup = state.pendingBattlePartyLineup;
            }
            battle::app::BattleResultPresentationConfig resultPresentation{};
            if (state.pendingBattleFlowMode == BattleFlowMode::CampaignStory &&
                !state.pendingBattleLoseScript.empty()) {
                resultPresentation.defeatAction = battle::app::BattleDefeatResultAction::ContinueStory;
            } else if (state.pendingBattleFlowMode == BattleFlowMode::CampaignStory) {
                resultPresentation.defeatAction = battle::app::BattleDefeatResultAction::RestartStory;
            } else {
                resultPresentation.defeatAction = battle::app::BattleDefeatResultAction::Return;
            }
            state.pendingBattlePartyLineup.clear();
            if (!battleSession->initialize(window, state.settings, battleKey, state.pendingBattleFlowMode, state.progression,
                                           std::move(initialPartyLineup), resultPresentation)) {
                battleSession.reset();
                state.screen = ScreenState::MainMenu;
                state.pauseContext = PauseContext::Story;
                state.mainSelection = MainMenuAction::Battle;
                clearPendingBattleState(state);
                state.noticeText = "Battle failed to load.";
                state.noticeTimer = 2.8f;
            }
        }

#ifdef RMLUI_SDL_VERSION_MAJOR
        if (state.screen == ScreenState::BossSelector && bossSelectorSession == nullptr) {
            releaseRendererUi(menuResources, rendererUiReady);
#ifdef APP_ENABLE_RMLUI
            if (frontUi.isInitialized()) {
                frontUi.shutdown();
            }
#endif
            vn::stopVoicePlayback();
            vn::shutdown();

            if (!window.enableOpenGL()) {
                state.screen = ScreenState::MainMenu;
                state.noticeText = "Boss selector failed to open.";
                state.noticeTimer = 2.8f;
            } else {
                bossSelectorSession = std::make_unique<battle::selector::Session>();
                if (!bossSelectorSession->initialize(window)) {
                    bossSelectorSession->shutdown();
                    bossSelectorSession.reset();
                    if (!restoreRendererUi(window, menuResources, state.settings)) {
                        std::cerr << "Failed to restore renderer UI after selector load failure\n";
                        return 1;
                    }
                    state.screen = ScreenState::MainMenu;
                    state.noticeText = "Boss selector failed to load.";
                    state.noticeTimer = 2.8f;
                }
            }
        }

        if (state.screen == ScreenState::PartyLoader && partyLoaderSession == nullptr) {
            releaseRendererUi(menuResources, rendererUiReady);
#ifdef APP_ENABLE_RMLUI
            if (frontUi.isInitialized()) {
                frontUi.shutdown();
            }
#endif
            if (bossSelectorSession != nullptr) {
                bossSelectorSession->shutdown();
                bossSelectorSession.reset();
            }
            vn::stopVoicePlayback();
            vn::shutdown();

            battle::BattleDefinition battleDefinition;
            const std::string battleKey = state.pendingBattleKey.empty() ? "tutorial_vs_lyoo" : state.pendingBattleKey;
            if (!battle::loader::loadBattleDefinition(battleKey, battleDefinition)) {
                clearPendingBattleState(state);
                state.screen = ScreenState::MainMenu;
                state.mainSelection = MainMenuAction::Battle;
                state.noticeText = "Party loader failed to resolve the battle.";
                state.noticeTimer = 2.8f;
            } else if (!window.enableOpenGL()) {
                clearPendingBattleState(state);
                state.screen = ScreenState::MainMenu;
                state.mainSelection = MainMenuAction::Battle;
                state.noticeText = "Party loader failed to open.";
                state.noticeTimer = 2.8f;
            } else {
                partyLoaderSession = std::make_unique<battle::partyloader::Session>();
                if (!partyLoaderSession->initialize(window, battleDefinition, state.progression)) {
                    partyLoaderSession->shutdown();
                    partyLoaderSession.reset();
                    clearPendingBattleState(state);
                    state.screen = ScreenState::MainMenu;
                    state.mainSelection = MainMenuAction::Battle;
                    state.noticeText = "Party loader failed to load.";
                    state.noticeTimer = 2.8f;
                }
            }
        }

        if (state.screen == ScreenState::PostBattle && postBattleSession == nullptr) {
            if (!pendingPostBattleContext.has_value()) {
                pendingPostBattleContext.reset();
                state.screen = ScreenState::MainMenu;
                state.mainSelection = MainMenuAction::Battle;
                state.noticeText = "Post-battle screen lost its battle context.";
                state.noticeTimer = 2.8f;
            } else {
                releaseRendererUi(menuResources, rendererUiReady);
#ifdef APP_ENABLE_RMLUI
                if (frontUi.isInitialized()) {
                    frontUi.shutdown();
                }
#endif
                if (bossSelectorSession != nullptr) {
                    bossSelectorSession->shutdown();
                    bossSelectorSession.reset();
                }
                if (partyLoaderSession != nullptr) {
                    partyLoaderSession->shutdown();
                    partyLoaderSession.reset();
                }
                vn::stopVoicePlayback();
                vn::shutdown();

                if (!window.enableOpenGL()) {
                    std::cerr << "[PostBattle] Failed to reopen OpenGL window.\n";
                    routeResolvedBattleOutcome(*pendingPostBattleContext);
                    pendingPostBattleContext.reset();
                } else {
                    postBattleSession = std::make_unique<battle::postbattle::Session>();
                    const PendingPostBattleContext& context = *pendingPostBattleContext;
                    if (!postBattleSession->initialize(window,
                                                      context.battleDefinition,
                                                      context.outcome == battle::app::BattleOutcome::Victory,
                                                      context.summary,
                                                      state.progression,
                                                      context.currentPartyLineup)) {
                        std::cerr << "[PostBattle] Failed to initialize post-battle session; falling back.\n";
                        postBattleSession->shutdown();
                        postBattleSession.reset();
                        routeResolvedBattleOutcome(context);
                        pendingPostBattleContext.reset();
                    }
                }
            }
        }

        if (state.screen != ScreenState::PartyLoader && partyLoaderSession != nullptr) {
            partyLoaderSession->shutdown();
            partyLoaderSession.reset();
        }
        if (state.screen != ScreenState::PostBattle && postBattleSession != nullptr) {
            postBattleSession->shutdown();
            postBattleSession.reset();
        }
#endif

        if (state.screen == ScreenState::MainMenu && battleSession != nullptr) {
            battleSession->shutdown();
            battleSession.reset();
            pendingPostBattleContext.reset();
            pendingTutorialDrillContext.reset();
            pendingBossPreviewContext.reset();
            state.pauseContext = PauseContext::Story;
            clearPendingBattleState(state);
        }

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
                if (battleSession->isFinished() && !loadingTransition.active) {
                    const battle::app::BattleOutcome outcome = battleSession->outcome();
                    const bool exitedToMainMenu = battleSession->exitedToMainMenu();
                    const std::string completedTutorialKey = battleSession->completedTutorialKey();
                    const std::string completedBattleKey = state.pendingBattleKey;
                    const std::vector<std::string> currentPartyLineup = battleSession->currentPartyLineup();

                    if (!completedTutorialKey.empty()) {
                        battle::markCompletedTutorial(state.progression, completedTutorialKey);
                        battle::normalizePlayerProgression(state.progression);
                    }

                    if (exitedToMainMenu) {
                        if (!currentPartyLineup.empty()) {
                            state.progression.currentPartyLineup = currentPartyLineup;
                            battle::normalizePlayerProgression(state.progression);
                        }
                        (void)save::writeProfileProgression(state.progression);
                        battleSession->shutdown();
                        battleSession.reset();
                        clearPendingBattleState(state);
                        pendingTutorialDrillContext.reset();
                        pendingBossPreviewContext.reset();
                        state.requestStoryExitToMainMenu = false;
                        state.requestStoryReturnToBossSelector = false;
                        vn::setPaused(false);
                        vn::reset();
                        state.story.entryIndex = 0;
                        state.pauseSelection = PauseAction::Continue;
                        state.pauseContext = PauseContext::Story;
                        state.confirmSelection = ConfirmAction::Cancel;
                        state.screen = ScreenState::MainMenu;
                        state.mainSelection = MainMenuAction::Start;
                        state.noticeText.clear();
                        state.noticeTimer = 0.0f;
                        continue;
                    }

                    const BattleFlowMode battleFlowMode = state.pendingBattleFlowMode;
                    const ScreenState battleReturnScreen = state.pendingBattleReturnScreen;
                    const std::string battleWinScript = state.pendingBattleWinScript;
                    const std::string battleLoseScript = state.pendingBattleLoseScript;
                    const std::string battleNextStoryScript = state.pendingBattleNextStoryScript;
                    const std::string battleSourceStoryScript =
                        battleFlowMode == BattleFlowMode::CampaignStory && state.story.loaded
                        ? save::chapterIdFromScript(state.story.script)
                        : std::string();

                    if (battleFlowMode == BattleFlowMode::UnlockCharacterDrill) {
                        (void)save::writeProfileProgression(state.progression);

                        if (pendingTutorialDrillContext.has_value()) {
                            const PendingPostBattleContext resumeContext =
                                pendingTutorialDrillContext->resumeContext;
                            startLoadingTransition(
                                loadingTransition,
                                LoadingTransitionPresentation::RmlUi,
                                std::function<bool()>{},
                                [&, resumeContext]() -> bool {
                                    const bool result =
                                        routeResolvedBattleOutcomeContinuation(resumeContext);
                                    pendingTutorialDrillContext.reset();
                                    return result;
                                });
                        } else {
                            startLoadingTransition(
                                loadingTransition,
                                LoadingTransitionPresentation::RmlUi,
                                std::function<bool()>{},
                                [&]() -> bool {
                                    if (battleSession != nullptr) {
                                        battleSession->shutdown();
                                        battleSession.reset();
                                    }
                                    clearPendingBattleState(state);
                                    state.screen = ScreenState::MainMenu;
                                    state.pauseContext = PauseContext::Story;
                                    state.mainSelection = MainMenuAction::Start;
                                    return true;
                                });
                        }
                        continue;
                    }

                    if (battleFlowMode == BattleFlowMode::BossPreview) {
                        (void)save::writeProfileProgression(state.progression);

                        if (pendingBossPreviewContext.has_value()) {
                            const PendingBossPreviewContext previewContext =
                                *pendingBossPreviewContext;
                            startLoadingTransition(
                                loadingTransition,
                                LoadingTransitionPresentation::RmlUi,
                                std::function<bool()>{},
                                [&, previewContext]() -> bool {
                                    if (battleSession != nullptr) {
                                        battleSession->shutdown();
                                        battleSession.reset();
                                    }
                                    restoreBossPreviewBattleContext(previewContext);
                                    pendingBossPreviewContext.reset();
                                    beginBattle(state, previewContext.battleKey);
                                    return true;
                                });
                        } else {
                            startLoadingTransition(
                                loadingTransition,
                                LoadingTransitionPresentation::RmlUi,
                                std::function<bool()>{},
                                [&]() -> bool {
                                    if (battleSession != nullptr) {
                                        battleSession->shutdown();
                                        battleSession.reset();
                                    }
                                    clearPendingBattleState(state);
                                    state.screen = ScreenState::MainMenu;
                                    state.pauseContext = PauseContext::Story;
                                    state.mainSelection = MainMenuAction::Start;
                                    return true;
                                });
                        }
                        continue;
                    }

                    battle::BattleDefinition completedBattleDefinition;
                    if (!completedBattleKey.empty() &&
                        !battle::loader::loadBattleDefinition(completedBattleKey, completedBattleDefinition)) {
                        completedBattleDefinition.key = completedBattleKey;
                        completedBattleDefinition.name = completedBattleKey;
                    }
                    const PendingPostBattleContext resolvedBattleContext{
                        completedBattleDefinition,
                        battleSession->postBattleSummary(),
                        outcome,
                        currentPartyLineup,
                        battleFlowMode,
                        battleReturnScreen,
                        battleWinScript,
                        battleLoseScript,
                        battleNextStoryScript,
                        battleSourceStoryScript,
                    };
#ifdef RMLUI_SDL_VERSION_MAJOR
                    pendingPostBattleContext = resolvedBattleContext;
                    startLoadingTransition(
                        loadingTransition,
                        LoadingTransitionPresentation::RmlUi,
                        std::function<bool()>{},
                        [&]() -> bool {
                            if (battleSession != nullptr) {
                                battleSession->shutdown();
                                battleSession.reset();
                            }
                            state.screen = ScreenState::PostBattle;
                            return true;
                        });
#else
                    startLoadingTransition(
                        loadingTransition,
                        LoadingTransitionPresentation::RmlUi,
                        std::function<bool()>{},
                        [&, resolvedBattleContext]() -> bool {
                            return routeResolvedBattleOutcome(resolvedBattleContext);
                        });
#endif
                }
            }
        }
#ifdef RMLUI_SDL_VERSION_MAJOR
        else if (state.screen == ScreenState::BossSelector) {
            if (bossSelectorSession != nullptr) {
                bossSelectorSession->update(deltaSeconds);
                if (bossSelectorSession->isFinished()) {
                    bossSelectorSession->shutdown();
                    bossSelectorSession.reset();
                    state.screen = ScreenState::MainMenu;
                    state.mainSelection = MainMenuAction::Battle;
                }
            }
        }
        else if (state.screen == ScreenState::PartyLoader) {
            if (partyLoaderSession != nullptr) {
                partyLoaderSession->update(deltaSeconds);
            }
        }
        else if (state.screen == ScreenState::PostBattle) {
            if (postBattleSession != nullptr) {
                postBattleSession->update(deltaSeconds);
                if (postBattleSession->isFinished() &&
                    pendingPostBattleContext.has_value() &&
                    !loadingTransition.active) {
                    state.progression = postBattleSession->progression();
                    const PendingPostBattleContext resolvedBattleContext = *pendingPostBattleContext;
                    startLoadingTransition(
                        loadingTransition,
                        LoadingTransitionPresentation::RmlUi,
                        std::function<bool()>{},
                        [&, resolvedBattleContext]() -> bool {
                            const bool result = routeResolvedBattleOutcome(resolvedBattleContext);
                            pendingPostBattleContext.reset();
                            return result;
                        });
                }
            }
        }
#endif
        else if (state.screen == ScreenState::Playing) {
            vn::update(deltaSeconds);

            if (vn::consumeAdvanceRequest()) {
                const vn::ScriptEntry currentEntry =
                    state.story.entryIndex < state.story.script.entries.size()
                    ? state.story.script.entries[state.story.entryIndex]
                    : vn::ScriptEntry{};
                const std::string pendingBattleKey = currentEntry.battleKey;
                const int pendingBattleId = currentEntry.battleId;
                state.story.entryIndex++;
                if (!pendingBattleKey.empty()) {
                    state.pendingBattleFlowMode = state.storyFlowMode == StoryFlowMode::PracticeReplay
                        ? BattleFlowMode::PracticeReplayStory
                        : BattleFlowMode::CampaignStory;
                    state.pendingBattleReturnScreen = state.storyEndReturnScreen;
                    state.pendingBattleWinScript = currentEntry.battleWinScript;
                    state.pendingBattleLoseScript = state.storyFlowMode == StoryFlowMode::Campaign
                        ? currentEntry.battleLoseScript
                        : std::string();
                    battle::BattleDefinition battleDefinition;
                    if (!battle::loader::loadBattleDefinition(pendingBattleKey, battleDefinition)) {
                        clearPendingBattleState(state);
                        state.noticeText = "Battle not found: " + pendingBattleKey;
                        state.noticeTimer = 2.6f;
                        state.screen = ScreenState::MainMenu;
                    } else {
                        state.pendingStoryNextScript = state.storyFlowMode == StoryFlowMode::Campaign
                            ? battleDefinition.nextStoryScript
                            : std::string();
                        startLoadingTransition(
                            loadingTransition,
                            LoadingTransitionPresentation::RmlUi,
                            std::function<bool()>{},
                            [&, battleDefinition]() -> bool {
                                if (shouldLaunchBossPreview(battleDefinition.key)) {
                                    beginBossPreview(battleDefinition);
                                } else {
                                    pendingBossPreviewContext.reset();
                                    beginBattle(state, battleDefinition.key);
                                }
                                return true;
                            });
                    }
                } else if (pendingBattleId >= 0) {
                    state.pendingBattleFlowMode = state.storyFlowMode == StoryFlowMode::PracticeReplay
                        ? BattleFlowMode::PracticeReplayStory
                        : BattleFlowMode::CampaignStory;
                    state.pendingBattleReturnScreen = state.storyEndReturnScreen;
                    state.pendingBattleWinScript = currentEntry.battleWinScript;
                    state.pendingBattleLoseScript = state.storyFlowMode == StoryFlowMode::Campaign
                        ? currentEntry.battleLoseScript
                        : std::string();
                    battle::BattleDefinition battleDefinition;
                    if (!battle::loader::loadBattleDefinitionById(pendingBattleId, battleDefinition)) {
                        clearPendingBattleState(state);
                        state.noticeText = "Battle not found: " + std::to_string(pendingBattleId);
                        state.noticeTimer = 2.6f;
                        state.screen = ScreenState::MainMenu;
                    } else {
                        state.pendingStoryNextScript = state.storyFlowMode == StoryFlowMode::Campaign
                            ? battleDefinition.nextStoryScript
                            : std::string();
                        startLoadingTransition(
                            loadingTransition,
                            LoadingTransitionPresentation::RmlUi,
                            std::function<bool()>{},
                            [&, battleDefinition]() -> bool {
                                if (shouldLaunchBossPreview(battleDefinition.key)) {
                                    beginBossPreview(battleDefinition);
                                } else {
                                    pendingBossPreviewContext.reset();
                                    beginBattle(state, battleDefinition.key);
                                }
                                return true;
                            });
                    }
                } else if (!state.pendingStoryNextScript.empty() &&
                           state.story.entryIndex >= state.story.script.entries.size()) {
                    const std::string nextStoryScript = state.pendingStoryNextScript;
                    state.pendingStoryNextScript.clear();
                    startLoadingTransition(
                        loadingTransition,
                        LoadingTransitionPresentation::RmlUi,
                        std::function<bool()>{},
                        [&, nextStoryScript]() -> bool {
                            beginStory(state, nextStoryScript, state.storyEndReturnScreen);
                            return true;
                        });
                } else if (state.story.entryIndex >= state.story.script.entries.size()) {
                    if (shouldLaunchCreditsAfterStory(state.story)) {
                        beginCredits(state);
                    } else if (state.storyEndReturnScreen == ScreenState::BossSelector) {
                        state.requestStoryReturnToBossSelector = true;
                    } else {
                        state.screen = ScreenState::MainMenu;
                        state.mainSelection = MainMenuAction::Start;
                        state.noticeText = "End of story.";
                        state.noticeTimer = 2.2f;
                    }
                } else {
                    (void)save::autosave(buildStorySaveGame(state));
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
        } else if (!shouldDeferBackendRestore(state) &&
                   !restoreNonMainMenuUi(window, frontUi, menuResources, rendererUiReady, state.settings)) {
            std::cerr << "Failed to restore renderer UI\n";
            return 1;
        }
#endif

        applyRuntimeSettings();

        if (!shouldKeepStoryBgmPlaying(state)) {
            vn::stopBgmPlayback();
        }

        uiMusic.update(state, loadingTransition.active, deltaSeconds);

#if defined(APP_ENABLE_RMLUI) || defined(RMLUI_SDL_VERSION_MAJOR)
        const game::audio::UiMusicVisualState& uiMusicVisualState = uiMusic.visualState();
        const graphics::RmlUiLoadingOverlayState hiddenRmlLoadingOverlay;
#ifdef APP_ENABLE_RMLUI
        if (frontUi.isInitialized()) {
            frontUi.setUiMusicVisualState(uiMusicVisualState);
            frontUi.setLoadingOverlay(hiddenRmlLoadingOverlay);
        }
#endif
#ifdef RMLUI_SDL_VERSION_MAJOR
        if (bossSelectorSession != nullptr) {
            bossSelectorSession->setUiMusicVisualState(uiMusicVisualState);
            bossSelectorSession->setLoadingOverlay(hiddenRmlLoadingOverlay);
        }
        if (partyLoaderSession != nullptr) {
            partyLoaderSession->setUiMusicVisualState(uiMusicVisualState);
            partyLoaderSession->setLoadingOverlay(hiddenRmlLoadingOverlay);
        }
        if (postBattleSession != nullptr) {
            postBattleSession->setUiMusicVisualState(uiMusicVisualState);
            postBattleSession->setLoadingOverlay(hiddenRmlLoadingOverlay);
        }
#endif

        if (loadingTransitionUsesRmlUi(loadingTransition)) {
            const graphics::RmlUiLoadingOverlayState rmlLoadingOverlay =
                buildRmlLoadingOverlayState(loadingTransition);
            bool overlayApplied = false;
#ifdef RMLUI_SDL_VERSION_MAJOR
            if (state.screen == ScreenState::PostBattle && postBattleSession != nullptr) {
                postBattleSession->setLoadingOverlay(rmlLoadingOverlay);
                overlayApplied = true;
            }
            if (state.screen == ScreenState::PartyLoader && partyLoaderSession != nullptr) {
                partyLoaderSession->setLoadingOverlay(rmlLoadingOverlay);
                overlayApplied = true;
            }
            if (!overlayApplied &&
                state.screen == ScreenState::BossSelector &&
                bossSelectorSession != nullptr) {
                bossSelectorSession->setLoadingOverlay(rmlLoadingOverlay);
                overlayApplied = true;
            }
#endif
            if (!overlayApplied &&
                state.screen == ScreenState::BattleDemo &&
                battleSession != nullptr) {
                battleSession->setLoadingOverlay(rmlLoadingOverlay);
                overlayApplied = true;
            }
#ifdef APP_ENABLE_RMLUI
            if (!overlayApplied && frontUi.isInitialized()) {
                frontUi.setLoadingOverlay(rmlLoadingOverlay);
            }
#endif
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
            battleSession->render();
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
        } else if (state.screen == ScreenState::LoadGameMenu ||
                   state.screen == ScreenState::LoadConfirmDelete) {
            renderLoadScreen(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
        } else if (state.screen == ScreenState::BattleDemo && battleSession != nullptr) {
            battleSession->render();
#ifdef RMLUI_SDL_VERSION_MAJOR
        } else if (state.screen == ScreenState::PostBattle && postBattleSession != nullptr) {
            postBattleSession->render();
        } else if (state.screen == ScreenState::PartyLoader && partyLoaderSession != nullptr) {
            partyLoaderSession->render();
        } else if (state.screen == ScreenState::BossSelector && bossSelectorSession != nullptr) {
            bossSelectorSession->render();
#endif
        } else {
#ifdef APP_ENABLE_RMLUI
            frontUi.render();
#else
            renderMainMenu(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
#endif
        }

        const float overlayAlpha = loadingOverlayAlpha(loadingTransition);
        if (!loadingTransitionUsesRmlUi(loadingTransition) && overlayAlpha > 0.0f) {
            if (SDL_Renderer* renderer = window.getRenderer(); renderer != nullptr) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(
                    renderer,
                    0,
                    0,
                    0,
                    static_cast<Uint8>(std::lround(std::clamp(overlayAlpha, 0.0f, 1.0f) * 255.0f)));
                const SDL_Rect overlayRect{0, 0, window.getWidth(), window.getHeight()};
                SDL_RenderFillRect(renderer, &overlayRect);
            } else if (window.getGlContext() != nullptr) {
                renderLoadingOverlayGl(
                    loadingOverlayGl,
                    window.getGlContext(),
                    window.getDrawableWidth(),
                    window.getDrawableHeight(),
                    overlayAlpha);
            }
        }

        window.present();

        if (loadingTransition.active &&
            loadingTransition.elapsedSeconds >= totalLoadingTransitionSeconds()) {
            clearLoadingTransition(loadingTransition);
        }
    }

    if (battleSession != nullptr) {
        battleSession->shutdown();
        battleSession.reset();
    }
#ifdef RMLUI_SDL_VERSION_MAJOR
    if (postBattleSession != nullptr) {
        postBattleSession->shutdown();
        postBattleSession.reset();
    }
#endif
#ifdef RMLUI_SDL_VERSION_MAJOR
    if (partyLoaderSession != nullptr) {
        partyLoaderSession->shutdown();
        partyLoaderSession.reset();
    }
    if (bossSelectorSession != nullptr) {
        bossSelectorSession->shutdown();
        bossSelectorSession.reset();
    }
#endif

#ifdef APP_ENABLE_RMLUI
    frontUi.shutdown();
#endif
    destroyLoadingLogoTexture();
    destroyLoadingOverlayGlState(loadingOverlayGl);
    destroyMenuResources(menuResources);
    uiMusic.shutdown();
    vn::shutdown();
    return 0;
}
