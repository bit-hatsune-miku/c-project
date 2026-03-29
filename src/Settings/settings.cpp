#include "settings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "../game/vn/vn_system.h"
#include "../window.h"

namespace {

constexpr int kReferenceWidth = 1280;
constexpr float kSettingsShellWidth = 968.0f;
constexpr float kSettingsTitleWidth = 920.0f;
constexpr float kMinTextSpeed = 18.0f;
constexpr float kMaxTextSpeed = 90.0f;
constexpr float kTextSpeedRange = kMaxTextSpeed - kMinTextSpeed;
constexpr int kSettingsItemCount = 5;

using Item = SettingsItem;

constexpr float centeredX(float width) {
    return (static_cast<float>(kReferenceWidth) - width) * 0.5f;
}

constexpr SDL_FRect kSettingsTitleBackdropRect{centeredX(kSettingsShellWidth), 28.0f, kSettingsShellWidth, 106.0f};
constexpr SDL_FRect kSettingsTitleRect{centeredX(kSettingsTitleWidth), 38.0f, kSettingsTitleWidth, 86.0f};
constexpr SDL_FRect kSettingsShellRect{centeredX(kSettingsShellWidth), 154.0f, kSettingsShellWidth, 498.0f};
constexpr SDL_FRect kSettingsSectionLabelRect{centeredX(kSettingsTitleWidth) + 28.0f, 188.0f, 250.0f, 28.0f};
constexpr SDL_FRect kSettingsHintRect{centeredX(kSettingsTitleWidth) + 28.0f, 214.0f, 640.0f, 26.0f};
constexpr SDL_FRect kSettingsFooterBandRect{centeredX(kSettingsTitleWidth), 612.0f, kSettingsTitleWidth, 26.0f};
constexpr SDL_FRect kDisplayModeRowRect{centeredX(kSettingsTitleWidth), 262.0f, kSettingsTitleWidth, 52.0f};
constexpr SDL_FRect kMusicRowRect{centeredX(kSettingsTitleWidth), 326.0f, kSettingsTitleWidth, 52.0f};
constexpr SDL_FRect kVolumeRowRect{centeredX(kSettingsTitleWidth), 390.0f, kSettingsTitleWidth, 52.0f};
constexpr SDL_FRect kSpeedRowRect{centeredX(kSettingsTitleWidth), 454.0f, kSettingsTitleWidth, 52.0f};
constexpr SDL_FRect kBackRowRect{centeredX(kSettingsTitleWidth), 518.0f, kSettingsTitleWidth, 52.0f};
constexpr SDL_FRect kDisplayModeValueRect{centeredX(kSettingsTitleWidth) + 692.0f, 270.0f, 206.0f, 36.0f};
constexpr SDL_FRect kMusicSliderRect{centeredX(kSettingsTitleWidth) + 390.0f, 334.0f, 298.0f, 36.0f};
constexpr SDL_FRect kVolumeSliderRect{centeredX(kSettingsTitleWidth) + 390.0f, 398.0f, 298.0f, 36.0f};
constexpr SDL_FRect kSpeedSliderRect{centeredX(kSettingsTitleWidth) + 390.0f, 462.0f, 298.0f, 36.0f};
constexpr SDL_FRect kMusicValueRect{centeredX(kSettingsTitleWidth) + 708.0f, 334.0f, 190.0f, 36.0f};
constexpr SDL_FRect kVolumeValueRect{centeredX(kSettingsTitleWidth) + 708.0f, 398.0f, 190.0f, 36.0f};
constexpr SDL_FRect kSpeedValueRect{centeredX(kSettingsTitleWidth) + 708.0f, 462.0f, 190.0f, 36.0f};

struct SettingsRow {
    SettingsItem selection;
    const char* code;
    const char* label;
    SDL_FRect rowRect;
    SDL_FRect valueRect;
};

constexpr std::array<SettingsRow, kSettingsItemCount> kSettingsRows{{
    {Item::DisplayMode, "01", "Display Mode", kDisplayModeRowRect, kDisplayModeValueRect},
    {Item::MusicVolume, "02", "Music Volume", kMusicRowRect, kMusicValueRect},
    {Item::VoiceVolume, "03", "Voice Volume", kVolumeRowRect, kVolumeValueRect},
    {Item::TextSpeed, "04", "Text Speed", kSpeedRowRect, kSpeedValueRect},
    {Item::Back, "05", "Back", kBackRowRect, SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f}}
}};

