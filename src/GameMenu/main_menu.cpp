#include "menu_shared.h"

#include <algorithm>
#include <array>

#include "../window.h"

namespace {

int actionIndex(MainMenuAction action) {
    return static_cast<int>(action);
}

MainMenuAction actionForIndex(int index) {
    const int clampedIndex = std::clamp(index, 0, static_cast<int>(kMainMenuPresentation.size()) - 1);
    return kMainMenuPresentation[static_cast<std::size_t>(clampedIndex)].action;
}

}  // namespace

void applyMainMenuAction(AppState& state, Window& window, MainMenuAction action) {
    state.mainSelection = action;

    switch (action) {
        case MainMenuAction::Start:
            beginStory(state);
            break;

        case MainMenuAction::Load:
            openLoadMenu(state, ScreenState::MainMenu);
            break;

        case MainMenuAction::Battle:
            beginBattleDemo(state);
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

#if !defined(APP_ENABLE_RMLUI)

namespace {

constexpr float kReferenceWidth = 1280.0f;

constexpr SDL_FRect kMenuShellRect{64.0f, 56.0f, 486.0f, 608.0f};
constexpr SDL_FRect kTitleShellRect{64.0f, 56.0f, 486.0f, 170.0f};
constexpr SDL_FRect kStatusShellRect{64.0f, 248.0f, 486.0f, 82.0f};
constexpr SDL_FRect kFooterRect{64.0f, 612.0f, 486.0f, 52.0f};
constexpr SDL_FRect kCaptionShellRect{900.0f, 544.0f, 314.0f, 116.0f};
constexpr SDL_FRect kHeroFrameRect{336.0f, 72.0f, 888.0f, 532.0f};

constexpr std::array<SDL_FRect, 5> kButtonRects{{
    {86.0f, 360.0f, 442.0f, 38.0f},
    {86.0f, 408.0f, 442.0f, 38.0f},
    {86.0f, 456.0f, 442.0f, 38.0f},
    {86.0f, 504.0f, 442.0f, 38.0f},
    {86.0f, 552.0f, 442.0f, 38.0f},
}};

constexpr SDL_Color kOverlayTint{2, 10, 11, 178};
constexpr SDL_Color kShellFill{4, 12, 14, 208};
constexpr SDL_Color kPanelFill{8, 20, 22, 220};
constexpr SDL_Color kOutline{93, 255, 241, 102};
constexpr SDL_Color kOutlineSoft{93, 255, 241, 58};
constexpr SDL_Color kHighlight{160, 255, 248, 174};
constexpr SDL_Color kShadow{0, 0, 0, 154};
constexpr SDL_Color kTextPrimary{240, 255, 253, 255};
constexpr SDL_Color kTextMuted{208, 255, 250, 178};
constexpr SDL_Color kAccentSoft{57, 197, 187, 90};
constexpr SDL_Color kAccentLine{114, 255, 245, 210};

const SDL_FRect& buttonRect(MainMenuAction action) {
    return kButtonRects[static_cast<std::size_t>(actionIndex(action))];
}

void drawMenuButton(SDL_Renderer* renderer,
                    const MenuResources& resources,
                    const MainMenuPresentation& item,
                    bool selected) {
    const SDL_FRect rect = buttonRect(item.action);
    const SDL_Color fill = selected ? SDL_Color{18, 70, 74, 220} : SDL_Color{8, 22, 24, 136};
    const SDL_Color outline = selected ? SDL_Color{125, 255, 245, 188} : SDL_Color{93, 255, 241, 52};
    const SDL_Color highlight = selected ? SDL_Color{205, 255, 250, 220} : kHighlight;
    const SDL_Color shadow = SDL_Color{0, 0, 0, 170};
    drawJaggedButtonPanel(renderer, rect, 20.0f, 26.0f, 16.0f, fill, outline, highlight, shadow);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, selected ? 125 : 98, 255, selected ? 245 : 243, selected ? 220 : 90);
    const SDL_FRect beam{rect.x - 1.0f, rect.y - 1.0f, 4.0f, rect.h + 2.0f};
    SDL_RenderFillRectF(renderer, &beam);

#ifdef VN_ENABLE_TTF
    if (resources.smallFont != nullptr) {
        drawTextInRect(renderer, resources.smallFont, item.focusCode,
                       selected ? SDL_Color{184, 255, 249, 255} : SDL_Color{184, 255, 248, 190},
                       SDL_FRect{rect.x + 14.0f, rect.y + 8.0f, 36.0f, 18.0f}, false);
    }
    if (resources.itemFont != nullptr) {
        drawTextInRect(renderer, resources.itemFont, item.displayLabel,
                       selected ? kTextPrimary : SDL_Color{232, 255, 252, 244},
                       SDL_FRect{rect.x + 72.0f, rect.y + 3.0f, 180.0f, 28.0f}, false);
    }
    if (resources.tinyFont != nullptr) {
        drawTextInRect(renderer, resources.tinyFont, item.displayDetail,
                       selected ? SDL_Color{235, 255, 252, 194} : SDL_Color{189, 255, 248, 150},
                       SDL_FRect{rect.x + 248.0f, rect.y + 10.0f, 182.0f, 14.0f}, false);
    }
#endif
}

void handleMouseSelection(AppState& state, float x, float y) {
    for (const MainMenuPresentation& item : kMainMenuPresentation) {
        if (pointInRect(x, y, buttonRect(item.action))) {
            state.mainSelection = item.action;
            return;
        }
    }
}

}  // namespace

void applyMainMenuAction(AppState& state, Window& window, MainMenuAction action) {
    state.mainSelection = action;

    switch (action) {
        case MainMenuAction::Start:
            beginStory(state);
            break;
        case MainMenuAction::Load:
            openLoadMenu(state, ScreenState::MainMenu);
            break;
        case MainMenuAction::Battle:
            beginBattleDemo(state);
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

void renderMainMenu(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                    int windowWidth, int windowHeight) {
    if (renderer == nullptr) {
        return;
    }

    renderBackgroundCover(renderer, resources.background, windowWidth, windowHeight);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, kOverlayTint.r, kOverlayTint.g, kOverlayTint.b, kOverlayTint.a);
    SDL_FRect fullscreenRect{0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)};
    SDL_RenderFillRectF(renderer, &fullscreenRect);

    const bool usingReferenceLayout = beginReferenceLayout(renderer, windowWidth, windowHeight);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, kAccentSoft.r, kAccentSoft.g, kAccentSoft.b, 30);
    SDL_FRect topGlow{0.0f, 0.0f, kReferenceWidth, 240.0f};
    SDL_RenderFillRectF(renderer, &topGlow);

    drawNeonLine(renderer, SDL_FPoint{56.0f, 36.0f}, SDL_FPoint{56.0f, 684.0f},
                 SDL_Color{86, 255, 241, 52}, SDL_Color{86, 255, 241, 98});
    drawNeonLine(renderer, SDL_FPoint{56.0f, 634.0f}, SDL_FPoint{1224.0f, 634.0f},
                 SDL_Color{86, 255, 241, 32}, SDL_Color{86, 255, 241, 58});
    drawCyberPanel(renderer, kMenuShellRect, 28.0f, 18.0f, kShellFill, kOutline, kHighlight, kShadow);
    drawCyberPanel(renderer, kStatusShellRect, 16.0f, 12.0f, kPanelFill, kOutlineSoft, kHighlight, kShadow);
    drawCyberPanel(renderer, kCaptionShellRect, 14.0f, 10.0f, kPanelFill, kOutlineSoft, kHighlight, kShadow);
    drawNeonLine(renderer, SDL_FPoint{kHeroFrameRect.x, kHeroFrameRect.y},
                 SDL_FPoint{kHeroFrameRect.x + kHeroFrameRect.w, kHeroFrameRect.y},
                 SDL_Color{87, 255, 241, 10}, SDL_Color{87, 255, 241, 42});
    drawNeonLine(renderer, SDL_FPoint{kHeroFrameRect.x, kHeroFrameRect.y},
                 SDL_FPoint{kHeroFrameRect.x, kHeroFrameRect.y + kHeroFrameRect.h},
                 SDL_Color{87, 255, 241, 10}, SDL_Color{87, 255, 241, 42});

    const SDL_FRect logoBounds{kTitleShellRect.x + 18.0f, kTitleShellRect.y + 36.0f, 430.0f, 130.0f};
    renderTextureContain(renderer, resources.titleLogo, logoBounds);

#ifdef VN_ENABLE_TTF
    if (resources.smallFont != nullptr) {
        drawTextInRect(renderer, resources.smallFont, "MAIN MENU",
                       SDL_Color{143, 255, 246, 255},
                       SDL_FRect{kMenuShellRect.x + 22.0f, kMenuShellRect.y + 20.0f, 160.0f, 16.0f}, false);
        drawTextInRect(renderer, resources.smallFont, "ARROWS / ENTER / CLICK",
                       SDL_Color{194, 255, 249, 164},
                       SDL_FRect{kFooterRect.x + 22.0f, kFooterRect.y + 18.0f, 200.0f, 14.0f}, false);
        drawTextInRect(renderer, resources.smallFont, "CHAPTER 0 ONLINE",
                       SDL_Color{194, 255, 249, 164},
                       SDL_FRect{kFooterRect.x + 282.0f, kFooterRect.y + 18.0f, 170.0f, 14.0f}, false);
    }

    if (resources.itemFont != nullptr) {
        const MainMenuPresentation& selectedItem = mainMenuPresentation(state.mainSelection);
        drawTextInRect(renderer, resources.itemFont, selectedItem.focusCode,
                       SDL_Color{147, 255, 247, 255},
                       SDL_FRect{kStatusShellRect.x + 16.0f, kStatusShellRect.y + 18.0f, 56.0f, 24.0f}, false);
        drawTextInRect(renderer, resources.itemFont, selectedItem.statusTitle,
                       kTextPrimary,
                       SDL_FRect{kStatusShellRect.x + 80.0f, kStatusShellRect.y + 10.0f, 330.0f, 24.0f}, false);
        drawWrappedTextInRect(renderer, resources.smallFont != nullptr ? resources.smallFont : resources.itemFont,
                              selectedItem.statusBody, kTextMuted,
                              SDL_FRect{kStatusShellRect.x + 80.0f, kStatusShellRect.y + 34.0f, 340.0f, 28.0f}, false);
    }
#endif

    for (const MainMenuPresentation& item : kMainMenuPresentation) {
        drawMenuButton(renderer, resources, item, item.action == state.mainSelection);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, kAccentLine.r, kAccentLine.g, kAccentLine.b, 210);
    SDL_FRect captionLine{kCaptionShellRect.x + 22.0f, kCaptionShellRect.y + 18.0f, 82.0f, 2.0f};
    SDL_RenderFillRectF(renderer, &captionLine);

    if (!state.noticeText.empty()) {
#ifdef VN_ENABLE_TTF
        const SDL_FRect noticeRect{624.0f, 72.0f, 404.0f, 54.0f};
        drawCyberPanel(renderer, noticeRect, 18.0f, 12.0f,
                       SDL_Color{10, 28, 30, 220}, kOutlineSoft, kHighlight, kShadow);
        if (resources.smallFont != nullptr) {
            drawWrappedTextInRect(renderer, resources.smallFont, state.noticeText,
                                  SDL_Color{232, 255, 252, 224},
                                  SDL_FRect{noticeRect.x + 18.0f, noticeRect.y + 16.0f, noticeRect.w - 36.0f, 22.0f},
                                  false);
        }
#endif
    }

    if (usingReferenceLayout) {
        endReferenceLayout(renderer);
    }
}

