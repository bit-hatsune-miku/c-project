#include "menu_shared.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "../Settings/settings.h"
#include "../game/save/save.h"

namespace {

constexpr int kReferenceWidth = 1280;
constexpr float kLoadShellWidth = 920.0f;
constexpr float kLoadTitleWidth = 760.0f;
constexpr float kLoadListWidth = 860.0f;
constexpr float kLoadSlotHeight = 76.0f;
constexpr float kLoadSlotGap = 14.0f;
constexpr int kVisibleSlotCount = 5;

constexpr float centeredX(float width) {
    return (static_cast<float>(kReferenceWidth) - width) * 0.5f;
}

constexpr SDL_FRect kLoadTitleBackdropRect{centeredX(kLoadTitleWidth), 34.0f, kLoadTitleWidth, 106.0f};
constexpr SDL_FRect kLoadTitleRect{centeredX(kLoadTitleWidth) + 30.0f, 44.0f, kLoadTitleWidth - 60.0f, 86.0f};
constexpr SDL_FRect kLoadShellRect{centeredX(kLoadShellWidth), 166.0f, kLoadShellWidth, 466.0f};
constexpr SDL_FRect kLoadSectionLabelRect{centeredX(kLoadTitleWidth) + 28.0f, 196.0f, 260.0f, 28.0f};
constexpr SDL_FRect kLoadHintRect{centeredX(kLoadTitleWidth) + 28.0f, 222.0f, 650.0f, 26.0f};
constexpr SDL_FRect kLoadFooterBandRect{centeredX(kLoadTitleWidth), 652.0f, kLoadTitleWidth, 26.0f};
constexpr float kLoadFooterButtonWidth = 280.0f;
constexpr float kLoadFooterButtonGap = 44.0f;
constexpr float kLoadFooterButtonsWidth = kLoadFooterButtonWidth * 2.0f + kLoadFooterButtonGap;
constexpr SDL_FRect kDeleteRowRect{centeredX(kLoadFooterButtonsWidth), 642.0f, kLoadFooterButtonWidth, 52.0f};
constexpr SDL_FRect kBackRowRect{kDeleteRowRect.x + kLoadFooterButtonWidth + kLoadFooterButtonGap,
                                 642.0f, kLoadFooterButtonWidth, 52.0f};
constexpr SDL_FRect kDeleteConfirmShellRect{356.0f, 206.0f, 568.0f, 308.0f};
constexpr SDL_FRect kDeleteConfirmTitleRect{kDeleteConfirmShellRect.x + 36.0f, kDeleteConfirmShellRect.y + 28.0f,
                                            kDeleteConfirmShellRect.w - 72.0f, 46.0f};
constexpr SDL_FRect kDeleteConfirmBodyRect{kDeleteConfirmShellRect.x + 46.0f, kDeleteConfirmShellRect.y + 88.0f,
                                           kDeleteConfirmShellRect.w - 92.0f, 94.0f};
constexpr SDL_FRect kDeleteConfirmFooterBandRect{kDeleteConfirmShellRect.x + 24.0f, kDeleteConfirmShellRect.y + 268.0f,
                                                 kDeleteConfirmShellRect.w - 48.0f, 18.0f};

struct ConfirmButton {
    ConfirmAction action;
    SDL_FRect rect;
};

constexpr std::array<ConfirmButton, 2> kDeleteConfirmButtons{{
    {ConfirmAction::Cancel, SDL_FRect{kDeleteConfirmShellRect.x + 42.0f, kDeleteConfirmShellRect.y + 188.0f, 196.0f, 58.0f}},
    {ConfirmAction::ExitToMainMenu, SDL_FRect{kDeleteConfirmShellRect.x + kDeleteConfirmShellRect.w - 238.0f,
                                              kDeleteConfirmShellRect.y + 188.0f, 196.0f, 58.0f}}
}};

SDL_FRect slotRectForVisibleIndex(int visibleIndex) {
    return SDL_FRect{
        centeredX(kLoadListWidth),
        270.0f + static_cast<float>(visibleIndex) * (kLoadSlotHeight + kLoadSlotGap),
        kLoadListWidth,
        kLoadSlotHeight
    };
}

std::size_t totalSelectableItems(std::size_t slotCount) {
    return slotCount + 2; // + delete button + back button
}

void clampSelection(AppState& state, std::size_t slotCount) {
    const std::size_t total = totalSelectableItems(slotCount);
    if (total == 0) {
        state.loadSelection = 0;
        state.loadSlotSelection = 0;
        return;
    }
    if (state.loadSelection >= total) {
        state.loadSelection = total - 1;
    }
    if (slotCount == 0) {
        state.loadSlotSelection = 0;
    } else if (state.loadSlotSelection >= slotCount) {
        state.loadSlotSelection = slotCount - 1;
    }
}

std::size_t visibleWindowStart(std::size_t selection, std::size_t slotCount) {
    if (slotCount <= static_cast<std::size_t>(kVisibleSlotCount)) {
        return 0;
    }
    if (selection >= slotCount) {
        return slotCount - static_cast<std::size_t>(kVisibleSlotCount);
    }
    if (selection < static_cast<std::size_t>(kVisibleSlotCount)) {
        return 0;
    }
    const std::size_t maxStart = slotCount - static_cast<std::size_t>(kVisibleSlotCount);
    return std::min(selection - static_cast<std::size_t>(kVisibleSlotCount) + 1, maxStart);
}

bool isBackSelected(const AppState& state, std::size_t slotCount) {
    return state.loadSelection == slotCount + 1;
}

bool isDeleteSelected(const AppState& state, std::size_t slotCount) {
    return state.loadSelection == slotCount;
}

const save::SlotInfo* selectedSlot(const AppState& state, const std::vector<save::SlotInfo>& slots) {
    if (slots.empty()) {
        return nullptr;
    }
    const std::size_t selectedIndex =
        state.loadSelection < slots.size() ? state.loadSelection : state.loadSlotSelection;
    if (selectedIndex >= slots.size()) {
        return nullptr;
    }
    return &slots[selectedIndex];
}

bool selectedSlotCanDelete(const AppState& state, const std::vector<save::SlotInfo>& slots) {
    const save::SlotInfo* slot = selectedSlot(state, slots);
    return slot != nullptr && !slot->isAutosave;
}

void startDeletePrompt(AppState& state, const std::vector<save::SlotInfo>& slots);

void moveSelection(AppState& state, int delta, std::size_t slotCount) {
    const std::size_t total = totalSelectableItems(slotCount);
    if (total == 0) {
        state.loadSelection = 0;
        return;
    }

    const int current = static_cast<int>(std::min(state.loadSelection, total - 1));
    int next = (current + delta) % static_cast<int>(total);
    if (next < 0) {
        next += static_cast<int>(total);
    }
    state.loadSelection = static_cast<std::size_t>(next);
    if (state.loadSelection < slotCount) {
        state.loadSlotSelection = state.loadSelection;
    }
}

void activateSelection(AppState& state, const std::vector<save::SlotInfo>& slots) {
    clampSelection(state, slots.size());
    if (isDeleteSelected(state, slots.size())) {
        startDeletePrompt(state, slots);
        return;
    }
    if (isBackSelected(state, slots.size())) {
        state.screen = state.loadReturnScreen;
        return;
    }
    if (state.loadSelection < slots.size()) {
        state.loadSlotSelection = state.loadSelection;
        state.pendingLoadPath = slots[state.loadSelection].path.string();
    }
}

void clearDeletePromptState(AppState& state) {
    state.pendingDeletePath.clear();
    state.pendingDeleteSelection = 0;
    state.confirmSelection = ConfirmAction::Cancel;
}

void startDeletePrompt(AppState& state, const std::vector<save::SlotInfo>& slots) {
    const save::SlotInfo* slot = selectedSlot(state, slots);
    if (slot == nullptr) {
        state.noticeText = "Select a manual save first.";
        state.noticeTimer = 2.0f;
        return;
    }
    if (slot->isAutosave) {
        state.noticeText = "Autosave cannot be deleted.";
        state.noticeTimer = 2.0f;
        return;
    }

    state.pendingDeletePath = slot->path.string();
    state.pendingDeleteSelection = state.loadSlotSelection;
    state.confirmSelection = ConfirmAction::Cancel;
    state.screen = ScreenState::LoadConfirmDelete;
}

void applySelectionAfterDelete(AppState& state, std::size_t remainingSlotCount) {
    if (remainingSlotCount == 0) {
        state.loadSelection = 0;
        state.loadSlotSelection = 0;
    } else if (state.pendingDeleteSelection < remainingSlotCount) {
        state.loadSelection = state.pendingDeleteSelection;
        state.loadSlotSelection = state.pendingDeleteSelection;
    } else {
        state.loadSelection = remainingSlotCount - 1;
        state.loadSlotSelection = remainingSlotCount - 1;
    }
}

class LoadMenuController {
public:
    void render(SDL_Renderer* renderer, const MenuResources& resources, AppState& state,
                int windowWidth, int windowHeight) {
        const std::vector<save::SlotInfo>& slots = currentSlots(state);

        if (state.loadReturnScreen == ScreenState::PauseMenu) {
            renderPauseBackdrop(renderer, windowWidth, windowHeight);
        } else {
            renderBackgroundCover(renderer, resources.background, windowWidth, windowHeight);
        }

        const bool usingReferenceLayout = beginReferenceLayout(renderer, windowWidth, windowHeight);
        renderOverlay(renderer, resources, state, slots);
        if (state.screen == ScreenState::LoadConfirmDelete) {
            renderDeleteConfirmOverlay(renderer, resources, state);
        }
        if (usingReferenceLayout) {
            endReferenceLayout(renderer);
        }
    }

