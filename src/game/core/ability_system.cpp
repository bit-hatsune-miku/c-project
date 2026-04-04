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

void applyBossOffenseResult(const AbilityExecutionContext& context,
                            int amount,
                            int& bossCurrentHp) {
    if (amount <= 0) {
        return;
    }

    if (context.playerDamageHealsBoss) {
        bossCurrentHp = std::min(context.bossMaxHp, bossCurrentHp + amount);
        return;
    }

    bossCurrentHp = std::max(0, bossCurrentHp - amount);
}

int resolveLegacyShieldAmount(const AbilityDefinition& ability,
                              const AbilityExecutionContext& context,
                              const std::vector<BattleCharacter>& characters) {
    if (ability.baseShield > 0) {
        return ability.baseShield;
    }

    if (context.casterPartyIndex >= 0 &&
        context.casterPartyIndex < static_cast<int>(characters.size())) {
        return characters[static_cast<size_t>(context.casterPartyIndex)].definition().baseShield;
    }

    return 0;
}

int resolveCasterMaxHp(const AbilityExecutionContext& context,
                      const std::vector<BattleCharacter>& characters) {
    if (context.isBossCaster) {
        return std::max(1, context.bossMaxHp);
    }

    if (context.casterPartyIndex < 0 ||
        context.casterPartyIndex >= static_cast<int>(characters.size())) {
        return 1;
    }

    return std::max(1, characters[static_cast<size_t>(context.casterPartyIndex)].maxHp());
}

} // namespace

bool isTeamShieldBurstUltimate(const AbilityDefinition& ability) {
    return ability.id == "8888" || ability.presentationId == "wechatalipay_ultimate";
}

int resolveSupportAmount(const AbilityDefinition& ability,
                         int casterMaxHp,
                         float presentationMultiplier,
                         int legacyFallbackAmount) {
    const float clampedMultiplier = std::max(0.0f, presentationMultiplier);
    float baseAmount = static_cast<float>(std::max(0, legacyFallbackAmount));

    if (ability.amountPercentOfCasterMaxHp.has_value()) {
        baseAmount = (static_cast<float>(std::max(1, casterMaxHp)) *
                      std::max(0.0f, *ability.amountPercentOfCasterMaxHp)) /
                     100.0f;
    }

    if (baseAmount <= 0.0f) {
        return 0;
    }

    return std::max(1, static_cast<int>(std::lround(baseAmount * clampedMultiplier)));
}

/**
 * @brief Execute an ability's effect, mutating boss HP and party characters as appropriate.
 *
 * Applies the provided ability from `context` to update `bossCurrentHp` and to modify
 * `characters` (healing, reviving, or adding shields). Behavior varies by ability type:
 * - Attack / Debuff: computes final damage from `context.baseDamage`, `ability.multiplier`,
 *   `context.presentationMultiplier`, and `context.comboMultiplier`, enforces a minimum of 1 when
 *   nonzero, and applies it to `bossCurrentHp` (damage may instead heal the boss depending on
 *   `context.playerDamageHealsBoss`).
 * - Heal: resolves a base amount from the caster's max HP when configured, otherwise falls back
 *   to legacy flat healing, then scales by `context.presentationMultiplier`, rounds and enforces
 *   a minimum of 1 when nonzero; if the ability targets all allies, applies healing to alive
 *   characters and revives dead characters when `ability.reviveDeadAllies` is true.
 * - Shield: resolves a base amount from the caster's max HP when configured, otherwise falls back
 *   to legacy shield values, then scales by `context.presentationMultiplier` and adds that shield
 *   to targets according to the ability's
 *   target rule.
 * - Buff: no effect.
 *
 * @param context Execution parameters and ability definition used to compute effects.
 * @param[out] bossCurrentHp Reference to the boss's current HP; may be increased or decreased.
 * @param[in,out] characters Vector of party characters that may be healed, revived, or granted shields.
 */

void executeAbilityEffect(const AbilityExecutionContext& context,
                          int& bossCurrentHp,
                          std::vector<BattleCharacter>& characters) {
    if (context.ability == nullptr) {
        return;
    }

    const AbilityDefinition& ability = *context.ability;
    const int casterMaxHp = resolveCasterMaxHp(context, characters);
    switch (ability.type) {
        case AbilityType::Attack: {
            const int finalDamage = normalizeDamage(static_cast<int>(
                context.baseDamage *
                ability.multiplier *
                context.presentationMultiplier *
                context.comboMultiplier
            ));
            applyBossOffenseResult(context, finalDamage, bossCurrentHp);
            break;
        }

        case AbilityType::Heal: {
            const int legacyBaseHeal = std::max(0, context.baseHeal > 0 ? context.baseHeal : ability.flatHeal);
            const int healAmount = resolveSupportAmount(
                ability,
                casterMaxHp,
                context.presentationMultiplier,
                legacyBaseHeal
            );
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


        case AbilityType::Shield: {
            const int legacyBaseShield = resolveLegacyShieldAmount(ability, context, characters);
            const int shieldAmount = resolveSupportAmount(
                ability,
                casterMaxHp,
                context.presentationMultiplier,
                legacyBaseShield
            );
            if (ability.targetRule == TargetRule::AllAllies) {
                for (BattleCharacter& c : characters) {
                    if (c.isAlive()) {
                        c.addShield(shieldAmount);
                    }
                }
            } else if (ability.targetRule == TargetRule::SingleAlly && context.casterPartyIndex >= 0 && context.casterPartyIndex < (int)characters.size()) {
                characters[context.casterPartyIndex].addShield(shieldAmount);
            }
            break;
        }

        case AbilityType::Buff:
            break;

        case AbilityType::Debuff:
            if (ability.multiplier > 0.0f) {
                const int finalDamage = normalizeDamage(static_cast<int>(
                    context.baseDamage *
                    ability.multiplier *
                    context.presentationMultiplier *
                    context.comboMultiplier
                ));
                applyBossOffenseResult(context, finalDamage, bossCurrentHp);
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
