#include "battle_party_setup_ui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "../../platform/path_resolution.h"

namespace battle::demo::ui {

namespace {

constexpr float kOuterMargin = 34.0f;
constexpr float kTopMargin = 28.0f;
constexpr float kPanelGap = 32.0f;
constexpr float kRosterTop = 92.0f;
constexpr float kRosterWidth = 416.0f;
constexpr float kRosterHeight = 544.0f;
constexpr float kRosterInnerPadding = 18.0f;
constexpr float kRosterHeaderHeight = 76.0f;
constexpr float kRosterRowHeight = 88.0f;
constexpr float kRosterRowGap = 12.0f;
constexpr float kScrollbarWidth = 12.0f;
constexpr float kScrollbarGutter = 24.0f;
constexpr float kStagePanelTop = 168.0f;
constexpr float kStagePanelHeight = 430.0f;
constexpr float kStagePanelPadding = 18.0f;
constexpr float kStageCardGapX = 18.0f;
constexpr float kStageCardGapY = 18.0f;
constexpr float kSelectedCountGap = 14.0f;
constexpr float kStartButtonWidth = 328.0f;
constexpr float kStartButtonHeight = 68.0f;

#ifdef BATTLE_ENABLE_TTF
bool containsNonAscii(const std::string& text) {
    for (unsigned char c : text) {
        if (c >= 0x80u) {
            return true;
        }
    }
    return false;
}

float computeFontBaselineOffset(TTF_Font* referenceFont, TTF_Font* activeFont) {
    if (referenceFont == nullptr || activeFont == nullptr || referenceFont == activeFont) {
        return 0.0f;
    }
    return static_cast<float>(TTF_FontAscent(referenceFont) - TTF_FontAscent(activeFont));
}
#endif

} // namespace

LayoutMetrics computeLayout(int windowWidth, int windowHeight) {
    const float scale = std::min(
        static_cast<float>(windowWidth) / kReferenceWidth,
        static_cast<float>(windowHeight) / kReferenceHeight
    );
    return LayoutMetrics{
        scale,
        (static_cast<float>(windowWidth) - (kReferenceWidth * scale)) * 0.5f,
        (static_cast<float>(windowHeight) - (kReferenceHeight * scale)) * 0.5f
    };
}

PartySetupGeometry computeGeometry() {
    PartySetupGeometry geometry;

    geometry.rosterPanel = SDL_FRect{kOuterMargin, kRosterTop, kRosterWidth, kRosterHeight};
    geometry.rosterTitleRect = SDL_FRect{
        geometry.rosterPanel.x + kRosterInnerPadding,
        geometry.rosterPanel.y + kRosterInnerPadding,
        geometry.rosterPanel.w - (kRosterInnerPadding * 2.0f),
        28.0f
    };
    geometry.rosterHintRect = SDL_FRect{
        geometry.rosterPanel.x + kRosterInnerPadding,
        geometry.rosterPanel.y + kRosterInnerPadding + 30.0f,
        geometry.rosterPanel.w - (kRosterInnerPadding * 2.0f),
        24.0f
    };

    const float rosterTrackHeight = (kVisibleRosterRows * kRosterRowHeight) + ((kVisibleRosterRows - 1) * kRosterRowGap);
    geometry.rosterTrackRect = SDL_FRect{
        geometry.rosterPanel.x + geometry.rosterPanel.w - kRosterInnerPadding - kScrollbarWidth,
        geometry.rosterPanel.y + kRosterInnerPadding + kRosterHeaderHeight,
        kScrollbarWidth,
        rosterTrackHeight
    };

    const float stageLeft = geometry.rosterPanel.x + geometry.rosterPanel.w + kPanelGap;
    const float stageWidth = kReferenceWidth - kOuterMargin - stageLeft;
    geometry.stageTitleRect = SDL_FRect{stageLeft, kTopMargin + 2.0f, stageWidth - 220.0f, 52.0f};
    geometry.stageKickerRect = SDL_FRect{stageLeft, kTopMargin + 58.0f, 360.0f, 26.0f};
    geometry.stageDescriptionRect = SDL_FRect{stageLeft, kTopMargin + 86.0f, stageWidth - 8.0f, 56.0f};
    geometry.stagePanel = SDL_FRect{stageLeft, kStagePanelTop, stageWidth, kStagePanelHeight};
    geometry.rosterPanel.h = (geometry.stagePanel.y + geometry.stagePanel.h) - geometry.rosterPanel.y;
    geometry.selectedCountRect = SDL_FRect{
        geometry.stagePanel.x + geometry.stagePanel.w - kStartButtonWidth,
        geometry.stagePanel.y + geometry.stagePanel.h + kSelectedCountGap,
        kStartButtonWidth,
        18.0f
    };
    geometry.startButton = SDL_FRect{
        geometry.stagePanel.x + geometry.stagePanel.w - kStartButtonWidth,
        geometry.selectedCountRect.y + geometry.selectedCountRect.h + 10.0f,
        kStartButtonWidth,
        kStartButtonHeight
    };

    return geometry;
}

SDL_FRect rosterRowRect(const PartySetupGeometry& geometry, int visibleIndex) {
    const float left = geometry.rosterPanel.x + kRosterInnerPadding;
    const float top = geometry.rosterPanel.y + kRosterInnerPadding + kRosterHeaderHeight +
                      visibleIndex * (kRosterRowHeight + kRosterRowGap);
    const float width = geometry.rosterTrackRect.x - left - kScrollbarGutter;
    return SDL_FRect{left, top, width, kRosterRowHeight};
}

SDL_FRect rosterThumbRect(const PartySetupGeometry& geometry, int rosterCount, int scrollOffset) {
    const float trackHeight = geometry.rosterTrackRect.h;
    const float thumbHeight = std::max(
        42.0f,
        trackHeight * (static_cast<float>(kVisibleRosterRows) / std::max(1.0f, static_cast<float>(rosterCount)))
    );
    const float maxThumbTravel = std::max(0.0f, trackHeight - thumbHeight);
    const float scrollRatio = rosterCount <= kVisibleRosterRows
        ? 0.0f
        : static_cast<float>(scrollOffset) / static_cast<float>(rosterCount - kVisibleRosterRows);
    return SDL_FRect{
        geometry.rosterTrackRect.x - 2.0f,
        geometry.rosterTrackRect.y + maxThumbTravel * scrollRatio,
        geometry.rosterTrackRect.w + 4.0f,
        thumbHeight
    };
}

SDL_FRect selectedCardRect(const PartySetupGeometry& geometry, int slotIndex) {
    const float innerWidth = geometry.stagePanel.w - (kStagePanelPadding * 2.0f);
    const float innerHeight = geometry.stagePanel.h - (kStagePanelPadding * 2.0f);
    const float cardWidth = (innerWidth - kStageCardGapX) * 0.5f;
    const float cardHeight = (innerHeight - kStageCardGapY) * 0.5f;
    const int column = slotIndex % 2;
    const int row = slotIndex / 2;
    return SDL_FRect{
        geometry.stagePanel.x + kStagePanelPadding + column * (cardWidth + kStageCardGapX),
        geometry.stagePanel.y + kStagePanelPadding + row * (cardHeight + kStageCardGapY),
        cardWidth,
        cardHeight
    };
}

SDL_FRect toWindowRect(const SDL_FRect& referenceRect, const LayoutMetrics& layout) {
    return SDL_FRect{
        layout.originX + referenceRect.x * layout.scale,
        layout.originY + referenceRect.y * layout.scale,
        referenceRect.w * layout.scale,
        referenceRect.h * layout.scale
    };
}

bool pointInRect(float x, float y, const SDL_FRect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

float saturate(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float easeOutCubic(float t) {
    const float clamped = saturate(t);
    return 1.0f - std::pow(1.0f - clamped, 3.0f);
}

float easeInOutSine(float t) {
    const float clamped = saturate(t);
    return 0.5f - 0.5f * std::cos(clamped * kPi);
}

float remap01(float value, float start, float end) {
    if (end <= start) {
        return value >= end ? 1.0f : 0.0f;
    }
    return saturate((value - start) / (end - start));
}

bool mapWindowPointToReference(float windowX,
                               float windowY,
                               int windowWidth,
                               int windowHeight,
                               float& outRefX,
                               float& outRefY) {
    const LayoutMetrics layout = computeLayout(windowWidth, windowHeight);
    const SDL_FRect bounds = toWindowRect(SDL_FRect{0.0f, 0.0f, kReferenceWidth, kReferenceHeight}, layout);
    if (!pointInRect(windowX, windowY, bounds)) {
        return false;
    }

    outRefX = (windowX - bounds.x) / layout.scale;
    outRefY = (windowY - bounds.y) / layout.scale;
    return true;
}

std::string findTexturePath(const std::string& folder, const std::string& assetName) {
    const std::array<std::string, 2> extensions = {".png", ".webp"};
    for (const std::string& extension : extensions) {
        const std::string candidate = platform::path::resolvePath("assets/combat/" + folder + "/" + assetName + extension);
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return std::string();
}

SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path) {
    if (renderer == nullptr || path.empty()) {
        return nullptr;
    }

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(path.c_str());
#else
    SDL_Surface* surface = SDL_LoadBMP(path.c_str());
#endif
    if (surface == nullptr) {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture != nullptr) {
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
#endif
    }
    return texture;
}

#ifdef BATTLE_ENABLE_TTF
TTF_Font* openFont(int pointSize) {
    const std::string fontPath = platform::path::findFontPath();
    if (fontPath.empty()) {
        return nullptr;
    }
    return TTF_OpenFont(fontPath.c_str(), pointSize);
}

TTF_Font* openCjkFont(int pointSize) {
    const std::string fontPath = platform::path::findCjkFontPath();
    if (fontPath.empty()) {
        return nullptr;
    }
    return TTF_OpenFont(fontPath.c_str(), pointSize);
}

void drawText(SDL_Renderer* renderer,
              TTF_Font* font,
              TTF_Font* cjkFont,
              const std::string& text,
              const SDL_Color& color,
              const SDL_FRect& rect,
              bool centerX,
              bool centerY) {
    TTF_Font* activeFont = containsNonAscii(text) && cjkFont != nullptr ? cjkFont : font;
    if (renderer == nullptr || activeFont == nullptr || text.empty()) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(activeFont, text.c_str(), color);
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_FRect dst = rect;
    dst.w = static_cast<float>(surface->w);
    dst.h = static_cast<float>(surface->h);
    if (centerX) {
        dst.x += (rect.w - dst.w) * 0.5f;
    }
    if (centerY) {
        dst.y += (rect.h - dst.h) * 0.5f;
    }
    dst.y += computeFontBaselineOffset(font, activeFont);

    SDL_RenderCopyF(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void drawWrappedText(SDL_Renderer* renderer,
                     TTF_Font* font,
                     TTF_Font* cjkFont,
                     const std::string& text,
                     const SDL_Color& color,
                     const SDL_FRect& rect) {
    TTF_Font* activeFont = containsNonAscii(text) && cjkFont != nullptr ? cjkFont : font;
    if (renderer == nullptr || activeFont == nullptr || text.empty()) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(
        activeFont,
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

    const SDL_FRect dst{
        rect.x,
        rect.y + computeFontBaselineOffset(font, activeFont),
        static_cast<float>(surface->w),
        static_cast<float>(surface->h)
    };
    SDL_RenderCopyF(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}
#else
TTF_Font* openFont(int) { return nullptr; }
TTF_Font* openCjkFont(int) { return nullptr; }
void drawText(SDL_Renderer*, TTF_Font*, TTF_Font*, const std::string&, const SDL_Color&, const SDL_FRect&, bool, bool) {}
void drawWrappedText(SDL_Renderer*, TTF_Font*, TTF_Font*, const std::string&, const SDL_Color&, const SDL_FRect&) {}
#endif

int fontLineHeight(TTF_Font* primaryFont, TTF_Font* fallbackFont) {
#ifdef BATTLE_ENABLE_TTF
    TTF_Font* activeFont = primaryFont != nullptr ? primaryFont : fallbackFont;
    return activeFont != nullptr ? TTF_FontHeight(activeFont) : 0;
#else
    (void) primaryFont;
    (void) fallbackFont;
    return 0;
#endif
}

void fillRect(SDL_Renderer* renderer, const SDL_FRect& rect, const SDL_Color& color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRectF(renderer, &rect);
}

void drawRect(SDL_Renderer* renderer, const SDL_FRect& rect, const SDL_Color& color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRectF(renderer, &rect);
}

void drawProgressBar(SDL_Renderer* renderer,
                     const SDL_FRect& trackRect,
                     float valueRatio,
                     const SDL_Color& fillColor) {
    const float clampedRatio = std::clamp(valueRatio, 0.0f, 1.0f);
    fillRect(renderer, trackRect, SDL_Color{16, 24, 48, 196});
    if (clampedRatio > 0.0f) {
        fillRect(renderer,
                 SDL_FRect{trackRect.x, trackRect.y, std::round(trackRect.w * clampedRatio), trackRect.h},
                 fillColor);
    }
}

SDL_Rect toClipRect(const SDL_FRect& rect) {
    return SDL_Rect{
        static_cast<int>(std::floor(rect.x)),
        static_cast<int>(std::floor(rect.y)),
        std::max(0, static_cast<int>(std::ceil(rect.w))),
        std::max(0, static_cast<int>(std::ceil(rect.h)))
    };
}

void renderTextureCover(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& bounds) {
    if (renderer == nullptr || texture == nullptr) {
        return;
    }

    int textureWidth = 0;
    int textureHeight = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight) != 0 ||
        textureWidth <= 0 || textureHeight <= 0) {
        return;
    }

    const float scale = std::max(bounds.w / static_cast<float>(textureWidth),
                                 bounds.h / static_cast<float>(textureHeight));
    const float srcWidth = bounds.w / scale;
    const float srcHeight = bounds.h / scale;
    const SDL_Rect src{
        static_cast<int>(std::lround((static_cast<float>(textureWidth) - srcWidth) * 0.5f)),
        static_cast<int>(std::lround((static_cast<float>(textureHeight) - srcHeight) * 0.5f)),
        std::max(1, static_cast<int>(std::lround(srcWidth))),
        std::max(1, static_cast<int>(std::lround(srcHeight)))
    };

    SDL_RenderCopyF(renderer, texture, &src, &bounds);
}

void renderTextureContain(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& bounds) {
    if (renderer == nullptr || texture == nullptr) {
        return;
    }

    int textureWidth = 0;
    int textureHeight = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight) != 0 ||
        textureWidth <= 0 || textureHeight <= 0) {
        return;
    }

    const float scale = std::min(bounds.w / static_cast<float>(textureWidth),
                                 bounds.h / static_cast<float>(textureHeight));
    const float width = static_cast<float>(textureWidth) * scale;
    const float height = static_cast<float>(textureHeight) * scale;
    const SDL_FRect dst{
        bounds.x + (bounds.w - width) * 0.5f,
        bounds.y + (bounds.h - height) * 0.5f,
        width,
        height
    };
    SDL_RenderCopyF(renderer, texture, nullptr, &dst);
}

void renderTexturePixelPerfect(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& bounds) {
    if (renderer == nullptr || texture == nullptr) {
        return;
    }

    int textureWidth = 0;
    int textureHeight = 0;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight) != 0 ||
        textureWidth <= 0 || textureHeight <= 0) {
        return;
    }

    const int maxScaleX = std::max(1, static_cast<int>(std::floor(bounds.w / static_cast<float>(textureWidth))));
    const int maxScaleY = std::max(1, static_cast<int>(std::floor(bounds.h / static_cast<float>(textureHeight))));
    const int pixelScale = std::max(1, std::min(maxScaleX, maxScaleY));
    const float width = static_cast<float>(textureWidth * pixelScale);
    const float height = static_cast<float>(textureHeight * pixelScale);
    const SDL_FRect dst{
        std::round(bounds.x + (bounds.w - width) * 0.5f),
        std::round(bounds.y + (bounds.h - height) * 0.5f),
        width,
        height
    };
    SDL_RenderCopyF(renderer, texture, nullptr, &dst);
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

} // namespace battle::demo::ui