    void handleEvent(AppState& state, Window& window, const SDL_Event& event,
                     int windowWidth, int windowHeight) {
        const std::vector<save::SlotInfo>& slots = currentSlots(state);

        if (state.screen == ScreenState::LoadConfirmDelete) {
            handleDeleteConfirmEvent(state, window, event, windowWidth, windowHeight);
            return;
        }

        if (event.type == SDL_MOUSEMOTION ||
            (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
            SDL_FPoint loadPoint{};
            if (!mapWindowPointToReference(
                    static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x),
                    static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y),
                    windowWidth, windowHeight, loadPoint)) {
                return;
            }

            if (pointInRect(loadPoint.x, loadPoint.y, kDeleteRowRect)) {
                state.loadSelection = slots.size();
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    startDeletePrompt(state, slots);
                }
                return;
            }

            const std::size_t visibleSelection =
                state.loadSelection < slots.size() ? state.loadSelection : state.loadSlotSelection;
            const std::size_t start = visibleWindowStart(visibleSelection, slots.size());
            const std::size_t visibleCount = std::min<std::size_t>(kVisibleSlotCount, slots.size() - std::min(start, slots.size()));
            for (std::size_t i = 0; i < visibleCount; ++i) {
                if (pointInRect(loadPoint.x, loadPoint.y, slotRectForVisibleIndex(static_cast<int>(i)))) {
                    state.loadSelection = start + i;
                    state.loadSlotSelection = start + i;
                    if (event.type == SDL_MOUSEBUTTONDOWN) {
                        activateSelection(state, slots);
                    }
                    return;
                }
            }

