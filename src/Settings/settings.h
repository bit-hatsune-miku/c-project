#ifndef SETTINGS_H
#define SETTINGS_H

#include <SDL2/SDL.h>

#include "../GameMenu/menu_shared.h"

class Window;

class SettingsMenuController {
public:
    void render(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state,
                int windowWidth, int windowHeight, bool pausedBackdrop) const;
    void handleEvent(AppState& state, Window& window, const SDL_Event& event,
                     int windowWidth, int windowHeight) const;

    static void applyDisplayMode(Window& window, GameSettings& settings, bool fullscreen);

private:
    void renderOverlay(SDL_Renderer* renderer, const MenuResources& resources, const AppState& state) const;
    void handleMouseEvent(AppState& state, Window& window, const SDL_Event& event,
                          int windowWidth, int windowHeight) const;
    void handleKeyboardEvent(AppState& state, Window& window, const SDL_KeyboardEvent& event) const;
    void updateSelectionFromMouse(AppState& state, float mouseX, float mouseY) const;
    void updateFromPointer(AppState& state, Window& window, float mouseX, float mouseY,
                           bool allowNavigation) const;
};

#endif
