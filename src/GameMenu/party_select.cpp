#include "menu_shared.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "../window.h"
#include "../game/core/battle_loader.h"
#include "../game/core/battle_manager.h"

namespace {

// ── Roster data ────────────────────────────────────────────────────────────

static const std::vector<std::string> kRosterKeys = {
    "miku", "cupcakke", "lyoo", "iroha", "kaguya",
    "jiafei", "luotianyi", "wechatalipay", "ari", "teto", "sans"
};

struct RosterEntry {
    std::string key;
    battle::CharacterDefinition def;
    bool loaded = false;
};

static std::vector<RosterEntry> sRoster;
static bool sRosterLoaded = false;

void ensureRosterLoaded() {
    if (sRosterLoaded) {
        return;
    }

    sRoster.clear();
    sRoster.reserve(kRosterKeys.size());
    for (const auto& key : kRosterKeys) {
        RosterEntry entry;
        entry.key = key;
        entry.loaded = battle::loader::loadCharacterDefinition(key, entry.def);
        sRoster.push_back(std::move(entry));
    }
    sRosterLoaded = true;
}

// ── Layout constants (1280x720 reference) ──────────────────────────────────

constexpr int kRosterColumns = 3;
constexpr int kMaxParty = 4;

// Header
constexpr SDL_FRect kHeaderPanel{26.0f, 8.0f, 1228.0f, 64.0f};
constexpr SDL_FRect kKickerRect{44.0f, 14.0f, 300.0f, 20.0f};
constexpr SDL_FRect kTitleRect{44.0f, 36.0f, 600.0f, 34.0f};
constexpr float kNeonLineY = 78.0f;

// Roster panel (left)
constexpr SDL_FRect kRosterPanel{26.0f, 88.0f, 796.0f, 568.0f};
constexpr SDL_FRect kRosterLabelRect{44.0f, 92.0f, 200.0f, 22.0f};
constexpr float kCardW = 242.0f;
constexpr float kCardH = 120.0f;
constexpr float kCardGapX = 14.0f;
constexpr float kCardGapY = 14.0f;
constexpr float kGridOriginX = 42.0f;
constexpr float kGridOriginY = 118.0f;

// Party panel (right)
constexpr SDL_FRect kPartyPanel{838.0f, 88.0f, 416.0f, 440.0f};
constexpr SDL_FRect kPartyLabelRect{856.0f, 94.0f, 200.0f, 22.0f};
constexpr SDL_FRect kPartyCountRect{1104.0f, 94.0f, 130.0f, 22.0f};
constexpr float kSlotW = 386.0f;
constexpr float kSlotH = 82.0f;
constexpr float kSlotGap = 14.0f;
constexpr float kSlotOriginX = 854.0f;
constexpr float kSlotOriginY = 126.0f;

// Buttons
constexpr SDL_FRect kBackButton{838.0f, 548.0f, 196.0f, 52.0f};
constexpr SDL_FRect kConfirmButton{1050.0f, 548.0f, 204.0f, 52.0f};
constexpr SDL_FRect kHintRect{838.0f, 616.0f, 416.0f, 24.0f};

// Decorative
constexpr SDL_FRect kRosterAccentLine{kRosterPanel.x + 20.0f, kRosterPanel.y + 16.0f,
                                       kRosterPanel.w - 120.0f, 5.0f};
constexpr SDL_FRect kPartyAccentLine{kPartyPanel.x + 20.0f, kPartyPanel.y + 16.0f,
                                      kPartyPanel.w - 80.0f, 4.0f};

// Cursor indices
constexpr int kCursorBack = 11;
constexpr int kCursorConfirm = 12;

// Stat normalization maxima
constexpr float kMaxHp = 80.0f;
constexpr float kMaxAtk = 45.0f;
constexpr float kMaxSpd = 210.0f;

// ── Colors ─────────────────────────────────────────────────────────────────

SDL_Color classAccentColor(const std::string& cls) {
    if (cls == "DPS") return {255, 107, 107, 255};
    if (cls == "Healer") return {72, 191, 123, 255};
    if (cls == "Tank") return {61, 183, 255, 255};
    if (cls == "Buffer") return {240, 205, 77, 255};
    if (cls == "Vanguard") return {255, 157, 87, 255};
    if (cls == "Shielder") return {167, 139, 250, 255};
    if (cls.find("DOT") != std::string::npos) return {255, 77, 141, 255};
    if (cls.find("aoe") != std::string::npos || cls.find("fua") != std::string::npos)
        return {255, 140, 66, 255};
    return {180, 190, 210, 255};
}

constexpr SDL_Color kHpColor{72, 191, 123, 255};
constexpr SDL_Color kAtkColor{255, 157, 87, 255};
constexpr SDL_Color kSpdColor{61, 183, 255, 255};
constexpr SDL_Color kBarTrackColor{28, 37, 49, 255};
constexpr SDL_Color kGold{240, 205, 77, 255};
constexpr SDL_Color kCyan{110, 240, 255, 255};
constexpr SDL_Color kWarmWhite{245, 239, 226, 255};
constexpr SDL_Color kTextPrimary{238, 242, 248, 255};
constexpr SDL_Color kTextDim{160, 175, 200, 200};

// ── Geometry helpers ───────────────────────────────────────────────────────

SDL_FRect rosterCardRect(int index) {
    const int row = index / kRosterColumns;
    const int col = index % kRosterColumns;
    return SDL_FRect{
        kGridOriginX + static_cast<float>(col) * (kCardW + kCardGapX),
        kGridOriginY + static_cast<float>(row) * (kCardH + kCardGapY),
        kCardW,
        kCardH
    };
}

SDL_FRect partySlotRect(int slotIndex) {
    return SDL_FRect{
        kSlotOriginX,
        kSlotOriginY + static_cast<float>(slotIndex) * (kSlotH + kSlotGap),
        kSlotW,
        kSlotH
    };
}

// ── Selection helpers ──────────────────────────────────────────────────────

int selectionSlot(const std::vector<std::string>& party, const std::string& key) {
    for (size_t i = 0; i < party.size(); ++i) {
        if (party[i] == key) return static_cast<int>(i);
    }
    return -1;
}

void toggleSelection(std::vector<std::string>& party, const std::string& key) {
    auto it = std::find(party.begin(), party.end(), key);
    if (it != party.end()) {
        party.erase(it);
    } else if (static_cast<int>(party.size()) < kMaxParty) {
        party.push_back(key);
    }
}

// ── Stat bar ───────────────────────────────────────────────────────────────

void renderStatBar(SDL_Renderer* renderer, float x, float y, float maxWidth, float height,
                   float fillFraction, const SDL_Color& color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_FRect track{x, y, maxWidth, height};
    SDL_SetRenderDrawColor(renderer, kBarTrackColor.r, kBarTrackColor.g, kBarTrackColor.b, kBarTrackColor.a);
    SDL_RenderFillRectF(renderer, &track);

    if (fillFraction > 0.0f) {
        SDL_FRect fill{x, y, maxWidth * std::min(1.0f, fillFraction), height};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRectF(renderer, &fill);
    }
}

// ── Roster card rendering ──────────────────────────────────────────────────

void renderRosterCard(SDL_Renderer* renderer, const MenuResources& resources,
                      const RosterEntry& entry, const SDL_FRect& rect,
                      bool isCursor, bool selected, int slotNum) {
    if (!entry.loaded) {
        return;
    }

    const SDL_Color accent = classAccentColor(entry.def.characterClass);

    // Glow behind card
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (isCursor) {
        SDL_SetRenderDrawColor(renderer, 240, 205, 77, 44);
        SDL_FRect glow{rect.x - 4.0f, rect.y - 4.0f, rect.w + 8.0f, rect.h + 8.0f};
        SDL_RenderFillRectF(renderer, &glow);
    } else if (selected) {
        SDL_SetRenderDrawColor(renderer, 61, 183, 255, 28);
        SDL_FRect glow{rect.x - 3.0f, rect.y - 3.0f, rect.w + 6.0f, rect.h + 6.0f};
        SDL_RenderFillRectF(renderer, &glow);
    }

    // Card panel
    const SDL_Color fillColor = selected
        ? SDL_Color{16, 28, 48, 245}
        : SDL_Color{10, 18, 30, 230};
    const SDL_Color outlineColor = isCursor
        ? SDL_Color{240, 205, 77, 255}
        : (selected
            ? SDL_Color{61, 183, 255, 220}
            : SDL_Color{41, 69, 97, 180});
    const SDL_Color highlightColor = isCursor
        ? SDL_Color{240, 205, 77, 36}
        : (selected ? SDL_Color{61, 183, 255, 24} : SDL_Color{60, 120, 200, 16});
    const SDL_Color shadowColor{4, 8, 16, 160};

    drawCyberPanel(renderer, rect, 12.0f, 8.0f, fillColor, outlineColor, highlightColor, shadowColor);

    // Class accent strip
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_FRect accentStrip{rect.x + 4.0f, rect.y + 8.0f, 5.0f, rect.h - 16.0f};
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, accent.a);
    SDL_RenderFillRectF(renderer, &accentStrip);