            if (pointInRect(loadPoint.x, loadPoint.y, kBackRowRect)) {
                state.loadSelection = slots.size() + 1;
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    state.screen = state.loadReturnScreen;
                }
            }
            return;
        }

        if (event.type != SDL_KEYDOWN) {
            return;
        }

        if (event.key.keysym.sym == SDLK_ESCAPE) {
            state.screen = state.loadReturnScreen;
            return;
        }
        if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
            moveSelection(state, -1, slots.size());
            return;
        }
        if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
            moveSelection(state, 1, slots.size());
            return;
        }
        if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) {
            if (state.loadSelection < slots.size()) {
                state.loadSelection = slots.size();
            } else if (isBackSelected(state, slots.size())) {
                state.loadSelection = slots.size();
            }
            return;
        }
        if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) {
            if (state.loadSelection < slots.size()) {
                state.loadSelection = slots.size() + 1;
            } else if (isDeleteSelected(state, slots.size())) {
                state.loadSelection = slots.size() + 1;
            }
            return;
        }
        if (event.key.keysym.sym == SDLK_DELETE) {
            startDeletePrompt(state, slots);
            return;
        }
        if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
            event.key.keysym.sym == SDLK_SPACE) {
            activateSelection(state, slots);
            return;
        }
        if (event.key.keysym.sym == SDLK_F11) {
            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
        }
    }

    void open(AppState& state) {
        refreshSlots(state);
    }