/**
 * @brief Convert a SettingsItem enum value to its integer index.
 *
 * @return int Integer index corresponding to the enum value.
 */
constexpr int toIndex(SettingsItem item) {
    return static_cast<int>(item);
}

constexpr SettingsItem nextItem(SettingsItem item) {
    return static_cast<SettingsItem>((toIndex(item) + 1) % kSettingsItemCount);
}

constexpr SettingsItem previousItem(SettingsItem item) {
    return static_cast<SettingsItem>((toIndex(item) + (kSettingsItemCount - 1)) % kSettingsItemCount);
}

std::string formatPercent(float value) {
    return std::to_string(static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 100.0f))) + "%";
}

std::string formatCharsPerSecond(float value) {
    return std::to_string(static_cast<int>(std::lround(value))) + " cps";
}

std::string formatDisplayMode(bool fullscreen) {
    return fullscreen ? "Fullscreen" : "Windowed";
}

float normalizeTextSpeed(float textSpeed) {
    return std::clamp((textSpeed - kMinTextSpeed) / kTextSpeedRange, 0.0f, 1.0f);
}

/**
 * @brief Map a normalized value in [0,1] to the configured text speed range.
 *
 * @param normalizedValue Normalized input where 0 corresponds to the minimum text speed
 *                        and 1 corresponds to the maximum; values outside [0,1] are clamped.
 * @return float Text speed in characters per second, clamped to [kMinTextSpeed, kMaxTextSpeed].
 */
float denormalizeTextSpeed(float normalizedValue) {
    return kMinTextSpeed + std::clamp(normalizedValue, 0.0f, 1.0f) * kTextSpeedRange;
}

/**
 * @brief Update the game's music volume setting and apply it to the audio system.
 *
 * Clamps the provided value to the range [0.0, 1.0], stores it in `settings.musicVolume`,
 * and forwards the new volume to the audio subsystem.
 *
 * @param settings Mutable game settings object to update.
 * @param value Desired volume where 0.0 is silent and 1.0 is maximum.
 */
void setMusicVolume(GameSettings& settings, float value) {
    settings.musicVolume = std::clamp(value, 0.0f, 1.0f);
    vn::setMusicVolume(settings.musicVolume);
}

/**
 * @brief Adjusts the music volume by a fixed step in the specified direction.
 *
 * Adjusts and persists the music volume in `settings` by 0.05 (5%) per unit of `direction`,
 * clamping the resulting value to the valid [0, 1] range.
 *
 * @param settings Game settings instance to update with the new clamped music volume.
 * @param direction Positive to increase volume, negative to decrease volume; magnitude scales the change (0 = no change).
 */
void adjustMusicVolume(GameSettings& settings, int direction) {
    setMusicVolume(settings, settings.musicVolume + 0.05f * static_cast<float>(direction));
}

/**
 * @brief Set and apply the voice volume level.
 *
 * Clamps the provided value to the range [0.0, 1.0], stores it in settings.voiceVolume,
 * and updates the runtime voice volume.
 *
 * @param settings Mutable GameSettings instance to update.
 * @param value Desired volume level where 0.0 is silent and 1.0 is maximum.
 */
void setVoiceVolume(GameSettings& settings, float value) {
    settings.voiceVolume = std::clamp(value, 0.0f, 1.0f);
    vn::setVoiceVolume(settings.voiceVolume);
}

void adjustVoiceVolume(GameSettings& settings, int direction) {
    setVoiceVolume(settings, settings.voiceVolume + 0.05f * static_cast<float>(direction));
}

