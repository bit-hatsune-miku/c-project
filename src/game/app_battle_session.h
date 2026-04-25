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

/**
 * Create a new battle session.
 */

/**
 * Destroy the session and release associated resources.
 */

/**
 * Initialize the session with the provided window, settings, battle identifier, and player progression.
 * Optionally provide an initial party lineup and presentation configuration for defeat handling.
 * @param window The Window the session will render into and receive input from.
 * @param settings Game settings that influence session behavior.
 * @param battleKey Identifier of the battle scenario to run.
 * @param flowMode Launch context for the battle; used for story/practice-specific setup.
 * @param progression Player progression state to apply to the battle.
 * @param initialPartyLineup Optional explicit party lineup; if not provided the session will use the default lineup.
 * @param resultPresentation Configuration that controls how defeat results are presented and handled.
 * @returns `true` if initialization succeeded, `false` otherwise.
 */

/**
 * Shut down the session and perform necessary cleanup so the object can be destroyed or reinitialized.
 */

/**
 * Process a single SDL event for the session.
 * @param event SDL event to handle.
 */

/**
 * Advance session logic by the given elapsed time.
 * @param deltaSeconds Time elapsed since the last update, in seconds.
 */

/**
 * Render the session's current visual state to the associated window/context.
 */

/**
 * Set the state of the RmlUI loading overlay used by the session.
 * @param state Loading overlay state to apply.
 */

/**
 * Determine whether the session has finished (battle completed and post-battle flow concluded).
 * @returns `true` if the session is finished, `false` otherwise.
 */

/**
 * Determine whether the session ended by returning to the main menu.
 * @returns `true` if the session exited to the main menu, `false` otherwise.
 */

/**
 * Get the terminal outcome of the battle.
 * @returns The current or final BattleOutcome.
 */

/**
 * Get the current party lineup used during the session.
 * @returns Reference to the vector of character identifiers representing the current party lineup.
 */

/**
 * Retrieve the post-battle summary data produced after the battle completes.
 * @returns Reference to the session's post-battle Summary.
 */
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
                    BattleFlowMode flowMode,
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
    const std::string& completedTutorialKey() const;
    const battle::postbattle::Summary& postBattleSummary() const;

private:
    std::unique_ptr<SessionImpl> impl_;
};

} // namespace battle::app

#endif
