#include <iostream>
#include <string>
#include <vector>

#include "window.h"
#include "game/vn_system.h"
#include "game/vn_script.h"

int main() {
    Window window("VN Testing", 1280, 720);
    
    if (!window.isOpen()) {
        std::cerr << "Failed to initialize window" << std::endl;
        return 1;
    }

    if (!vn::initialize(window.getRenderer(), window.getWidth(), window.getHeight())) {
        std::cerr << "Failed to initialize VN system" << std::endl;
        return 1;
    }

    // Load chapter 0 script from JSON
    vn::Script script;
    if (!vn::loadScript("assets/vn/json/ch0.json", script)) {
        std::cerr << "Failed to load chapter 0 script" << std::endl;
        vn::shutdown();
        return 1;
    }

    vn::setTypewriterSpeed(42.0f);

    size_t entryIndex = 0;
    if (!script.entries.empty()) {
        const auto& entry = script.entries[entryIndex];
        vn::showLine(
            entry.text,
            vn::getDisplaySpeakerName(entry),
            entry.icon,
            entry.voice,
            entry.fontPath,
            entry.autoAdvanceOnVoiceEnd,
            entry.iconFrameCount,
            entry.iconFps,
            entry.background
        );
    }

    Uint64 lastCounter = SDL_GetPerformanceCounter();
    
    while (window.isOpen()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            window.handleEvent(event);

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
                vn::onSpacePressed();
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        float delta = static_cast<float>(now - lastCounter) /
            static_cast<float>(SDL_GetPerformanceFrequency());
        lastCounter = now;

        vn::update(delta);

        // Check if we should advance to the next line
        if (vn::consumeAdvanceRequest()) {
            entryIndex++;
            if (entryIndex >= script.entries.size()) {
                break;
            }

            const auto& entry = script.entries[entryIndex];
            vn::showLine(
                entry.text,
                vn::getDisplaySpeakerName(entry),
                entry.icon,
                entry.voice,
                entry.fontPath,
                entry.autoAdvanceOnVoiceEnd,
                entry.iconFrameCount,
                entry.iconFps,
                entry.background
            );
        }

        window.clear(24, 24, 32, 255);
        vn::render();
        window.present();
    }

    vn::shutdown();
    
    return 0;
}
