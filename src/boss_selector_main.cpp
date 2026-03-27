#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include <unistd.h>

#include "window.h"
#include "game/boss_selector_session.h"

namespace {

std::filesystem::path executableDirectory() {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path procExe("/proc/self/exe");
    if (fs::exists(procExe, ec)) {
        const fs::path exePath = fs::read_symlink(procExe, ec);
        if (!ec && !exePath.empty()) {
            return exePath.parent_path();
        }
    }
    return fs::current_path(ec);
}

std::optional<std::filesystem::path> resolveSiblingExecutable(const std::string& name) {
    namespace fs = std::filesystem;
    const fs::path candidate = executableDirectory() / name;
    std::error_code ec;
    if (fs::exists(candidate, ec)) {
        return candidate;
    }
    return std::nullopt;
}

int launchRequest(const battle::selector::LaunchRequest& request) {
    std::optional<std::filesystem::path> executablePath;
    std::vector<std::string> arguments;

    executablePath = resolveSiblingExecutable("OurUndergroundBITIdol");
    if (executablePath.has_value()) {
        arguments.push_back(executablePath->string());
        if (request.type == battle::selector::LaunchRequest::Type::Battle) {
            arguments.push_back("battle");
            arguments.push_back(request.reference);
        } else {
            arguments.push_back("story");
            arguments.push_back(request.reference);
        }
    } else if (request.type == battle::selector::LaunchRequest::Type::Battle) {
        executablePath = resolveSiblingExecutable("demo");
        if (!executablePath.has_value()) {
            std::cerr << "[BossSelector] Could not find sibling executable: OurUndergroundBITIdol or demo\n";
            return 1;
        }
        arguments.push_back(executablePath->string());
        arguments.push_back(request.reference);
    } else {
        executablePath = resolveSiblingExecutable(request.reference);
        if (!executablePath.has_value()) {
            std::cerr << "[BossSelector] Could not find sibling executable for story: "
                      << request.reference << " or OurUndergroundBITIdol\n";
            return 1;
        }
        arguments.push_back(executablePath->string());
    }

    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    execv(executablePath->c_str(), argv.data());
    std::cerr << "[BossSelector] Failed to exec " << executablePath->string()
              << ": " << std::strerror(errno) << "\n";
    return 1;
}

} // namespace

int main() {
    Window window("Boss Selector", 1280, 720);
    if (!window.isOpen()) {
        std::cerr << "[BossSelector] Failed to initialize window\n";
        return 1;
    }

    window.setEscapeToQuitEnabled(false);
    if (!window.enableOpenGL()) {
        std::cerr << "[BossSelector] Failed to enable OpenGL backend\n";
        return 1;
    }

    battle::selector::Session session;
    if (!session.initialize(window)) {
        session.shutdown();
        return 1;
    }

    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (window.isOpen() && !session.isFinished()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            window.handleEvent(event);
            session.handleEvent(event);

            if (const auto request = session.consumeLaunchRequest(); request.has_value()) {
                return launchRequest(*request);
            }
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(now - lastCounter) /
            static_cast<float>(SDL_GetPerformanceFrequency());
        lastCounter = now;

        session.update(deltaSeconds);

        if (const auto request = session.consumeLaunchRequest(); request.has_value()) {
            return launchRequest(*request);
        }

        session.render();
        window.present();
    }

    if (const auto request = session.consumeLaunchRequest(); request.has_value()) {
        return launchRequest(*request);
    }

    session.shutdown();
    return 0;
}
