#ifndef BATTLE_POST_BATTLE_SESSION_H
#define BATTLE_POST_BATTLE_SESSION_H

#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "../game/audio/ui_music_types.h"
#include "../graphics/rmlui_loading_overlay.h"
#include "core/battle_manager.h"
#include "core/player_progression.h"
#include "post_battle_types.h"

class Window;

namespace battle::postbattle {

class SessionImpl;

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool initialize(Window& window,
                    const BattleDefinition& battleDefinition,
                    bool victory,
                    const Summary& summary,
                    const PlayerProgression& progression,
                    const std::vector<std::string>& partyLineup);
    void shutdown();
    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render();
    void setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state);
    void setUiMusicVisualState(const game::audio::UiMusicVisualState& state);
    bool isFinished() const;
    const PlayerProgression& progression() const;

private:
    std::unique_ptr<SessionImpl> impl_;
};

}  // namespace battle::postbattle

#endif  // BATTLE_POST_BATTLE_SESSION_H
