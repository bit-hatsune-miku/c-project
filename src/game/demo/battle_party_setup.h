#ifndef BATTLE_DEMO_PARTY_SETUP_H
#define BATTLE_DEMO_PARTY_SETUP_H

#include <unordered_map>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#else
struct TTF_Font;
#endif

#include "../core/battle_manager.h"
#include "battle_party_setup_ui.h"

namespace battle::demo {

struct PartySetupResult {
    std::string bossKey;
    std::vector<std::string> partyKeys;
};

class BattlePartySetupScreen {
public:
    BattlePartySetupScreen() = default;
    BattlePartySetupScreen(const BattlePartySetupScreen&) = delete;
    BattlePartySetupScreen& operator=(const BattlePartySetupScreen&) = delete;

    bool initialize(SDL_Renderer* renderer, const BattleDefinition& battleDefinition);
    void shutdown();

    void handleEvent(const SDL_Event& event, int windowWidth, int windowHeight);
    void render(SDL_Renderer* renderer, int windowWidth, int windowHeight);

    bool isActive() const;
    bool shouldSkipSetup() const;
    bool consumeStartRequest(PartySetupResult& outResult);
    bool consumeCancelRequest();
    void complete();

    const PartySetupResult& currentSelection() const;

private:
    struct Entry {
        CharacterDefinition character;
        SDL_Texture* iconTexture = nullptr;
        SDL_Texture* spriteTexture = nullptr;
    };

    struct FontSet {
        TTF_Font* title = nullptr;
        TTF_Font* body = nullptr;
        TTF_Font* small = nullptr;
        TTF_Font* rosterName = nullptr;
        TTF_Font* rosterMeta = nullptr;
        TTF_Font* cjkTitle = nullptr;
        TTF_Font* cjkBody = nullptr;
        TTF_Font* cjkSmall = nullptr;
        TTF_Font* cjkRosterName = nullptr;
        TTF_Font* cjkRosterMeta = nullptr;
    };

    struct TextureSet {
        SDL_Texture* lock = nullptr;
        SDL_Texture* atk = nullptr;
        SDL_Texture* def = nullptr;
        SDL_Texture* spd = nullptr;
        SDL_Texture* bigPlus = nullptr;
        SDL_Texture* remove = nullptr;
    };

    bool validateBattleDefinition() const;
    bool loadRoster(SDL_Renderer* renderer);
    void seedSelection();
    void resetInteractionState();

    bool canStart() const;
    int findRosterIndexByKey(const std::string& characterKey) const;
    bool isLockedCharacter(const std::string& characterKey) const;
    bool isSelectedCharacter(const std::string& characterKey) const;
    int selectedSlotForCharacter(const std::string& characterKey) const;
    bool shouldShowSelectedLockIcon(int slotIndex) const;
    void clampScrollOffset();
    void ensureFocusedEntryVisible();
    void setScrollFromPointerY(float refY);
    void updateHoverState(float windowX, float windowY, int windowWidth, int windowHeight);
    bool isMouseHoverActive() const;
    void toggleFocusedCharacter();
    void setSelectedCharacter(const std::string& characterKey, bool selected);
    void handlePointerDown(float windowX, float windowY, int windowWidth, int windowHeight);
    void releasePointer();
    void syncResult();
    void releaseFonts();
    void loadFontsForScale(float scale);
    void releaseTextures();
    void renderRosterPanel(SDL_Renderer* renderer,
                           const ui::PartySetupGeometry& geometry,
                           const ui::LayoutMetrics& layout);
    void renderSelectedPartyPanel(SDL_Renderer* renderer,
                                  const ui::PartySetupGeometry& geometry,
                                  const ui::LayoutMetrics& layout);
    void renderStartButton(SDL_Renderer* renderer,
                           const ui::PartySetupGeometry& geometry,
                           const ui::LayoutMetrics& layout);
    void renderTransitionOverlay(SDL_Renderer* renderer,
                                 const SDL_FRect& screenRect,
                                 const ui::LayoutMetrics& layout);
    void beginStartTransition();
    bool isStartTransitionActive() const;

    bool initialized_ = false;
    bool startRequested_ = false;
    bool cancelRequested_ = false;
    bool skipSetup_ = false;
    bool startTransitionActive_ = false;
    Uint32 startTransitionTick_ = 0;

    BattleDefinition battleDefinition_;
    std::vector<Entry> roster_;
    std::unordered_map<std::string, int> rosterIndexByKey_;
    std::vector<std::string> selectedKeys_;
    int focusedRosterIndex_ = 0;
    int scrollOffset_ = 0;
    int hoveredRosterIndex_ = -1;
    int hoveredSelectedSlot_ = -1;
    bool hoveredStartButton_ = false;
    Uint32 lastMouseMoveTick_ = 0;
    bool draggingScrollbar_ = false;
    float scrollbarDragGrabOffset_ = 0.0f;

    FontSet fonts_{};
    int fontScalePercent_ = -1;
    TextureSet textures_{};

    PartySetupResult result_;
};

} // namespace battle::demo

#endif
