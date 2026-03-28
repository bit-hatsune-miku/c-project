#ifndef MENU_SHARED_H
#define MENU_SHARED_H

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#ifdef VN_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include "../game/core/player_progression.h"
#include "../game/vn/vn_script.h"

class Window;

enum class ScreenState {
    MainMenu,
    BossSelector,
    Settings,
    LoadGameMenu,
    LoadConfirmDelete,
    BattleDemo,
    Playing,
    PauseMenu,
    PauseConfirmExit,
    PauseConfirmOverwriteSave
};

enum class MainMenuAction {
    Start,
    Load,
    Battle,
    Settings,
    Exit
};

inline constexpr const char* kMainMenuBackgroundArtPath = "assets/vn/backgrounds/General_Art/mainmenu art.png";
inline constexpr const char* kMainMenuTitleLogoPath = "assets/vn/backgrounds/General_Art/MainMenuTitle.png";

struct MainMenuPresentation {
    MainMenuAction action;
    const char* buttonId;
    const char* codeId;
    const char* labelId;
    const char* detailId;
    const char* focusCode;
    const char* displayLabel;
    const char* displayDetail;
    const char* statusTitle;
    const char* statusBody;
};

inline constexpr std::array<MainMenuPresentation, 5> kMainMenuPresentation{{
    {MainMenuAction::Start,
     "menu-button-start",
     "menu-button-code-start",
     "menu-button-label-start",
     "menu-button-detail-start",
     "01",
     "PLAY",
     "CHAPTER 0 / STORY",
     "ENTER THE OPENING SIGNAL",
     "Start chapter 0 from the first scene of the visual novel."},
    {MainMenuAction::Load,
     "menu-button-load",
     "menu-button-code-load",
     "menu-button-label-load",
     "menu-button-detail-load",
     "02",
     "LOAD",
     "SAVE DATA / AUTOSAVE",
     "RESUME A SAVED THREAD",
     "Open manual saves and autosaves, then jump back into the route."},
    {MainMenuAction::Battle,
     "menu-button-battle",
     "menu-button-code-battle",
     "menu-button-label-battle",
     "menu-button-detail-battle",
     "03",
     "BATTLE SELECTOR",
     "BOSS LINEUP / STORY ENTRY",
     "OPEN THE BATTLE SELECTOR",
     "Browse bosses, preview progress, and launch a battle or its linked story scene."},
    {MainMenuAction::Settings,
     "menu-button-settings",
     "menu-button-code-settings",
     "menu-button-label-settings",
     "menu-button-detail-settings",
     "04",
     "SETTINGS",
     "DISPLAY / VOICE / TEXT",
     "TUNE THE VN SYSTEM",
     "Adjust fullscreen mode, voice volume, and text speed."},
    {MainMenuAction::Exit,
     "menu-button-exit",
     "menu-button-code-exit",
     "menu-button-label-exit",
     "menu-button-detail-exit",
     "05",
     "QUIT",
     "CLOSE CLIENT / DESKTOP",
     "CUT THE SIGNAL",
     "Exit the game client and return to desktop."}
}};

inline constexpr const MainMenuPresentation& mainMenuPresentation(MainMenuAction action) {
    return kMainMenuPresentation[static_cast<std::size_t>(action)];
}

enum class PauseAction {
    Continue,
    Save,
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
    ScreenState loadReturnScreen = ScreenState::MainMenu;
    MainMenuAction mainSelection = MainMenuAction::Start;
    SettingsItem settingsSelection = SettingsItem::DisplayMode;
    PauseAction pauseSelection = PauseAction::Continue;
    PauseContext pauseContext = PauseContext::Story;
    ConfirmAction confirmSelection = ConfirmAction::Cancel;
    std::size_t loadSelection = 0;
    std::size_t loadSlotSelection = 0;
    float menuIntroTime = 0.0f;
    float pauseIntroTime = 0.0f;
    GameSettings settings;
    StorySession story;
    battle::PlayerProgression progression;
    std::string noticeText;
    float noticeTimer = 0.0f;
    bool requestStoryManualSave = false;
    bool requestStoryOverwriteSave = false;
    ScreenState storyEndReturnScreen = ScreenState::MainMenu;
    std::string pendingBattleKey;
    bool pendingBattleLaunchedFromStory = false;
    ScreenState pendingBattleReturnScreen = ScreenState::MainMenu;
    std::string pendingBattleWinScript;
    std::string pendingBattleLoseScript;
    std::string pendingLoadPath;
    std::string pendingOverwriteSavePath;
    std::string pendingDeletePath;
    std::size_t pendingDeleteSelection = 0;
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

void beginStory(AppState& state,
                const std::string& scriptRef = std::string(),
                ScreenState endReturnScreen = ScreenState::MainMenu);
void beginBossSelector(AppState& state);
void beginBattleDemo(AppState& state);
void beginBattle(AppState& state, int battleId);  // dispatches to the right battle by ID
void beginBattle(AppState& state, const std::string& battleKey);
void applyMainMenuAction(AppState& state, Window& window, MainMenuAction action);

void renderMainMenu(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                    int windowWidth, int windowHeight);
void handleMainMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight);

void openPauseMenu(AppState& state, PauseContext context = PauseContext::Story);
void openLoadMenu(AppState& state, ScreenState returnScreen);
void renderPauseBackdrop(SDL_Renderer* renderer, int windowWidth, int windowHeight);
void renderPauseScreen(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                       int windowWidth, int windowHeight);
void renderExitToMainMenuOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state);
void handlePauseMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight);
void handlePauseConfirmEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight);
void renderLoadScreen(SDL_Renderer* renderer, const MenuResources& resources, AppState& state,
                      int windowWidth, int windowHeight);
void handleLoadMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight);

#endif
