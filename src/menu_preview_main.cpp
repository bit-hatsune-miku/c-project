#include <iostream>

#include <SDL2/SDL.h>

#include "graphics/menu_preview_session.h"
#include "window.h"

int main() {
    Window window("Play Menu Preview", 960, 540, false);
    if (!window.isOpen()) {
        std::cerr << "[MenuPreview] Failed to initialize window\n";
        return 1;
    }

    if (!window.enableOpenGL()) {
        std::cerr << "[MenuPreview] Failed to enable OpenGL backend\n";
        return 1;
    }

    graphics::preview::PlayMenuPreviewSession session;
    if (!session.initialize(window)) {
        session.shutdown();
        return 1;
    }

    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (window.isOpen()) {
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