    // Slot number badge (top-right)
    if (selected && slotNum >= 0) {
        constexpr float badgeSize = 24.0f;
        const SDL_FRect badge{rect.x + rect.w - badgeSize - 8.0f, rect.y + 6.0f, badgeSize, badgeSize};
        SDL_SetRenderDrawColor(renderer, kGold.r, kGold.g, kGold.b, kGold.a);
        SDL_RenderFillRectF(renderer, &badge);

#ifdef VN_ENABLE_TTF
        drawTextInRect(renderer, resources.tinyFont, std::to_string(slotNum + 1),
                       SDL_Color{10, 16, 28, 255}, badge, true);
#endif
    }

#ifdef VN_ENABLE_TTF
    // Name
    const SDL_FRect nameRect{rect.x + 18.0f, rect.y + 6.0f, rect.w - 56.0f, 22.0f};
    drawTextInRect(renderer, resources.smallFont, entry.def.title,
                   kTextPrimary, nameRect, false);

    // Class
    const SDL_FRect classRect{rect.x + 18.0f, rect.y + 28.0f, rect.w - 36.0f, 18.0f};
    drawTextInRect(renderer, resources.tinyFont, entry.def.characterClass,
                   SDL_Color{accent.r, accent.g, accent.b, 200}, classRect, false);

