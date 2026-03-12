#include <iostream>

#include <SDL2/SDL.h>

#include "window.h"
#include "game/demo_battle_session.h"
#include "game/vn_system.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Window window("Demo - Tutorial Battle (Space=dialogue/act, F=toggle freeview)", 1280, 720);
    if (!window.isOpen()) {
        std::cerr << "Failed to initialize window\n";
        return 1;
    }

    if (!vn::initialize(window.getRenderer(), window.getWidth(), window.getHeight())) {
        std::cerr << "[Demo] VN system initialization failed\n";
        return 1;
    }

    battle::demo::Session session;
    if (!session.initialize(window.getRenderer())) {
        session.shutdown();
        vn::shutdown();
        return 1;
    }

    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (window.isOpen() && !session.isFinished()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            window.handleEvent(event);

            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESIZED)) {
                vn::setViewportSize(window.getWidth(), window.getHeight());
            }

            session.handleEvent(event);
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(now - lastCounter) /
            static_cast<float>(SDL_GetPerformanceFrequency());
        lastCounter = now;

        session.update(deltaSeconds);
        session.render(window.getRenderer(), window.getWidth(), window.getHeight());
        window.present();
    }

    session.shutdown();
    vn::shutdown();
    return 0;
}
