#ifndef DEMO_BATTLE_SESSION_H
#define DEMO_BATTLE_SESSION_H

#include <memory>
#include <string>

#include <SDL2/SDL.h>

namespace battle::demo {

class SessionImpl;

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool initialize(SDL_Renderer* renderer, const std::string& battleKey = "tutorial_vs_lyoo");
    void shutdown();
    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render(SDL_Renderer* renderer, int screenWidth, int screenHeight);
    bool isFinished() const;

private:
    std::unique_ptr<SessionImpl> impl_;
};

} // namespace battle::demo

#endif