void setTextSpeed(GameSettings& settings, float value) {
    settings.textSpeed = std::clamp(value, kMinTextSpeed, kMaxTextSpeed);
    vn::setTypewriterSpeed(settings.textSpeed);
}

void adjustTextSpeed(GameSettings& settings, int direction) {
    setTextSpeed(settings, settings.textSpeed + 6.0f * static_cast<float>(direction));
}

void renderSettingsRowBase(SDL_Renderer* renderer, const SDL_FRect& rect, bool selected) {
    const SDL_Color fillColor = selected ? SDL_Color{156, 246, 255, 255} : SDL_Color{18, 44, 78, 236};
    const SDL_Color outlineColor = selected ? SDL_Color{255, 116, 204, 255} : SDL_Color{72, 232, 255, 224};
    const SDL_Color highlightColor = selected ? SDL_Color{255, 255, 255, 58} : SDL_Color{110, 208, 255, 34};
    const SDL_Color shadowColor = selected ? SDL_Color{22, 6, 40, 214} : SDL_Color{4, 8, 20, 194};
    drawJaggedButtonPanel(renderer, rect, 22.0f, 30.0f, 22.0f,
                          fillColor, outlineColor, highlightColor, shadowColor);

    const SDL_FRect leftTag{rect.x + 14.0f, rect.y + 9.0f, 78.0f, rect.h - 18.0f};
    drawSlantedPanel(renderer, leftTag, 16.0f,
                     selected ? SDL_Color{255, 104, 194, 228} : SDL_Color{64, 138, 214, 176});

    const SDL_FRect topStrip{rect.x + 92.0f, rect.y + 9.0f, rect.w - 126.0f, 6.0f};
    drawSlantedPanel(renderer, topStrip, 12.0f,
                     selected ? SDL_Color{255, 218, 104, 210} : SDL_Color{90, 246, 255, 160});

    const SDL_FRect lowerStrip{rect.x + 98.0f, rect.y + rect.h - 11.0f, rect.w - 170.0f, 4.0f};
    drawSlantedPanel(renderer, lowerStrip, 12.0f,
                     selected ? SDL_Color{72, 220, 255, 176} : SDL_Color{255, 118, 200, 130});
}

void renderSettingsValueBadge(SDL_Renderer* renderer, const SDL_FRect& rect, bool selected) {
    drawCyberPanel(renderer, rect, 14.0f, 10.0f,
                   selected ? SDL_Color{206, 242, 255, 255} : SDL_Color{32, 90, 146, 240},
                   selected ? SDL_Color{255, 116, 204, 232} : SDL_Color{72, 232, 255, 220},
                   selected ? SDL_Color{255, 255, 255, 48} : SDL_Color{108, 208, 255, 26},
                   selected ? SDL_Color{20, 8, 38, 180} : SDL_Color{8, 18, 34, 180});
}

void renderSlider(SDL_Renderer* renderer, const SDL_FRect& rect, float value, bool selected) {
    const float clampedValue = std::clamp(value, 0.0f, 1.0f);
    drawCyberPanel(renderer, rect, 12.0f, 10.0f,
                   selected ? SDL_Color{34, 104, 164, 246} : SDL_Color{18, 56, 98, 236},
                   selected ? SDL_Color{255, 118, 200, 210} : SDL_Color{72, 232, 255, 204},
                   selected ? SDL_Color{96, 222, 255, 26} : SDL_Color{84, 198, 255, 18},
                   SDL_Color{5, 12, 24, 140});

    const SDL_FRect fill{rect.x + 6.0f, rect.y + 5.0f, std::max(0.0f, (rect.w - 16.0f) * clampedValue), rect.h - 10.0f};
    if (fill.w > 0.0f) {
        drawSlantedPanel(renderer, fill, 10.0f,
                         selected ? SDL_Color{181, 246, 255, 255} : SDL_Color{90, 236, 255, 238});
    }

    const SDL_FRect knob{
        rect.x + 8.0f + (rect.w - 32.0f) * clampedValue,
        rect.y - 2.0f,
        24.0f,
        rect.h + 4.0f
    };
    drawCyberPanel(renderer, knob, 8.0f, 7.0f,
                   SDL_Color{224, 246, 255, 255},
                   selected ? SDL_Color{255, 118, 204, 236} : SDL_Color{72, 232, 255, 222},
                   SDL_Color{255, 255, 255, 58},
                   SDL_Color{16, 28, 52, 210});
}

}  // namespace