    // Stats
    constexpr float statY0 = 54.0f;
    constexpr float statRowH = 18.0f;
    constexpr float labelX = 18.0f;
    constexpr float barX = 52.0f;
    constexpr float barW = 100.0f;
    constexpr float barH = 7.0f;
    constexpr float valX = 160.0f;

    // HP
    drawTextInRect(renderer, resources.tinyFont, "HP",
                   SDL_Color{kHpColor.r, kHpColor.g, kHpColor.b, 200},
                   SDL_FRect{rect.x + labelX, rect.y + statY0, 28.0f, statRowH}, false);
    renderStatBar(renderer, rect.x + barX, rect.y + statY0 + 5.0f, barW, barH,
                  static_cast<float>(entry.def.hp) / kMaxHp, kHpColor);
    drawTextInRect(renderer, resources.tinyFont, std::to_string(entry.def.hp),
                   kTextDim, SDL_FRect{rect.x + valX, rect.y + statY0, 60.0f, statRowH}, false);

    // ATK
    drawTextInRect(renderer, resources.tinyFont, "ATK",
                   SDL_Color{kAtkColor.r, kAtkColor.g, kAtkColor.b, 200},
                   SDL_FRect{rect.x + labelX, rect.y + statY0 + statRowH, 28.0f, statRowH}, false);
    renderStatBar(renderer, rect.x + barX, rect.y + statY0 + statRowH + 5.0f, barW, barH,
                  static_cast<float>(entry.def.atk) / kMaxAtk, kAtkColor);
    drawTextInRect(renderer, resources.tinyFont, std::to_string(entry.def.atk),
                   kTextDim, SDL_FRect{rect.x + valX, rect.y + statY0 + statRowH, 60.0f, statRowH}, false);

    // SPD
    drawTextInRect(renderer, resources.tinyFont, "SPD",
                   SDL_Color{kSpdColor.r, kSpdColor.g, kSpdColor.b, 200},
                   SDL_FRect{rect.x + labelX, rect.y + statY0 + statRowH * 2.0f, 28.0f, statRowH}, false);
    renderStatBar(renderer, rect.x + barX, rect.y + statY0 + statRowH * 2.0f + 5.0f, barW, barH,
                  static_cast<float>(entry.def.spd) / kMaxSpd, kSpdColor);
    drawTextInRect(renderer, resources.tinyFont, std::to_string(entry.def.spd),
                   kTextDim, SDL_FRect{rect.x + valX, rect.y + statY0 + statRowH * 2.0f, 60.0f, statRowH}, false);
#endif

