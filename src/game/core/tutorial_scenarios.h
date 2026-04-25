#ifndef BATTLE_TUTORIAL_SCENARIOS_H
#define BATTLE_TUTORIAL_SCENARIOS_H

#include <optional>
#include <string>
#include <vector>

#include "battle_manager.h"

namespace battle {

enum class TutorialScenarioType {
    UnlockDrill,
    BossPreview
};

enum class TutorialScenarioActionKind {
    CharacterSkill,
    CharacterUltimate,
    BossSkill
};

struct TutorialScenarioPartyMemberState {
    std::string characterKey;
    std::optional<int> hp;
    std::optional<int> hpPercent;
    std::optional<bool> alive;
    std::optional<int> shield;
    std::optional<int> ultimateCharge;
    std::optional<int> speedBuff;
    std::optional<int> atkBuff;
    std::optional<int> damageBuff;
    std::string abilityKitId;
};

struct TutorialScenarioBattleStatePatch {
    std::vector<TutorialScenarioPartyMemberState> party;
    std::optional<int> bossHp;
    std::optional<int> bossHpPercent;
    std::optional<int> bossPhaseIndex;
    std::optional<int> luotianyiCorrectTones;
    std::optional<int> tetoHealingTally;
    std::optional<bool> sailorVenusTransformed;
    std::optional<int> sailorVenusTurnsRemaining;
    std::optional<int> sailorVenusSpaceTally;
    std::optional<bool> sailorVenusFinisherQueued;
};

struct TutorialScenarioStep {
    std::string promptKey;
    std::optional<TutorialScenarioActionKind> actionKind;
    std::string actorKey;
    std::string failurePromptKey;
    std::string abilityKitId;
    std::string targetCharacterKey;
    std::optional<AbilityTutorialDefinition> tutorialOverride;
    std::optional<int> presentationValue;
    std::vector<int> resolvedRolls;
    std::vector<int> resolvedTargetIndices;
    std::optional<int> bossPhaseIndex;
    bool suppressBossWarning = false;
    TutorialScenarioBattleStatePatch preState;
    TutorialScenarioBattleStatePatch postState;
};

struct TutorialScenarioDefinition {
    TutorialScenarioType type = TutorialScenarioType::UnlockDrill;
    std::string key;
    bool enabled = true;
    std::string stageKey;
    std::vector<std::string> lineup;
    TutorialScenarioBattleStatePatch initialState;
    std::vector<TutorialScenarioStep> steps;
};

namespace tutorial {

bool loadScenario(TutorialScenarioType type,
                  const std::string& key,
                  TutorialScenarioDefinition& outScenario);
bool loadUnlockDrillScenario(const std::string& characterKey,
                             TutorialScenarioDefinition& outScenario);
bool loadBossPreviewScenario(const std::string& battleKey,
                             TutorialScenarioDefinition& outScenario);
bool hasEnabledBossPreviewScenario(const std::string& battleKey);

} // namespace tutorial

} // namespace battle

#endif // BATTLE_TUTORIAL_SCENARIOS_H
