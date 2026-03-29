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

constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr const char* kScrollSfxRelativePath = "assets/ui/sfx/UI_notification-done.wav";
constexpr const char* kConfirmSfxRelativePath = "assets/ui/sfx/Selection_roulette-result.wav";
constexpr const char* kLoadingOverlayDocumentPath = "assets/rmlui/shared/loading_overlay.rml";

std::vector<ScreenId> stackForAppState(const AppState& state) {
    switch (state.screen) {
        case ScreenState::MainMenu:
            return {ScreenId::MainMenu};

        case ScreenState::Settings:
            if (state.settingsReturnScreen == ScreenState::MainMenu) {
                return {ScreenId::MainMenu, ScreenId::Settings};
            }
            if (state.settingsReturnScreen == ScreenState::PauseMenu &&
                state.pauseContext == PauseContext::Story) {
                return {ScreenId::Story, ScreenId::Settings};
            }
            return {};

        case ScreenState::Playing:
            return {ScreenId::Story};

        case ScreenState::PauseMenu:
        case ScreenState::PauseConfirmExit:
        case ScreenState::PauseConfirmOverwriteSave:
            if (state.pauseContext == PauseContext::Story) {
                return {ScreenId::Story, ScreenId::Pause};
            }
            return {};

        case ScreenState::LoadMenu:
        case ScreenState::LoadConfirmDelete:
            if (state.loadReturnScreen == ScreenState::MainMenu) {
                return {ScreenId::MainMenu, ScreenId::Load};
            }
            if (state.loadReturnScreen == ScreenState::PauseMenu &&
                state.pauseContext == PauseContext::Story) {
                return {ScreenId::Story, ScreenId::Pause, ScreenId::Load};
            }
            return {};

        case ScreenState::BattleDemo:
            return {};
    }

    return {};
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

    initializeAudio();

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
    renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
    context_ = Rml::CreateContext("front-ui", Rml::Vector2i(windowWidth_, windowHeight_));
    if (context_ == nullptr) {
        std::cerr << "[FrontUi] Failed to create RmlUi context.\n";
        shutdown();
        return false;
    }
    applyContextScale();

    if (!syncStackForState(state)) {
        std::cerr << "[FrontUi] Failed to show front-ui document.\n";
        shutdown();
        return false;
    }

    if (!loadingOverlay_.initialize(*context_, platform::path::resolvePath(kLoadingOverlayDocumentPath))) {
        std::cerr << "[FrontUi] Failed to load loading overlay document.\n";
        shutdown();
        return false;
    }
    loadingOverlayState_ = RmlUiLoadingOverlayState{};

    initialized_ = true;
    return true;
}

void Session::shutdown() {
    initialized_ = false;
    SDL_StopTextInput();
    sfxPlayer_.shutdown();
    scrollSfxPath_.clear();
    confirmSfxPath_.clear();
    audioReady_ = false;

    while (!documents_.empty()) {
        popScreen();
    }

    loadingOverlay_.shutdown();
    loadingOverlayState_ = RmlUiLoadingOverlayState{};

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
    return syncStackForState(state);
}