void SettingsMenuController::render(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                                    int windowWidth, int windowHeight, bool pausedBackdrop) const {
    if (pausedBackdrop) {
        renderPauseBackdrop(renderer, windowWidth, windowHeight);
    } else {
        renderBackgroundCover(renderer, resources.background, windowWidth, windowHeight);
    }

    const bool usingReferenceLayout = beginReferenceLayout(renderer, windowWidth, windowHeight);
    renderOverlay(renderer, resources, state);
    if (usingReferenceLayout) {
        endReferenceLayout(renderer);
    }
}

void SettingsMenuController::handleEvent(AppState& state, Window& window, const SDL_Event& event,
                                         int windowWidth, int windowHeight) const {
    if (event.type == SDL_MOUSEMOTION ||
        (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
        handleMouseEvent(state, window, event, windowWidth, windowHeight);
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        handleKeyboardEvent(state, window, event.key);
    }
}

void SettingsMenuController::applyDisplayMode(Window& window, GameSettings& settings, bool fullscreen) {
    if (window.setFullscreen(fullscreen)) {
        settings.fullscreen = fullscreen;
        vn::setViewportSize(window.getWidth(), window.getHeight());
    }
}

/**
 * @brief Render the settings menu overlay, including panels, labels, value badges, and sliders.
 *
 * Renders the decorative UI, text, individual setting rows, value badges, and three sliders (music, voice, text speed)
 * according to the provided application state; highlights the currently selected row and displays current values.
 *
 * @param renderer SDL renderer used for drawing.
 * @param resources Fonts and other resources required for text and UI rendering.
 * @param state Current application state used to determine selection and setting values to display.
 */
void SettingsMenuController::renderOverlay(SDL_Renderer* renderer, const MenuResources& resources,
                                           const AppState& state) const {
    const float shellAccentBottom = kBackRowRect.y + kBackRowRect.h + 20.0f;
    drawSlantedPanel(renderer,
                     SDL_FRect{kSettingsShellRect.x - 18.0f, kSettingsTitleBackdropRect.y + 18.0f,
                               18.0f, shellAccentBottom - (kSettingsTitleBackdropRect.y + 18.0f)},
                     6.0f,
                     SDL_Color{255, 102, 196, 164});
    drawSlantedPanel(renderer,
                     SDL_FRect{kSettingsShellRect.x + kSettingsShellRect.w + 6.0f, kSettingsTitleBackdropRect.y + 48.0f,
                               16.0f, shellAccentBottom - (kSettingsTitleBackdropRect.y + 48.0f)},
                     5.0f,
                     SDL_Color{72, 224, 255, 168});
    drawNeonLine(renderer,
                 SDL_FPoint{kSettingsTitleBackdropRect.x + 34.0f, kSettingsTitleBackdropRect.y + kSettingsTitleBackdropRect.h + 26.0f},
                 SDL_FPoint{kSettingsTitleBackdropRect.x + kSettingsTitleBackdropRect.w - 30.0f, kSettingsTitleBackdropRect.y + kSettingsTitleBackdropRect.h + 26.0f},
                 SDL_Color{255, 74, 164, 82}, SDL_Color{255, 128, 210, 180});

    drawCyberPanel(renderer, kSettingsTitleBackdropRect, 46.0f, 28.0f,
                   SDL_Color{18, 30, 56, 240},
                   SDL_Color{90, 238, 255, 255},
                   SDL_Color{116, 220, 255, 42},
                   SDL_Color{4, 8, 20, 220});
    drawSlantedPanel(renderer,
                     SDL_FRect{kSettingsTitleBackdropRect.x + 34.0f, kSettingsTitleBackdropRect.y + 18.0f,
                               kSettingsTitleBackdropRect.w - 160.0f, 7.0f},
                     18.0f,
                     SDL_Color{255, 110, 198, 204});
    drawSlantedPanel(renderer,
                     SDL_FRect{kSettingsTitleBackdropRect.x + 164.0f, kSettingsTitleBackdropRect.y + kSettingsTitleBackdropRect.h - 20.0f,
                               kSettingsTitleBackdropRect.w - 210.0f, 5.0f},
                     16.0f,
                     SDL_Color{255, 220, 118, 192});

    drawCyberPanel(renderer, kSettingsShellRect, 52.0f, 34.0f,
                   SDL_Color{10, 20, 42, 236},
                   SDL_Color{80, 228, 255, 244},
                   SDL_Color{72, 156, 255, 34},
                   SDL_Color{3, 8, 18, 220});
    drawCyberPanel(renderer,
                   SDL_FRect{kSettingsShellRect.x + 12.0f, kSettingsShellRect.y + 12.0f,
                             kSettingsShellRect.w - 24.0f, 94.0f},
                   32.0f, 18.0f,
                   SDL_Color{24, 44, 78, 220},
                   SDL_Color{255, 118, 200, 210},
                   SDL_Color{106, 224, 255, 30},
                   SDL_Color{4, 8, 20, 120});
    drawSlantedPanel(renderer,
                     SDL_FRect{kSettingsShellRect.x + 22.0f, kSettingsShellRect.y + 98.0f, kSettingsShellRect.w - 120.0f, 6.0f},
                     16.0f,
                     SDL_Color{255, 206, 102, 188});
    drawCyberPanel(renderer, kSettingsFooterBandRect, 16.0f, 12.0f,
                   SDL_Color{18, 36, 64, 230},
                   SDL_Color{72, 232, 255, 220},
                   SDL_Color{255, 255, 255, 24},
                   SDL_Color{4, 8, 16, 138});

#ifdef VN_ENABLE_TTF
    drawTextInRect(renderer, resources.smallFont, "SETTINGS",
                   SDL_Color{110, 240, 255, 255}, kSettingsSectionLabelRect, false);
    drawTextInRect(renderer, resources.smallFont, "ARROW KEYS OR MOUSE TO ADJUST LIVE VN BEHAVIOR",
                   SDL_Color{214, 246, 255, 255}, kSettingsHintRect, false);
    drawShadowedTextInRect(renderer, resources.titleFont, "System Settings",
                           SDL_Color{240, 248, 255, 255}, kSettingsTitleRect);
    drawTextInRect(renderer, resources.tinyFont != nullptr ? resources.tinyFont : resources.smallFont,
                   "ENTER TO TOGGLE DISPLAY MODE. DRAG OR TAP THE SLIDERS FOR MUSIC, VOICE, AND TEXT SPEED.",
                   SDL_Color{198, 246, 255, 255},
                   SDL_FRect{kSettingsFooterBandRect.x + 12.0f, kSettingsFooterBandRect.y,
                             kSettingsFooterBandRect.w - 24.0f, kSettingsFooterBandRect.h});
#endif

    for (const SettingsRow& row : kSettingsRows) {
        const bool selected = state.settingsSelection == row.selection;
        renderSettingsRowBase(renderer, row.rowRect, selected);
        if (row.valueRect.w > 0.0f) {
            renderSettingsValueBadge(renderer, row.valueRect, selected);
        }
    }

#ifdef VN_ENABLE_TTF
    const SDL_Color labelColor = SDL_Color{216, 248, 255, 255};
    const SDL_Color selectedLabelColor = SDL_Color{12, 28, 48, 255};
    for (const SettingsRow& row : kSettingsRows) {
        const bool selected = state.settingsSelection == row.selection;
        drawTextInRect(renderer, resources.smallFont, row.code,
                       selected ? SDL_Color{24, 34, 68, 255} : SDL_Color{236, 246, 255, 255},
                       SDL_FRect{row.rowRect.x + 10.0f, row.rowRect.y, 72.0f, row.rowRect.h});
        drawShadowedTextInRect(renderer, resources.itemFont, row.label,
                               selected ? selectedLabelColor : labelColor,
                               SDL_FRect{row.rowRect.x + 106.0f, row.rowRect.y,
                                         row.rowRect.w - 212.0f, row.rowRect.h},
                               false);
    }

    drawTextInRect(renderer, resources.smallFont, formatDisplayMode(state.settings.fullscreen),
                   state.settingsSelection == Item::DisplayMode ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                   kDisplayModeValueRect);
    drawTextInRect(renderer, resources.smallFont, formatPercent(state.settings.musicVolume),
                   state.settingsSelection == Item::MusicVolume ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                   kMusicValueRect);
    drawTextInRect(renderer, resources.smallFont, formatPercent(state.settings.voiceVolume),
                   state.settingsSelection == Item::VoiceVolume ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                   kVolumeValueRect);
    drawTextInRect(renderer, resources.smallFont, formatCharsPerSecond(state.settings.textSpeed),
                   state.settingsSelection == Item::TextSpeed ? SDL_Color{12, 28, 48, 255} : SDL_Color{216, 248, 255, 255},
                   kSpeedValueRect);
#endif

    renderSlider(renderer, kMusicSliderRect, state.settings.musicVolume, state.settingsSelection == Item::MusicVolume);
    renderSlider(renderer, kVolumeSliderRect, state.settings.voiceVolume, state.settingsSelection == Item::VoiceVolume);
    renderSlider(renderer, kSpeedSliderRect, normalizeTextSpeed(state.settings.textSpeed), state.settingsSelection == Item::TextSpeed);
}

void SettingsMenuController::handleMouseEvent(AppState& state, Window& window, const SDL_Event& event,
                                              int windowWidth, int windowHeight) const {
    SDL_FPoint settingsPoint{};
    const float inputX = static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x);
    const float inputY = static_cast<float>(event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y);
    if (!mapWindowPointToReference(inputX, inputY, windowWidth, windowHeight, settingsPoint)) {
        return;
    }

    updateSelectionFromMouse(state, settingsPoint.x, settingsPoint.y);
    if (event.type == SDL_MOUSEMOTION) {
        if ((event.motion.state & SDL_BUTTON_LMASK) != 0U) {
            updateFromPointer(state, window, settingsPoint.x, settingsPoint.y, false);
        }
        return;
    }

    updateFromPointer(state, window, settingsPoint.x, settingsPoint.y, true);
}

