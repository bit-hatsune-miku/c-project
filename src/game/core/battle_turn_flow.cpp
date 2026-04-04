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

PreviewAbilityContext inspectPreviewAbility(const BattleManager& manager) {
    PreviewAbilityContext ctx;

    const PreviewActorContext actor = inspectPreviewActor(manager);
    if (!actor.valid) {
        return ctx;
    }

    ctx.valid = true;
    ctx.actorType = actor.type;
    ctx.partyIndex = actor.partyIndex;
    ctx.action = actor.isExtraTurn ? actor.extraTurnAction : BattleAction::Skill;

    const BattleState& state = manager.getBattleState();
    if (actor.type == ParticipantType::Character) {
        if (actor.partyIndex < 0 || actor.partyIndex >= static_cast<int>(state.party.size())) {
            return PreviewAbilityContext{};
        }

        const CharacterDefinition& definition = state.party[static_cast<size_t>(actor.partyIndex)];
        ctx.abilityId = (ctx.action == BattleAction::Ultimate)
            ? definition.ultimate
            : getCharacterRegularAbilityId(definition);
        return ctx.abilityId.empty() ? PreviewAbilityContext{} : ctx;
    }

    if (actor.type == ParticipantType::Boss) {
        ctx.abilityId = getBossNormalAbilityId(state.boss);
        return ctx.abilityId.empty() ? PreviewAbilityContext{} : ctx;
    }

    return PreviewAbilityContext{};
}

} // namespace battle::flow
