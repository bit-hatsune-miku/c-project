#include <iostream>
#include <string>

#include <SDL2/SDL.h>

#include "window.h"
#include "game/demo_battle_session.h"
#include "game/vn/vn_system.h"

int main(int argc, char** argv) {
    #ifdef LYOO_PLOT_TWIST_DEMO
        std::string battleKey = "lyoo_plot_twist";
    #elif defined(WECHATALIPAY_DEMO)
        std::string battleKey = "scan_to_pay";
    #elif defined(JIAFEI_DEMO)
        std::string battleKey = "wild_scent_flowers";
    #else
        std::string battleKey = "test_ground";
    #endif
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        battleKey = argv[1];
    }

    Window window("Demo - Tutorial Battle (Space=dialogue/act, F=toggle freeview)", 1280, 720);
    if (!window.isOpen()) {
        std::cerr << "Failed to initialize window\n";
        return 1;
    }
    SDL_SetWindowResizable(window.getNativeWindow(), SDL_FALSE);

    if (!vn::initialize(window.getRenderer(), window.getWidth(), window.getHeight())) {
        std::cerr << "[Demo] VN system initialization failed\n";
        return 1;
    }

    battle::demo::Session session;
    if (!session.initialize(window.getRenderer(), battleKey)) {
        session.shutdown();
        vn::shutdown();
        return 1;
    }

    Uint64 lastCounter = SDL_GetPerformanceCounter();

    std::cerr << "[Demo] Entering main loop\n";
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
