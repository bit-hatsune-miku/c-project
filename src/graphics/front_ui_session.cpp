#define GL_GLEXT_PROTOTYPES

#include "front_ui_session.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Log.h>

#include "RmlUi_Renderer_GL3.h"

#include "../platform/path_resolution.h"
#include "../window.h"
#include "rmlui_sdl_gl_renderer.h"

namespace graphics::frontui {
namespace {

std::optional<ScreenId> screenForAppState(const AppState& state) {
    switch (state.screen) {
        case ScreenState::MainMenu:
            return ScreenId::MainMenu;

        case ScreenState::Settings:
            if (state.settingsReturnScreen == ScreenState::MainMenu) {
                return ScreenId::Settings;
            }
            return std::nullopt;

        case ScreenState::LoadMenu:
        case ScreenState::LoadConfirmDelete:
        case ScreenState::BattleDemo:
        case ScreenState::Playing:
        case ScreenState::PauseMenu:
        case ScreenState::PauseConfirmExit:
        case ScreenState::PauseConfirmOverwriteSave:
            return std::nullopt;
    }

    return std::nullopt;
}

}  // namespace

Session::~Session() {
    shutdown();
}

bool Session::initialize(Window& window, const AppState& state) {
    shutdown();

    windowHost_ = &window;
    window_ = window.getNativeWindow();
    glContext_ = window.getGlContext();
    if (window_ == nullptr || glContext_ == nullptr) {
        std::cerr << "[FrontUi] Window is not in OpenGL mode.\n";
        return false;
    }

    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(1);
    SDL_StartTextInput();

    Rml::String glInitMessage;
    if (!RmlGL3::Initialize(&glInitMessage)) {
        std::cerr << "[FrontUi] RmlGL3 initialization failed: " << glInitMessage << "\n";
        shutdown();
        return false;
    }
    rmlGlInitialized_ = true;

    systemInterface_.SetWindow(window_);
    renderInterface_ = std::make_unique<RmlUiSdlGlRenderInterface>();
    if (!(*renderInterface_)) {
        std::cerr << "[FrontUi] Failed to construct GL render interface.\n";
        shutdown();
        return false;
    }

    Rml::SetSystemInterface(&systemInterface_);
    Rml::SetRenderInterface(renderInterface_.get());
    if (!Rml::Initialise()) {
        std::cerr << "[FrontUi] RmlUi core initialization failed.\n";
        shutdown();
        return false;
    }
    rmlInitialized_ = true;

    if (!loadFonts()) {
        std::cerr << "[FrontUi] No usable fonts were loaded for the main menu.\n";
        shutdown();
        return false;
    }

    updateViewportFromWindow();
    renderInterface_->SetViewport(windowWidth_, windowHeight_);
    context_ = Rml::CreateContext("front-ui", Rml::Vector2i(windowWidth_, windowHeight_));
    if (context_ == nullptr) {
        std::cerr << "[FrontUi] Failed to create RmlUi context.\n";
        shutdown();
        return false;
    }

    if (!showForState(state)) {
        std::cerr << "[FrontUi] Failed to show front-ui document.\n";
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void Session::shutdown() {
    initialized_ = false;
    SDL_StopTextInput();

    if (controller_ != nullptr) {
        controller_->unbind();
    }
    if (document_ != nullptr) {
        document_->Close();
        document_ = nullptr;
    }
    controller_.reset();

    if (rmlInitialized_) {
        Rml::Shutdown();
        rmlInitialized_ = false;
    }

    if (rmlGlInitialized_) {
        RmlGL3::Shutdown();
        rmlGlInitialized_ = false;
    }

    renderInterface_.reset();
    context_ = nullptr;
    glContext_ = nullptr;
    window_ = nullptr;
    windowHost_ = nullptr;
}

bool Session::showForState(const AppState& state) {
    const std::optional<ScreenId> requestedScreen = screenForAppState(state);
    if (!requestedScreen.has_value()) {
        return false;
    }
    if (document_ != nullptr && activeScreen_ == *requestedScreen) {
        return true;
    }
    return showScreen(*requestedScreen, state);
}

void Session::handleEvent(const SDL_Event& event, AppState& state) {
    if (!initialized_ || context_ == nullptr || window_ == nullptr) {
        return;
    }

    if (event.type == SDL_WINDOWEVENT &&
        (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         event.window.event == SDL_WINDOWEVENT_RESIZED)) {
        updateViewportFromWindow();
        renderInterface_->SetViewport(windowWidth_, windowHeight_);
        context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
    }

    SDL_Event mutableEvent = event;
    RmlSDL::InputEventHandler(context_, window_, mutableEvent);

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
            case SDLK_w:
                if (controller_ != nullptr) {
                    controller_->moveSelection(-1);
                }
                break;

            case SDLK_DOWN:
            case SDLK_s:
                if (controller_ != nullptr) {
                    controller_->moveSelection(1);
                }
                break;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (controller_ != nullptr) {
                    controller_->activateSelection();
                }
                break;

            case SDLK_LEFT:
            case SDLK_a:
                if (controller_ != nullptr) {
                    controller_->adjustSelection(-1);
                }
                break;

            case SDLK_RIGHT:
            case SDLK_d:
                if (controller_ != nullptr) {
                    controller_->adjustSelection(1);
                }
                break;

            case SDLK_ESCAPE:
            case SDLK_BACKSPACE:
                if (controller_ != nullptr) {
                    controller_->cancel();
                }
                break;

            default:
                break;
        }
    }

    syncState(state);
}

void Session::update(const AppState& state, float deltaSeconds) {
    if (!initialized_ || controller_ == nullptr) {
        return;
    }

    (void)showForState(state);

    controller_->sync(state);
    controller_->update(state, deltaSeconds);
}

void Session::render() {
    if (!initialized_ || renderInterface_ == nullptr || context_ == nullptr || window_ == nullptr) {
        return;
    }

    SDL_GL_MakeCurrent(window_, glContext_);
    glViewport(0, 0, windowWidth_, windowHeight_);
    glClearColor(0.015f, 0.025f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    renderInterface_->BeginFrame();
    context_->Update();
    context_->Render();
    renderInterface_->EndFrame();
}

std::optional<Command> Session::consumeCommand() {
    if (controller_ == nullptr) {
        return std::nullopt;
    }
    return controller_->consumeCommand();
}

bool Session::showScreen(ScreenId screen, const AppState& state) {
    if (context_ == nullptr || !isScreenImplemented(screen)) {
        return false;
    }

    if (controller_ != nullptr) {
        controller_->unbind();
    }
    if (document_ != nullptr) {
        document_->Close();
        document_ = nullptr;
    }
    controller_.reset();

    const std::string documentPath = platform::path::resolvePath(resolveDocumentPath(screen));
    document_ = context_->LoadDocument(documentPath);
    if (document_ == nullptr) {
        std::cerr << "[FrontUi] Failed to load document: " << documentPath << "\n";
        return false;
    }

    controller_ = createControllerForScreen(screen);
    if (controller_ == nullptr || !controller_->bind(*document_, state)) {
        document_->Close();
        document_ = nullptr;
        controller_.reset();
        return false;
    }

    document_->Show();
    activeScreen_ = screen;
    return true;
}

void Session::syncState(AppState& state) {
    if (controller_ == nullptr) {
        return;
    }
    controller_->applyState(state);
}

void Session::updateViewportFromWindow() {
    if (windowHost_ == nullptr) {
        return;
    }

    windowWidth_ = std::max(1, windowHost_->getWidth());
    windowHeight_ = std::max(1, windowHost_->getHeight());
    glViewport(0, 0, windowWidth_, windowHeight_);
}

bool Session::loadFonts() const {
    const std::vector<std::string> candidates = platform::path::preferredCjkFontPaths();
    bool loadedAnyFont = false;

    for (const std::string& path : candidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        loadedAnyFont = Rml::LoadFontFace(path) || loadedAnyFont;
    }

    return loadedAnyFont;
}

}  // namespace graphics::frontui
