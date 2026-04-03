#ifndef COMBAT_FEEDBACK_H
#define COMBAT_FEEDBACK_H

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace battle {

enum class CombatJudgement {
    Perfect,
    Good,
    Okay,
    Flop
};

enum class CombatFeedbackEffectType {
    Damage,
    Reduction,
    Heal,
    Shield
};

/**
 * Representation of a feedback signal used to derive combat judgement and presentation.
 *
 * Encapsulates a presentation mode and a normalized score in [0.0, 1.0].
 */

/**
 * Create a signal that forces a perfect judgement.
 *
 * @returns A PresentationFeedbackSignal with mode ForcedPerfect and normalizedScore 1.0.
 */

/**
 * Create a graded signal clamping the provided score to [0.0, 1.0].
 *
 * @param score Raw score to clamp.
 * @returns A PresentationFeedbackSignal with mode Graded and the clamped normalizedScore.
 */

/**
 * Create a binary signal representing success or failure.
 *
 * @param success If true the normalizedScore will be 1.0, otherwise 0.0.
 * @returns A PresentationFeedbackSignal with mode Binary and normalizedScore corresponding to success.
 */

/**
 * Indicates whether the signal contains a meaningful mode.
 *
 * @returns `true` if mode is not PresentationFeedbackMode::None, `false` otherwise.
 */

/**
 * Aggregated presentation feedback for a combat event.
 *
 * Contains the feedback signal, a numeric multiplier for presentation effects, optional reward text,
 * whether the event is eligible to contribute to combos, and a count of correct tones.
 */

/**
 * Indicates whether the event contains a valid feedback signal.
 *
 * @returns `true` if the embedded signal is valid, `false` otherwise.
 */

/**
 * Derive a CombatJudgement from a PresentationFeedbackSignal.
 *
 * Uses mode and normalizedScore (clamped to [0,1]) with thresholds to select among Perfect, Good, Okay, and Flop.
 *
 * @param signal PresentationFeedbackSignal to classify.
 * @returns The corresponding CombatJudgement.
 */

/**
 * Human-readable uppercase label for a CombatJudgement suitable for display.
 *
 * @param judgement Judgement to label.
 * @returns One of "PERFECT", "GOOD", "OKAY", or "FLOP".
 */

/**
 * CSS/class-style name for a CombatJudgement in lowercase.
 *
 * @param judgement Judgement to convert.
 * @returns One of "perfect", "good", "okay", or "flop".
 */

/**
 * Short label for a CombatFeedbackEffectType used in compact UI displays.
 *
 * @param effectType Effect type to label.
 * @returns One of "DMG", "RED", "HEAL", or "SHIELD".
 */

/**
 * Indicates whether a judgement normally increments the player's combo.
 *
 * @param judgement Judgement to evaluate.
 * @returns `true` for Perfect, Good, or Okay; `false` for Flop.
 */

/**
 * Compute the fractional damage bonus contributed by a combo count.
 *
 * The function returns 0.0 for non-positive comboCount and a log-scaled fraction for positive counts.
 *
 * @param comboCount Current combo count (values <= 0 produce 0.0).
 * @returns Fractional damage bonus (e.g., 0.05 represents +5% damage).
 */

/**
 * Invert the comboDamageBonusFraction mapping to estimate the combo count that would produce a given bonus.
 *
 * @param damageBonusFraction Fractional damage bonus (values <= 0.0 produce 0.0).
 * @returns Estimated combo count that yields the provided damage bonus fraction.
 */

/**
 * Compute the visual fill ratio of the combo meter.
 *
 * The result is clamped to [0.0, 1.0] where 1.0 represents a visually full combo at a predefined threshold.
 *
 * @param comboCount Current combo count (negative values treated as 0).
 * @returns Fill ratio in [0.0, 1.0].
 */

/**
 * Compute the overall damage multiplier contributed by combo.
 *
 * @param comboCount Current combo count.
 * @returns Damage multiplier equal to 1.0 plus the combo damage bonus fraction.
 */

/**
 * Compute the per-ally heal amount awarded when a combo breaks.
 *
 * The value is derived from comboCount, floored and clamped to the integer range [0,4].
 *
 * @param comboCount Current combo count (negative values treated as 0).
 * @returns Integer heal-per-ally in [0,4].
 */
enum class PresentationFeedbackMode {
    None,
    ForcedPerfect,
    Graded,
    Binary
};

struct PresentationFeedbackSignal {
    PresentationFeedbackMode mode = PresentationFeedbackMode::None;
    float normalizedScore = 1.0f;

    static PresentationFeedbackSignal forcedPerfect() {
        return PresentationFeedbackSignal{PresentationFeedbackMode::ForcedPerfect, 1.0f};
    }

    static PresentationFeedbackSignal graded(float score) {
        return PresentationFeedbackSignal{
            PresentationFeedbackMode::Graded,
            std::clamp(score, 0.0f, 1.0f)
        };
    }

    static PresentationFeedbackSignal binary(bool success) {
        return PresentationFeedbackSignal{
            PresentationFeedbackMode::Binary,
            success ? 1.0f : 0.0f
        };
    }

    bool valid() const {
        return mode != PresentationFeedbackMode::None;
    }
};

struct PresentationFeedbackEvent {
    PresentationFeedbackSignal signal{};
    float multiplier = 1.0f;
    std::string rewardText;
    bool comboEligible = true;
    int correctToneCount = 0;

    bool valid() const {
        return signal.valid();
    }
};

inline CombatJudgement classifyCombatJudgement(const PresentationFeedbackSignal& signal) {
    if (signal.mode == PresentationFeedbackMode::ForcedPerfect) {
        return CombatJudgement::Perfect;
    }

    if (signal.mode == PresentationFeedbackMode::Binary) {
        return signal.normalizedScore >= 0.5f ? CombatJudgement::Perfect : CombatJudgement::Flop;
    }

    const float score = std::clamp(signal.normalizedScore, 0.0f, 1.0f);
    if (score >= 0.85f) {
        return CombatJudgement::Perfect;
    }
    if (score >= 0.60f) {
        return CombatJudgement::Good;
    }
    if (score >= 0.25f) {
        return CombatJudgement::Okay;
    }
    return CombatJudgement::Flop;
}

inline const char* combatJudgementLabel(CombatJudgement judgement) {
    switch (judgement) {
        case CombatJudgement::Perfect:
            return "PERFECT";
        case CombatJudgement::Good:
            return "GOOD";
        case CombatJudgement::Okay:
            return "OKAY";
        case CombatJudgement::Flop:
        default:
            return "FLOP";
    }
}

inline const char* combatJudgementClassName(CombatJudgement judgement) {
    switch (judgement) {
        case CombatJudgement::Perfect:
            return "perfect";
        case CombatJudgement::Good:
            return "good";
        case CombatJudgement::Okay:
            return "okay";
        case CombatJudgement::Flop:
        default:
            return "flop";
    }
}

inline const char* combatFeedbackEffectLabel(CombatFeedbackEffectType effectType) {
    switch (effectType) {
        case CombatFeedbackEffectType::Reduction:
            return "RED";
        case CombatFeedbackEffectType::Heal:
            return "HEAL";
        case CombatFeedbackEffectType::Shield:
            return "SHIELD";
        case CombatFeedbackEffectType::Damage:
        default:
            return "DMG";
    }
}

inline bool judgementNaturallyIncreasesCombo(CombatJudgement judgement) {
    return judgement == CombatJudgement::Perfect ||
           judgement == CombatJudgement::Good ||
           judgement == CombatJudgement::Okay;
}

inline float comboDamageBonusFraction(int comboCount) {
    constexpr float kComboBonusScale = 0.076f;
    constexpr float kComboBonusCurve = 35.0f;
    const float combo = static_cast<float>(std::max(0, comboCount));
    if (combo <= 0.0f) {
        return 0.0f;
    }
    return kComboBonusScale * std::log1p(combo / kComboBonusCurve);
}

inline float comboCountForDamageBonusFraction(float damageBonusFraction) {
    constexpr float kComboBonusScale = 0.076f;
    constexpr float kComboBonusCurve = 35.0f;
    if (damageBonusFraction <= 0.0f) {
        return 0.0f;
    }
    return kComboBonusCurve * std::expm1(damageBonusFraction / kComboBonusScale);
}

inline float comboMeterFillRatio(int comboCount) {
    constexpr float kVisualFullComboCount = 200.0f;
    return std::clamp(static_cast<float>(std::max(0, comboCount)) / kVisualFullComboCount, 0.0f, 1.0f);
}

inline float comboDamageMultiplier(int comboCount) {
    return 1.0f + comboDamageBonusFraction(comboCount);
}

inline int comboBreakHealPerAlly(int comboCount) {
    const float raw = (static_cast<float>(std::max(0, comboCount)) / 4.0f) * 0.2f;
    return std::clamp(static_cast<int>(std::floor(raw)), 0, 4);
}

} // namespace battle

#endif // COMBAT_FEEDBACK_H