private:
    const std::vector<save::SlotInfo>& currentSlots(AppState& state) {
        if (!slotsLoaded_) {
            refreshSlots(state);
        }
        return cachedSlots_;
    }

    void refreshSlots(AppState& state) {
        cachedSlots_ = save::listSlots();
        slotsLoaded_ = true;
        clampSelection(state, cachedSlots_.size());
    }

    void renderOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                       const std::vector<save::SlotInfo>& slots) const {
        drawSlantedPanel(renderer,
                         SDL_FRect{kLoadShellRect.x - 18.0f, kLoadTitleBackdropRect.y + 18.0f,
                                   18.0f, kBackRowRect.y - (kLoadTitleBackdropRect.y + 18.0f)},
                         6.0f,
                         SDL_Color{255, 102, 196, 164});
        drawSlantedPanel(renderer,
                         SDL_FRect{kLoadShellRect.x + kLoadShellRect.w + 6.0f, kLoadTitleBackdropRect.y + 48.0f,
                                   16.0f, kBackRowRect.y - (kLoadTitleBackdropRect.y + 48.0f)},
                         5.0f,
                         SDL_Color{72, 224, 255, 168});
        drawNeonLine(renderer,
                     SDL_FPoint{kLoadTitleBackdropRect.x + 34.0f, kLoadTitleBackdropRect.y + kLoadTitleBackdropRect.h + 26.0f},
                     SDL_FPoint{kLoadTitleBackdropRect.x + kLoadTitleBackdropRect.w - 30.0f, kLoadTitleBackdropRect.y + kLoadTitleBackdropRect.h + 26.0f},
                     SDL_Color{255, 74, 164, 82}, SDL_Color{255, 128, 210, 180});

        drawCyberPanel(renderer, kLoadTitleBackdropRect, 44.0f, 28.0f,
                       SDL_Color{18, 30, 56, 240}, SDL_Color{90, 238, 255, 255},
                       SDL_Color{116, 220, 255, 42}, SDL_Color{4, 8, 20, 220});
        drawCyberPanel(renderer, kLoadShellRect, 52.0f, 34.0f,
                       SDL_Color{10, 20, 42, 236}, SDL_Color{80, 228, 255, 244},
                       SDL_Color{72, 156, 255, 34}, SDL_Color{3, 8, 18, 220});
        drawCyberPanel(renderer, kLoadFooterBandRect, 16.0f, 12.0f,
                       SDL_Color{18, 36, 64, 230}, SDL_Color{72, 232, 255, 220},
                       SDL_Color{255, 255, 255, 24}, SDL_Color{4, 8, 16, 138});

#ifdef VN_ENABLE_TTF
        drawShadowedTextInRect(renderer, resources.titleFont, "Load Game",
                               SDL_Color{240, 248, 255, 255}, kLoadTitleRect);
        drawTextInRect(renderer, resources.smallFont, "SAVE SLOTS",
                       SDL_Color{110, 240, 255, 255}, kLoadSectionLabelRect, false);
        drawTextInRect(renderer, resources.smallFont,
                       "ENTER TO LOAD. DELETE REMOVES THE SELECTED MANUAL SAVE.",
                       SDL_Color{214, 246, 255, 255}, kLoadHintRect, false);
#endif

        if (slots.empty()) {
            drawCyberPanel(renderer, SDL_FRect{centeredX(620.0f), 330.0f, 620.0f, 120.0f}, 24.0f, 18.0f,
                           SDL_Color{24, 44, 78, 220}, SDL_Color{255, 118, 200, 210},
                           SDL_Color{106, 224, 255, 30}, SDL_Color{4, 8, 20, 120});
#ifdef VN_ENABLE_TTF
            drawShadowedTextInRect(renderer, resources.itemFont, "No Save Files Found",
                                   SDL_Color{236, 246, 252, 255},
                                   SDL_FRect{centeredX(620.0f), 350.0f, 620.0f, 32.0f});
            drawTextInRect(renderer, resources.smallFont,
                           "Start the story once to create the first autosave.",
                           SDL_Color{198, 246, 255, 255},
                           SDL_FRect{centeredX(620.0f), 390.0f, 620.0f, 24.0f});
#endif
        } else {
            const std::size_t visibleSelection =
                state.loadSelection < slots.size() ? state.loadSelection : state.loadSlotSelection;
            const std::size_t start = visibleWindowStart(visibleSelection, slots.size());
            const std::size_t visibleCount = std::min<std::size_t>(kVisibleSlotCount, slots.size() - start);
            for (std::size_t i = 0; i < visibleCount; ++i) {
                const std::size_t slotIndex = start + i;
                const bool selected = state.loadSlotSelection == slotIndex;
                const SDL_FRect slotRect = slotRectForVisibleIndex(static_cast<int>(i));

                drawJaggedButtonPanel(renderer, slotRect, 24.0f, 28.0f, 22.0f,
                                      selected ? SDL_Color{156, 246, 255, 255} : SDL_Color{18, 44, 78, 236},
                                      selected ? SDL_Color{255, 116, 204, 255} : SDL_Color{72, 232, 255, 224},
                                      selected ? SDL_Color{255, 255, 255, 58} : SDL_Color{110, 208, 255, 34},
                                      selected ? SDL_Color{22, 6, 40, 214} : SDL_Color{4, 8, 20, 194});

                const SDL_FRect badgeRect{slotRect.x + 16.0f, slotRect.y + 12.0f, 120.0f, slotRect.h - 24.0f};
                drawCyberPanel(renderer, badgeRect, 12.0f, 10.0f,
                               selected ? SDL_Color{255, 240, 150, 255} : SDL_Color{32, 90, 146, 240},
                               selected ? SDL_Color{255, 116, 204, 232} : SDL_Color{72, 232, 255, 220},
                               selected ? SDL_Color{255, 255, 255, 48} : SDL_Color{108, 208, 255, 26},
                               selected ? SDL_Color{20, 8, 38, 180} : SDL_Color{8, 18, 34, 180});

#ifdef VN_ENABLE_TTF
                drawTextInRect(renderer, resources.smallFont,
                               slots[slotIndex].isAutosave ? "AUTOSAVE" : "MANUAL",
                               selected ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                               badgeRect);
                drawShadowedTextInRect(renderer, resources.itemFont, slots[slotIndex].label,
                                       selected ? SDL_Color{12, 28, 48, 255} : SDL_Color{236, 246, 252, 255},
                                       SDL_FRect{slotRect.x + 156.0f, slotRect.y + 10.0f, slotRect.w - 182.0f, 28.0f},
                                       false);
                drawTextInRect(renderer, resources.smallFont,
                               save::formatTimestampForDisplay(slots[slotIndex].timestamp),
                               selected ? SDL_Color{12, 28, 48, 255} : SDL_Color{198, 246, 255, 255},
                               SDL_FRect{slotRect.x + 156.0f, slotRect.y + 40.0f, slotRect.w - 182.0f, 22.0f},
                               false);
#endif
            }
        }

        const bool deleteEnabled = selectedSlotCanDelete(state, slots);
        const bool deleteSelected = isDeleteSelected(state, slots.size());
        drawJaggedButtonPanel(renderer, kDeleteRowRect, 22.0f, 28.0f, 22.0f,
                              deleteSelected
                                  ? (deleteEnabled ? SDL_Color{255, 196, 168, 255} : SDL_Color{132, 140, 156, 255})
                                  : SDL_Color{28, 36, 54, 224},
                              deleteSelected
                                  ? (deleteEnabled ? SDL_Color{255, 116, 204, 255} : SDL_Color{188, 196, 208, 220})
                                  : SDL_Color{82, 108, 132, 180},
                              deleteSelected
                                  ? SDL_Color{255, 255, 255, 48}
                                  : SDL_Color{255, 255, 255, 18},
                              deleteSelected
                                  ? (deleteEnabled ? SDL_Color{42, 10, 24, 210} : SDL_Color{18, 20, 28, 190})
                                  : SDL_Color{6, 10, 18, 160});
        const bool backSelected = isBackSelected(state, slots.size());
        drawJaggedButtonPanel(renderer, kBackRowRect, 22.0f, 28.0f, 22.0f,
                              backSelected ? SDL_Color{156, 246, 255, 255} : SDL_Color{18, 44, 78, 236},
                              backSelected ? SDL_Color{255, 116, 204, 255} : SDL_Color{72, 232, 255, 224},
                              backSelected ? SDL_Color{255, 255, 255, 58} : SDL_Color{110, 208, 255, 34},
                              backSelected ? SDL_Color{22, 6, 40, 214} : SDL_Color{4, 8, 20, 194});
#ifdef VN_ENABLE_TTF
        drawShadowedTextInRect(renderer, resources.itemFont, "Delete",
                               deleteSelected
                                   ? (deleteEnabled ? SDL_Color{52, 18, 36, 255} : SDL_Color{40, 44, 52, 255})
                                   : (deleteEnabled ? SDL_Color{224, 198, 190, 255} : SDL_Color{168, 180, 192, 255}),
                               kDeleteRowRect);
        drawShadowedTextInRect(renderer, resources.itemFont, "Back",
                               backSelected ? SDL_Color{12, 28, 48, 255} : SDL_Color{236, 246, 252, 255},
                               kBackRowRect);
        if (state.noticeTimer > 0.0f && !state.noticeText.empty()) {
            drawTextInRect(renderer, resources.smallFont, state.noticeText,
                           SDL_Color{255, 208, 132, 255},
                           SDL_FRect{centeredX(620.0f), 606.0f, 620.0f, 22.0f});
        }
#endif
    }

    const ConfirmButton* findConfirmButtonAt(float x, float y) const {
        for (const ConfirmButton& button : kDeleteConfirmButtons) {
            if (pointInRect(x, y, button.rect)) {
                return &button;
            }
        }
        return nullptr;
    }

    void finishDelete(AppState& state) {
        const std::filesystem::path path = state.pendingDeletePath;
        const bool deleted = !path.empty() && save::deleteManualSave(path);
        refreshSlots(state);
        const std::size_t remainingSlotCount = cachedSlots_.size();

        if (deleted) {
            applySelectionAfterDelete(state, remainingSlotCount);
            state.noticeText = "Save deleted.";
            state.noticeTimer = 2.0f;
        } else {
            clampSelection(state, remainingSlotCount);
            state.noticeText = "Delete failed.";
            state.noticeTimer = 2.0f;
        }

        clearDeletePromptState(state);
        state.screen = ScreenState::LoadMenu;
    }

    void handleDeleteConfirmEvent(AppState& state, Window& window, const SDL_Event& event,
                                  int windowWidth, int windowHeight) {
        if (event.type == SDL_MOUSEMOTION ||
            (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
            SDL_FPoint confirmPoint{};
            if (mapWindowPointToReference(
                    static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x),
                    static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y),
                    windowWidth, windowHeight, confirmPoint)) {
                if (const ConfirmButton* button = findConfirmButtonAt(confirmPoint.x, confirmPoint.y)) {
                    state.confirmSelection = button->action;
                    if (event.type == SDL_MOUSEBUTTONDOWN) {
                        activateDeleteConfirmAction(state);
                    }
                }
            }
            return;
        }

        if (event.type != SDL_KEYDOWN) {
            return;
        }

        if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a ||
            event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
            state.confirmSelection = ConfirmAction::Cancel;
        } else if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d ||
                   event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
            state.confirmSelection = ConfirmAction::ExitToMainMenu;
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                   event.key.keysym.sym == SDLK_SPACE) {
            activateDeleteConfirmAction(state);
        } else if (event.key.keysym.sym == SDLK_ESCAPE) {
            clearDeletePromptState(state);
            state.screen = ScreenState::LoadMenu;
        } else if (event.key.keysym.sym == SDLK_F11) {
            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
        }
    }

    void activateDeleteConfirmAction(AppState& state) {
        if (state.confirmSelection == ConfirmAction::Cancel) {
            clearDeletePromptState(state);
            state.screen = ScreenState::LoadMenu;
            return;
        }

        finishDelete(state);
    }

    void renderConfirmButtonSkin(SDL_Renderer* renderer, const SDL_FRect& rect, bool selected) const {
        const SDL_Color fillColor = selected ? SDL_Color{156, 246, 255, 255} : SDL_Color{18, 44, 78, 236};
        const SDL_Color outlineColor = selected ? SDL_Color{255, 116, 204, 255} : SDL_Color{72, 232, 255, 224};
        const SDL_Color highlightColor = selected ? SDL_Color{255, 255, 255, 58} : SDL_Color{110, 208, 255, 34};
        const SDL_Color shadowColor = selected ? SDL_Color{22, 6, 40, 214} : SDL_Color{4, 8, 20, 194};
        drawJaggedButtonPanel(renderer, rect, 18.0f, 22.0f, 18.0f,
                              fillColor, outlineColor, highlightColor, shadowColor);

        const SDL_FRect leftTag{rect.x + 11.0f, rect.y + 9.0f, 56.0f, rect.h - 18.0f};
        drawSlantedPanel(renderer, leftTag, 12.0f,
                         selected ? SDL_Color{255, 104, 194, 228} : SDL_Color{64, 138, 214, 176});

        const SDL_FRect topStrip{rect.x + 66.0f, rect.y + 9.0f, rect.w - 94.0f, 5.0f};
        drawSlantedPanel(renderer, topStrip, 10.0f,
                         selected ? SDL_Color{255, 218, 104, 210} : SDL_Color{90, 246, 255, 160});

        const SDL_FRect lowerStrip{rect.x + 72.0f, rect.y + rect.h - 10.0f, rect.w - 102.0f, 3.0f};
        drawSlantedPanel(renderer, lowerStrip, 10.0f,
                         selected ? SDL_Color{72, 220, 255, 176} : SDL_Color{255, 118, 200, 130});
    }

    const char* confirmButtonLabel(ConfirmAction action) const {
        return action == ConfirmAction::Cancel ? "Cancel" : "Delete Save";
    }

    void renderDeleteConfirmOverlay(SDL_Renderer* renderer, const MenuResources& resources,
                                    const AppState& state) const {
        drawCyberPanel(renderer, kDeleteConfirmShellRect, 28.0f, 20.0f,
                       SDL_Color{10, 20, 42, 242}, SDL_Color{80, 228, 255, 244},
                       SDL_Color{72, 156, 255, 30}, SDL_Color{3, 8, 18, 220});
        drawSlantedPanel(renderer,
                         SDL_FRect{kDeleteConfirmShellRect.x + 24.0f, kDeleteConfirmShellRect.y + 24.0f,
                                   kDeleteConfirmShellRect.w - 128.0f, 6.0f},
                         16.0f, SDL_Color{255, 110, 198, 188});
        drawCyberPanel(renderer, kDeleteConfirmFooterBandRect, 8.0f, 6.0f,
                       SDL_Color{18, 36, 64, 230}, SDL_Color{72, 232, 255, 220},
                       SDL_Color{255, 255, 255, 24}, SDL_Color{4, 8, 16, 138});

#ifdef VN_ENABLE_TTF
        drawShadowedTextInRect(renderer, resources.itemFont, "Delete Save?",
                               SDL_Color{240, 248, 255, 255}, kDeleteConfirmTitleRect);
        drawShadowedWrappedTextInRect(renderer, resources.smallFont,
                                      "Are you sure you want to delete this save file?\nThis cannot be undone.",
                                      SDL_Color{236, 246, 252, 255}, kDeleteConfirmBodyRect);
#endif

        for (const ConfirmButton& button : kDeleteConfirmButtons) {
            const bool selected = button.action == state.confirmSelection;
            renderConfirmButtonSkin(renderer, button.rect, selected);
#ifdef VN_ENABLE_TTF
            const SDL_FRect codeRect{button.rect.x + 12.0f, button.rect.y, 50.0f, button.rect.h};
            drawTextInRect(renderer, resources.smallFont,
                           button.action == ConfirmAction::Cancel ? "01" : "02",
                           selected ? SDL_Color{24, 34, 68, 255} : SDL_Color{236, 246, 255, 255},
                           codeRect);
            drawShadowedTextInRect(renderer,
                                   button.action == ConfirmAction::ExitToMainMenu && resources.tinyFont != nullptr
                                       ? resources.tinyFont
                                       : resources.smallFont,
                                   confirmButtonLabel(button.action),
                                   selected ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                                   SDL_FRect{button.rect.x + 68.0f, button.rect.y, button.rect.w - 82.0f, button.rect.h},
                                   false);
#endif
        }
    }

    std::vector<save::SlotInfo> cachedSlots_;
    bool slotsLoaded_ = false;
};

LoadMenuController& loadMenuController() {
    static LoadMenuController controller;
    return controller;
}

} // namespace

void openLoadMenu(AppState& state, ScreenState returnScreen) {
    state.loadReturnScreen = returnScreen;
    state.loadSelection = 0;
    state.loadSlotSelection = 0;
    clearDeletePromptState(state);
    loadMenuController().open(state);
    state.screen = ScreenState::LoadMenu;
}

void renderLoadScreen(SDL_Renderer* renderer, const MenuResources& resources, AppState& state,
                      int windowWidth, int windowHeight) {
    loadMenuController().render(renderer, resources, state, windowWidth, windowHeight);
}

void handleLoadMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight) {
    loadMenuController().handleEvent(state, window, event, windowWidth, windowHeight);
}