void handleMainMenuEvent(AppState& state, Window& window, const SDL_Event& event, int windowWidth, int windowHeight) {
    switch (event.type) {
        case SDL_MOUSEMOTION: {
            SDL_FPoint point{};
            if (mapWindowPointToReference(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y),
                                          windowWidth, windowHeight, point)) {
                handleMouseSelection(state, point.x, point.y);
            }
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                SDL_FPoint point{};
                if (mapWindowPointToReference(static_cast<float>(event.button.x), static_cast<float>(event.button.y),
                                              windowWidth, windowHeight, point)) {
                    handleMouseSelection(state, point.x, point.y);
                    if (pointInRect(point.x, point.y, buttonRect(state.mainSelection))) {
                        applyMainMenuAction(state, window, state.mainSelection);
                    }
                }
            }
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
                case SDLK_UP:
                case SDLK_w:
                    state.mainSelection =
                        actionForIndex((actionIndex(state.mainSelection) + static_cast<int>(kMainMenuPresentation.size()) - 1) %
                                       static_cast<int>(kMainMenuPresentation.size()));
                    break;

                case SDLK_DOWN:
                case SDLK_s:
                    state.mainSelection =
                        actionForIndex((actionIndex(state.mainSelection) + 1) %
                                       static_cast<int>(kMainMenuPresentation.size()));
                    break;

                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:
                    applyMainMenuAction(state, window, state.mainSelection);
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

#endif
