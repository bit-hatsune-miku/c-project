#ifndef DEMO_BATTLE_SESSION_H
#define DEMO_BATTLE_SESSION_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "core/player_progression.h"

namespace battle::demo {

class SessionImpl;

enum class BattleOutcome {
    None,
    Victory,
    Defeat
};

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool initialize(SDL_Renderer* renderer,
                    const std::string& battleKey = "tutorial_vs_lyoo",
                    const PlayerProgression& progression = PlayerProgression{},
                    std::optional<std::vector<std::string>> initialPartyLineup = std::nullopt);
    void shutdown();
    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render(SDL_Renderer* renderer, int screenWidth, int screenHeight);
    bool isFinished() const;
    BattleOutcome outcome() const;
    const std::vector<std::string>& currentPartyLineup() const;

private:
    std::unique_ptr<SessionImpl> impl_;
};

} // namespace battle::demo

#endif
