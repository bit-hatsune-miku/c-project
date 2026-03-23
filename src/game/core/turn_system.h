#ifndef TURN_SYSTEM_H
#define TURN_SYSTEM_H

#include "battle_manager.h"

namespace battle::turn {

float actionValueFromSpeed(int spd);
bool turnPriorityLess(const TurnActor& a, const TurnActor& b);
bool buildInitialTurnState(const BattleState& state, TurnState& outTurnState);
TurnEvent peekNextTurnEvent(const TurnState& turnState);
TurnEvent advanceToNextTurnEvent(TurnState& turnState);
void queueExtraTurnForCharacter(TurnState& turnState,
                                const CharacterDefinition& character,
                                int partyIndex,
                                int spd,
                                BattleAction action = BattleAction::Ultimate,
                                bool autoExecute = false,
                                bool grantsUltimatePointOnAction = false,
                                int priority = 100);

} // namespace battle::turn

#endif // TURN_SYSTEM_H
