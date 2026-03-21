#include "menu_shared.h"

#include <array>
#include <cmath>
#include <string>

#include "../window.h"

namespace {

constexpr float kMenuContentInsetX = 24.0f;
constexpr SDL_FRect kMenuShellRect{46.0f, 196.0f, 450.0f, 454.0f};
constexpr SDL_FRect kMenuTitleBackdropRect{kMenuShellRect.x, 22.0f, kMenuShellRect.w, 148.0f};
constexpr SDL_FRect kMenuTitleRect{
    kMenuTitleBackdropRect.x + kMenuContentInsetX,
    28.0f,
    kMenuTitleBackdropRect.w - kMenuContentInsetX * 2.0f,
    136.0f
};
constexpr SDL_FRect kMenuSectionLabelRect{kMenuShellRect.x + kMenuContentInsetX, 180.0f, 220.0f, 34.0f};
constexpr SDL_FRect kFooterBandRect{
    kMenuShellRect.x + kMenuContentInsetX,
    620.0f,
    kMenuShellRect.w - kMenuContentInsetX * 2.0f,
    26.0f
};
constexpr SDL_FRect kNoticeRect{640.0f, 560.0f, 566.0f, 86.0f};
constexpr float kMenuIntroDuration = 0.52f;
constexpr float kMenuIntroStagger = 0.09f;
constexpr float kMenuIntroTravel = 230.0f;

struct MenuButton {
    MainMenuAction action;
    const char* label;
    SDL_FRect rect;
};

constexpr std::array<MenuButton, 5> kMenuButtons{{
    {MainMenuAction::Start, "Start", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 246.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}},
    {MainMenuAction::Load, "Load", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 320.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}},
    {MainMenuAction::Battle, "Battle", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 394.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}},
    {MainMenuAction::Settings, "Settings", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 468.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}},
    {MainMenuAction::Exit, "Exit", SDL_FRect{kMenuShellRect.x + kMenuContentInsetX, 542.0f, kMenuShellRect.w - kMenuContentInsetX * 2.0f, 58.0f}}
}};

constexpr int actionIndex(MainMenuAction action) {
    return static_cast<int>(action);
}

constexpr MainMenuAction nextAction(MainMenuAction action) {
    return static_cast<MainMenuAction>((actionIndex(action) + 1) % static_cast<int>(kMenuButtons.size()));
}

constexpr MainMenuAction previousAction(MainMenuAction action) {
    return static_cast<MainMenuAction>(
        (actionIndex(action) + static_cast<int>(kMenuButtons.size()) - 1) % static_cast<int>(kMenuButtons.size())
    );
}

class MainMenuController {
public:
    void render(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                int windowWidth, int windowHeight) const {
        renderBackgroundCover(renderer, resources.background, windowWidth, windowHeight);

        const bool usingCanvas = beginMenuCanvas(renderer, resources.menuCanvas);
        const bool usingReferenceLayout = !usingCanvas && beginReferenceLayout(renderer, windowWidth, windowHeight);
        renderOverlay(renderer, resources, state);
        if (usingCanvas) {
            endMenuCanvas(renderer, resources.menuCanvas, windowWidth, windowHeight);
        } else if (usingReferenceLayout) {
            endReferenceLayout(renderer);
        }
    }

    void handleEvent(AppState& state, Window& window, const SDL_Event& event,
                     int windowWidth, int windowHeight) const {
        if (event.type == SDL_MOUSEMOTION) {
            SDL_FPoint menuPoint{};
            if (mapWindowPointToReference(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y),
                                          windowWidth, windowHeight, menuPoint)) {
                if (const MenuButton* button = findButtonAt(state, menuPoint.x, menuPoint.y)) {
                    state.mainSelection = button->action;
                }
            }
            return;
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            SDL_FPoint menuPoint{};
            if (!mapWindowPointToReference(static_cast<float>(event.button.x), static_cast<float>(event.button.y),
                                           windowWidth, windowHeight, menuPoint)) {
                return;
            }

            if (const MenuButton* button = findButtonAt(state, menuPoint.x, menuPoint.y)) {
                state.mainSelection = button->action;
                activate(state, window, button->action);
            }
            return;
        }