/**
 * @brief Handle keyboard input for navigating and changing Settings menu options.
 *
 * Processes navigation keys (Up/Down/W/S) to move the selection, Left/Right/A/D to
 * adjust the currently selected setting (toggle display mode or change volume/text speed),
 * Enter/Space to toggle or activate the selected item, and Escape to return to the
 * previous screen. Updates the provided application state and may modify the window's
 * display mode.
 *
 * Key behavior summary:
 * - Escape: set the current screen to the settings return screen.
 * - Up / W: move selection to the previous settings item.
 * - Down / S: move selection to the next settings item.
 * - Left / A: decrease the selected setting (or set windowed mode for display mode).
 * - Right / D: increase the selected setting (or set fullscreen for display mode).
 * - Enter / Keypad Enter / Space: toggle display mode if selected, or activate Back.
 *
 * @param state Application state to update (selection, settings, and screen).
 * @param window Window object used when applying display mode changes.
 * @param event Keyboard event to handle.
 */
void SettingsMenuController::handleKeyboardEvent(AppState& state, Window& window,
                                                 const SDL_KeyboardEvent& event) const {
    if (event.keysym.sym == SDLK_ESCAPE) {
        state.screen = state.settingsReturnScreen;
        return;
    }

    if (event.keysym.sym == SDLK_UP || event.keysym.sym == SDLK_w) {
        state.settingsSelection = previousItem(state.settingsSelection);
        return;
    }

    if (event.keysym.sym == SDLK_DOWN || event.keysym.sym == SDLK_s) {
        state.settingsSelection = nextItem(state.settingsSelection);
        return;
    }

    if (event.keysym.sym == SDLK_LEFT || event.keysym.sym == SDLK_a) {
        if (state.settingsSelection == Item::DisplayMode) {
            applyDisplayMode(window, state.settings, false);
        } else if (state.settingsSelection == Item::MusicVolume) {
            adjustMusicVolume(state.settings, -1);
        } else if (state.settingsSelection == Item::VoiceVolume) {
            adjustVoiceVolume(state.settings, -1);
        } else if (state.settingsSelection == Item::TextSpeed) {
            adjustTextSpeed(state.settings, -1);
        }
        return;
    }

    if (event.keysym.sym == SDLK_RIGHT || event.keysym.sym == SDLK_d) {
        if (state.settingsSelection == Item::DisplayMode) {
            applyDisplayMode(window, state.settings, true);
        } else if (state.settingsSelection == Item::MusicVolume) {
            adjustMusicVolume(state.settings, 1);
        } else if (state.settingsSelection == Item::VoiceVolume) {
            adjustVoiceVolume(state.settings, 1);
        } else if (state.settingsSelection == Item::TextSpeed) {
            adjustTextSpeed(state.settings, 1);
        }
        return;
    }

    if (event.keysym.sym == SDLK_RETURN || event.keysym.sym == SDLK_KP_ENTER || event.keysym.sym == SDLK_SPACE) {
        if (state.settingsSelection == Item::DisplayMode) {
            applyDisplayMode(window, state.settings, !state.settings.fullscreen);
        } else if (state.settingsSelection == Item::Back) {
            state.screen = state.settingsReturnScreen;
        }
    }
}

