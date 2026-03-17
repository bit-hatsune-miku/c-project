#include "menu_shared.h"

#include <array>
#include <cmath>
#include <string>

#include "../Settings/settings.h"
#include "../game/vn/vn_system.h"

namespace {

constexpr int kReferenceWidth = 1280;

constexpr float centeredX(float width) {
    return (static_cast<float>(kReferenceWidth) - width) * 0.5f;
}

constexpr float kMenuContentInsetX = 24.0f;
constexpr float kPauseShellWidth = 450.0f;
constexpr float kPauseTitleWidth = kPauseShellWidth;
constexpr float kPauseButtonWidth = 360.0f;
constexpr float kPauseButtonX = centeredX(kPauseButtonWidth);
constexpr SDL_FRect kPauseTitleBackdropRect{centeredX(kPauseTitleWidth), 72.0f, kPauseTitleWidth, 118.0f};
constexpr SDL_FRect kPauseTitleRect{centeredX(kPauseTitleWidth) + kMenuContentInsetX, 80.0f,
                                    kPauseTitleWidth - kMenuContentInsetX * 2.0f, 100.0f};
constexpr SDL_FRect kPauseShellRect{centeredX(kPauseShellWidth), 208.0f, kPauseShellWidth, 460.0f};
constexpr SDL_FRect kPauseHeaderBandRect{kPauseShellRect.x + 12.0f, kPauseShellRect.y + 12.0f,
                                         kPauseShellRect.w - 24.0f, 118.0f};
constexpr SDL_FRect kPauseSectionLabelRect{kPauseShellRect.x + kMenuContentInsetX, kPauseShellRect.y + 18.0f,
                                           210.0f, 28.0f};
constexpr SDL_FRect kPauseHintRect{kPauseShellRect.x + kMenuContentInsetX, kPauseShellRect.y + 48.0f,
                                   kPauseShellRect.w - kMenuContentInsetX * 2.0f, 46.0f};
constexpr SDL_FRect kPauseNoticeRect{kPauseShellRect.x + kMenuContentInsetX, 676.0f,
                                     kPauseShellRect.w - kMenuContentInsetX * 2.0f, 42.0f};
constexpr float kPauseIntroDuration = 0.38f;
constexpr float kPauseIntroStagger = 0.07f;
constexpr float kPauseIntroTravel = 180.0f;

struct PauseButton {
    PauseAction action;
    const char* label;
    SDL_FRect rect;
};

constexpr std::array<PauseButton, 5> kStoryPauseButtons{{
    {PauseAction::Continue, "Continue", SDL_FRect{kPauseButtonX, 332.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::Save, "Save", SDL_FRect{kPauseButtonX, 398.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::Load, "Load", SDL_FRect{kPauseButtonX, 464.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::Settings, "Settings", SDL_FRect{kPauseButtonX, 530.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::ExitToMainMenu, "Exit", SDL_FRect{kPauseButtonX, 596.0f, kPauseButtonWidth, 58.0f}}
}};

constexpr std::array<PauseButton, 3> kBattlePauseButtons{{
    {PauseAction::Continue, "Continue", SDL_FRect{kPauseButtonX, 392.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::Settings, "Settings", SDL_FRect{kPauseButtonX, 466.0f, kPauseButtonWidth, 58.0f}},
    {PauseAction::ExitToMainMenu, "Exit", SDL_FRect{kPauseButtonX, 540.0f, kPauseButtonWidth, 58.0f}}
}};

struct PauseButtonSet {
    const PauseButton* data = nullptr;
    std::size_t size = 0;
};

PauseButtonSet activePauseButtons(const AppState& state) {
    if (state.pauseContext == PauseContext::Battle) {
        return PauseButtonSet{kBattlePauseButtons.data(), kBattlePauseButtons.size()};
    }
    return PauseButtonSet{kStoryPauseButtons.data(), kStoryPauseButtons.size()};
}

class PauseMenuController {
public:
    void render(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                int windowWidth, int windowHeight) const {
        renderPauseBackdrop(renderer, windowWidth, windowHeight);

        const bool usingReferenceLayout = beginReferenceLayout(renderer, windowWidth, windowHeight);
        renderPauseMenuOverlay(renderer, resources, state);
        if (state.screen == ScreenState::PauseConfirmExit ||
            state.screen == ScreenState::PauseConfirmOverwriteSave) {
            renderExitToMainMenuOverlay(renderer, resources, state);
        }
        if (usingReferenceLayout) {
            endReferenceLayout(renderer);
        }
    }

    void handlePauseMenuEvent(AppState& state, Window& window, const SDL_Event& event,
                              int windowWidth, int windowHeight) const {
        if (event.type == SDL_MOUSEMOTION ||
            (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
            SDL_FPoint pausePoint{};
            if (mapWindowPointToReference(
                    static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x),
                    static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y),
                    windowWidth, windowHeight, pausePoint)) {
                if (const PauseButton* button = findPauseButtonAt(state, pausePoint.x, pausePoint.y)) {
                    state.pauseSelection = button->action;
                    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                        activatePauseAction(state);
                    }
                }
            }
            return;
        }

        if (event.type != SDL_KEYDOWN) {
            return;
        }

        if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
            state.pauseSelection = previousPauseAction(state, state.pauseSelection);
        } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
            state.pauseSelection = nextPauseAction(state, state.pauseSelection);
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                   event.key.keysym.sym == SDLK_SPACE) {
            activatePauseAction(state);
        } else if (event.key.keysym.sym == SDLK_F11) {
            SettingsMenuController::applyDisplayMode(window, state.settings, !state.settings.fullscreen);
        }
    }

private:
    float buttonProgress(const AppState& state, std::size_t index) const {
        const float startTime = kPauseIntroStagger * static_cast<float>(index);
        return clamp01((state.pauseIntroTime - startTime) / kPauseIntroDuration);
    }

    SDL_FRect animatedPauseButtonRect(const AppState& state, std::size_t index) const {
        const PauseButtonSet buttons = activePauseButtons(state);
        SDL_FRect rect = buttons.data[index].rect;
        const float progress = easeOutBack(buttonProgress(state, index));
        const float shellProgress = smoothstep01(state.pauseIntroTime / kPauseIntroDuration);
        rect.x -= (1.0f - progress) * (kPauseIntroTravel + 18.0f * static_cast<float>(index));
        rect.y += (1.0f - shellProgress) * 26.0f;
        return rect;
    }

    const PauseButton* findPauseButtonAt(const AppState& state, float x, float y) const {
        const PauseButtonSet buttons = activePauseButtons(state);
        for (std::size_t i = 0; i < buttons.size; ++i) {
            if (pointInRect(x, y, animatedPauseButtonRect(state, i))) {
                return &buttons.data[i];
            }
        }
        return nullptr;
    }

    PauseAction nextPauseAction(const AppState& state, PauseAction action) const {
        const PauseButtonSet buttons = activePauseButtons(state);
        for (std::size_t i = 0; i < buttons.size; ++i) {
            if (buttons.data[i].action == action) {
                return buttons.data[(i + 1) % buttons.size].action;
            }
        }
        return buttons.data[0].action;
    }

    PauseAction previousPauseAction(const AppState& state, PauseAction action) const {
        const PauseButtonSet buttons = activePauseButtons(state);
        for (std::size_t i = 0; i < buttons.size; ++i) {
            if (buttons.data[i].action == action) {
                return buttons.data[(i + buttons.size - 1) % buttons.size].action;
            }
        }
        return buttons.data[0].action;
    }

    void resumePausedMode(AppState& state) const {
        vn::setPaused(false);
        state.screen = state.pauseContext == PauseContext::Battle ? ScreenState::BattleDemo : ScreenState::Playing;
    }

    void activatePauseAction(AppState& state) const {
        switch (state.pauseSelection) {
            case PauseAction::Continue:
                resumePausedMode(state);
                break;
            case PauseAction::Save:
                state.requestStoryManualSave = true;
                break;
            case PauseAction::Load:
                openLoadMenu(state, ScreenState::PauseMenu);
                break;
            case PauseAction::Settings:
                state.settingsSelection = SettingsItem::DisplayMode;
                state.settingsReturnScreen = ScreenState::PauseMenu;
                state.screen = ScreenState::Settings;
                break;
            case PauseAction::ExitToMainMenu:
                state.confirmSelection = ConfirmAction::Cancel;
                state.screen = ScreenState::PauseConfirmExit;
                break;
        }
    }

    void renderPauseButtonSkin(SDL_Renderer* renderer, const SDL_FRect& rect, bool selected) const {
        const SDL_Color fillColor = selected ? SDL_Color{156, 246, 255, 255} : SDL_Color{18, 44, 78, 236};
        const SDL_Color outlineColor = selected ? SDL_Color{255, 116, 204, 255} : SDL_Color{72, 232, 255, 224};
        const SDL_Color highlightColor = selected ? SDL_Color{255, 255, 255, 58} : SDL_Color{110, 208, 255, 34};
        const SDL_Color shadowColor = selected ? SDL_Color{22, 6, 40, 214} : SDL_Color{4, 8, 20, 194};
        drawJaggedButtonPanel(renderer, rect, 22.0f, 28.0f, 22.0f,
                              fillColor, outlineColor, highlightColor, shadowColor);

        const SDL_FRect leftTag{rect.x + 14.0f, rect.y + 9.0f, 84.0f, rect.h - 18.0f};
        drawSlantedPanel(renderer, leftTag, 16.0f,
                         selected ? SDL_Color{255, 104, 194, 228} : SDL_Color{64, 138, 214, 176});

        const SDL_FRect topStrip{rect.x + 98.0f, rect.y + 9.0f, rect.w - 134.0f, 6.0f};
        drawSlantedPanel(renderer, topStrip, 12.0f,
                         selected ? SDL_Color{255, 218, 104, 210} : SDL_Color{90, 246, 255, 160});

        const SDL_FRect lowerStrip{rect.x + 106.0f, rect.y + rect.h - 11.0f, rect.w - 176.0f, 4.0f};
        drawSlantedPanel(renderer, lowerStrip, 12.0f,
                         selected ? SDL_Color{72, 220, 255, 176} : SDL_Color{255, 118, 200, 130});
    }

    void renderPauseMenuOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) const {
        const float shellProgress = smoothstep01(state.pauseIntroTime / kPauseIntroDuration);
        const float shellOffset = (1.0f - shellProgress) * 26.0f;

        const SDL_FRect titleBackdrop = offsetRect(kPauseTitleBackdropRect, 0.0f, shellOffset);
        const SDL_FRect titleRect = offsetRect(kPauseTitleRect, 0.0f, shellOffset);
        const SDL_FRect shellRect = offsetRect(kPauseShellRect, 0.0f, shellOffset);
        const SDL_FRect headerBandRect = offsetRect(kPauseHeaderBandRect, 0.0f, shellOffset);
        const SDL_FRect sectionLabelRect = offsetRect(kPauseSectionLabelRect, 0.0f, shellOffset);
        const SDL_FRect hintRect = offsetRect(kPauseHintRect, 0.0f, shellOffset);
        const SDL_FRect leftAccent{shellRect.x - 18.0f, titleBackdrop.y + 20.0f, 18.0f,
                                   shellRect.y + shellRect.h - (titleBackdrop.y + 20.0f)};
        const SDL_FRect rightAccent{shellRect.x + shellRect.w + 6.0f, titleBackdrop.y + 52.0f, 16.0f,
                                    shellRect.y + shellRect.h - (titleBackdrop.y + 52.0f)};

        drawSlantedPanel(renderer, leftAccent, 6.0f, SDL_Color{255, 102, 196, 164});
        drawSlantedPanel(renderer, rightAccent, 5.0f, SDL_Color{72, 224, 255, 168});
        drawNeonLine(renderer,
                     SDL_FPoint{titleBackdrop.x + 34.0f, titleBackdrop.y + titleBackdrop.h + 22.0f},
                     SDL_FPoint{titleBackdrop.x + titleBackdrop.w - 26.0f, titleBackdrop.y + titleBackdrop.h + 22.0f},
                     SDL_Color{255, 74, 164, 82}, SDL_Color{255, 128, 210, 180});

        drawCyberPanel(renderer, titleBackdrop, 34.0f, 24.0f,
                       SDL_Color{18, 30, 56, 240}, SDL_Color{90, 238, 255, 255},
                       SDL_Color{116, 220, 255, 42}, SDL_Color{4, 8, 20, 220});
        drawSlantedPanel(renderer,
                         SDL_FRect{titleBackdrop.x + 28.0f, titleBackdrop.y + 18.0f, titleBackdrop.w - 136.0f, 7.0f},
                         18.0f, SDL_Color{255, 110, 198, 204});
        drawSlantedPanel(renderer,
                         SDL_FRect{titleBackdrop.x + 136.0f, titleBackdrop.y + titleBackdrop.h - 18.0f, titleBackdrop.w - 178.0f, 5.0f},
                         16.0f, SDL_Color{255, 220, 118, 192});

        drawCyberPanel(renderer, shellRect, 42.0f, 28.0f,
                       SDL_Color{10, 20, 42, 236}, SDL_Color{80, 228, 255, 244},
                       SDL_Color{72, 156, 255, 34}, SDL_Color{3, 8, 18, 220});
        drawCyberPanel(renderer, headerBandRect, 24.0f, 16.0f,
                       SDL_Color{24, 44, 78, 220}, SDL_Color{255, 118, 200, 210},
                       SDL_Color{106, 224, 255, 30}, SDL_Color{4, 8, 20, 120});
        drawSlantedPanel(renderer,
                         SDL_FRect{shellRect.x + 22.0f, shellRect.y + 118.0f, shellRect.w - 102.0f, 6.0f},
                         16.0f, SDL_Color{255, 206, 102, 188});

#ifdef VN_ENABLE_TTF
        drawShadowedTextInRect(renderer, resources.titleFont, "Paused",
                               SDL_Color{240, 248, 255, 255}, titleRect);
        drawTextInRect(renderer, resources.smallFont, "PAUSE MENU",
                       SDL_Color{110, 240, 255, 255}, sectionLabelRect, false);
        drawShadowedWrappedTextInRect(renderer, resources.smallFont,
                                      state.pauseContext == PauseContext::Battle
                                          ? "THE BATTLE IS FROZEN UNTIL YOU CONTINUE"
                                          : "THE STORY IS FROZEN UNTIL YOU CONTINUE",
                                      SDL_Color{214, 246, 255, 255}, hintRect, false);
#endif

        const PauseButtonSet buttons = activePauseButtons(state);
        const SDL_Rect buttonClipRect{
            static_cast<int>(std::lround(shellRect.x + 8.0f)),
            static_cast<int>(std::lround(buttons.data[0].rect.y + shellOffset - 8.0f)),
            static_cast<int>(std::lround(shellRect.w - 16.0f)),
            static_cast<int>(std::lround((buttons.data[buttons.size - 1].rect.y +
                                          buttons.data[buttons.size - 1].rect.h + shellOffset) -
                                         (buttons.data[0].rect.y + shellOffset - 8.0f) + 8.0f))
        };
        SDL_RenderSetClipRect(renderer, &buttonClipRect);

        for (std::size_t i = 0; i < buttons.size; ++i) {
            const PauseButton& button = buttons.data[i];
            const bool selected = button.action == state.pauseSelection;
            const float visibleProgress = smoothstep01(buttonProgress(state, i));
            if (visibleProgress <= 0.0f) {
                continue;
            }

            const SDL_FRect buttonRect = animatedPauseButtonRect(state, i);
            renderPauseButtonSkin(renderer, buttonRect, selected);

#ifdef VN_ENABLE_TTF
            const SDL_FRect codeRect{buttonRect.x + 18.0f, buttonRect.y, 68.0f, buttonRect.h};
            drawTextInRect(renderer, resources.smallFont, "0" + std::to_string(i + 1),
                           selected ? SDL_Color{24, 34, 68, 255} : SDL_Color{236, 246, 255, 255},
                           codeRect);

            const SDL_FRect labelRect{buttonRect.x + 96.0f, buttonRect.y, buttonRect.w - 152.0f, buttonRect.h};
            drawShadowedTextInRect(renderer, resources.itemFont, button.label,
                                   selected ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                                   labelRect, false);
#endif
        }

        SDL_RenderSetClipRect(renderer, nullptr);

        if (state.noticeTimer > 0.0f && !state.noticeText.empty()) {
            const SDL_FRect noticeRect = offsetRect(kPauseNoticeRect, 0.0f, shellOffset);
            drawCyberPanel(renderer, noticeRect, 20.0f, 14.0f,
                           SDL_Color{12, 18, 34, 232}, SDL_Color{255, 118, 198, 218},
                           SDL_Color{96, 238, 255, 24}, SDL_Color{6, 10, 18, 92});
#ifdef VN_ENABLE_TTF
            drawTextInRect(renderer, resources.smallFont, state.noticeText,
                           SDL_Color{240, 246, 250, 255},
                           SDL_FRect{noticeRect.x + 14.0f, noticeRect.y, noticeRect.w - 28.0f, noticeRect.h});
#endif
        }
    }

};

const PauseMenuController& pauseMenuController() {
    static const PauseMenuController controller;
    return controller;
}

}  // namespace

