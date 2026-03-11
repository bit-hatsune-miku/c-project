#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#ifdef VN_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif
#ifdef VN_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "window.h"
#include "game/vn_script.h"
#include "game/vn_system.h"

namespace {

constexpr const char* kChapterScriptPath = "assets/vn/json/ch0.json";
constexpr const char* kMainMenuArtPath = "assets/vn/backgrounds/ch0/mainmenu art.png";
constexpr const char* kMainMenuTitlePath = "assets/vn/images/MainMenuTitle.png";
constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr float centeredX(float width) {
    return (static_cast<float>(kReferenceWidth) - width) * 0.5f;
}
constexpr float kMenuContentInsetX = 24.0f;
constexpr SDL_FRect kMenuShellRect{46.0f, 196.0f, 450.0f, 454.0f};
constexpr SDL_FRect kMenuTitleBackdropRect{kMenuShellRect.x, 22.0f, kMenuShellRect.w, 148.0f};
constexpr SDL_FRect kMenuTitleRect{
    kMenuTitleBackdropRect.x + kMenuContentInsetX,
    28.0f,
    kMenuTitleBackdropRect.w - kMenuContentInsetX * 2.0f,
    136.0f
};
constexpr SDL_FRect kMenuSectionLabelRect{kMenuShellRect.x + kMenuContentInsetX, 180.0f, 220.0f, 34.0f};
constexpr SDL_FRect kFooterBandRect{
    kMenuShellRect.x + kMenuContentInsetX,
    620.0f,
    kMenuShellRect.w - kMenuContentInsetX * 2.0f,
    26.0f
};
constexpr SDL_FRect kNoticeRect{640.0f, 560.0f, 566.0f, 86.0f};
constexpr float kSettingsShellWidth = 968.0f;
constexpr float kSettingsTitleWidth = 920.0f;
constexpr SDL_FRect kSettingsTitleBackdropRect{centeredX(kSettingsShellWidth), 28.0f, kSettingsShellWidth, 106.0f};
constexpr SDL_FRect kSettingsTitleRect{centeredX(kSettingsTitleWidth), 38.0f, kSettingsTitleWidth, 86.0f};
constexpr SDL_FRect kSettingsShellRect{centeredX(kSettingsShellWidth), 154.0f, kSettingsShellWidth, 498.0f};
constexpr SDL_FRect kSettingsHeaderBandRect{centeredX(kSettingsTitleWidth), 176.0f, kSettingsTitleWidth, 86.0f};
constexpr SDL_FRect kSettingsSectionLabelRect{centeredX(kSettingsTitleWidth) + 28.0f, 188.0f, 250.0f, 28.0f};
constexpr SDL_FRect kSettingsHintRect{centeredX(kSettingsTitleWidth) + 28.0f, 214.0f, 640.0f, 26.0f};
constexpr SDL_FRect kSettingsFooterBandRect{centeredX(kSettingsTitleWidth), 612.0f, kSettingsTitleWidth, 26.0f};
constexpr SDL_FRect kDisplayModeRowRect{centeredX(kSettingsTitleWidth), 278.0f, kSettingsTitleWidth, 58.0f};
constexpr SDL_FRect kVolumeRowRect{centeredX(kSettingsTitleWidth), 354.0f, kSettingsTitleWidth, 58.0f};
constexpr SDL_FRect kSpeedRowRect{centeredX(kSettingsTitleWidth), 430.0f, kSettingsTitleWidth, 58.0f};
constexpr SDL_FRect kBackRowRect{centeredX(kSettingsTitleWidth), 506.0f, kSettingsTitleWidth, 58.0f};
constexpr SDL_FRect kDisplayModeValueRect{centeredX(kSettingsTitleWidth) + 692.0f, 289.0f, 206.0f, 36.0f};
constexpr SDL_FRect kVolumeSliderRect{centeredX(kSettingsTitleWidth) + 390.0f, 365.0f, 298.0f, 36.0f};
constexpr SDL_FRect kSpeedSliderRect{centeredX(kSettingsTitleWidth) + 390.0f, 441.0f, 298.0f, 36.0f};
constexpr SDL_FRect kVolumeValueRect{centeredX(kSettingsTitleWidth) + 708.0f, 365.0f, 190.0f, 36.0f};
constexpr SDL_FRect kSpeedValueRect{centeredX(kSettingsTitleWidth) + 708.0f, 441.0f, 190.0f, 36.0f};
constexpr float kMinTextSpeed = 18.0f;
constexpr float kMaxTextSpeed = 90.0f;
constexpr float kTextSpeedRange = kMaxTextSpeed - kMinTextSpeed;
constexpr float kMenuCanvasScale = 4.0f;
constexpr float kMenuIntroDuration = 0.52f;
constexpr float kMenuIntroStagger = 0.09f;
constexpr float kMenuIntroTravel = 230.0f;
constexpr float kMenuIntroMaxTime = kMenuIntroDuration + kMenuIntroStagger * 3.0f;
constexpr float kPauseShellWidth = kMenuShellRect.w;
constexpr float kPauseTitleWidth = kMenuShellRect.w;
constexpr float kPauseInnerWidth = kPauseShellWidth - kMenuContentInsetX * 2.0f;
constexpr float kPauseButtonWidth = 360.0f;
constexpr float kPauseButtonX = centeredX(kPauseButtonWidth);
constexpr SDL_FRect kPauseTitleBackdropRect{centeredX(kPauseTitleWidth), 72.0f, kPauseTitleWidth, 118.0f};
constexpr SDL_FRect kPauseTitleRect{centeredX(kPauseTitleWidth) + kMenuContentInsetX, 80.0f,
                                    kPauseTitleWidth - kMenuContentInsetX * 2.0f, 100.0f};
constexpr SDL_FRect kPauseShellRect{centeredX(kPauseShellWidth), 208.0f, kPauseShellWidth, 460.0f};
constexpr SDL_FRect kPauseHeaderBandRect{kPauseShellRect.x + 12.0f, kPauseShellRect.y + 12.0f,
                                         kPauseShellRect.w - 24.0f, 118.0f};
constexpr SDL_FRect kPauseSectionLabelRect{kPauseShellRect.x + kMenuContentInsetX, kPauseShellRect.y + 18.0f,
                                           210.0f, 28.0f};
constexpr SDL_FRect kPauseHintRect{kPauseShellRect.x + kMenuContentInsetX, kPauseShellRect.y + 48.0f,
                                   kPauseShellRect.w - kMenuContentInsetX * 2.0f, 46.0f};
constexpr SDL_FRect kPauseNoticeRect{kPauseShellRect.x + kMenuContentInsetX, 676.0f,
                                     kPauseShellRect.w - kMenuContentInsetX * 2.0f, 42.0f};
constexpr SDL_FRect kPauseConfirmShellRect{356.0f, 206.0f, 568.0f, 308.0f};
constexpr SDL_FRect kPauseConfirmTitleRect{kPauseConfirmShellRect.x + 36.0f, kPauseConfirmShellRect.y + 28.0f,
                                           kPauseConfirmShellRect.w - 72.0f, 46.0f};
constexpr SDL_FRect kPauseConfirmBodyRect{kPauseConfirmShellRect.x + 46.0f, kPauseConfirmShellRect.y + 92.0f,
                                          kPauseConfirmShellRect.w - 92.0f, 82.0f};
constexpr SDL_FRect kPauseConfirmFooterBandRect{kPauseConfirmShellRect.x + 24.0f, kPauseConfirmShellRect.y + 268.0f,
                                                kPauseConfirmShellRect.w - 48.0f, 18.0f};
constexpr float kPauseIntroDuration = 0.38f;
constexpr float kPauseIntroStagger = 0.07f;
constexpr float kPauseIntroTravel = 180.0f;
constexpr float kPauseIntroMaxTime = kPauseIntroDuration + kPauseIntroStagger * 3.0f;

enum class ScreenState {
    MainMenu,
    Settings,
    Playing,
    PauseMenu,
    PauseConfirmExit
};

enum class MainMenuAction {
    Start,
    Load,
    Settings,
    Exit
};

enum class PauseAction {
    Continue,
    Load,
    Settings,
    ExitToMainMenu
};

enum class ConfirmAction {
    Cancel,
    ExitToMainMenu
};

struct GameSettings {
    bool fullscreen = false;
    float voiceVolume = 0.82f;
    float textSpeed = 42.0f;
};

struct StorySession {
    vn::Script script;
    size_t entryIndex = 0;
    bool loaded = false;
};

struct MenuResources {
    SDL_Texture* background = nullptr;
    SDL_Texture* menuCanvas = nullptr;
    SDL_Texture* titleLogo = nullptr;
#ifdef VN_ENABLE_TTF
    TTF_Font* titleFont = nullptr;
    TTF_Font* subtitleFont = nullptr;
    TTF_Font* itemFont = nullptr;
    TTF_Font* smallFont = nullptr;
    TTF_Font* tinyFont = nullptr;
#endif
};

struct AppState {
    ScreenState screen = ScreenState::MainMenu;
    ScreenState settingsReturnScreen = ScreenState::MainMenu;
    int mainSelection = 0;
    int settingsSelection = 0;
    int pauseSelection = 0;
    int confirmSelection = 0;
    float menuIntroTime = 0.0f;
    float pauseIntroTime = 0.0f;
    GameSettings settings;
    StorySession story;
    std::string noticeText;
    float noticeTimer = 0.0f;
};

struct MenuButton {
    MainMenuAction action;
    const char* label;
    SDL_FRect rect;
};

struct PauseButton {
    PauseAction action;
    const char* label;
    SDL_FRect rect;
};

struct ConfirmButton {
    ConfirmAction action;
    const char* label;
    SDL_FRect rect;
};

constexpr std::array<MenuButton, 4> kMenuButtons{{
    {MainMenuAction::Start, "Start", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 246.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}},
    {MainMenuAction::Load, "Load", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 320.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}},
    {MainMenuAction::Settings, "Settings", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 394.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}},
    {MainMenuAction::Exit, "Exit", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 468.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}}
}};

