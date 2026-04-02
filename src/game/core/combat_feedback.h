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
