#ifndef APP_BATTLE_SESSION_H
#define APP_BATTLE_SESSION_H

#include <memory>

#include <SDL2/SDL.h>

#include "../GameMenu/menu_shared.h"

class Window;

namespace battle::app {

class SessionImpl;

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool initialize(Window& window, GameSettings& settings);
    void shutdown();
    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render();
    bool isFinished() const;

private:
    std::unique_ptr<SessionImpl> impl_;
};

} // namespace battle::app

#endif