constexpr std::array<PauseButton, 4> kPauseButtons{{
    {PauseAction::Continue, "Continue", SDL_FRect{kPauseButtonX, 356.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::Load, "Load", SDL_FRect{kPauseButtonX, 430.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::Settings, "Settings", SDL_FRect{kPauseButtonX, 504.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::ExitToMainMenu, "Exit", SDL_FRect{kPauseButtonX, 578.0f, kPauseButtonWidth, 58.0f}}
}};

constexpr std::array<ConfirmButton, 2> kConfirmButtons{{
    {ConfirmAction::Cancel, "Stay", SDL_FRect{kPauseConfirmShellRect.x + 42.0f, kPauseConfirmShellRect.y + 202.0f, 196.0f, 58.0f}},
    {ConfirmAction::ExitToMainMenu, "Exit To Menu", SDL_FRect{kPauseConfirmShellRect.x + kPauseConfirmShellRect.w - 238.0f,
                                                              kPauseConfirmShellRect.y + 202.0f, 196.0f, 58.0f}}
}};

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

void drawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
              const SDL_Color& color, int x, int y, bool centered = false) {
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

    SDL_Rect dst{x, y, surface->w, surface->h};
    if (centered) {
        dst.x -= dst.w / 2;
    }

    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void drawShadowedText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                      const SDL_Color& color, int x, int y, bool centered = false) {
    drawText(renderer, font, text, SDL_Color{12, 18, 28, 160}, x + 3, y + 3, centered);
    drawText(renderer, font, text, color, x, y, centered);
}

void drawTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                    const SDL_Color& color, const SDL_FRect& rect, bool centerX = true) {
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
                           const SDL_Color& color, const SDL_FRect& rect, bool centerX = true) {
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
                            const SDL_Color& color, const SDL_FRect& rect, bool centerX = true) {
    drawTextInRect(renderer, font, text, SDL_Color{8, 12, 22, 150},
                   SDL_FRect{rect.x + 2.0f, rect.y + 2.0f, rect.w, rect.h}, centerX);
    drawTextInRect(renderer, font, text, color, rect, centerX);
}

void drawShadowedWrappedTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                                   const SDL_Color& color, const SDL_FRect& rect, bool centerX = true) {
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

void appendArcPoints(std::vector<SDL_FPoint>& points, float cx, float cy,
                     float radius, float startAngle, float endAngle, int segments,
                     bool skipFirst) {
    if (radius <= 0.0f) {
        return;
    }

    const int startIndex = skipFirst ? 1 : 0;
    for (int i = startIndex; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = startAngle + (endAngle - startAngle) * t;
        points.push_back(SDL_FPoint{
            cx + std::cos(angle) * radius,
            cy + std::sin(angle) * radius
        });
    }
}

std::vector<SDL_FPoint> buildRoundedRectPoints(const SDL_FRect& rect, float radius) {
    const float clampedRadius = std::min(radius, std::min(rect.w, rect.h) * 0.5f);
    if (clampedRadius <= 0.5f) {
        return {
            SDL_FPoint{rect.x, rect.y},
            SDL_FPoint{rect.x + rect.w, rect.y},
            SDL_FPoint{rect.x + rect.w, rect.y + rect.h},
            SDL_FPoint{rect.x, rect.y + rect.h}
        };
    }

    constexpr int kArcSegments = 8;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kHalfPi = kPi * 0.5f;
    std::vector<SDL_FPoint> points;
    points.reserve(static_cast<size_t>(kArcSegments * 4 + 4));

    appendArcPoints(points, rect.x + rect.w - clampedRadius, rect.y + clampedRadius,
                    clampedRadius, -kHalfPi, 0.0f, kArcSegments, false);
    appendArcPoints(points, rect.x + rect.w - clampedRadius, rect.y + rect.h - clampedRadius,
                    clampedRadius, 0.0f, kHalfPi, kArcSegments, true);
    appendArcPoints(points, rect.x + clampedRadius, rect.y + rect.h - clampedRadius,
                    clampedRadius, kHalfPi, kPi, kArcSegments, true);
    appendArcPoints(points, rect.x + clampedRadius, rect.y + clampedRadius,
                    clampedRadius, kPi, kPi * 1.5f, kArcSegments, true);

    return points;
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

void drawRoundedPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float radius, const SDL_Color& color) {
    drawPolygon(renderer, buildRoundedRectPoints(rect, radius), color);
}

void drawSlantedOutline(SDL_Renderer* renderer, const SDL_FRect& rect, float slant, const SDL_Color& color) {
    SDL_FPoint points[4];
    buildSlantedPoints(rect, slant, points);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int i = 0; i < 4; ++i) {
        const SDL_FPoint& a = points[i];
        const SDL_FPoint& b = points[(i + 1) % 4];
        SDL_RenderDrawLine(
            renderer,
            static_cast<int>(std::lround(a.x)),
            static_cast<int>(std::lround(a.y)),
            static_cast<int>(std::lround(b.x)),
            static_cast<int>(std::lround(b.y))
        );
    }
}

void drawSoftSlantedPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float slant,
                          const SDL_Color& fillColor, const SDL_Color& outlineColor,
                          const SDL_Color& shadowColor) {
    drawSlantedPanel(renderer, offsetRect(rect, 8.0f, 10.0f), slant, shadowColor);
    drawSlantedPanel(renderer, rect, slant, fillColor);
    drawSlantedPanel(renderer, insetRect(rect, 2.0f, 2.0f), std::max(0.0f, slant - 2.0f), SDL_Color{255, 255, 255, 18});
    drawSlantedOutline(renderer, rect, slant, outlineColor);
}

void drawSoftRoundedPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float radius,
                          const SDL_Color& fillColor, const SDL_Color& highlightColor,
                          const SDL_Color& shadowColor) {
    drawRoundedPanel(renderer, offsetRect(rect, 0.0f, 8.0f), radius, shadowColor);
    drawRoundedPanel(renderer, rect, radius, fillColor);
    drawRoundedPanel(renderer, insetRect(rect, 2.0f, 2.0f), std::max(0.0f, radius - 2.0f), highlightColor);
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
                          Uint8 alpha = 255, float alignX = 0.5f, float alignY = 0.5f) {
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

float menuButtonProgress(const AppState& state, size_t index) {
    const float startTime = kMenuIntroStagger * static_cast<float>(index);
    return clamp01((state.menuIntroTime - startTime) / kMenuIntroDuration);
}

SDL_FRect animatedMenuButtonRect(const AppState& state, size_t index) {
    SDL_FRect rect = kMenuButtons[index].rect;
    const float progress = easeOutBack(menuButtonProgress(state, index));
    rect.x -= (1.0f - progress) * (kMenuIntroTravel + 22.0f * static_cast<float>(index));
    return rect;
}

const MenuButton* findMenuButtonAt(const AppState& state, float x, float y) {
    for (size_t i = 0; i < kMenuButtons.size(); ++i) {
        if (pointInRect(x, y, animatedMenuButtonRect(state, i))) {
            return &kMenuButtons[i];
        }
    }
    return nullptr;
}

float pauseButtonProgress(const AppState& state, size_t index) {
    const float startTime = kPauseIntroStagger * static_cast<float>(index);
    return clamp01((state.pauseIntroTime - startTime) / kPauseIntroDuration);
}

SDL_FRect animatedPauseButtonRect(const AppState& state, size_t index) {
    SDL_FRect rect = kPauseButtons[index].rect;
    const float progress = easeOutBack(pauseButtonProgress(state, index));
    const float shellProgress = smoothstep01(state.pauseIntroTime / kPauseIntroDuration);
    rect.x -= (1.0f - progress) * (kPauseIntroTravel + 18.0f * static_cast<float>(index));
    rect.y += (1.0f - shellProgress) * 26.0f;
    return rect;
}

const PauseButton* findPauseButtonAt(const AppState& state, float x, float y) {
    for (size_t i = 0; i < kPauseButtons.size(); ++i) {
        if (pointInRect(x, y, animatedPauseButtonRect(state, i))) {
            return &kPauseButtons[i];
        }
    }
    return nullptr;
}

const ConfirmButton* findConfirmButtonAt(float x, float y) {
    for (const ConfirmButton& button : kConfirmButtons) {
        if (pointInRect(x, y, button.rect)) {
            return &button;
        }
    }
    return nullptr;
}

std::string formatPercent(float value) {
    return std::to_string(static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 100.0f))) + "%";
}

std::string formatCharsPerSecond(float value) {
    return std::to_string(static_cast<int>(std::lround(value))) + " cps";
}

std::string formatDisplayMode(bool fullscreen) {
    return fullscreen ? "Fullscreen" : "Windowed";
}

float normalizeTextSpeed(float textSpeed) {
    return std::clamp((textSpeed - kMinTextSpeed) / kTextSpeedRange, 0.0f, 1.0f);
}

float denormalizeTextSpeed(float normalizedValue) {
    return kMinTextSpeed + std::clamp(normalizedValue, 0.0f, 1.0f) * kTextSpeedRange;
}

void applyDisplayMode(Window& window, GameSettings& settings, bool fullscreen) {
    if (window.setFullscreen(fullscreen)) {
        settings.fullscreen = fullscreen;
        vn::setViewportSize(window.getWidth(), window.getHeight());
    }
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
    state.pauseSelection = 0;
    state.confirmSelection = 0;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::Playing;
    applyCurrentEntry(state.story, state.settings);
}

void adjustVoiceVolume(GameSettings& settings, int direction) {
    settings.voiceVolume = std::clamp(settings.voiceVolume + 0.05f * static_cast<float>(direction), 0.0f, 1.0f);
    vn::setVoiceVolume(settings.voiceVolume);
}

void adjustTextSpeed(GameSettings& settings, int direction) {
    settings.textSpeed = std::clamp(settings.textSpeed + 6.0f * static_cast<float>(direction), kMinTextSpeed, kMaxTextSpeed);
    vn::setTypewriterSpeed(settings.textSpeed);
}

void openPauseMenu(AppState& state) {
    state.pauseSelection = 0;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::PauseMenu;
    vn::setPaused(true);
}

void resumeStory(AppState& state) {
    vn::setPaused(false);
    state.screen = ScreenState::Playing;
}

void exitStoryToMainMenu(AppState& state) {
    vn::setPaused(false);
    vn::stopVoicePlayback();
    state.story.entryIndex = 0;
    state.pauseSelection = 0;
    state.confirmSelection = 0;
    state.settingsReturnScreen = ScreenState::MainMenu;
    state.screen = ScreenState::MainMenu;
    state.mainSelection = 0;
    state.noticeText = "Current progress was discarded.";
    state.noticeTimer = 2.6f;
}

void activatePauseAction(AppState& state, PauseAction action) {
    switch (action) {
        case PauseAction::Continue:
            resumeStory(state);
            break;
        case PauseAction::Load:
            state.noticeText = "Load is still a dummy button. No save data yet.";
            state.noticeTimer = 2.8f;
            break;
        case PauseAction::Settings:
            state.settingsSelection = 0;
            state.settingsReturnScreen = ScreenState::PauseMenu;
            state.screen = ScreenState::Settings;
            break;
        case PauseAction::ExitToMainMenu:
            state.confirmSelection = 0;
            state.screen = ScreenState::PauseConfirmExit;
            break;
    }
}

