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
#include "front_ui_document.h"
#include "rmlui_loading_overlay.h"
#include "rmlui_sdl_gl_renderer.h"

class Window;

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

    std::optional<Command> consumeCommand();
    bool isInitialized() const { return initialized_; }

private:
    struct ActiveDocument {
        ScreenId screen = ScreenId::MainMenu;
        Rml::ElementDocument* document = nullptr;
        std::unique_ptr<DocumentController> controller;
    };

    bool syncStackForState(const AppState& state);
    bool pushScreen(ScreenId screen, const AppState& state);
    void popScreen();
    DocumentController* topController() const;
    void syncState(AppState& state);
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
    game::audio::WavOneShotPlayer sfxPlayer_;
    std::string scrollSfxPath_;
    std::string confirmSfxPath_;
};

}  // namespace frontui
}  // namespace graphics
