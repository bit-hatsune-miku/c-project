#pragma once

#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "RmlUi_Platform_SDL.h"

#include "../game/audio/wav_one_shot.h"
#include "../game/core/battle_manager.h"
#include "rmlui_sdl_gl_renderer.h"

class Window;

namespace graphics::preview {

class PartyLoaderPreviewSession {
public:
    ~PartyLoaderPreviewSession();

    bool initialize(Window& window);
    void shutdown();

    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render();

    bool isInitialized() const { return initialized_; }

private:
    struct Entry {
        battle::CharacterDefinition character;
        std::string rosterImagePath;
        std::string stageImagePath;
    };

    bool loadFonts() const;
    bool initializeAudio();
    void updateViewportFromWindow();
    void applyContextScale();
    bool loadPreviewData();
    bool loadDocument();
    void cacheElements();
    void refreshDocument();
    void rebuildRosterMarkup();
    void rebuildSlotsMarkup();
    void cacheDynamicElements();
    void updateStaticCopy() const;
    void updateStageCopy() const;
    void updateScrollbar();
    void updateAnimationClasses() const;
    void moveFocus(int delta, bool shouldPlayScrollSfx);
    void pageFocus(int deltaRows, bool shouldPlayScrollSfx);
    void toggleFocusedCharacter();
    void toggleCharacter(const std::string& key);
    void removeSelectedAt(int slotIndex);
    void launchBattlePreview();
    void beginScrollbarDrag(float mouseY, bool centerThumb);
    void stopScrollbarDrag();
    void updateScrollFromPointer(float mouseY);
    void ensureFocusedVisible();
    void clampScrollOffset();
    bool pointInElement(float x, float y, Rml::Element* element) const;
    int hoveredRosterIndex(float x, float y) const;
    int clickedSlotIndex(float x, float y) const;
    int selectionCapacity() const;
    int maxScrollOffset() const;
    int selectedSlotForKey(const std::string& key) const;
    bool isLockedKey(const std::string& key) const;
    bool canStart() const;
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
    float lastMouseX_ = 0.0f;
    float lastMouseY_ = 0.0f;
    bool draggingScrollbar_ = false;
    float scrollbarDragGrabOffset_ = 0.0f;
    float toastTimer_ = 0.0f;
    float confirmTimer_ = 0.0f;
    float slotSettleTimer_ = 0.0f;
    int focusAnimationDirection_ = 0;

    SystemInterface_SDL systemInterface_;
    std::unique_ptr<RmlUiSdlGlRenderInterface> renderInterface_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    game::audio::WavOneShotPlayer sfxPlayer_;
    std::string scrollSfxPath_;
    std::string confirmSfxPath_;

    battle::BattleDefinition battleDefinition_{};
    battle::BossDefinition bossDefinition_{};
    std::vector<Entry> roster_;
    std::vector<std::string> selectedKeys_;
    std::string bossDescription_;
    int focusedRosterIndex_ = 0;
    int scrollOffset_ = 0;

    Rml::Element* rosterTrackElement_ = nullptr;
    Rml::Element* rosterViewportElement_ = nullptr;
    Rml::Element* scrollbarTrackElement_ = nullptr;
    Rml::Element* scrollbarThumbElement_ = nullptr;
    Rml::Element* stagePanelElement_ = nullptr;
    Rml::Element* stageKickerElement_ = nullptr;
    Rml::Element* stageTitleElement_ = nullptr;
    Rml::Element* stageDescriptionElement_ = nullptr;
    Rml::Element* stageCapacityElement_ = nullptr;
    Rml::Element* stageModeElement_ = nullptr;
    Rml::Element* slotGridElement_ = nullptr;
    Rml::Element* statusCodeElement_ = nullptr;
    Rml::Element* statusTitleElement_ = nullptr;
    Rml::Element* statusCopyElement_ = nullptr;
    Rml::Element* rosterCopyElement_ = nullptr;
    Rml::Element* rosterFooterCopyElement_ = nullptr;
    Rml::Element* startButtonElement_ = nullptr;
    Rml::Element* toastElement_ = nullptr;
    std::vector<Rml::Element*> rosterRowElements_;
    std::vector<int> rosterRowIndices_;
    std::vector<Rml::Element*> slotElements_;
};

}  // namespace graphics::preview
