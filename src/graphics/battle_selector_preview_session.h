#pragma once

#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>

#include "RmlUi_Platform_SDL.h"

#include "../game/audio/wav_one_shot.h"
#include "../game/core/battle_manager.h"
#include "../game/core/player_progression.h"
#include "rmlui_sdl_gl_renderer.h"

class Window;

namespace graphics::preview {

class BattleSelectorPreviewSession {
public:
    ~BattleSelectorPreviewSession();

    bool initialize(Window& window);
    void shutdown();

    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render();

    bool isInitialized() const { return initialized_; }

private:
    enum class FocusZone {
        Carousel,
        Finale,
    };

    struct EventListenerBinding {
        Rml::Element* element = nullptr;
        Rml::EventId eventId = Rml::EventId::Invalid;
        bool capturePhase = false;
        std::unique_ptr<Rml::EventListener> listener;
    };

    struct Entry {
        battle::BattleDefinition battle;
        battle::BossDefinition boss;
        std::string spritePath;
        std::string tag;
        std::string instructionHint;
        bool defeated = false;
    };

    void detachEventListeners();
    bool loadFonts() const;
    bool initializeAudio();
    void updateViewportFromWindow();
    void applyContextScale();
    bool loadEntries();
    bool loadDocument();
    void cacheElements();
    void buildTrack();
    void attachListeners();
    void applySelection();
    void updateAnimatedLayout();
    void updateInfoPanel() const;
    void setSelectedIndex(std::size_t index, bool shouldPlayScrollSfx);
    void moveSelection(int delta, bool shouldPlayScrollSfx);
    void setFocusZone(FocusZone zone, bool shouldPlayScrollSfx);
    void activateSelected();
    void activateFinale();
    void updateHeldInput(float deltaSeconds);
    void updateVisualSelection(float deltaSeconds);
    void updateToast(float deltaSeconds);
    void playScrollSfx();
    void playConfirmSfx();
    void showToast(const std::string& message);

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
    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    game::audio::WavOneShotPlayer sfxPlayer_;
    std::vector<Entry> entries_;
    battle::PlayerProgression progression_;
    std::size_t selectedIndex_ = 0;
    float visualSelectionIndex_ = 0.0f;
    float targetVisualSelectionIndex_ = 0.0f;
    FocusZone focusZone_ = FocusZone::Carousel;
    bool negativeHeld_ = false;
    bool positiveHeld_ = false;
    float holdNegativeElapsed_ = 0.0f;
    float holdPositiveElapsed_ = 0.0f;
    float toastTimer_ = 0.0f;
    std::string scrollSfxPath_;
    std::string confirmSfxPath_;

    Rml::Element* trackElement_ = nullptr;
    Rml::Element* rankValueElement_ = nullptr;
    Rml::Element* infoPortraitElement_ = nullptr;
    Rml::Element* infoPortraitImageElement_ = nullptr;
    Rml::Element* infoNameElement_ = nullptr;
    Rml::Element* infoBattleElement_ = nullptr;
    Rml::Element* infoCopyElement_ = nullptr;
    Rml::Element* infoHpElement_ = nullptr;
    Rml::Element* infoAtkElement_ = nullptr;
    Rml::Element* infoSpdElement_ = nullptr;
    Rml::Element* infoHintElement_ = nullptr;
    Rml::Element* finaleButtonElement_ = nullptr;
    Rml::Element* toastElement_ = nullptr;
    std::vector<Rml::Element*> cardElements_;
    std::vector<Rml::Element*> cardPortraitImageElements_;
};

}  // namespace graphics::preview