    (void)resources;
}

// ── Party slot rendering ───────────────────────────────────────────────────

void renderPartySlot(SDL_Renderer* renderer, const MenuResources& resources,
                     const SDL_FRect& rect, int slotIndex, const RosterEntry* entry) {
    if (entry == nullptr) {
        // Empty slot
        drawCyberPanel(renderer, rect, 8.0f, 6.0f,
                       SDL_Color{8, 14, 24, 140}, SDL_Color{41, 69, 97, 80},
                       SDL_Color{40, 80, 140, 8}, SDL_Color{4, 8, 16, 60});

#ifdef VN_ENABLE_TTF
        const std::string label = "SLOT " + std::to_string(slotIndex + 1) + "  ·  EMPTY";
        drawTextInRect(renderer, resources.tinyFont, label,
                       SDL_Color{60, 80, 110, 140}, rect, true);
#endif
        return;
    }

    const SDL_Color accent = classAccentColor(entry->def.characterClass);

    drawCyberPanel(renderer, rect, 8.0f, 6.0f,
                   SDL_Color{14, 24, 40, 240}, SDL_Color{61, 183, 255, 180},
                   SDL_Color{61, 183, 255, 20}, SDL_Color{4, 8, 16, 120});

    // Class accent
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_FRect accentStrip{rect.x + 4.0f, rect.y + 6.0f, 4.0f, rect.h - 12.0f};
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, accent.a);
    SDL_RenderFillRectF(renderer, &accentStrip);

    // Slot number badge
    const SDL_FRect numBadge{rect.x + 14.0f, rect.y + 8.0f, 22.0f, 22.0f};
    SDL_SetRenderDrawColor(renderer, kGold.r, kGold.g, kGold.b, kGold.a);
    SDL_RenderFillRectF(renderer, &numBadge);

#ifdef VN_ENABLE_TTF
    drawTextInRect(renderer, resources.tinyFont, std::to_string(slotIndex + 1),
                   SDL_Color{10, 16, 28, 255}, numBadge, true);

    // Name
    drawTextInRect(renderer, resources.smallFont, entry->def.title,
                   kTextPrimary,
                   SDL_FRect{rect.x + 44.0f, rect.y + 6.0f, 220.0f, 24.0f}, false);

    // Class
    drawTextInRect(renderer, resources.tinyFont, entry->def.characterClass,
                   SDL_Color{accent.r, accent.g, accent.b, 200},
                   SDL_FRect{rect.x + 44.0f, rect.y + 30.0f, 140.0f, 18.0f}, false);

    // Compact stats row
    const float statsY = rect.y + 54.0f;
    drawTextInRect(renderer, resources.tinyFont,
                   "HP " + std::to_string(entry->def.hp),
                   SDL_Color{kHpColor.r, kHpColor.g, kHpColor.b, 200},
                   SDL_FRect{rect.x + 44.0f, statsY, 68.0f, 18.0f}, false);
    drawTextInRect(renderer, resources.tinyFont,
                   "ATK " + std::to_string(entry->def.atk),
                   SDL_Color{kAtkColor.r, kAtkColor.g, kAtkColor.b, 200},
                   SDL_FRect{rect.x + 122.0f, statsY, 78.0f, 18.0f}, false);
    drawTextInRect(renderer, resources.tinyFont,
                   "SPD " + std::to_string(entry->def.spd),
                   SDL_Color{kSpdColor.r, kSpdColor.g, kSpdColor.b, 200},
                   SDL_FRect{rect.x + 210.0f, statsY, 78.0f, 18.0f}, false);
#endif

    (void)resources;
}

// ── Cursor navigation ──────────────────────────────────────────────────────