void Session::handleEvent(const SDL_Event& event, AppState& state) {
    if (!initialized_ || context_ == nullptr || window_ == nullptr) {
        return;
    }

    const std::optional<MainMenuAction> selectionBeforeEvent = currentMainMenuSelection();

    if (event.type == SDL_WINDOWEVENT &&
        (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         event.window.event == SDL_WINDOWEVENT_RESIZED)) {
        updateViewportFromWindow();
        renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
        context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
        applyContextScale();
    }

    SDL_Event mutableEvent = event;
    RmlSDL::InputEventHandler(context_, window_, mutableEvent);

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
            case SDLK_w:
                if (DocumentController* controller = topController()) {
                    controller->moveSelection(-1);
                }
                break;

            case SDLK_DOWN:
            case SDLK_s:
                if (DocumentController* controller = topController()) {
                    controller->moveSelection(1);
                }
                break;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (DocumentController* controller = topController()) {
                    controller->activateSelection();
                }
                break;

            case SDLK_LEFT:
            case SDLK_a:
                if (DocumentController* controller = topController()) {
                    controller->adjustSelection(-1);
                }
                break;

            case SDLK_RIGHT:
            case SDLK_d:
                if (DocumentController* controller = topController()) {
                    controller->adjustSelection(1);
                }
                break;

            case SDLK_ESCAPE:
            case SDLK_BACKSPACE:
                if (DocumentController* controller = topController()) {
                    controller->cancel();
                }
                break;

            default:
                break;
        }
    }

    syncState(state);
    playQueuedControllerSounds();

    const std::optional<MainMenuAction> selectionAfterEvent = currentMainMenuSelection();
    if (selectionBeforeEvent.has_value() &&
        selectionAfterEvent.has_value() &&
        selectionBeforeEvent != selectionAfterEvent) {
        playScrollSfx();
    }
}

void Session::update(const AppState& state, float deltaSeconds) {
    if (!initialized_) {
        return;
    }

    (void)showForState(state);

    for (ActiveDocument& entry : documents_) {
        if (entry.controller != nullptr) {
            entry.controller->sync(state);
            entry.controller->update(state, deltaSeconds);
        }
    }

    sfxPlayer_.cleanupFinishedPlayback();
}

