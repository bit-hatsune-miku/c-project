#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>

#include "RmlUi_Platform_SDL.h"

#include "../GameMenu/menu_shared.h"
#include "../game/audio/wav_one_shot.h"
#include "rmlui_sdl_gl_renderer.h"

class Window;

namespace graphics::preview {

class PlayMenuPreviewSession {
public:
    ~PlayMenuPreviewSession();

    bool initialize(Window& window);
    void shutdown();

    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render();

    bool isInitialized() const { return initialized_; }

private:
    struct EventListenerBinding {
        Rml::Element* element = nullptr;
        Rml::EventId eventId = Rml::EventId::Invalid;
        bool capturePhase = false;
        std::unique_ptr<Rml::EventListener> listener;
    };

    void detachEventListeners();
    bool loadFonts() const;
    void updateViewportFromWindow();
    void applyContextScale();
    bool loadDocument();
    void cacheElements();
    void attachListeners();
    void applyButtonCopy() const;
    void applySelection();
    void updateStatusCopy() const;
    void restartIntroAnimation();
    void setSelection(MainMenuAction action, bool shouldPlayScrollSfx);
    void moveSelection(int delta, bool shouldPlayScrollSfx);
    void activateSelection();
    void updateHeldInput(float deltaSeconds);
    void updateDrift(float deltaSeconds);
    void updateIntro(float deltaSeconds);
    void applyVisualState() const;
    void playScrollSfx();
    void playConfirmSfx();

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool initialized_ = false;
    bool audioInitialized_ = false;
    bool rmlInitialized_ = false;
    bool rmlGlInitialized_ = false;
    int windowWidth_ = 1280;
    int windowHeight_ = 720;
    int drawableWidth_ = 1280;
    int drawableHeight_ = 720;
    SystemInterface_SDL systemInterface_;
    std::unique_ptr<RmlUiSdlGlRenderInterface> renderInterface_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    game::audio::WavOneShotPlayer sfxPlayer_;
    MainMenuAction selection_ = MainMenuAction::Start;
    Rml::Element* whiteoutElement_ = nullptr;
    Rml::Element* discLayerElement_ = nullptr;
    Rml::Element* logoLayerElement_ = nullptr;
    Rml::Element* uiLayerElement_ = nullptr;
    float driftCurrentX_ = 0.0f;
    float driftCurrentY_ = 0.0f;
    float driftTargetX_ = 0.0f;
    float driftTargetY_ = 0.0f;
    float introElapsedSeconds_ = 0.0f;
    bool introActive_ = false;
    bool negativeHeld_ = false;
    bool positiveHeld_ = false;
    float holdNegativeElapsed_ = 0.0f;
    float holdPositiveElapsed_ = 0.0f;
    std::string scrollSfxPath_;
    std::string confirmSfxPath_;
};

}  // namespace graphics::preview