void navigateCursor(int& cursor, SDL_Keycode key) {
    const int lastCharIndex = static_cast<int>(kRosterKeys.size()) - 1;

    if (cursor <= lastCharIndex) {
        const int row = cursor / kRosterColumns;
        const int col = cursor % kRosterColumns;
        const int lastRow = lastCharIndex / kRosterColumns;

        switch (key) {
            case SDLK_UP:
                if (row > 0) {
                    cursor = std::min((row - 1) * kRosterColumns + col, lastCharIndex);
                }
                break;
            case SDLK_DOWN:
                if (row < lastRow) {
                    cursor = std::min((row + 1) * kRosterColumns + col, lastCharIndex);
                } else {
                    cursor = (col <= 0) ? kCursorBack : kCursorConfirm;
                }
                break;
            case SDLK_LEFT:
                if (col > 0 && cursor > 0) {
                    cursor--;
                }
                break;
            case SDLK_RIGHT:
                if (col < kRosterColumns - 1 && cursor < lastCharIndex) {
                    cursor++;
                }
                break;
            default:
                break;
        }
    } else {
        const int lastRow = static_cast<int>(kRosterKeys.size() - 1) / kRosterColumns;

        switch (key) {
            case SDLK_UP:
                cursor = (cursor == kCursorBack)
                    ? std::min(lastRow * kRosterColumns, static_cast<int>(kRosterKeys.size()) - 1)
                    : std::min(lastRow * kRosterColumns + 1, static_cast<int>(kRosterKeys.size()) - 1);
                break;
            case SDLK_LEFT:
                cursor = kCursorBack;
                break;
            case SDLK_RIGHT:
                cursor = kCursorConfirm;
                break;
            default:
                break;
        }
    }
}

// ── Button rendering ───────────────────────────────────────────────────────

void renderActionButton(SDL_Renderer* renderer, const MenuResources& resources,
                        const SDL_FRect& rect, const char* label,
                        bool selected, bool enabled) {
    const Uint8 baseAlpha = enabled ? static_cast<Uint8>(255) : static_cast<Uint8>(100);

    const SDL_Color fillColor = selected
        ? SDL_Color{156, 246, 255, baseAlpha}
        : SDL_Color{18, 44, 78, static_cast<Uint8>(std::min(228, static_cast<int>(baseAlpha)))};
    const SDL_Color outlineColor = selected
        ? SDL_Color{255, 116, 204, baseAlpha}
        : SDL_Color{72, 232, 255, static_cast<Uint8>(std::min(220, static_cast<int>(baseAlpha)))};
    const SDL_Color highlightColor = selected
        ? SDL_Color{255, 255, 255, 58}
        : SDL_Color{110, 208, 255, 34};
    const SDL_Color shadowColor = selected
        ? SDL_Color{22, 6, 40, 210}
        : SDL_Color{4, 8, 20, 190};

    drawJaggedButtonPanel(renderer, rect, 18.0f, 22.0f, 18.0f,
                          fillColor, outlineColor, highlightColor, shadowColor);

    // Accent bar inside button
    const SDL_FRect leftTag{rect.x + 10.0f, rect.y + 8.0f, 56.0f, rect.h - 16.0f};
    drawSlantedPanel(renderer, leftTag, 12.0f,
                     selected ? SDL_Color{255, 104, 194, 230} : SDL_Color{64, 138, 214, 160});

#ifdef VN_ENABLE_TTF
    const SDL_FRect labelRect{rect.x + 68.0f, rect.y, rect.w - 80.0f, rect.h};
    drawShadowedTextInRect(renderer, resources.smallFont, label,
                           selected ? SDL_Color{12, 28, 48, baseAlpha} : SDL_Color{216, 248, 255, baseAlpha},
                           labelRect, false);
#endif

    (void)resources;
}

// ── Controller ─────────────────────────────────────────────────────────────

class PartySelectController {
public:
    void render(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                int windowWidth, int windowHeight) const {
        ensureRosterLoaded();

        renderBackgroundCover(renderer, resources.background, windowWidth, windowHeight);

        const bool usingCanvas = beginMenuCanvas(renderer, resources.menuCanvas);
        const bool usingReference = !usingCanvas &&
            beginReferenceLayout(renderer, windowWidth, windowHeight);

        renderUI(renderer, resources, state);

        if (usingCanvas) {
            endMenuCanvas(renderer, resources.menuCanvas, windowWidth, windowHeight);
        } else if (usingReference) {
            endReferenceLayout(renderer);
        }
    }

