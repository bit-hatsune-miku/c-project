#ifndef BATTLE_BOSS_SELECTOR_SESSION_H
#define BATTLE_BOSS_SELECTOR_SESSION_H

#include <memory>
#include <optional>
#include <string>

#include <SDL2/SDL.h>

#include "../game/audio/ui_music_types.h"
#include "../graphics/rmlui_loading_overlay.h"

class Window;

/**
 * @brief Encapsulates a requested launch action produced by the boss selector session.
 *
 * Specifies what the session intends to start next.
 *
 * @note The meaning of `reference` depends on `mode` (for example, a level id for a straight-to-battle practice
 * or a replay identifier for story replay).
 *
 * @var LaunchRequest::Mode mode
 * Mode of the requested launch.
 *
 * @var std::string reference
 * Identifier or payload used by the target launch (mode-specific).
 */
namespace battle::selector {

struct LaunchRequest {
    enum class Mode {
        PracticeStraightToBattle,
        PracticeReplayStory
    };

    Mode mode = Mode::PracticeStraightToBattle;
    std::string reference;
};

class SessionImpl;

/**
 * Manage the boss selector session lifecycle, UI/audio state, and launch outcomes.
 */

/**
 * Construct an empty Session.
 */

/**
 * Destroy the Session and release its resources.
 */

/**
 * Initialize the session with the given application window.
 * @param window Window used for creating UI and rendering contexts.
 * @returns `true` if initialization succeeded, `false` otherwise.
 */

/**
 * Shutdown the session and stop any running activities.
 */

/**
 * Process a single SDL event for the session.
 * @param event SDL event to handle.
 */

/**
 * Advance the session state by the given time delta.
 * @param deltaSeconds Time elapsed since the last update in seconds.
 */

/**
 * Render the session visuals to the current render target.
 */

/**
 * Update the state of the on-screen loading overlay.
 * @param state New loading overlay state to apply.
 */

/**
 * Update the UI music visual state used by the session.
 * @param state New UI music visual state to apply.
 */

/**
 * Query whether the session has finished.
 * @returns `true` if the session has completed, `false` otherwise.
 */

/**
 * Retrieve and clear any pending launch request produced by the session.
 * @returns An optional LaunchRequest containing the next launch target if available, or empty otherwise.
 */
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