        if (event.type != SDL_KEYDOWN) {
            return;
        }

        if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
            state.mainSelection = previousAction(state.mainSelection);
        } else if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
            state.mainSelection = nextAction(state.mainSelection);
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                   event.key.keysym.sym == SDLK_SPACE) {
            activate(state, window, state.mainSelection);
        }
    }

private:
    float buttonProgress(const AppState& state, std::size_t index) const {
        const float startTime = kMenuIntroStagger * static_cast<float>(index);
        return clamp01((state.menuIntroTime - startTime) / kMenuIntroDuration);
    }

    SDL_FRect animatedButtonRect(const AppState& state, std::size_t index) const {
        SDL_FRect rect = kMenuButtons[index].rect;
        const float progress = easeOutBack(buttonProgress(state, index));
        rect.x -= (1.0f - progress) * (kMenuIntroTravel + 22.0f * static_cast<float>(index));
        return rect;
    }

    const MenuButton* findButtonAt(const AppState& state, float x, float y) const {
        for (std::size_t i = 0; i < kMenuButtons.size(); ++i) {
            if (pointInRect(x, y, animatedButtonRect(state, i))) {
                return &kMenuButtons[i];
            }
        }
        return nullptr;
    }

    void activate(AppState& state, Window& window, MainMenuAction action) const {
        switch (action) {
            case MainMenuAction::Start:
                beginStory(state);
                break;
            case MainMenuAction::Load:
                openLoadMenu(state, ScreenState::MainMenu);
                break;
            case MainMenuAction::Battle:
                beginPartySelect(state);
                break;
            case MainMenuAction::Settings:
                state.settingsSelection = SettingsItem::DisplayMode;
                state.settingsReturnScreen = ScreenState::MainMenu;
                state.screen = ScreenState::Settings;
                break;
            case MainMenuAction::Exit:
                window.close();
                break;
        }
    }

    void renderButtonSkin(SDL_Renderer* renderer, const SDL_FRect& rect, bool selected) const {
        const SDL_Color fillColor = selected ? SDL_Color{156, 246, 255, 255} : SDL_Color{18, 44, 78, 228};
        const SDL_Color outlineColor = selected ? SDL_Color{255, 116, 204, 255} : SDL_Color{72, 232, 255, 220};
        const SDL_Color highlightColor = selected ? SDL_Color{255, 255, 255, 58} : SDL_Color{110, 208, 255, 34};
        const SDL_Color shadowColor = selected ? SDL_Color{22, 6, 40, 210} : SDL_Color{4, 8, 20, 190};
        drawJaggedButtonPanel(renderer, rect, 24.0f, 30.0f, 24.0f,
                              fillColor, outlineColor, highlightColor, shadowColor);

        const SDL_FRect leftTag{rect.x + 14.0f, rect.y + 10.0f, 98.0f, rect.h - 20.0f};
        drawSlantedPanel(renderer, leftTag, 16.0f,
                         selected ? SDL_Color{255, 104, 194, 230} : SDL_Color{64, 138, 214, 180});

        const SDL_FRect topStrip{rect.x + 112.0f, rect.y + 9.0f, rect.w - 150.0f, 6.0f};
        drawSlantedPanel(renderer, topStrip, 12.0f,
                         selected ? SDL_Color{255, 218, 104, 210} : SDL_Color{90, 246, 255, 160});

        const SDL_FRect sideCut{rect.x + rect.w - 84.0f, rect.y + 8.0f, 60.0f, rect.h - 16.0f};
        drawSlantedPanel(renderer, sideCut, 22.0f,
                         selected ? SDL_Color{18, 34, 64, 120} : SDL_Color{10, 24, 44, 110});

        const SDL_FRect lowerStrip{rect.x + 126.0f, rect.y + rect.h - 12.0f, rect.w - 196.0f, 4.0f};
        drawSlantedPanel(renderer, lowerStrip, 14.0f,
                         selected ? SDL_Color{72, 220, 255, 180} : SDL_Color{255, 118, 200, 132});
    }

    void renderOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) const {
        const float accentBottom = kMenuButtons.back().rect.y + kMenuButtons.back().rect.h + 26.0f;
        const float rightAccentTop = kMenuButtons.front().rect.y - 4.0f;

        drawSlantedPanel(renderer, SDL_FRect{28.0f, 22.0f, 502.0f, 628.0f}, 42.0f, SDL_Color{5, 10, 28, 138});
        drawSlantedPanel(renderer, SDL_FRect{42.0f, 46.0f, 458.0f, 566.0f}, 36.0f, SDL_Color{14, 24, 48, 114});
        drawSlantedPanel(renderer, SDL_FRect{510.0f, rightAccentTop, 14.0f, accentBottom - rightAccentTop},
                         4.0f, SDL_Color{72, 224, 255, 168});
        drawNeonLine(renderer, SDL_FPoint{42.0f, 166.0f}, SDL_FPoint{512.0f, 166.0f},
                     SDL_Color{255, 74, 164, 92}, SDL_Color{255, 128, 210, 188});
        drawNeonLine(renderer, SDL_FPoint{74.0f, 182.0f}, SDL_FPoint{462.0f, 182.0f},
                     SDL_Color{72, 212, 255, 88}, SDL_Color{108, 244, 255, 196});

        drawCyberPanel(renderer, kMenuTitleBackdropRect, 38.0f, 24.0f,
                       SDL_Color{18, 30, 56, 238}, SDL_Color{90, 238, 255, 255},
                       SDL_Color{116, 220, 255, 44}, SDL_Color{4, 8, 20, 220});
        drawSlantedPanel(renderer,
                         SDL_FRect{kMenuTitleBackdropRect.x + 18.0f, kMenuTitleBackdropRect.y + 18.0f,
                                   kMenuTitleBackdropRect.w - 118.0f, 7.0f},
                         18.0f, SDL_Color{255, 110, 198, 204});
        drawSlantedPanel(renderer,
                         SDL_FRect{kMenuTitleBackdropRect.x + 102.0f, kMenuTitleBackdropRect.y + kMenuTitleBackdropRect.h - 21.0f,
                                   kMenuTitleBackdropRect.w - 138.0f, 5.0f},
                         16.0f, SDL_Color{255, 220, 118, 196});
        if (resources.titleLogo != nullptr) {
            renderTextureContain(renderer, resources.titleLogo,
                                 SDL_FRect{kMenuTitleRect.x + 8.0f, kMenuTitleRect.y + 8.0f, kMenuTitleRect.w, kMenuTitleRect.h},
                                 108, 0.0f, 0.5f);
            renderTextureContain(renderer, resources.titleLogo, kMenuTitleRect, 255, 0.0f, 0.5f);
        }

        drawCyberPanel(renderer, kMenuShellRect, 44.0f, 30.0f,
                       SDL_Color{10, 20, 42, 236}, SDL_Color{80, 228, 255, 244},
                       SDL_Color{72, 156, 255, 38}, SDL_Color{3, 8, 18, 220});
        drawSlantedPanel(renderer,
                         SDL_FRect{kMenuShellRect.x + kMenuShellRect.w - 88.0f, kMenuShellRect.y + 22.0f, 56.0f, 16.0f},
                         14.0f, SDL_Color{255, 118, 198, 210});
        drawCyberPanel(renderer, kFooterBandRect, 16.0f, 12.0f,
                       SDL_Color{18, 36, 64, 230}, SDL_Color{72, 232, 255, 220},
                       SDL_Color{255, 255, 255, 24}, SDL_Color{4, 8, 16, 138});

#ifdef VN_ENABLE_TTF
        if (resources.titleLogo == nullptr) {
            drawShadowedTextInRect(renderer, resources.subtitleFont, "HATSUNE MIKU",
                                   SDL_Color{96, 242, 255, 255},
                                   SDL_FRect{kMenuTitleRect.x, kMenuTitleRect.y + 8.0f, kMenuTitleRect.w, 42.0f},
                                   false);
            drawShadowedTextInRect(renderer, resources.titleFont, "UNDERGROUND BIT IDOL",
                                   SDL_Color{244, 250, 255, 255},
                                   SDL_FRect{kMenuTitleRect.x, kMenuTitleRect.y + 50.0f, kMenuTitleRect.w, 82.0f},
                                   false);
        }
        drawTextInRect(renderer, resources.smallFont, "MAIN MENU",
                       SDL_Color{110, 240, 255, 255}, kMenuSectionLabelRect, false);
        drawTextInRect(renderer, resources.smallFont, "CLICK OR PRESS ENTER",
                       SDL_Color{198, 246, 255, 255},
                       SDL_FRect{kFooterBandRect.x + 12.0f, kFooterBandRect.y, kFooterBandRect.w - 24.0f, kFooterBandRect.h});
#endif

        const SDL_Rect buttonClipRect{
            static_cast<int>(std::lround(kMenuShellRect.x + 8.0f)),
            static_cast<int>(std::lround(kMenuButtons.front().rect.y - 8.0f)),
            static_cast<int>(std::lround(kMenuShellRect.w - 16.0f)),
            static_cast<int>(std::lround((kMenuButtons.back().rect.y + kMenuButtons.back().rect.h) -
                                         (kMenuButtons.front().rect.y - 8.0f) + 8.0f))
        };
        SDL_RenderSetClipRect(renderer, &buttonClipRect);

        for (std::size_t i = 0; i < kMenuButtons.size(); ++i) {
            const MenuButton& button = kMenuButtons[i];
            const bool selected = button.action == state.mainSelection;
            const float visibleProgress = smoothstep01(buttonProgress(state, i));
            if (visibleProgress <= 0.0f) {
                continue;
            }

            const SDL_FRect buttonRect = animatedButtonRect(state, i);
            renderButtonSkin(renderer, buttonRect, selected);

#ifdef VN_ENABLE_TTF
            const SDL_FRect codeRect{buttonRect.x + 24.0f, buttonRect.y, 78.0f, buttonRect.h};
            drawTextInRect(renderer, resources.smallFont, "0" + std::to_string(i + 1),
                           selected ? SDL_Color{24, 34, 68, 255} : SDL_Color{236, 246, 255, 255},
                           codeRect);

            const SDL_FRect labelRect{buttonRect.x + 120.0f, buttonRect.y, buttonRect.w - 202.0f, buttonRect.h};
            drawShadowedTextInRect(renderer, resources.itemFont, button.label,
                                   selected ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                                   labelRect, false);
#endif
        }

        SDL_RenderSetClipRect(renderer, nullptr);

        if (state.noticeTimer > 0.0f && !state.noticeText.empty()) {
            drawCyberPanel(renderer, kNoticeRect, 28.0f, 20.0f,
                           SDL_Color{12, 18, 34, 222}, SDL_Color{255, 118, 198, 218},
                           SDL_Color{96, 238, 255, 24}, SDL_Color{6, 10, 18, 92});
#ifdef VN_ENABLE_TTF
            drawTextInRect(renderer, resources.itemFont, state.noticeText,
                           SDL_Color{240, 246, 250, 255},
                           SDL_FRect{kNoticeRect.x + 20.0f, kNoticeRect.y, kNoticeRect.w - 40.0f, kNoticeRect.h});
#endif
        }
    }
};

const MainMenuController& mainMenuController() {
    static const MainMenuController controller;
    return controller;
}

}  // namespace

void renderMainMenu(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                    int windowWidth, int windowHeight) {
    mainMenuController().render(renderer, resources, state, windowWidth, windowHeight);
}

void handleMainMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight) {
    mainMenuController().handleEvent(state, window, event, windowWidth, windowHeight);
}