    void handleEvent(AppState& state, Window& window, const SDL_Event& event,
                     int windowWidth, int windowHeight) const {
        ensureRosterLoaded();

        if (event.type == SDL_MOUSEMOTION) {
            SDL_FPoint pt{};
            if (mapWindowPointToReference(static_cast<float>(event.motion.x),
                                          static_cast<float>(event.motion.y),
                                          windowWidth, windowHeight, pt)) {
                updateCursorFromMouse(state, pt.x, pt.y);
            }
            return;
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            SDL_FPoint pt{};
            if (!mapWindowPointToReference(static_cast<float>(event.button.x),
                                           static_cast<float>(event.button.y),
                                           windowWidth, windowHeight, pt)) {
                return;
            }
            updateCursorFromMouse(state, pt.x, pt.y);
            activateCursor(state);
            return;
        }

        if (event.type != SDL_KEYDOWN) {
            return;
        }

        const auto sym = event.key.keysym.sym;

        if (sym == SDLK_UP || sym == SDLK_DOWN || sym == SDLK_LEFT || sym == SDLK_RIGHT ||
            sym == SDLK_w || sym == SDLK_s || sym == SDLK_a || sym == SDLK_d) {
            SDL_Keycode mapped = sym;
            if (sym == SDLK_w) mapped = SDLK_UP;
            if (sym == SDLK_s) mapped = SDLK_DOWN;
            if (sym == SDLK_a) mapped = SDLK_LEFT;
            if (sym == SDLK_d) mapped = SDLK_RIGHT;
            navigateCursor(state.partySelectCursor, mapped);
            return;
        }

        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE) {
            activateCursor(state);
            return;
        }

        if (sym == SDLK_ESCAPE || sym == SDLK_BACKSPACE) {
            state.screen = ScreenState::MainMenu;
            state.mainSelection = MainMenuAction::Battle;
            return;
        }
    }

private:
    void updateCursorFromMouse(AppState& state, float x, float y) const {
        for (int i = 0; i < static_cast<int>(sRoster.size()); ++i) {
            if (pointInRect(x, y, rosterCardRect(i))) {
                state.partySelectCursor = i;
                return;
            }
        }

        if (pointInRect(x, y, kBackButton)) {
            state.partySelectCursor = kCursorBack;
        } else if (pointInRect(x, y, kConfirmButton)) {
            state.partySelectCursor = kCursorConfirm;
        }
    }

    void activateCursor(AppState& state) const {
        const int cursor = state.partySelectCursor;

        if (cursor >= 0 && cursor < static_cast<int>(sRoster.size())) {
            toggleSelection(state.selectedPartyKeys, sRoster[static_cast<size_t>(cursor)].key);
            return;
        }

        if (cursor == kCursorBack) {
            state.screen = ScreenState::MainMenu;
            state.mainSelection = MainMenuAction::Battle;
            return;
        }

        if (cursor == kCursorConfirm && !state.selectedPartyKeys.empty()) {
            beginBattleDemo(state);
            return;
        }
    }

    void renderUI(SDL_Renderer* renderer, const MenuResources& resources,
                  const AppState& state) const {
        // Dark overlay for contrast
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 5, 8, 16, 100);
        SDL_FRect fullScreen{0.0f, 0.0f, 1280.0f, 720.0f};
        SDL_RenderFillRectF(renderer, &fullScreen);

        // ── Header ──
        drawCyberPanel(renderer, kHeaderPanel, 32.0f, 20.0f,
                       SDL_Color{10, 18, 32, 240}, SDL_Color{179, 106, 56, 200},
                       SDL_Color{255, 160, 80, 20}, SDL_Color{4, 8, 16, 160});

#ifdef VN_ENABLE_TTF
        drawTextInRect(renderer, resources.tinyFont, "PARTY SETUP",
                       kGold, kKickerRect, false);
        drawShadowedTextInRect(renderer, resources.subtitleFont, "SELECT YOUR PARTY",
                               kWarmWhite, kTitleRect, false);
