#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

#include "RmlUi_Platform_SDL.h"

#include "../GameMenu/menu_shared.h"
#include "../game/audio/wav_one_shot.h"
#include "../game/audio/ui_music_types.h"
#include "front_ui_document.h"
#include "rmlui_loading_overlay.h"
#include "rmlui_sdl_gl_renderer.h"
#include "ui_music_bars.h"

class Window;

/**
 * Initialize the UI session and its subsystems to match the provided application state.
 * @param window Host window used for rendering and input.
 * @param state Application state describing which screens and UI mode to initialize.
 * @returns `true` if initialization succeeded and the session is ready, `false` otherwise.
 */

/**
 * Shutdown the UI session and release all held resources.
 */

/**
 * Determine whether the UI should be visible/active for the given application state.
 * @param state Application state to evaluate.
 * @returns `true` if the UI should be shown for `state`, `false` otherwise.
 */

/**
 * Handle a platform event and route it into the UI system, potentially modifying application state.
 * @param event Platform event to process (SDL event).
 * @param state Application state which may be updated in response to the event.
 */

/**
 * Advance UI logic for the current frame and synchronize UI state with the application state.
 * @param state Current application state to synchronize into the UI.
 * @param deltaSeconds Elapsed time in seconds since the last update.
 */

/**
 * Render the current UI documents and any active overlays.
 */

/**
 * Set the loading overlay state to be applied during rendering.
 * @param state Loading overlay configuration to apply.
 */

/**
 * Update the UI music visualization state used to drive on-screen music bars and related visuals.
 * @param state Music visualization state (levels, enable flags, etc.) to apply to the UI.
 */

/**
 * Retrieve and clear a pending command produced by the UI.
 * @returns The next `Command` if one is available, or an empty `std::optional` if none exists.
 */

/**
 * Query whether the session has been successfully initialized.
 * @returns `true` if the session is initialized, `false` otherwise.
 */
namespace graphics {

namespace frontui {

class Session {
public:
    ~Session();

    bool initialize(Window& window, const AppState& state);
    void shutdown();
    bool showForState(const AppState& state);

    void handleEvent(const SDL_Event& event, AppState& state);
    void update(const AppState& state, float deltaSeconds);
    void render();
    void setLoadingOverlay(const RmlUiLoadingOverlayState& state);
    void setUiMusicVisualState(const game::audio::UiMusicVisualState& state);

    std::optional<Command> consumeCommand();
    bool isInitialized() const { return initialized_; }

private:
    struct ActiveDocument {
        ScreenId screen = ScreenId::MainMenu;
        Rml::ElementDocument* document = nullptr;
        std::unique_ptr<DocumentController> controller;
        UiMusicBarStrip uiMusicBars;
    };

    bool syncStackForState(const AppState& state);
    bool pushScreen(ScreenId screen, const AppState& state);
    void popScreen();
    DocumentController* topController() const;
    void syncState(AppState& state);
    void applyUiMusicVisualState();
    void playResolvedSfx(const SoundRequest& request);
    void playQueuedControllerSounds();
    void updateViewportFromWindow();
    void applyContextScale();
    bool loadFonts() const;
    bool initializeAudio();
    std::optional<MainMenuAction> currentMainMenuSelection() const;
    void playScrollSfx();
    void playConfirmSfx();

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool initialized_ = false;
    bool audioReady_ = false;
    bool rmlInitialized_ = false;
    bool rmlGlInitialized_ = false;
    int windowWidth_ = 1280;
    int windowHeight_ = 720;
    int drawableWidth_ = 1280;
    int drawableHeight_ = 720;
    SystemInterface_SDL systemInterface_;
    std::unique_ptr<RmlUiSdlGlRenderInterface> renderInterface_;
    Rml::Context* context_ = nullptr;
    std::vector<ActiveDocument> documents_;
    RmlUiLoadingOverlay loadingOverlay_;
    RmlUiLoadingOverlayState loadingOverlayState_;
    game::audio::UiMusicVisualState uiMusicVisualState_;
    game::audio::WavOneShotPlayer sfxPlayer_;
    std::string scrollSfxPath_;
    std::string confirmSfxPath_;
};

}  // namespace frontui
}  // namespace graphics
