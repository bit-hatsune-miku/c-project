#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
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
#include "game/app_battle_session.h"
#include "game/vn/vn_system.h"

constexpr const char* kChapterScriptPath = "assets/vn/json/ch0.json";
constexpr const char* kMainMenuArtPath = "assets/vn/backgrounds/ch0/mainmenu art.png";
constexpr const char* kMainMenuTitlePath = "assets/vn/images/MainMenuTitle.png";
constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr float kMenuCanvasScale = 4.0f;

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

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst{
        static_cast<int>(std::lround(centerX ? rect.x + (rect.w - static_cast<float>(surface->w)) * 0.5f : rect.x)),
        static_cast<int>(std::lround(rect.y + (rect.h - static_cast<float>(surface->h)) * 0.5f)),
        surface->w,
        surface->h
    };
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void drawWrappedTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                           const SDL_Color& color, const SDL_FRect& rect, bool centerX) {
    if (font == nullptr || text.empty()) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(
        font,
        text.c_str(),
        color,
        static_cast<Uint32>(std::max(1.0f, rect.w))
    );
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst{
        static_cast<int>(std::lround(centerX ? rect.x + (rect.w - static_cast<float>(surface->w)) * 0.5f : rect.x)),
        static_cast<int>(std::lround(rect.y + (rect.h - static_cast<float>(surface->h)) * 0.5f)),
        surface->w,
        surface->h
    };
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
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

void applyCurrentEntry(const StorySession& story, const GameSettings& settings) {
    if (story.script.entries.empty() || story.entryIndex >= story.script.entries.size()) {
        return;
    }

    vn::setVoiceVolume(settings.voiceVolume);
    vn::setTypewriterSpeed(settings.textSpeed);

    const auto& entry = story.script.entries[story.entryIndex];
    vn::showLine(
        entry.text,
        vn::getDisplaySpeakerName(entry),
        entry.icon,
        entry.voice,
        entry.fontPath,
        entry.autoAdvanceOnVoiceEnd,
        entry.iconFrameCount,
        entry.iconFps,
        entry.background
    );
}

bool ensureStoryLoaded(StorySession& story) {
    if (story.loaded) {
        return true;
    }

    if (!vn::loadScript(resolvePath(kChapterScriptPath), story.script)) {
        return false;
    }

    story.loaded = true;
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
    applyCurrentEntry(state.story, state.settings);
}

void beginBattleDemo(AppState& state) {
    state.pauseSelection = PauseAction::Continue;
    state.pauseContext = PauseContext::Battle;
    state.confirmSelection = ConfirmAction::Cancel;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::BattleDemo;
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
#endif
}

void loadMenuResources(MenuResources& resources, SDL_Renderer* renderer) {
    resources.background = loadTexture(renderer, resolvePath(kMainMenuArtPath));
    resources.menuCanvas = createRenderTarget(renderer, kReferenceWidth, kReferenceHeight, kMenuCanvasScale);
    resources.titleLogo = loadTexture(renderer, resolvePath(kMainMenuTitlePath));
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
    if (!vn::initialize(window.getRenderer(), window.getWidth(), window.getHeight())) {
        return false;
    }
    vn::setVoiceVolume(settings.voiceVolume);
    vn::setTypewriterSpeed(settings.textSpeed);
    loadMenuResources(menuResources, window.getRenderer());
    return true;
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "battle" && std::string(argv[2]) == "mode") {
        return launchDefaultBattleMode(argc, argv);
    }

    Window window("Hatsune Miku: Our Underground BIT Idol", 1280, 720);
    if (!window.isOpen()) {
        std::cerr << "Failed to initialize window\n";
        return 1;
    }

    window.setEscapeToQuitEnabled(false);

    if (!vn::initialize(window.getRenderer(), window.getWidth(), window.getHeight())) {
        std::cerr << "Failed to initialize VN system\n";
        return 1;
    }

    AppState state;
    state.settings.fullscreen = window.isFullscreen();
    vn::setVoiceVolume(state.settings.voiceVolume);
    vn::setTypewriterSpeed(state.settings.textSpeed);

    MenuResources menuResources;
    loadMenuResources(menuResources, window.getRenderer());
    SettingsMenuController settingsMenu;
    std::unique_ptr<battle::app::Session> battleSession;

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
                    handleMainMenuEvent(state, window, event, window.getWidth(), window.getHeight());
                    break;

                case ScreenState::Settings:
                    settingsMenu.handleEvent(state, window, event, window.getWidth(), window.getHeight());
                    break;

                case ScreenState::BattleDemo:
                    if (battleSession != nullptr) {
                        battleSession->handleEvent(event);
                    }
                    break;

                case ScreenState::Playing:
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            openPauseMenu(state);
                        } else if (event.key.keysym.sym == SDLK_F11) {
                            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else if (event.key.keysym.sym == SDLK_SPACE) {
                            vn::onSpacePressed();
                        }
                    }
                    break;

                case ScreenState::PauseMenu:
                    handlePauseMenuEvent(state, window, event, window.getWidth(), window.getHeight());
                    break;

                case ScreenState::PauseConfirmExit:
                    handlePauseConfirmEvent(state, window, event, window.getWidth(), window.getHeight());
                    break;
            }
        }

        if (state.screen == ScreenState::BattleDemo && battleSession == nullptr) {
            destroyMenuResources(menuResources);
            vn::shutdown();
            if (!window.enableOpenGL()) {
                state.screen = ScreenState::MainMenu;
                state.pauseContext = PauseContext::Story;
                state.mainSelection = MainMenuAction::Battle;
                state.noticeText = "Battle backend failed to load.";
                state.noticeTimer = 2.8f;
            } else {
                battleSession = std::make_unique<battle::app::Session>();
            }
            if (battleSession != nullptr && !battleSession->initialize(window, state.settings)) {
                battleSession.reset();
                if (!restoreRendererUi(window, menuResources, state.settings)) {
                    std::cerr << "Failed to restore renderer UI after battle load failure\n";
                    return 1;
                }
                state.screen = ScreenState::MainMenu;
                state.pauseContext = PauseContext::Story;
                state.mainSelection = MainMenuAction::Battle;
                state.noticeText = "Battle demo failed to load.";
                state.noticeTimer = 2.8f;
            }
        }

        if (state.screen == ScreenState::MainMenu && battleSession != nullptr) {
            battleSession->shutdown();
            battleSession.reset();
            if (!restoreRendererUi(window, menuResources, state.settings)) {
                std::cerr << "Failed to restore renderer UI\n";
                return 1;
            }
            state.pauseContext = PauseContext::Story;
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

        if ((state.screen == ScreenState::PauseMenu || state.screen == ScreenState::PauseConfirmExit) &&
            state.pauseIntroTime < kPauseIntroMaxTime) {
            state.pauseIntroTime = std::min(kPauseIntroMaxTime, state.pauseIntroTime + deltaSeconds);
        }

        if (state.screen == ScreenState::BattleDemo) {
            if (battleSession != nullptr) {
                battleSession->update(deltaSeconds);
                if (battleSession->isFinished()) {
                    battleSession->shutdown();
                    battleSession.reset();
                    if (!restoreRendererUi(window, menuResources, state.settings)) {
                        std::cerr << "Failed to restore renderer UI\n";
                        return 1;
                    }
                    state.screen = ScreenState::MainMenu;
                    state.pauseContext = PauseContext::Story;
                    state.mainSelection = MainMenuAction::Battle;
                }
            }
        } else if (state.screen == ScreenState::Playing) {
            vn::update(deltaSeconds);

            if (vn::consumeAdvanceRequest()) {
                state.story.entryIndex++;
                if (state.story.entryIndex >= state.story.script.entries.size()) {
                    state.screen = ScreenState::MainMenu;
                    state.mainSelection = MainMenuAction::Start;
                    state.noticeText = "End of chapter 0.";
                    state.noticeTimer = 2.2f;
                } else {
                    applyCurrentEntry(state.story, state.settings);
                }
            }
        }

        if (window.getRenderer() != nullptr) {
            window.clear(14, 18, 30, 255);
        }

        const bool renderStoryBackdrop =
            state.screen == ScreenState::Playing ||
            ((state.screen == ScreenState::PauseMenu || state.screen == ScreenState::PauseConfirmExit) &&
             state.pauseContext == PauseContext::Story) ||
            (state.screen == ScreenState::Settings &&
             state.settingsReturnScreen == ScreenState::PauseMenu &&
             state.pauseContext == PauseContext::Story);

        const bool renderBattleBackdrop =
            battleSession != nullptr &&
            (((state.screen == ScreenState::PauseMenu || state.screen == ScreenState::PauseConfirmExit) &&
              state.pauseContext == PauseContext::Battle) ||
             (state.screen == ScreenState::Settings &&
              state.settingsReturnScreen == ScreenState::PauseMenu &&
              state.pauseContext == PauseContext::Battle));

        if (renderStoryBackdrop) {
            vn::render();
            if (state.screen == ScreenState::PauseMenu || state.screen == ScreenState::PauseConfirmExit) {
                renderPauseScreen(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
            } else if (state.screen == ScreenState::Settings && state.settingsReturnScreen == ScreenState::PauseMenu) {
                settingsMenu.render(window.getRenderer(), menuResources, state,
                                    window.getWidth(), window.getHeight(), true);
            }
        } else if (renderBattleBackdrop) {
            battleSession->render();
            if (state.screen == ScreenState::PauseMenu || state.screen == ScreenState::PauseConfirmExit) {
                renderPauseScreen(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
            } else if (state.screen == ScreenState::Settings && state.settingsReturnScreen == ScreenState::PauseMenu) {
                settingsMenu.render(window.getRenderer(), menuResources, state,
                                    window.getWidth(), window.getHeight(), true);
            }
        } else if (state.screen == ScreenState::Settings) {
            settingsMenu.render(window.getRenderer(), menuResources, state,
                                window.getWidth(), window.getHeight(), false);
        } else if (state.screen == ScreenState::BattleDemo && battleSession != nullptr) {
            battleSession->render();
        } else {
            renderMainMenu(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
        }

        window.present();
    }

    if (battleSession != nullptr) {
        battleSession->shutdown();
        battleSession.reset();
    }
    destroyMenuResources(menuResources);
    vn::shutdown();
    return 0;
}