#endif

        // Neon accent lines
        drawNeonLine(renderer,
                     SDL_FPoint{26.0f, kNeonLineY}, SDL_FPoint{822.0f, kNeonLineY},
                     SDL_Color{255, 118, 200, 72}, SDL_Color{255, 148, 210, 160});
        drawNeonLine(renderer,
                     SDL_FPoint{838.0f, kNeonLineY}, SDL_FPoint{1254.0f, kNeonLineY},
                     SDL_Color{72, 212, 255, 72}, SDL_Color{108, 244, 255, 160});

        // ── Roster panel (left) ──
        drawCyberPanel(renderer, kRosterPanel, 36.0f, 24.0f,
                       SDL_Color{8, 14, 24, 226}, SDL_Color{41, 69, 97, 180},
                       SDL_Color{60, 140, 220, 14}, SDL_Color{3, 6, 14, 180});

        // Decorative accent inside roster panel
        drawSlantedPanel(renderer, kRosterAccentLine, 14.0f, SDL_Color{255, 118, 200, 100});

#ifdef VN_ENABLE_TTF
        drawTextInRect(renderer, resources.tinyFont, "ROSTER",
                       kCyan, kRosterLabelRect, false);
#endif

        // Roster cards
        for (int i = 0; i < static_cast<int>(sRoster.size()); ++i) {
            const SDL_FRect cardRect = rosterCardRect(i);
            const bool isCursor = state.partySelectCursor == i;
            const int slot = selectionSlot(state.selectedPartyKeys,
                                           sRoster[static_cast<size_t>(i)].key);
            renderRosterCard(renderer, resources, sRoster[static_cast<size_t>(i)],
                             cardRect, isCursor, slot >= 0, slot);
        }

        // ── Party panel (right) ──
        drawCyberPanel(renderer, kPartyPanel, 28.0f, 20.0f,
                       SDL_Color{8, 14, 24, 226}, SDL_Color{41, 69, 97, 180},
                       SDL_Color{60, 140, 220, 14}, SDL_Color{3, 6, 14, 180});

        // Decorative accent
        drawSlantedPanel(renderer, kPartyAccentLine, 12.0f, SDL_Color{72, 232, 255, 90});

#ifdef VN_ENABLE_TTF
        drawTextInRect(renderer, resources.tinyFont, "YOUR PARTY",
                       kCyan, kPartyLabelRect, false);
        const std::string countStr =
            std::to_string(state.selectedPartyKeys.size()) + " / " + std::to_string(kMaxParty);
        drawTextInRect(renderer, resources.tinyFont, countStr,
                       SDL_Color{kGold.r, kGold.g, kGold.b, 200}, kPartyCountRect, false);
#endif

        // Party slots
        for (int i = 0; i < kMaxParty; ++i) {
            const SDL_FRect slotRect = partySlotRect(i);
            const RosterEntry* entry = nullptr;
            if (i < static_cast<int>(state.selectedPartyKeys.size())) {
                for (const auto& r : sRoster) {
                    if (r.key == state.selectedPartyKeys[static_cast<size_t>(i)]) {
                        entry = &r;
                        break;
                    }
                }
            }
            renderPartySlot(renderer, resources, slotRect, i, entry);
        }

        // ── Action buttons ──
        const bool backSelected = state.partySelectCursor == kCursorBack;
        const bool confirmSelected = state.partySelectCursor == kCursorConfirm;
        const bool canConfirm = !state.selectedPartyKeys.empty();

        renderActionButton(renderer, resources, kBackButton, "BACK", backSelected, true);
        renderActionButton(renderer, resources, kConfirmButton, "DEPLOY", confirmSelected, canConfirm);

        // Hint text
#ifdef VN_ENABLE_TTF
        drawTextInRect(renderer, resources.tinyFont, "ENTER SELECT  ·  ESC BACK",
                       SDL_Color{100, 120, 150, 130}, kHintRect, true);
#endif
    }
};

const PartySelectController& partySelectController() {
    static const PartySelectController controller;
    return controller;
}

}  // namespace

void renderPartySelect(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                       int windowWidth, int windowHeight) {
    partySelectController().render(renderer, resources, state, windowWidth, windowHeight);
}

void handlePartySelectEvent(AppState& state, Window& window, const SDL_Event& event,
                            int windowWidth, int windowHeight) {
    partySelectController().handleEvent(state, window, event, windowWidth, windowHeight);
}
