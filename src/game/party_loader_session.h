#ifndef BATTLE_PARTY_LOADER_SESSION_H
#define BATTLE_PARTY_LOADER_SESSION_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "../game/audio/ui_music_types.h"
#include "../graphics/rmlui_loading_overlay.h"
#include "core/battle_manager.h"
#include "core/player_progression.h"

class Window;

namespace battle::partyloader {

struct Request {
    enum class Type {
        ConfirmBattle,
        CancelToMainMenu,
    };

    Type type = Type::ConfirmBattle;
    std::vector<std::string> partyKeys;
};

class SessionImpl;

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool initialize(Window& window,
                    const BattleDefinition& battleDefinition,
                    const PlayerProgression& progression);
    void shutdown();
    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render();
    void setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state);
    void setUiMusicVisualState(const game::audio::UiMusicVisualState& state);
    std::optional<Request> consumeRequest();

private:
    std::unique_ptr<SessionImpl> impl_;
};

}  // namespace battle::partyloader

#endif  // BATTLE_PARTY_LOADER_SESSION_H