void SettingsMenuController::updateSelectionFromMouse(AppState& state, float mouseX, float mouseY) const {
    for (const SettingsRow& row : kSettingsRows) {
        if (pointInRect(mouseX, mouseY, row.rowRect)) {
            state.settingsSelection = row.selection;
            return;
        }
    }
}

/**
 * @brief Updates the current settings selection and adjusts settings based on a pointer position.
 *
 * Interprets the given reference-space mouse coordinates to update which row is selected,
 * toggle display mode, change music/voice/text-speed values by slider position, or activate
 * the Back action depending on which UI region contains the pointer.
 *
 * @param state Application state containing current settings and screen selection; modified when navigation or value changes occur.
 * @param window Window used when applying display-mode changes.
 * @param mouseX X coordinate of the pointer in reference layout coordinates.
 * @param mouseY Y coordinate of the pointer in reference layout coordinates.
 * @param allowNavigation If true, pointer interaction may trigger navigation actions (toggle display mode or activate Back); if false, only value adjustments are applied.
 */
void SettingsMenuController::updateFromPointer(AppState& state, Window& window, float mouseX, float mouseY,
                                               bool allowNavigation) const {
    updateSelectionFromMouse(state, mouseX, mouseY);

    if (pointInRect(mouseX, mouseY, kDisplayModeRowRect) && allowNavigation) {
        applyDisplayMode(window, state.settings, !state.settings.fullscreen);
    } else if (pointInRect(mouseX, mouseY, kMusicSliderRect)) {
        setMusicVolume(state.settings, (mouseX - kMusicSliderRect.x) / kMusicSliderRect.w);
    } else if (pointInRect(mouseX, mouseY, kVolumeSliderRect)) {
        setVoiceVolume(state.settings, (mouseX - kVolumeSliderRect.x) / kVolumeSliderRect.w);
    } else if (pointInRect(mouseX, mouseY, kSpeedSliderRect)) {
        setTextSpeed(state.settings, denormalizeTextSpeed((mouseX - kSpeedSliderRect.x) / kSpeedSliderRect.w));
    } else if (allowNavigation && pointInRect(mouseX, mouseY, kBackRowRect)) {
        state.screen = state.settingsReturnScreen;
    }
}
