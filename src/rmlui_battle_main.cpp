#include <iostream>

#include <SDL2/SDL.h>

#include "window.h"
#include "game/app_battle_session.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Window window("Battle Testing - RmlUi HUD Smoke", 1280, 720);
    if (!window.isOpen()) {
        std::cerr << "Failed to initialize window\n";
        return 1;
    }

    window.setEscapeToQuitEnabled(false);
    if (!window.enableOpenGL()) {
        std::cerr << "Failed to enable OpenGL backend\n";
        return 1;
    }

    GameSettings settings;
    battle::app::Session session;
    if (!session.initialize(window, settings)) {
        session.shutdown();
        return 1;
    }

    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (window.isOpen() && !session.isFinished()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            window.handleEvent(event);
            session.handleEvent(event);
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(now - lastCounter) /
            static_cast<float>(SDL_GetPerformanceFrequency());
        lastCounter = now;

        session.update(deltaSeconds);
        session.render();
        window.present();
    }

    session.shutdown();
    return 0;
}