void activateConfirmAction(AppState& state, ConfirmAction action) {
    switch (action) {
        case ConfirmAction::Cancel:
            state.screen = ScreenState::PauseMenu;
            break;
        case ConfirmAction::ExitToMainMenu:
            exitStoryToMainMenu(state);
            break;
    }
}

void activateMenuAction(AppState& state, Window& window, MainMenuAction action) {
    switch (action) {
        case MainMenuAction::Start:
            beginStory(state);
            break;
        case MainMenuAction::Load:
            state.noticeText = "Load is a dummy button for now. No save data yet.";
            state.noticeTimer = 2.8f;
            break;
        case MainMenuAction::Settings:
            state.settingsSelection = 0;
            state.settingsReturnScreen = ScreenState::MainMenu;
            state.screen = ScreenState::Settings;
            break;
        case MainMenuAction::Exit:
            window.close();
            break;
    }
}

void renderMainMenuOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) {
    drawSoftRoundedPanel(renderer, kMenuTitleBackdropRect, 34.0f,
                         SDL_Color{56, 152, 210, 255},
                         SDL_Color{120, 214, 255, 255},
                         SDL_Color{10, 40, 78, 255});
    if (resources.titleLogo != nullptr) {
        renderTextureContain(renderer, resources.titleLogo,
                             SDL_FRect{kMenuTitleRect.x + 6.0f, kMenuTitleRect.y + 6.0f, kMenuTitleRect.w, kMenuTitleRect.h},
                             90, 0.0f, 0.5f);
        renderTextureContain(renderer, resources.titleLogo, kMenuTitleRect, 255, 0.0f, 0.5f);
    }

    drawSoftRoundedPanel(renderer, kMenuShellRect, 28.0f,
                         SDL_Color{52, 148, 208, 255},
                         SDL_Color{132, 220, 255, 255},
                         SDL_Color{12, 46, 84, 255});
    drawRoundedPanel(renderer, insetRect(kMenuShellRect, 10.0f, 10.0f), 22.0f,
                     SDL_Color{68, 172, 222, 255});
    drawRoundedPanel(renderer, SDL_FRect{kMenuShellRect.x + 12.0f, kMenuShellRect.y + 12.0f,
                                         kMenuShellRect.w - 24.0f, 132.0f},
                     18.0f,
                     SDL_Color{90, 190, 232, 255});
    drawSoftRoundedPanel(renderer, kFooterBandRect, 13.0f,
                         SDL_Color{34, 108, 166, 255},
                         SDL_Color{118, 210, 248, 255},
                         SDL_Color{12, 44, 80, 255});

#ifdef VN_ENABLE_TTF
    if (resources.titleLogo == nullptr) {
        drawShadowedTextInRect(renderer, resources.subtitleFont, "HATSUNE MIKU",
                               SDL_Color{244, 250, 252, 255},
                               SDL_FRect{kMenuTitleRect.x, kMenuTitleRect.y + 8.0f, kMenuTitleRect.w, 42.0f},
                               false);
        drawShadowedTextInRect(renderer, resources.titleFont, "UNDERGROUND BIT IDOL",
                               SDL_Color{18, 34, 58, 255},
                               SDL_FRect{kMenuTitleRect.x, kMenuTitleRect.y + 50.0f, kMenuTitleRect.w, 82.0f},
                               false);
    }
    drawTextInRect(renderer, resources.smallFont, "MAIN MENU",
                   SDL_Color{234, 246, 255, 255},
                   kMenuSectionLabelRect, false);
    drawTextInRect(renderer, resources.smallFont, "CLICK OR PRESS ENTER",
                   SDL_Color{234, 246, 255, 255},
                   SDL_FRect{kFooterBandRect.x + 12.0f, kFooterBandRect.y, kFooterBandRect.w - 24.0f, kFooterBandRect.h});
#endif

    const SDL_Rect buttonClipRect{
        static_cast<int>(std::lround(kMenuShellRect.x + 8.0f)),
        static_cast<int>(std::lround(kMenuButtons.front().rect.y - 8.0f)),
        static_cast<int>(std::lround(kMenuShellRect.w - 16.0f)),
        static_cast<int>(std::lround((kMenuButtons.back().rect.y + kMenuButtons.back().rect.h) - (kMenuButtons.front().rect.y - 8.0f) + 8.0f))
    };
    SDL_RenderSetClipRect(renderer, &buttonClipRect);

    for (size_t i = 0; i < kMenuButtons.size(); ++i) {
        const MenuButton& button = kMenuButtons[i];
        const bool selected = static_cast<int>(button.action) == state.mainSelection;
        const float rawProgress = menuButtonProgress(state, i);
        const float visibleProgress = smoothstep01(rawProgress);
        if (visibleProgress <= 0.0f) {
            continue;
        }

        const SDL_FRect buttonRect = animatedMenuButtonRect(state, i);
        drawSoftRoundedPanel(renderer, buttonRect, buttonRect.h * 0.5f,
                             selected ? SDL_Color{108, 196, 242, 255} : SDL_Color{42, 126, 188, 255},
                             selected ? SDL_Color{224, 246, 255, 255} : SDL_Color{108, 198, 242, 255},
                             selected ? SDL_Color{16, 56, 96, 255} : SDL_Color{10, 40, 72, 255});

        if (selected) {
            const SDL_FRect accentRect{buttonRect.x + 10.0f, buttonRect.y + 9.0f, 72.0f, buttonRect.h - 18.0f};
            drawRoundedPanel(renderer, accentRect, accentRect.h * 0.5f,
                             SDL_Color{196, 236, 255, 255});
        }

#ifdef VN_ENABLE_TTF
        const SDL_FRect labelRect{buttonRect.x + 24.0f, buttonRect.y, buttonRect.w - 48.0f, buttonRect.h};
        const SDL_Color textColor = selected ? SDL_Color{16, 42, 68, 255} : SDL_Color{240, 248, 255, 255};
        drawShadowedTextInRect(renderer, resources.itemFont, button.label, textColor, labelRect);
#endif
    }

    SDL_RenderSetClipRect(renderer, nullptr);

    if (state.noticeTimer > 0.0f && !state.noticeText.empty()) {
        drawSoftSlantedPanel(renderer, kNoticeRect, 32.0f,
                             SDL_Color{15, 23, 41, 210},
                             SDL_Color{72, 220, 224, 210},
                             SDL_Color{6, 10, 18, 60});
#ifdef VN_ENABLE_TTF
        drawTextInRect(renderer, resources.itemFont, state.noticeText, SDL_Color{240, 246, 250, 255},
                       SDL_FRect{kNoticeRect.x + 20.0f, kNoticeRect.y, kNoticeRect.w - 40.0f, kNoticeRect.h});
#endif
    }
}

void renderMainMenu(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                    int windowWidth, int windowHeight) {
    renderBackgroundCover(renderer, resources.background, windowWidth, windowHeight);

    const bool usingCanvas = beginMenuCanvas(renderer, resources.menuCanvas);
    renderMainMenuOverlay(renderer, resources, state);
    if (usingCanvas) {
        endMenuCanvas(renderer, resources.menuCanvas, windowWidth, windowHeight);
    }
}