void renderPauseBackdrop(SDL_Renderer* renderer, int windowWidth, int windowHeight) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 6, 10, 14, 160);
    SDL_Rect fullRect{0, 0, windowWidth, windowHeight};
    SDL_RenderFillRect(renderer, &fullRect);

    SDL_SetRenderDrawColor(renderer, 20, 30, 34, 84);
    SDL_Rect topBand{0, 0, windowWidth, windowHeight / 4};
    SDL_Rect bottomBand{0, windowHeight - windowHeight / 4, windowWidth, windowHeight / 4};
    SDL_RenderFillRect(renderer, &topBand);
    SDL_RenderFillRect(renderer, &bottomBand);
}

void openPauseMenu(AppState& state, PauseContext context) {
    state.pauseSelection = PauseAction::Continue;
    state.pauseContext = context;
    state.confirmSelection = ConfirmAction::Cancel;
    state.pauseIntroTime = 0.0f;
    state.screen = ScreenState::PauseMenu;
    vn::setPaused(true);
}

void renderPauseScreen(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                       int windowWidth, int windowHeight) {
    pauseMenuController().render(renderer, resources, state, windowWidth, windowHeight);
}

void handlePauseMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight) {
    pauseMenuController().handlePauseMenuEvent(state, window, event, windowWidth, windowHeight);
}
