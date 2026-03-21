#ifndef PARTY_SELECT_SESSION_H
#define PARTY_SELECT_SESSION_H

#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

class Window;

namespace party_select {

class SessionImpl;

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool initialize(Window& window);
    void shutdown();
    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render();
    bool isFinished() const;
    bool isConfirmed() const;
    std::vector<std::string> getSelectedPartyKeys() const;

private:
    std::unique_ptr<SessionImpl> impl_;
};

} // namespace party_select

#endif
