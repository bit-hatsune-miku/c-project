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

        case ScreenState::Credits:
            return {ScreenId::Credits};

        case ScreenState::PauseMenu:
        case ScreenState::PauseConfirmExit:
        case ScreenState::PauseConfirmOverwriteSave:
            if (state.pauseContext == PauseContext::Story) {
                return {ScreenId::Story, ScreenId::Pause};
            }
            return {};

        case ScreenState::LoadGameMenu:
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
        case ScreenState::BossSelector:
        case ScreenState::PartyLoader:
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

/**
 * @brief Tears down the Session, releasing all UI, audio, and GL resources.
 *
 * Stops SDL text input and the internal SFX player, clears stored SFX paths and
 * UI music visual state, closes and removes all active documents, shuts down
 * the loading overlay, calls Rml and RmlGL shutdown if they were initialized,
 * and releases render interface, context, GL context, and window handles.
 */
void Session::shutdown() {
    initialized_ = false;
    SDL_StopTextInput();
    sfxPlayer_.shutdown();
    scrollSfxPath_.clear();
    confirmSfxPath_.clear();
    audioReady_ = false;
    uiMusicVisualState_ = game::audio::UiMusicVisualState{};

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

/**
 * @brief Processes a single SDL event for the front-end UI, routing input to Rml, updating
 * the viewport on window resize, dispatching keyboard navigation to the top document controller,
 * synchronizing controllers with the application state, and triggering any resulting UI sounds.
 *
 * @param event SDL event to handle.
 * @param state Current application state which controllers may read from and modify.
 */
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
        if (DocumentController* controller = topController()) {
            controller->handleKeyDown(event.key);
        }

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
    if (event.type == SDL_KEYUP) {
        if (DocumentController* controller = topController()) {
            controller->handleKeyUp(event.key);
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

/**
 * @brief Advance the UI session to reflect the given application state and elapsed time.
 *
 * Synchronizes visible documents with the supplied AppState, reapplies the UI music visual
 * state, updates each active document controller, and performs sound-effect playback cleanup.
 *
 * @param state Current application state to apply to session controllers.
 * @param deltaSeconds Time in seconds since the previous update used to advance controllers.
 */
void Session::update(const AppState& state, float deltaSeconds) {
    if (!initialized_) {
        return;
    }

    (void)showForState(state);
    applyUiMusicVisualState();

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

/**
 * @brief Update and apply the loading overlay state for the UI.
 *
 * Stores the provided loading overlay state and immediately applies it to the active loading overlay document.
 *
 * @param state Loading overlay visual and state parameters to set.
 */
void Session::setLoadingOverlay(const RmlUiLoadingOverlayState& state) {
    loadingOverlayState_ = state;
    loadingOverlay_.apply(loadingOverlayState_);
}

/**
 * @brief Update the stored UI music visual state and apply it to all active documents.
 *
 * Stores the provided visual state and immediately updates each active document's cached
 * UI music bar strip to reflect the new state.
 *
 * @param state The UI music visual state to store and apply.
 */
void Session::setUiMusicVisualState(const game::audio::UiMusicVisualState& state) {
    uiMusicVisualState_ = state;
    applyUiMusicVisualState();
}

/**
 * @brief Plays a one-shot WAV sound described by a SoundRequest if audio is available.
 *
 * Resolves the request's relativePath to an absolute filesystem path and, if the file
 * exists and audio is ready, plays it once at the requested volume (clamped to the
 * range [0.0, 1.0]). Does nothing if audio is not ready, the relativePath is empty,
 * path resolution fails, or the resolved file does not exist.
 *
 * @param request SoundRequest containing `relativePath` (relative filesystem path to the WAV)
 *                and `volume` (requested playback volume).
 */
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

/**
 * @brief Consumes pending sound requests from the active top document controller and plays each available sound.
 *
 * If there is no top controller or it has no pending requests, the function does nothing.
 */
void Session::playQueuedControllerSounds() {
    if (DocumentController* controller = topController()) {
        for (const SoundRequest& request : controller->consumeSoundRequests()) {
            playResolvedSfx(request);
        }
    }
}

/**
 * Consume and return the next command produced by the top active document controller.
 *
 * If the top controller provides a command of type `ActivateMainMenuAction`, a confirmation
 * sound effect is played as a side effect.
 *
 * @return std::optional<Command> The consumed command if one was available, `std::nullopt` otherwise.
 */
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

/**
 * @brief Pushes a UI screen onto the session's active document stack.
 *
 * Loads and shows the RmlUi document for the specified screen, creates and binds
 * its controller using the provided application state, caches the document's UI
 * music bar strip, updates the internal document stack, and applies the current
 * UI music visual state.
 *
 * @param screen Identifier of the screen/document to push.
 * @param state Application state passed to the controller's bind call.
 * @return true if the document was successfully loaded, its controller created
 *         and bound, and the screen pushed; `false` if the context is null,
 *         the screen is not implemented, the document failed to load, or the
 *         controller could not be created or bound.
 */
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
    ActiveDocument entry{screen, document, std::move(controller)};
    entry.uiMusicBars = cacheUiMusicBarStrip(*document);
    documents_.push_back(std::move(entry));
    applyUiMusicVisualState();
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

/**
 * @brief Apply the given AppState to all active document controllers.
 *
 * For each active document with an associated controller, forwards `state`
 * so the controller can apply or modify state-dependent UI information.
 *
 * @param state Application state object that controllers may read and modify.
 */
void Session::syncState(AppState& state) {
    for (ActiveDocument& entry : documents_) {
        if (entry.controller != nullptr) {
            entry.controller->applyState(state);
        }
    }
}

/**
 * @brief Applies the stored UI music visual state to every active document's cached music bar strip.
 *
 * Iterates the active document stack and updates each entry's cached UI music bar strip using the
 * current `uiMusicVisualState_`.
 */
void Session::applyUiMusicVisualState() {
    for (ActiveDocument& entry : documents_) {
        applyUiMusicBarStrip(entry.uiMusicBars, uiMusicVisualState_);
    }
}

/**
 * @brief Update stored window and drawable dimensions from the window host and apply the GL viewport.
 *
 * Reads window and drawable sizes from the internal window host, clamps each dimension to at least 1,
 * stores them in the session's width/height members, and calls glViewport(0, 0, drawableWidth_, drawableHeight_).
 * If no window host is available, the function returns without changing state.
 */
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

/**
 * @brief Prepares the session's audio support and resolves UI sound file paths.
 *
 * Resolves and stores absolute paths for the scroll and confirm UI sound effects, ensures the SDL audio subsystem is initialized, and marks the session as audio-ready when successful. Sets internal path members to empty and clears the audio-ready flag on failure.
 *
 * @return true if the SDL audio subsystem is available and the session is marked audio-ready, `false` if audio initialization failed.
 */
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
