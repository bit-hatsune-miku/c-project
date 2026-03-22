#include "ability_system.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace battle::ability {
namespace {

PresentationInteractionRunner gPresentationInteractionRunner;

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
            const int baseHeal = std::max(0, context.baseHeal > 0 ? context.baseHeal : ability.flatHeal);
            const int healAmount = baseHeal <= 0
                ? 0
                : std::max(1, static_cast<int>(std::lround(
                    static_cast<float>(baseHeal) * std::max(0.0f, context.presentationMultiplier)
                )));
            if (ability.targetRule == TargetRule::AllAllies) {
                for (BattleCharacter& c : characters) {
                    if (c.isAlive()) {
                        c.receiveHealing(healAmount);
                    } else if (ability.reviveDeadAllies && healAmount > 0) {
                        c.revive(healAmount);
                    }
                }
            }
            break;
        }

        case AbilityType::Buff:
            // TODO: Implement buff system later
            break;

        case AbilityType::Debuff:
            if (ability.multiplier > 0.0f) {
                const int finalDamage = normalizeDamage(static_cast<int>(
                    context.baseDamage * ability.multiplier * context.presentationMultiplier
                ));
                bossCurrentHp = std::max(0, bossCurrentHp - finalDamage);
            }
            break;
    }
}

float runPresentationInteraction(const PresentationContext& context) {
    if (gPresentationInteractionRunner) {
        return gPresentationInteractionRunner(context);
    }
    return 1.0f;
}

void setPresentationInteractionRunner(PresentationInteractionRunner runner) {
    gPresentationInteractionRunner = std::move(runner);
}

} // namespace battle::ability
