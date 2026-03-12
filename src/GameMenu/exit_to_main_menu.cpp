#include "menu_shared.h"

#include <array>
#include <string>

#include "../Settings/settings.h"
#include "../game/vn_system.h"

namespace {

constexpr SDL_FRect kPauseConfirmShellRect{356.0f, 206.0f, 568.0f, 308.0f};
constexpr SDL_FRect kPauseConfirmTitleRect{kPauseConfirmShellRect.x + 36.0f, kPauseConfirmShellRect.y + 28.0f,
                                           kPauseConfirmShellRect.w - 72.0f, 46.0f};
constexpr SDL_FRect kPauseConfirmBodyRect{kPauseConfirmShellRect.x + 46.0f, kPauseConfirmShellRect.y + 88.0f,
                                          kPauseConfirmShellRect.w - 92.0f, 94.0f};
constexpr SDL_FRect kPauseConfirmFooterBandRect{kPauseConfirmShellRect.x + 24.0f, kPauseConfirmShellRect.y + 268.0f,
                                                kPauseConfirmShellRect.w - 48.0f, 18.0f};

struct ConfirmButton {
    ConfirmAction action;
    const char* label;
    SDL_FRect rect;
};

constexpr std::array<ConfirmButton, 2> kConfirmButtons{{
    {ConfirmAction::Cancel, "Stay", SDL_FRect{kPauseConfirmShellRect.x + 42.0f, kPauseConfirmShellRect.y + 188.0f, 196.0f, 58.0f}},
    {ConfirmAction::ExitToMainMenu, "Exit To Menu", SDL_FRect{kPauseConfirmShellRect.x + kPauseConfirmShellRect.w - 238.0f,
                                                              kPauseConfirmShellRect.y + 188.0f, 196.0f, 58.0f}}
}};

class ExitToMainMenuController {
public:
    void renderOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) const {
        drawCyberPanel(renderer, kPauseConfirmShellRect, 28.0f, 20.0f,
                       SDL_Color{10, 20, 42, 242}, SDL_Color{80, 228, 255, 244},
                       SDL_Color{72, 156, 255, 30}, SDL_Color{3, 8, 18, 220});
        drawSlantedPanel(renderer,
                         SDL_FRect{kPauseConfirmShellRect.x + 24.0f, kPauseConfirmShellRect.y + 24.0f,
                                   kPauseConfirmShellRect.w - 128.0f, 6.0f},
                         16.0f, SDL_Color{255, 110, 198, 188});
        drawCyberPanel(renderer, kPauseConfirmFooterBandRect, 8.0f, 6.0f,
                       SDL_Color{18, 36, 64, 230}, SDL_Color{72, 232, 255, 220},
                       SDL_Color{255, 255, 255, 24}, SDL_Color{4, 8, 16, 138});

#ifdef VN_ENABLE_TTF
        drawShadowedTextInRect(renderer, resources.itemFont, "Exit To Main Menu?",
                               SDL_Color{240, 248, 255, 255}, kPauseConfirmTitleRect);
        drawShadowedWrappedTextInRect(renderer, resources.smallFont,
                                      "You will lose the current chapter progress\nif you leave now.",
                                      SDL_Color{236, 246, 252, 255}, kPauseConfirmBodyRect);
#endif

        for (const ConfirmButton& button : kConfirmButtons) {
            const bool selected = button.action == state.confirmSelection;
            renderConfirmButtonSkin(renderer, button.rect, selected);
#ifdef VN_ENABLE_TTF
            const SDL_FRect codeRect{button.rect.x + 12.0f, button.rect.y, 50.0f, button.rect.h};
            drawTextInRect(renderer, resources.smallFont,
                           button.action == ConfirmAction::Cancel ? "01" : "02",
                           selected ? SDL_Color{24, 34, 68, 255} : SDL_Color{236, 246, 255, 255},
                           codeRect);
            const bool isExitButton = button.action == ConfirmAction::ExitToMainMenu;
            drawShadowedTextInRect(renderer,
                                   isExitButton && resources.tinyFont != nullptr ? resources.tinyFont : resources.smallFont,
                                   button.label,
                                   selected ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                                   SDL_FRect{button.rect.x + 68.0f, button.rect.y, button.rect.w - 82.0f, button.rect.h},
                                   false);
#endif
        }
    }

    void handleEvent(AppState& state, Window& window, const SDL_Event& event,
                     int windowWidth, int windowHeight) const {
        if (event.type == SDL_MOUSEMOTION ||
            (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
            SDL_FPoint confirmPoint{};
            if (mapWindowPointToReference(
                    static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x),
                    static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y),
                    windowWidth, windowHeight, confirmPoint)) {
                if (const ConfirmButton* button = findConfirmButtonAt(confirmPoint.x, confirmPoint.y)) {
                    state.confirmSelection = button->action;
                    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                        activateConfirmAction(state);
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
            activateConfirmAction(state);
        } else if (event.key.keysym.sym == SDLK_ESCAPE) {
            state.screen = ScreenState::PauseMenu;
        } else if (event.key.keysym.sym == SDLK_F11) {
            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
        }
    }

private:
    const ConfirmButton* findConfirmButtonAt(float x, float y) const {
        for (const ConfirmButton& button : kConfirmButtons) {
            if (pointInRect(x, y, button.rect)) {
                return &button;
            }
        }
        return nullptr;
    }

    void exitStoryToMainMenu(AppState& state) const {
        vn::setPaused(false);
        vn::stopVoicePlayback();
        state.story.entryIndex = 0;
        state.pauseSelection = PauseAction::Continue;
        state.confirmSelection = ConfirmAction::Cancel;
        state.settingsReturnScreen = ScreenState::MainMenu;
        state.screen = ScreenState::MainMenu;
        state.mainSelection = MainMenuAction::Start;
        state.noticeText = "Current progress was discarded.";
        state.noticeTimer = 2.6f;
    }

    void activateConfirmAction(AppState& state) const {
        switch (state.confirmSelection) {
            case ConfirmAction::Cancel:
                state.screen = ScreenState::PauseMenu;
                break;
            case ConfirmAction::ExitToMainMenu:
                exitStoryToMainMenu(state);
                break;
        }
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
};

const ExitToMainMenuController& exitToMainMenuController() {
    static const ExitToMainMenuController controller;
    return controller;
}

}  // namespace

void renderExitToMainMenuOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) {
    exitToMainMenuController().renderOverlay(renderer, resources, state);
}

void handlePauseConfirmEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight) {
    exitToMainMenuController().handleEvent(state, window, event, windowWidth, windowHeight);
}
