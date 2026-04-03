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

} /**
 * @brief Execute an ability's effect, mutating boss HP and party characters as appropriate.
 *
 * Applies the provided ability from `context` to update `bossCurrentHp` and to modify
 * `characters` (healing, reviving, or adding shields). Behavior varies by ability type:
 * - Special-case ultimate (ability id "8888" or presentationId "wechatalipay_ultimate"): subtracts
 *   the sum of all alive characters' shield values from `bossCurrentHp` without consuming shields.
 * - Attack / Debuff: computes final damage from `context.baseDamage`, `ability.multiplier`,
 *   `context.presentationMultiplier`, and `context.comboMultiplier`, enforces a minimum of 1 when
 *   nonzero, and applies it to `bossCurrentHp` (damage may instead heal the boss depending on
 *   `context.playerDamageHealsBoss`).
 * - Heal: determines a base heal from `context.baseHeal` or `ability.flatHeal`, scales by
 *   `context.presentationMultiplier`, rounds and enforces a minimum of 1 when nonzero; if the
 *   ability targets all allies, applies healing to alive characters and revives dead characters
 *   when `ability.reviveDeadAllies` is true.
 * - Shield: computes a shield amount from the caster's base shield scaled by
 *   `context.presentationMultiplier` and adds that shield to targets according to the ability's
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

    // Special case: Wechatalipay's ultimate (id: 8888, presentationId: wechatalipay_ultimate)
    if (ability.id == "8888" || ability.presentationId == "wechatalipay_ultimate") {
        // Sum all team shield values
        int totalShield = 0;
        for (BattleCharacter& c : characters) {
            if (c.isAlive()) {
                totalShield += c.getShield();
            }
        }
        if (totalShield > 0) {
            bossCurrentHp = std::max(0, bossCurrentHp - totalShield);
            // Shields are NOT consumed by the ultimate
        }
        return;
    }
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


        case AbilityType::Shield: {
            // Shielding ability (e.g., Wechatalipay's SaoMaZhiFu)
            int baseShield = 0;
            if (context.casterPartyIndex >= 0 && context.casterPartyIndex < (int)characters.size()) {
                baseShield = characters[context.casterPartyIndex].definition().baseShield;
            }
            int shieldAmount = static_cast<int>(std::round(baseShield * context.presentationMultiplier));
            std::cout << "[ShieldLogic] Entered: baseShield=" << baseShield << ", presentationMultiplier=" << context.presentationMultiplier << ", shieldAmount=" << shieldAmount << ", targetRule=" << static_cast<int>(ability.targetRule) << std::endl;
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
