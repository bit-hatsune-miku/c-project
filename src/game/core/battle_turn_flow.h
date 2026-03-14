#ifndef BATTLE_TURN_FLOW_H
#define BATTLE_TURN_FLOW_H

#include <string>

#include "battle_manager.h"

namespace battle::flow {

struct PreviewActorContext {
    bool valid = false;
    ParticipantType type = ParticipantType::Character;
    std::string key;
    int partyIndex = -1;
    bool isExtraTurn = false;
};

PreviewActorContext inspectPreviewActor(const BattleManager& manager);

} // namespace battle::flow

#endif