void renderSettingsRowBase(SDL_Renderer* renderer, const SDL_FRect& rect, bool selected) {
    drawSoftRoundedPanel(renderer, rect, rect.h * 0.5f,
                         selected ? SDL_Color{108, 196, 242, 255} : SDL_Color{42, 126, 188, 255},
                         selected ? SDL_Color{224, 246, 255, 255} : SDL_Color{108, 198, 242, 255},
                         selected ? SDL_Color{16, 56, 96, 255} : SDL_Color{10, 40, 72, 255});

    if (selected) {
        const SDL_FRect accentRect{rect.x + 10.0f, rect.y + 9.0f, 72.0f, rect.h - 18.0f};
        drawRoundedPanel(renderer, accentRect, accentRect.h * 0.5f, SDL_Color{196, 236, 255, 255});
    }
}

void renderSettingsValueBadge(SDL_Renderer* renderer, const SDL_FRect& rect, bool selected) {
    drawSoftRoundedPanel(renderer, rect, rect.h * 0.5f,
                         selected ? SDL_Color{194, 234, 255, 255} : SDL_Color{70, 164, 218, 255},
                         selected ? SDL_Color{236, 249, 255, 255} : SDL_Color{136, 216, 249, 255},
                         selected ? SDL_Color{18, 58, 98, 255} : SDL_Color{10, 40, 72, 255});
}

void renderSlider(SDL_Renderer* renderer, const SDL_FRect& rect, float value, bool selected) {
    const float clampedValue = std::clamp(value, 0.0f, 1.0f);
    drawRoundedPanel(renderer, offsetRect(rect, 0.0f, 5.0f), rect.h * 0.5f, SDL_Color{10, 40, 72, 180});
    drawRoundedPanel(renderer, rect, rect.h * 0.5f,
                     selected ? SDL_Color{72, 182, 228, 255} : SDL_Color{48, 132, 190, 255});

    const SDL_FRect fill{rect.x + 4.0f, rect.y + 4.0f, std::max(0.0f, (rect.w - 8.0f) * clampedValue), rect.h - 8.0f};
    if (fill.w > 0.0f) {
        drawRoundedPanel(renderer, fill, fill.h * 0.5f, SDL_Color{212, 244, 255, 255});
    }

    const SDL_FRect knob{
        rect.x + 8.0f + (rect.w - 32.0f) * clampedValue,
        rect.y - 2.0f,
        24.0f,
        rect.h + 4.0f
    };
    drawSoftRoundedPanel(renderer, knob, knob.w * 0.5f,
                         SDL_Color{194, 234, 255, 255},
                         SDL_Color{240, 248, 255, 255},
                         SDL_Color{14, 48, 84, 255});
}

void renderSettingsOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) {
    drawSoftRoundedPanel(renderer, kSettingsTitleBackdropRect, 32.0f,
                         SDL_Color{56, 152, 210, 255},
                         SDL_Color{120, 214, 255, 255},
                         SDL_Color{10, 40, 78, 255});
    drawSoftRoundedPanel(renderer, kSettingsShellRect, 30.0f,
                         SDL_Color{52, 148, 208, 255},
                         SDL_Color{132, 220, 255, 255},
                         SDL_Color{12, 46, 84, 255});
    drawRoundedPanel(renderer, insetRect(kSettingsShellRect, 10.0f, 10.0f), 24.0f, SDL_Color{68, 172, 222, 255});
    drawRoundedPanel(renderer, kSettingsHeaderBandRect, 18.0f, SDL_Color{90, 190, 232, 255});
    drawSoftRoundedPanel(renderer, kSettingsFooterBandRect, 13.0f,
                         SDL_Color{34, 108, 166, 255},
                         SDL_Color{118, 210, 248, 255},
                         SDL_Color{12, 44, 80, 255});

#ifdef VN_ENABLE_TTF
    drawTextInRect(renderer, resources.smallFont, "SETTINGS",
                   SDL_Color{234, 246, 255, 255}, kSettingsSectionLabelRect, false);
    drawTextInRect(renderer, resources.smallFont, "ARROW KEYS OR MOUSE TO ADJUST LIVE VN BEHAVIOR",
                   SDL_Color{218, 240, 252, 255}, kSettingsHintRect, false);
    drawShadowedTextInRect(renderer, resources.titleFont, "System Settings",
                           SDL_Color{240, 248, 255, 255}, kSettingsTitleRect);
    drawTextInRect(renderer, resources.tinyFont != nullptr ? resources.tinyFont : resources.smallFont,
                   "ENTER TO TOGGLE DISPLAY MODE. DRAG OR TAP THE SLIDERS FOR VOICE AND TEXT SPEED.",
                   SDL_Color{234, 246, 255, 255},
                   SDL_FRect{kSettingsFooterBandRect.x + 12.0f, kSettingsFooterBandRect.y,
                             kSettingsFooterBandRect.w - 24.0f, kSettingsFooterBandRect.h});
#endif

    const bool displaySelected = state.settingsSelection == 0;
    const bool volumeSelected = state.settingsSelection == 1;
    const bool speedSelected = state.settingsSelection == 2;
    const bool backSelected = state.settingsSelection == 3;

    renderSettingsRowBase(renderer, kDisplayModeRowRect, displaySelected);
    renderSettingsRowBase(renderer, kVolumeRowRect, volumeSelected);
    renderSettingsRowBase(renderer, kSpeedRowRect, speedSelected);
    renderSettingsRowBase(renderer, kBackRowRect, backSelected);
    renderSettingsValueBadge(renderer, kDisplayModeValueRect, displaySelected);
    renderSettingsValueBadge(renderer, kVolumeValueRect, volumeSelected);
    renderSettingsValueBadge(renderer, kSpeedValueRect, speedSelected);

#ifdef VN_ENABLE_TTF
    const SDL_Color labelColor = SDL_Color{240, 248, 255, 255};
    const SDL_Color selectedLabelColor = SDL_Color{16, 42, 68, 255};
    drawShadowedTextInRect(renderer, resources.itemFont, "Display Mode",
                           displaySelected ? selectedLabelColor : labelColor,
                           SDL_FRect{kDisplayModeRowRect.x + 96.0f, kDisplayModeRowRect.y, 260.0f, kDisplayModeRowRect.h},
                           false);
    drawShadowedTextInRect(renderer, resources.itemFont, "Voice Volume",
                           volumeSelected ? selectedLabelColor : labelColor,
                           SDL_FRect{kVolumeRowRect.x + 96.0f, kVolumeRowRect.y, 250.0f, kVolumeRowRect.h},
                           false);
    drawShadowedTextInRect(renderer, resources.itemFont, "Text Speed",
                           speedSelected ? selectedLabelColor : labelColor,
                           SDL_FRect{kSpeedRowRect.x + 96.0f, kSpeedRowRect.y, 220.0f, kSpeedRowRect.h},
                           false);
    drawShadowedTextInRect(renderer, resources.itemFont, "Back",
                           backSelected ? selectedLabelColor : labelColor,
                           SDL_FRect{kBackRowRect.x + 96.0f, kBackRowRect.y, 160.0f, kBackRowRect.h},
                           false);

    drawTextInRect(renderer, resources.smallFont, formatDisplayMode(state.settings.fullscreen),
                   displaySelected ? SDL_Color{16, 42, 68, 255} : SDL_Color{240, 248, 255, 255},
                   kDisplayModeValueRect);
    drawTextInRect(renderer, resources.smallFont, formatPercent(state.settings.voiceVolume),
                   volumeSelected ? SDL_Color{16, 42, 68, 255} : SDL_Color{240, 248, 255, 255},
                   kVolumeValueRect);
    drawTextInRect(renderer, resources.smallFont, formatCharsPerSecond(state.settings.textSpeed),
                   speedSelected ? SDL_Color{16, 42, 68, 255} : SDL_Color{240, 248, 255, 255},
                   kSpeedValueRect);
#endif

    renderSlider(renderer, kVolumeSliderRect, state.settings.voiceVolume, volumeSelected);
    renderSlider(renderer, kSpeedSliderRect, normalizeTextSpeed(state.settings.textSpeed), speedSelected);
}

