#ifndef BATTLE_TURN_FLOW_H
#define BATTLE_TURN_FLOW_H

#include <string>

#include "battle_manager.h"

namespace battle::flow {

struct PreviewActorContext {
    bool valid = false;
    ParticipantType type = ParticipantType::Character;
    std::string key;
    std::string abilityKitOverride;
    int partyIndex = -1;
    bool isExtraTurn = false;
    BattleAction extraTurnAction = BattleAction::Skill;
    bool autoExecute = false;
};

struct PreviewAbilityContext {
    bool valid = false;
    ParticipantType actorType = ParticipantType::Character;
    int partyIndex = -1;
    BattleAction action = BattleAction::Skill;
    std::string abilityId;
};

PreviewActorContext inspectPreviewActor(const BattleManager& manager);
PreviewAbilityContext inspectPreviewAbility(const BattleManager& manager);

} // namespace battle::flow

#endif
