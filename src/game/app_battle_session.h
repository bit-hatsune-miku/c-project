#ifndef APP_BATTLE_SESSION_H
#define APP_BATTLE_SESSION_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "../GameMenu/menu_shared.h"
#include "../graphics/rmlui_loading_overlay.h"
#include "core/player_progression.h"
#include "post_battle_types.h"

class Window;

namespace battle::app {

enum class BattleOutcome {
    None,
    Victory,
    Defeat
};

enum class BattleDefeatResultAction {
    Return,
    ContinueStory,
    RestartStory
};

struct BattleResultPresentationConfig {
    BattleDefeatResultAction defeatAction = BattleDefeatResultAction::Return;
};

class SessionImpl;

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool initialize(Window& window,
                    GameSettings& settings,
                    const std::string& battleKey,
                    const PlayerProgression& progression,
                    std::optional<std::vector<std::string>> initialPartyLineup = std::nullopt,
                    BattleResultPresentationConfig resultPresentation = {});
    void shutdown();
    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render();
    void setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state);
    bool isFinished() const;
    bool exitedToMainMenu() const;
    BattleOutcome outcome() const;
    const std::vector<std::string>& currentPartyLineup() const;
    const battle::postbattle::Summary& postBattleSummary() const;

private:
    std::unique_ptr<SessionImpl> impl_;
};

} // namespace battle::app

#endif