void renderSettingsMenu(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                        int windowWidth, int windowHeight) {
    renderBackgroundCover(renderer, resources.background, windowWidth, windowHeight);

    const bool usingReferenceLayout = beginReferenceLayout(renderer, windowWidth, windowHeight);
    renderSettingsOverlay(renderer, resources, state);
    if (usingReferenceLayout) {
        endReferenceLayout(renderer);
    }
}

void renderPauseBackdrop(SDL_Renderer* renderer, int windowWidth, int windowHeight) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 6, 10, 14, 160);
    SDL_Rect fullRect{0, 0, windowWidth, windowHeight};
    SDL_RenderFillRect(renderer, &fullRect);

    SDL_SetRenderDrawColor(renderer, 20, 30, 34, 84);
    SDL_Rect topBand{0, 0, windowWidth, windowHeight / 4};
    SDL_Rect bottomBand{0, windowHeight - windowHeight / 4, windowWidth, windowHeight / 4};
    SDL_RenderFillRect(renderer, &topBand);
    SDL_RenderFillRect(renderer, &bottomBand);
}

void renderPauseMenuOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) {
    const float shellProgress = smoothstep01(state.pauseIntroTime / kPauseIntroDuration);
    const float shellOffset = (1.0f - shellProgress) * 26.0f;

    const SDL_FRect titleBackdrop = offsetRect(kPauseTitleBackdropRect, 0.0f, shellOffset);
    const SDL_FRect titleRect = offsetRect(kPauseTitleRect, 0.0f, shellOffset);
    const SDL_FRect shellRect = offsetRect(kPauseShellRect, 0.0f, shellOffset);
    const SDL_FRect headerBandRect = offsetRect(kPauseHeaderBandRect, 0.0f, shellOffset);
    const SDL_FRect sectionLabelRect = offsetRect(kPauseSectionLabelRect, 0.0f, shellOffset);
    const SDL_FRect hintRect = offsetRect(kPauseHintRect, 0.0f, shellOffset);

    drawSoftRoundedPanel(renderer, titleBackdrop, 34.0f,
                         SDL_Color{56, 152, 210, 242},
                         SDL_Color{120, 214, 255, 250},
                         SDL_Color{10, 40, 78, 210});
    drawSoftRoundedPanel(renderer, shellRect, 30.0f,
                         SDL_Color{52, 148, 208, 236},
                         SDL_Color{132, 220, 255, 245},
                         SDL_Color{12, 46, 84, 220});
    drawRoundedPanel(renderer, insetRect(shellRect, 10.0f, 10.0f), 24.0f, SDL_Color{68, 172, 222, 238});
    drawRoundedPanel(renderer, headerBandRect, 18.0f, SDL_Color{90, 190, 232, 242});

#ifdef VN_ENABLE_TTF
    drawShadowedTextInRect(renderer, resources.titleFont, "Paused",
                           SDL_Color{240, 248, 255, 255},
                           titleRect);
    drawTextInRect(renderer, resources.smallFont, "PAUSE MENU",
                   SDL_Color{234, 246, 255, 255},
                   sectionLabelRect, false);
    drawShadowedWrappedTextInRect(renderer, resources.smallFont, "THE STORY IS FROZEN UNTIL YOU CONTINUE",
                                  SDL_Color{224, 242, 252, 255},
                                  hintRect, false);
#endif

    for (size_t i = 0; i < kPauseButtons.size(); ++i) {
        const PauseButton& button = kPauseButtons[i];
        const bool selected = static_cast<int>(button.action) == state.pauseSelection;
        const float rawProgress = pauseButtonProgress(state, i);
        const float visibleProgress = smoothstep01(rawProgress);
        if (visibleProgress <= 0.0f) {
            continue;
        }

        const SDL_FRect buttonRect = animatedPauseButtonRect(state, i);
        drawSoftRoundedPanel(renderer, buttonRect, buttonRect.h * 0.5f,
                             selected ? SDL_Color{210, 236, 249, 255} : SDL_Color{84, 182, 228, 244},
                             selected ? SDL_Color{244, 250, 255, 255} : SDL_Color{140, 220, 252, 240},
                             selected ? SDL_Color{16, 56, 96, 255} : SDL_Color{10, 40, 72, 220});

        if (selected) {
            const SDL_FRect accentRect{buttonRect.x + 12.0f, buttonRect.y + 9.0f, 82.0f, buttonRect.h - 18.0f};
            drawRoundedPanel(renderer, accentRect, accentRect.h * 0.5f, SDL_Color{194, 226, 246, 255});
        }

#ifdef VN_ENABLE_TTF
        drawShadowedTextInRect(renderer, resources.itemFont, button.label,
                               selected ? SDL_Color{16, 42, 68, 255} : SDL_Color{240, 248, 255, 255},
                               SDL_FRect{buttonRect.x + 24.0f, buttonRect.y, buttonRect.w - 48.0f, buttonRect.h});
#endif
    }

    if (state.noticeTimer > 0.0f && !state.noticeText.empty()) {
        const SDL_FRect noticeRect = offsetRect(kPauseNoticeRect, 0.0f, shellOffset);
        drawSoftRoundedPanel(renderer, noticeRect, noticeRect.h * 0.5f,
                             SDL_Color{34, 108, 166, 236},
                             SDL_Color{118, 210, 248, 245},
                             SDL_Color{12, 44, 80, 200});
#ifdef VN_ENABLE_TTF
        drawTextInRect(renderer, resources.smallFont, state.noticeText,
                       SDL_Color{240, 246, 250, 255},
                       SDL_FRect{noticeRect.x + 14.0f, noticeRect.y, noticeRect.w - 28.0f, noticeRect.h});
#endif
    }
}

void renderPauseConfirmOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) {
    drawSoftRoundedPanel(renderer, kPauseConfirmShellRect, 26.0f,
                         SDL_Color{52, 148, 208, 246},
                         SDL_Color{132, 220, 255, 250},
                         SDL_Color{12, 46, 84, 220});
    drawRoundedPanel(renderer, insetRect(kPauseConfirmShellRect, 10.0f, 10.0f), 20.0f,
                     SDL_Color{88, 190, 232, 244});
    drawSoftRoundedPanel(renderer, kPauseConfirmFooterBandRect, 8.0f,
                         SDL_Color{110, 214, 247, 255},
                         SDL_Color{214, 242, 255, 255},
                         SDL_Color{10, 40, 72, 180});

#ifdef VN_ENABLE_TTF
    drawShadowedTextInRect(renderer, resources.itemFont, "Exit To Main Menu?",
                           SDL_Color{240, 248, 255, 255},
                           kPauseConfirmTitleRect);
    drawShadowedWrappedTextInRect(renderer, resources.smallFont,
                                  "You will lose the current chapter progress\nif you leave now.",
                                  SDL_Color{236, 246, 252, 255},
                                  kPauseConfirmBodyRect);
#endif

    for (const ConfirmButton& button : kConfirmButtons) {
        const bool selected = static_cast<int>(button.action) == state.confirmSelection;
        drawSoftRoundedPanel(renderer, button.rect, button.rect.h * 0.5f,
                             selected ? SDL_Color{210, 236, 249, 255} : SDL_Color{84, 182, 228, 244},
                             selected ? SDL_Color{244, 250, 255, 255} : SDL_Color{140, 220, 252, 240},
                             selected ? SDL_Color{16, 56, 96, 255} : SDL_Color{10, 40, 72, 220});
#ifdef VN_ENABLE_TTF
        if (selected) {
            const SDL_FRect accentRect{button.rect.x + 12.0f, button.rect.y + 9.0f, 60.0f, button.rect.h - 18.0f};
            drawRoundedPanel(renderer, accentRect, accentRect.h * 0.5f, SDL_Color{194, 226, 246, 255});
        }
        drawShadowedTextInRect(renderer, resources.smallFont, button.label,
                               selected ? SDL_Color{16, 42, 68, 255} : SDL_Color{240, 248, 255, 255},
                               button.rect);
#endif
    }
}

void renderPausedSettingsMenu(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                              int windowWidth, int windowHeight) {
    renderPauseBackdrop(renderer, windowWidth, windowHeight);

    const bool usingReferenceLayout = beginReferenceLayout(renderer, windowWidth, windowHeight);
    renderSettingsOverlay(renderer, resources, state);
    if (usingReferenceLayout) {
        endReferenceLayout(renderer);
    }
}

void handleMainMenuMouse(AppState& state, const SDL_Event& event, int windowWidth, int windowHeight) {
    if (event.type == SDL_MOUSEMOTION) {
        SDL_FPoint menuPoint{};
        if (!mapWindowPointToReference(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y),
                                       windowWidth, windowHeight, menuPoint)) {
            return;
        }

        if (const MenuButton* button = findMenuButtonAt(state, menuPoint.x, menuPoint.y)) {
            state.mainSelection = static_cast<int>(button->action);
        }
    }
}

void updateSettingsSelectionFromMouse(AppState& state, float mouseX, float mouseY) {
    if (pointInRect(mouseX, mouseY, kDisplayModeRowRect)) {
        state.settingsSelection = 0;
    } else if (pointInRect(mouseX, mouseY, kVolumeRowRect)) {
        state.settingsSelection = 1;
    } else if (pointInRect(mouseX, mouseY, kSpeedRowRect)) {
        state.settingsSelection = 2;
    } else if (pointInRect(mouseX, mouseY, kBackRowRect)) {
        state.settingsSelection = 3;
    }
}

void updateSettingsFromPointer(AppState& state, Window& window, float mouseX, float mouseY, bool allowNavigation) {
    updateSettingsSelectionFromMouse(state, mouseX, mouseY);

    if (pointInRect(mouseX, mouseY, kDisplayModeRowRect) && allowNavigation) {
        applyDisplayMode(window, state.settings, !state.settings.fullscreen);
    } else if (pointInRect(mouseX, mouseY, kVolumeSliderRect)) {
        state.settings.voiceVolume = std::clamp((mouseX - kVolumeSliderRect.x) / kVolumeSliderRect.w, 0.0f, 1.0f);
        vn::setVoiceVolume(state.settings.voiceVolume);
    } else if (pointInRect(mouseX, mouseY, kSpeedSliderRect)) {
        state.settings.textSpeed = denormalizeTextSpeed((mouseX - kSpeedSliderRect.x) / kSpeedSliderRect.w);
        vn::setTypewriterSpeed(state.settings.textSpeed);
    } else if (allowNavigation && pointInRect(mouseX, mouseY, kBackRowRect)) {
        state.screen = state.settingsReturnScreen;
    }
}

void handleSettingsMouse(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight) {
    if (event.type == SDL_MOUSEMOTION) {
        SDL_FPoint settingsPoint{};
        if (!mapWindowPointToReference(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y),
                                       windowWidth, windowHeight, settingsPoint)) {
            return;
        }
        updateSettingsSelectionFromMouse(state, settingsPoint.x, settingsPoint.y);
        if ((event.motion.state & SDL_BUTTON_LMASK) != 0U) {
            updateSettingsFromPointer(state, window, settingsPoint.x, settingsPoint.y, false);
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        SDL_FPoint settingsPoint{};
        if (!mapWindowPointToReference(static_cast<float>(event.button.x), static_cast<float>(event.button.y),
                                       windowWidth, windowHeight, settingsPoint)) {
            return;
        }
        updateSettingsFromPointer(state, window, settingsPoint.x, settingsPoint.y, true);
    }
}

void handlePauseMenuMouse(AppState& state, const SDL_Event& event, int windowWidth, int windowHeight) {
    if (event.type != SDL_MOUSEMOTION &&
        !(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
        return;
    }

    SDL_FPoint pausePoint{};
    const bool insideOverlay = mapWindowPointToReference(
        static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x),
        static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y),
        windowWidth,
        windowHeight,
        pausePoint
    );
    if (!insideOverlay) {
        return;
    }

    if (const PauseButton* button = findPauseButtonAt(state, pausePoint.x, pausePoint.y)) {
        state.pauseSelection = static_cast<int>(button->action);
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            activatePauseAction(state, button->action);
        }
    }
}

void handlePauseConfirmMouse(AppState& state, const SDL_Event& event, int windowWidth, int windowHeight) {
    if (event.type != SDL_MOUSEMOTION &&
        !(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
        return;
    }

    SDL_FPoint confirmPoint{};
    const bool insideOverlay = mapWindowPointToReference(
        static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x),
        static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y),
        windowWidth,
        windowHeight,
        confirmPoint
    );
    if (!insideOverlay) {
        return;
    }

    if (const ConfirmButton* button = findConfirmButtonAt(confirmPoint.x, confirmPoint.y)) {
        state.confirmSelection = static_cast<int>(button->action);
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            activateConfirmAction(state, button->action);
        }
    }
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

} // namespace

