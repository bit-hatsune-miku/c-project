#ifndef BATTLE_FLOW_CONTROLLER_H
#define BATTLE_FLOW_CONTROLLER_H

#include "battle_manager.h"

namespace battle::flow {

struct PlayerTurnExecution {
    bool executed = false;
    bool actorWasMiku = false;
    bool actorWasMikuUltimateExtra = false;
};

PlayerTurnExecution executeDefaultPlayerTurn(BattleManager& manager);
bool isNextActorBoss(const BattleManager& manager);

} // namespace battle::flow

#endif
