#ifndef BATTLE_BOSS_SELECTOR_SESSION_H
#define BATTLE_BOSS_SELECTOR_SESSION_H

#include <memory>
#include <optional>
#include <string>

#include <SDL2/SDL.h>

#include "../game/audio/ui_music_types.h"
#include "../graphics/rmlui_loading_overlay.h"

class Window;

namespace battle::selector {

struct LaunchRequest {
    enum class Type {
        Battle,
        Story
    };

    Type type = Type::Battle;
    std::string reference;
};

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
    void setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state);
    void setUiMusicVisualState(const game::audio::UiMusicVisualState& state);
    bool isFinished() const;
    std::optional<LaunchRequest> consumeLaunchRequest();

private:
    std::unique_ptr<SessionImpl> impl_;
};

} // namespace battle::selector

#endif // BATTLE_BOSS_SELECTOR_SESSION_H