int main() {
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
    menuResources.background = loadTexture(window.getRenderer(), resolvePath(kMainMenuArtPath));
    menuResources.menuCanvas = createRenderTarget(window.getRenderer(), kReferenceWidth, kReferenceHeight, kMenuCanvasScale);
    menuResources.titleLogo = loadTexture(window.getRenderer(), resolvePath(kMainMenuTitlePath));

#ifdef VN_ENABLE_TTF
    menuResources.titleFont = openBestAvailableFont({}, 52);
    menuResources.subtitleFont = openBestAvailableFont({}, 28);
    menuResources.itemFont = openBestAvailableFont({}, 30);
    menuResources.smallFont = openBestAvailableFont({}, 18);
    menuResources.tinyFont = openBestAvailableFont({}, 14);
    if (menuResources.titleFont == nullptr || menuResources.itemFont == nullptr || menuResources.smallFont == nullptr) {
        std::cerr << "Menu font loading failed; UI text may be missing.\n";
    }
#endif

    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (window.isOpen()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            window.handleEvent(event);

            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESIZED)) {
                vn::setViewportSize(window.getWidth(), window.getHeight());
            }

            if (event.type == SDL_KEYDOWN && event.key.repeat != 0) {
                continue;
            }

            switch (state.screen) {
                case ScreenState::MainMenu:
                    handleMainMenuMouse(state, event, window.getWidth(), window.getHeight());

                    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                        SDL_FPoint menuPoint{};
                        const bool insideMenu = mapWindowPointToReference(
                            static_cast<float>(event.button.x),
                            static_cast<float>(event.button.y),
                            window.getWidth(),
                            window.getHeight(),
                            menuPoint
                        );
                        const MenuButton* button = insideMenu ? findMenuButtonAt(state, menuPoint.x, menuPoint.y) : nullptr;
                        if (button != nullptr) {
                            state.mainSelection = static_cast<int>(button->action);
                            activateMenuAction(state, window, button->action);
                        }
                    } else if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
                            state.mainSelection = (state.mainSelection + 3) % 4;
                        } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
                            state.mainSelection = (state.mainSelection + 1) % 4;
                        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                                   event.key.keysym.sym == SDLK_SPACE) {
                            activateMenuAction(
                                state,
                                window,
                                static_cast<MainMenuAction>(state.mainSelection)
                            );
                        }
                    }
                    break;

                case ScreenState::Settings:
                    handleSettingsMouse(state, window, event, window.getWidth(), window.getHeight());

                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            state.screen = state.settingsReturnScreen;
                        } else if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
                            state.settingsSelection = (state.settingsSelection + 3) % 4;
                        } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
                            state.settingsSelection = (state.settingsSelection + 1) % 4;
                        } else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) {
                            if (state.settingsSelection == 0) {
                                applyDisplayMode(window, state.settings, false);
                            } else if (state.settingsSelection == 1) {
                                adjustVoiceVolume(state.settings, -1);
                            } else if (state.settingsSelection == 2) {
                                adjustTextSpeed(state.settings, -1);
                            }
                        } else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) {
                            if (state.settingsSelection == 0) {
                                applyDisplayMode(window, state.settings, true);
                            } else if (state.settingsSelection == 1) {
                                adjustVoiceVolume(state.settings, 1);
                            } else if (state.settingsSelection == 2) {
                                adjustTextSpeed(state.settings, 1);
                            }
                        } else if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                                    event.key.keysym.sym == SDLK_SPACE)) {
                            if (state.settingsSelection == 0) {
                                applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                            } else if (state.settingsSelection == 3) {
                                state.screen = state.settingsReturnScreen;
                            }
                        }
                    }
                    break;

                case ScreenState::Playing:
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            openPauseMenu(state);
                        } else if (event.key.keysym.sym == SDLK_F11) {
                            applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        } else if (event.key.keysym.sym == SDLK_SPACE) {
                            vn::onSpacePressed();
                        }
                    }
                    break;

                case ScreenState::PauseMenu:
                    handlePauseMenuMouse(state, event, window.getWidth(), window.getHeight());

                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
                            state.pauseSelection = (state.pauseSelection + 3) % 4;
                        } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
                            state.pauseSelection = (state.pauseSelection + 1) % 4;
                        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                                   event.key.keysym.sym == SDLK_SPACE) {
                            activatePauseAction(state, static_cast<PauseAction>(state.pauseSelection));
                        } else if (event.key.keysym.sym == SDLK_F11) {
                            applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        }
                    }
                    break;

                case ScreenState::PauseConfirmExit:
                    handlePauseConfirmMouse(state, event, window.getWidth(), window.getHeight());

                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a ||
                            event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
                            state.confirmSelection = 0;
                        } else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d ||
                                   event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
                            state.confirmSelection = 1;
                        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                                   event.key.keysym.sym == SDLK_SPACE) {
                            activateConfirmAction(state, static_cast<ConfirmAction>(state.confirmSelection));
                        } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                            state.screen = ScreenState::PauseMenu;
                        } else if (event.key.keysym.sym == SDLK_F11) {
                            applyDisplayMode(window, state.settings, !state.settings.fullscreen);
                        }
                    }
                    break;
            }
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

        if (state.screen == ScreenState::Playing) {
            vn::update(deltaSeconds);

            if (vn::consumeAdvanceRequest()) {
                state.story.entryIndex++;
                if (state.story.entryIndex >= state.story.script.entries.size()) {
                    state.screen = ScreenState::MainMenu;
                    state.mainSelection = 0;
                    state.noticeText = "End of chapter 0.";
                    state.noticeTimer = 2.2f;
                } else {
                    applyCurrentEntry(state.story, state.settings);
                }
            }
        }

        window.clear(14, 18, 30, 255);

        const bool renderStoryBackdrop =
            state.screen == ScreenState::Playing ||
            state.screen == ScreenState::PauseMenu ||
            state.screen == ScreenState::PauseConfirmExit ||
            (state.screen == ScreenState::Settings && state.settingsReturnScreen == ScreenState::PauseMenu);

        if (renderStoryBackdrop) {
            vn::render();
            if (state.screen == ScreenState::PauseMenu || state.screen == ScreenState::PauseConfirmExit) {
                renderPauseBackdrop(window.getRenderer(), window.getWidth(), window.getHeight());
                const bool usingReferenceLayout = beginReferenceLayout(window.getRenderer(), window.getWidth(), window.getHeight());
                renderPauseMenuOverlay(window.getRenderer(), menuResources, state);
                if (state.screen == ScreenState::PauseConfirmExit) {
                    renderPauseConfirmOverlay(window.getRenderer(), menuResources, state);
                }
                if (usingReferenceLayout) {
                    endReferenceLayout(window.getRenderer());
                }
            } else if (state.screen == ScreenState::Settings && state.settingsReturnScreen == ScreenState::PauseMenu) {
                renderPausedSettingsMenu(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
            }
        } else if (state.screen == ScreenState::Settings) {
            renderSettingsMenu(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
        } else {
            renderMainMenu(window.getRenderer(), menuResources, state, window.getWidth(), window.getHeight());
        }

        window.present();
    }

    destroyMenuResources(menuResources);
    vn::shutdown();
    return 0;
}
