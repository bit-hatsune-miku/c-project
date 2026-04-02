#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "window.h"
#include "game/app_battle_session.h"
#include "platform/runtime_flags.h"

namespace {

struct StartupArguments {
    bool skipAnimationsAndWaits = false;
    std::optional<std::string> battleKey;
};

StartupArguments parseStartupArguments(int argc, char** argv) {
    StartupArguments parsed;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr || argv[i][0] == '\0') {
            continue;
        }

        const std::string arg = argv[i];
        if (arg == "--skip-animations" || arg == "--fast") {
            parsed.skipAnimationsAndWaits = true;
            continue;
        }

        if (!parsed.battleKey.has_value()) {
            parsed.battleKey = arg;
        }
    }
    return parsed;
}

} // namespace

int main(int argc, char** argv) {
    const StartupArguments startupArgs = parseStartupArguments(argc, argv);
    platform::runtime::setSkipAnimationsAndWaitsEnabled(startupArgs.skipAnimationsAndWaits);

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
    battle::PlayerProgression progression = battle::makeDefaultPlayerProgression();
    battle::normalizePlayerProgression(progression, battle::ProgressionFallbackPolicy::FullRoster);

    const std::string battleKey = startupArgs.battleKey.value_or("scan_to_pay");
    const std::vector<std::string> fallbackLineup = {
        "miku",
        "cupcakke",
        "jiafei",
        "lyoo"
    };

    battle::app::Session session;
    if (!session.initialize(window, settings, battleKey, progression, fallbackLineup)) {
        std::cerr << "Failed to initialize battle session for battle key '" << battleKey << "'\n";
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