void Session::render() {
    if (!initialized_ || renderInterface_ == nullptr || context_ == nullptr || window_ == nullptr) {
        return;
    }

    SDL_GL_MakeCurrent(window_, glContext_);
    glViewport(0, 0, drawableWidth_, drawableHeight_);
    glClearColor(0.015f, 0.025f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    loadingOverlay_.apply(loadingOverlayState_);
    context_->Update();
    renderInterface_->BeginFrame();
    context_->Render();
    renderInterface_->EndFrame();
}

void Session::setLoadingOverlay(const RmlUiLoadingOverlayState& state) {
    loadingOverlayState_ = state;
    loadingOverlay_.apply(loadingOverlayState_);
}

void Session::playResolvedSfx(const SoundRequest& request) {
    if (!audioReady_ || request.relativePath.empty()) {
        return;
    }

    const std::string resolvedPath = platform::path::resolvePath(request.relativePath);
    if (resolvedPath.empty() || !std::filesystem::exists(resolvedPath)) {
        return;
    }

    (void)sfxPlayer_.playWavOneShot(resolvedPath, std::clamp(request.volume, 0.0f, 1.0f), true);
}

void Session::playQueuedControllerSounds() {
    if (DocumentController* controller = topController()) {
        for (const SoundRequest& request : controller->consumeSoundRequests()) {
            playResolvedSfx(request);
        }
    }
}

std::optional<Command> Session::consumeCommand() {
    if (DocumentController* controller = topController()) {
        std::optional<Command> command = controller->consumeCommand();
        if (command.has_value() && command->type == CommandType::ActivateMainMenuAction) {
            playConfirmSfx();
        }
        return command;
    }
    return std::nullopt;
}

bool Session::syncStackForState(const AppState& state) {
    if (context_ == nullptr) {
        return false;
    }

    const std::vector<ScreenId> targetStack = stackForAppState(state);
    if (targetStack.empty()) {
        return false;
    }

    std::size_t commonPrefix = 0;
    while (commonPrefix < documents_.size() &&
           commonPrefix < targetStack.size() &&
           documents_[commonPrefix].screen == targetStack[commonPrefix]) {
        ++commonPrefix;
    }

    while (documents_.size() > commonPrefix) {
        popScreen();
    }

    while (commonPrefix < targetStack.size()) {
        if (!pushScreen(targetStack[commonPrefix], state)) {
            return false;
        }
        ++commonPrefix;
    }

    return true;
}

bool Session::pushScreen(ScreenId screen, const AppState& state) {
    if (context_ == nullptr || !isScreenImplemented(screen)) {
        return false;
    }

    const std::string documentPath = platform::path::resolvePath(resolveDocumentPath(screen));
    Rml::ElementDocument* document = context_->LoadDocument(documentPath);
    if (document == nullptr) {
        std::cerr << "[FrontUi] Failed to load document: " << documentPath << "\n";
        return false;
    }

    std::unique_ptr<DocumentController> controller = createControllerForScreen(screen);
    if (controller == nullptr || !controller->bind(*document, state)) {
        document->Close();
        return false;
    }

    document->Show();
    documents_.push_back(ActiveDocument{screen, document, std::move(controller)});
    return true;
}

void Session::popScreen() {
    if (documents_.empty()) {
        return;
    }

    ActiveDocument& entry = documents_.back();
    if (entry.controller != nullptr) {
        entry.controller->unbind();
    }
    if (entry.document != nullptr) {
        entry.document->Close();
    }
    documents_.pop_back();
}

DocumentController* Session::topController() const {
    if (documents_.empty() || documents_.back().controller == nullptr) {
        return nullptr;
    }
    return documents_.back().controller.get();
}

void Session::syncState(AppState& state) {
    for (ActiveDocument& entry : documents_) {
        if (entry.controller != nullptr) {
            entry.controller->applyState(state);
        }
    }
}

void Session::updateViewportFromWindow() {
    if (windowHost_ == nullptr) {
        return;
    }

    windowWidth_ = std::max(1, windowHost_->getWindowWidth());
    windowHeight_ = std::max(1, windowHost_->getWindowHeight());
    drawableWidth_ = std::max(1, windowHost_->getDrawableWidth());
    drawableHeight_ = std::max(1, windowHost_->getDrawableHeight());
    glViewport(0, 0, drawableWidth_, drawableHeight_);
}

void Session::applyContextScale() {
    if (context_ == nullptr) {
        return;
    }

    const float widthScale = static_cast<float>(windowWidth_) / static_cast<float>(kReferenceWidth);
    const float heightScale = static_cast<float>(windowHeight_) / static_cast<float>(kReferenceHeight);
    const float scale = std::min(widthScale, heightScale);
    context_->SetDensityIndependentPixelRatio(std::max(scale, 0.01f));
}

bool Session::loadFonts() const {
    bool loadedLatin = false;
    for (const std::string& path : platform::path::preferredLatinFontPaths()) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        loadedLatin = Rml::LoadFontFace(path) || loadedLatin;
    }

    const std::string cjkPath = platform::path::findCjkFontPath();
    bool loadedFallback = false;
    if (!cjkPath.empty() && std::filesystem::exists(cjkPath)) {
        loadedFallback = Rml::LoadFontFace(cjkPath, true);
    }

    return loadedLatin || loadedFallback;
}

bool Session::initializeAudio() {
    scrollSfxPath_ = platform::path::resolvePath(kScrollSfxRelativePath);
    confirmSfxPath_ = platform::path::resolvePath(kConfirmSfxRelativePath);

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "[FrontUi] Audio init failed: " << SDL_GetError() << "\n";
            scrollSfxPath_.clear();
            confirmSfxPath_.clear();
            audioReady_ = false;
            return false;
        }
    }

    audioReady_ = true;
    return audioReady_;
}

std::optional<MainMenuAction> Session::currentMainMenuSelection() const {
    if (DocumentController* controller = topController()) {
        return controller->selectedMainMenuAction();
    }
    return std::nullopt;
}

void Session::playScrollSfx() {
    if (!audioReady_ || scrollSfxPath_.empty()) {
        return;
    }
    (void)sfxPlayer_.playWavOneShot(scrollSfxPath_, 0.78f, true);
}

void Session::playConfirmSfx() {
    if (!audioReady_ || confirmSfxPath_.empty()) {
        return;
    }
    (void)sfxPlayer_.playWavOneShot(confirmSfxPath_, 0.92f, true);
}

}  // namespace graphics::frontui
