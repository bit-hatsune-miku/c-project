#include "ability_system.h"

#include <algorithm>

namespace battle::ability {
namespace {

int normalizeDamage(int value) {
    return std::max(1, value);
}

} // namespace

void executeAbilityEffect(const AbilityExecutionContext& context,
                          int& bossCurrentHp,
                          std::vector<BattleCharacter>& characters) {
    if (context.ability == nullptr) {
        return;
    }

    const AbilityDefinition& ability = *context.ability;

    switch (ability.type) {
        case AbilityType::Attack: {
            const int finalDamage = normalizeDamage(static_cast<int>(
                context.baseDamage * ability.multiplier * context.presentationMultiplier
            ));
            bossCurrentHp = std::max(0, bossCurrentHp - finalDamage);
            break;
        }

        case AbilityType::Heal: {
            const int healAmount = ability.flatHeal;
            if (ability.targetRule == TargetRule::AllAllies) {
                for (BattleCharacter& c : characters) {
                    if (c.isAlive()) {
                        c.receiveHealing(healAmount);
                    }
                }
            }
            break;
        }

        case AbilityType::Buff:
        case AbilityType::Debuff:
            // TODO: Implement buff/debuff system later
            break;
    }
}

float runPresentationInteraction(const PresentationContext& context) {
    // TODO: In SDL mode, this will launch rhythm/parry mini-games
    // and return the actual multiplier based on player performance.
    // For now in testing mode, we auto-return 1.0x (no interaction).
    (void)context;
    return 1.0f;
}

} // namespace battle::ability
