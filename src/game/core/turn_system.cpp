#include "turn_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace battle::turn {
namespace {

constexpr float kActionValueNumerator = 10000.0f;
constexpr float kActionValueEpsilon = 0.0001f;

} // namespace

float actionValueFromSpeed(int spd) {
    const int safeSpd = std::max(1, spd);
    return kActionValueNumerator / static_cast<float>(safeSpd);
}

bool turnPriorityLess(const TurnActor& a, const TurnActor& b) {
    if (a.priority != b.priority) {
        return a.priority > b.priority;
    }

    if (a.type != b.type) {
        return a.type == ParticipantType::Boss;
    }

    if (a.type == ParticipantType::Character && b.type == ParticipantType::Character) {
        return a.partyIndex < b.partyIndex;
    }

    return a.key < b.key;
}

bool buildInitialTurnState(const BattleState& state, TurnState& outTurnState) {
    outTurnState = TurnState{};

    TurnActor bossActor;
    bossActor.type = ParticipantType::Boss;
    bossActor.key = state.boss.key;
    bossActor.assetId = state.boss.assets;
    bossActor.title = state.boss.title;
    bossActor.partyIndex = -1;
    bossActor.priority = 0;
    bossActor.isExtraTurn = false;
    bossActor.extraTurnAction = BattleAction::Skill;
    bossActor.autoExecute = false;
    bossActor.grantsUltimatePointOnAction = false;
    bossActor.spd = state.boss.spd;
    bossActor.baseActionValue = actionValueFromSpeed(state.boss.spd);
    bossActor.currentActionValue = bossActor.baseActionValue;
    outTurnState.actors.push_back(bossActor);

    for (size_t i = 0; i < state.party.size(); ++i) {
        const CharacterDefinition& c = state.party[i];
        TurnActor actor;
        actor.type = ParticipantType::Character;
        actor.key = c.key;
        actor.assetId = c.assets;
        actor.title = c.title;
        actor.partyIndex = static_cast<int>(i);
        actor.priority = 0;
        actor.isExtraTurn = false;
        actor.extraTurnAction = BattleAction::Skill;
        actor.autoExecute = false;
        actor.grantsUltimatePointOnAction = true;
        actor.spd = c.spd;
        actor.baseActionValue = actionValueFromSpeed(c.spd);
        actor.currentActionValue = actor.baseActionValue;
        outTurnState.actors.push_back(actor);
    }

    return !outTurnState.actors.empty();
}

TurnEvent peekNextTurnEvent(const TurnState& turnState) {
    TurnEvent event;
    if (turnState.actors.empty()) {
        return event;
    }

    float minAv = std::numeric_limits<float>::max();
    for (const TurnActor& actor : turnState.actors) {
        minAv = std::min(minAv, actor.currentActionValue);
    }

    std::vector<size_t> candidates;
    for (size_t i = 0; i < turnState.actors.size(); ++i) {
        if (std::fabs(turnState.actors[i].currentActionValue - minAv) <= kActionValueEpsilon) {
            candidates.push_back(i);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [&](size_t lhs, size_t rhs) {
        return turnPriorityLess(turnState.actors[lhs], turnState.actors[rhs]);
    });

    if (candidates.empty()) {
        return event;
    }

    event.valid = true;
    event.consumedActionValue = minAv;
    event.actingActorIndex = candidates.front();
    return event;
}

TurnEvent advanceToNextTurnEvent(TurnState& turnState) {
    TurnEvent event = peekNextTurnEvent(turnState);
    if (!event.valid) {
        return event;
    }

    for (TurnActor& actor : turnState.actors) {
        actor.currentActionValue -= event.consumedActionValue;
        if (std::fabs(actor.currentActionValue) <= kActionValueEpsilon) {
            actor.currentActionValue = 0.0f;
        }
    }

    return event;
}

void queueExtraTurnForCharacter(TurnState& turnState,
                                const CharacterDefinition& character,
                                int partyIndex,
                                int spd,
                                BattleAction action,
                                bool autoExecute,
                                bool grantsUltimatePointOnAction,
                                const std::string& abilityKitOverride,
                                int priority) {
    TurnActor extra;
    extra.type = ParticipantType::Character;
    extra.key = character.key;
    extra.assetId = character.assets;
    extra.title = character.title;
    extra.abilityKitOverride = abilityKitOverride;
    extra.partyIndex = partyIndex;
    extra.priority = priority;
    extra.isExtraTurn = true;
    extra.extraTurnAction = action;
    extra.autoExecute = autoExecute;
    extra.grantsUltimatePointOnAction = grantsUltimatePointOnAction;
    extra.spd = std::max(1, spd);
    extra.baseActionValue = actionValueFromSpeed(extra.spd);
    extra.currentActionValue = 0.0f;

    turnState.actors.push_back(extra);
}

} // namespace battle::turn
