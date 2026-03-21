#include "battle_turn_flow.h"

namespace battle::flow {

PreviewActorContext inspectPreviewActor(const BattleManager& manager) {
    PreviewActorContext ctx;

    const int previewActorIndex = manager.getPreviewNextActorIndex();
    if (previewActorIndex < 0) {
        return ctx;
    }

    const TurnState& turnState = manager.getTurnState();
    if (previewActorIndex >= static_cast<int>(turnState.actors.size())) {
        return ctx;
    }

    const TurnActor& actor = turnState.actors[static_cast<size_t>(previewActorIndex)];
    ctx.valid = true;
    ctx.type = actor.type;
    ctx.key = actor.key;
    ctx.partyIndex = actor.partyIndex;
    ctx.isExtraTurn = actor.isExtraTurn;
    ctx.extraTurnAction = actor.extraTurnAction;
    ctx.autoExecute = actor.autoExecute;
    return ctx;
}

} // namespace battle::flow
