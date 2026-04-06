#ifndef ABILITY_SYSTEM_H
#define ABILITY_SYSTEM_H

#include <functional>
#include <vector>

#include "battle_manager.h"

namespace battle::ability {

bool isTeamShieldBurstUltimate(const AbilityDefinition& ability);
bool usesSpecialDamageSource(const AbilityDefinition& ability);
int resolveSupportAmount(const AbilityDefinition& ability,
                         int casterMaxHp,
                         float presentationMultiplier,
                         int legacyFallbackAmount);
void executeAbilityEffect(const AbilityExecutionContext& context,
                          int& bossCurrentHp,
                          std::vector<BattleCharacter>& characters);

float runPresentationInteraction(const PresentationContext& context);

using PresentationInteractionRunner = std::function<float(const PresentationContext&)>;

void setPresentationInteractionRunner(PresentationInteractionRunner runner);

} // namespace battle::ability

#endif // ABILITY_SYSTEM_H
