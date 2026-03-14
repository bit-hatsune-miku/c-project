#include "battle_flow_controller.h"

#include "battle_turn_flow.h"

namespace battle::flow {

PlayerTurnExecution executeDefaultPlayerTurn(BattleManager& manager) {
    PlayerTurnExecution result;

    const PreviewActorContext preview = inspectPreviewActor(manager);
    result.actorWasMiku = preview.valid &&
                          preview.type == ParticipantType::Character &&
                          preview.key == "miku";
    result.actorWasMikuUltimateExtra = result.actorWasMiku && preview.isExtraTurn;

    result.executed = manager.executePlayerTurn();
    return result;
}

bool isNextActorBoss(const BattleManager& manager) {
    const PreviewActorContext preview = inspectPreviewActor(manager);
    return preview.valid && preview.type == ParticipantType::Boss;
}

} // namespace battle::flow
