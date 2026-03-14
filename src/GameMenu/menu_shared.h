#ifndef MENU_SHARED_H
#define MENU_SHARED_H

#include <cstddef>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#ifdef VN_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include "../game/vn/vn_script.h"

class Window;

enum class ScreenState {
    MainMenu,
    Settings,
    BattleDemo,
    Playing,
    PauseMenu,
    PauseConfirmExit
};

enum class MainMenuAction {
    Start,
    Load,
    Battle,
    Settings,
    Exit
};

enum class PauseAction {
    Continue,
    Load,
    Settings,
    ExitToMainMenu
};

enum class PauseContext {
    Story,
    Battle
};

enum class ConfirmAction {
    Cancel,
    ExitToMainMenu
};

enum class SettingsItem {
    DisplayMode,
    VoiceVolume,
    TextSpeed,
    Back
};

struct GameSettings {
    bool fullscreen = false;
    float voiceVolume = 0.82f;
    float textSpeed = 42.0f;
};

struct StorySession {
    vn::Script script;
    std::size_t entryIndex = 0;
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
    MainMenuAction mainSelection = MainMenuAction::Start;
    SettingsItem settingsSelection = SettingsItem::DisplayMode;
    PauseAction pauseSelection = PauseAction::Continue;
    PauseContext pauseContext = PauseContext::Story;
    ConfirmAction confirmSelection = ConfirmAction::Cancel;
    float menuIntroTime = 0.0f;
    float pauseIntroTime = 0.0f;
    GameSettings settings;
    StorySession story;
    std::string noticeText;
    float noticeTimer = 0.0f;
};

inline constexpr float kMenuIntroMaxTime = 0.52f + 0.09f * 4.0f;
inline constexpr float kPauseIntroMaxTime = 0.38f + 0.07f * 3.0f;

std::string resolvePath(const std::string& relativePath);
SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path);

#ifdef VN_ENABLE_TTF
TTF_Font* openBestAvailableFont(const std::vector<std::string>& preferredPaths, int ptSize);
void drawTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                    const SDL_Color& color, const SDL_FRect& rect, bool centerX = true);
void drawWrappedTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                           const SDL_Color& color, const SDL_FRect& rect, bool centerX = true);
void drawShadowedTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                            const SDL_Color& color, const SDL_FRect& rect, bool centerX = true);
void drawShadowedWrappedTextInRect(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                                   const SDL_Color& color, const SDL_FRect& rect, bool centerX = true);
#endif

bool pointInRect(float x, float y, const SDL_FRect& rect);
float clamp01(float value);
float easeOutBack(float value);
float smoothstep01(float value);
SDL_FRect offsetRect(const SDL_FRect& rect, float dx, float dy);

void drawSlantedPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float slant, const SDL_Color& color);
void drawJaggedButtonPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float leadCut, float tailCut, float notchDepth,
                           const SDL_Color& fillColor, const SDL_Color& outlineColor,
                           const SDL_Color& highlightColor, const SDL_Color& shadowColor);
void drawCyberPanel(SDL_Renderer* renderer, const SDL_FRect& rect, float slant, float bevel,
                    const SDL_Color& fillColor, const SDL_Color& outlineColor,
                    const SDL_Color& highlightColor, const SDL_Color& shadowColor);
void drawNeonLine(SDL_Renderer* renderer, const SDL_FPoint& a, const SDL_FPoint& b,
                  const SDL_Color& glowColor, const SDL_Color& coreColor);

void renderBackgroundCover(SDL_Renderer* renderer, SDL_Texture* texture, int windowWidth, int windowHeight);
void renderTextureContain(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect& bounds,
                          Uint8 alpha = 255, float alignX = 0.5f, float alignY = 0.5f);
bool beginMenuCanvas(SDL_Renderer* renderer, SDL_Texture* canvas);
void endMenuCanvas(SDL_Renderer* renderer, SDL_Texture* canvas, int windowWidth, int windowHeight);
bool beginReferenceLayout(SDL_Renderer* renderer, int windowWidth, int windowHeight);
void endReferenceLayout(SDL_Renderer* renderer);
bool mapWindowPointToReference(float windowX, float windowY, int windowWidth, int windowHeight, SDL_FPoint& outPoint);

void beginStory(AppState& state);
void beginBattleDemo(AppState& state);
void beginBattle(AppState& state, int battleId);  // dispatches to the right battle by ID

void renderMainMenu(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                    int windowWidth, int windowHeight);
void handleMainMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight);

void openPauseMenu(AppState& state, PauseContext context = PauseContext::Story);
void renderPauseBackdrop(SDL_Renderer* renderer, int windowWidth, int windowHeight);
void renderPauseScreen(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                       int windowWidth, int windowHeight);
void renderExitToMainMenuOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state);
void handlePauseMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight);
void handlePauseConfirmEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight);

#endif
