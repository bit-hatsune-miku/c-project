#ifndef BATTLE_DEMO_PARTY_SETUP_UI_H
#define BATTLE_DEMO_PARTY_SETUP_UI_H

#include <string>

#include <SDL2/SDL.h>
#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#else
struct TTF_Font;
#endif

namespace battle::demo::ui {

constexpr float kReferenceWidth = 1280.0f;
constexpr float kReferenceHeight = 720.0f;
constexpr int kVisibleRosterRows = 4;
constexpr Uint32 kMouseHoverTimeoutMs = 700;
constexpr Uint32 kStartTransitionFadeMs = 200;
constexpr Uint32 kStartTransitionFirstSweepMs = 400;
constexpr Uint32 kStartTransitionSecondSweepMs = 200;
constexpr Uint32 kStartTransitionBlackHoldMs = 200;
constexpr Uint32 kStartTransitionDurationMs =
    kStartTransitionFadeMs +
    kStartTransitionFirstSweepMs +
    kStartTransitionSecondSweepMs +
    kStartTransitionBlackHoldMs;
constexpr float kPi = 3.14159265358979323846f;

struct LayoutMetrics {
    float scale = 1.0f;
    float originX = 0.0f;
    float originY = 0.0f;
};

struct PartySetupGeometry {
    SDL_FRect rosterPanel{};
    SDL_FRect rosterTitleRect{};
    SDL_FRect rosterHintRect{};
    SDL_FRect rosterTrackRect{};
    SDL_FRect stageTitleRect{};
    SDL_FRect stageKickerRect{};
    SDL_FRect stageDescriptionRect{};
    SDL_FRect stagePanel{};
    SDL_FRect selectedCountRect{};
    SDL_FRect startButton{};
};

LayoutMetrics computeLayout(int windowWidth, int windowHeight);
PartySetupGeometry computeGeometry();
SDL_FRect rosterRowRect(const PartySetupGeometry& geometry, int visibleIndex);
SDL_FRect rosterThumbRect(const PartySetupGeometry& geometry, int rosterCount, int scrollOffset);
SDL_FRect selectedCardRect(const PartySetupGeometry& geometry, int slotIndex);
SDL_FRect toWindowRect(const SDL_FRect& referenceRect, const LayoutMetrics& layout);
bool pointInRect(float x, float y, const SDL_FRect& rect);
float saturate(float value);
float easeOutCubic(float t);
float easeInOutSine(float t);
float remap01(float value, float start, float end);
bool mapWindowPointToReference(float windowX,
                               float windowY,
                               int windowWidth,
                               int windowHeight,
                               float& outRefX,
                               float& outRefY);

std::string findTexturePath(const std::string& folder, const std::string& assetName);
SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path);
TTF_Font* openFont(int pointSize);
TTF_Font* openCjkFont(int pointSize);
int fontLineHeight(TTF_Font* primaryFont, TTF_Font* fallbackFont);

void drawText(SDL_Renderer* renderer,
              TTF_Font* font,
              TTF_Font* cjkFont,
              const std::string& text,
              const SDL_Color& color,
              const SDL_FRect& rect,
              bool centerX = false,
              bool centerY = false);
void drawWrappedText(SDL_Renderer* renderer,
                     TTF_Font* font,
                     TTF_Font* cjkFont,
                     const std::string& text,
                     const SDL_Color& color,
                     const SDL_FRect& rect);
void fillRect(SDL_Renderer* renderer, const SDL_FRect& rect, const SDL_Color& color);
void drawRect(SDL_Renderer* renderer, const SDL_FRect& rect, const SDL_Color& color);
void drawProgressBar(SDL_Renderer* renderer,
                     const SDL_FRect& trackRect,
                     float valueRatio,
                     const SDL_Color& fillColor);
SDL_Rect toClipRect(const SDL_FRect& rect);
void renderTextureCover(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& bounds);
void renderTextureContain(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& bounds);
void renderTexturePixelPerfect(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& bounds);
std::string uppercase(std::string value);

} // namespace battle::demo::ui

#endif
