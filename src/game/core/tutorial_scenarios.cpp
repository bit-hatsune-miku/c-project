#include "tutorial_scenarios.h"

#include <algorithm>
#include <iostream>

#include <nlohmann/json.hpp>

#include "battle_loader.h"

namespace battle::tutorial {
namespace {

using json = nlohmann::json;

constexpr char kScenarioPath[] = "assets/combat/tutorial_scenarios.json";

TutorialRequirementKind parseTutorialRequirementKind(const std::string& value) {
    if (value == "observe_only") {
        return TutorialRequirementKind::ObserveOnly;
    }
    if (value == "feedback_event_count") {
        return TutorialRequirementKind::FeedbackEventCount;
    }
    if (value == "judgement_count") {
        return TutorialRequirementKind::JudgementCount;
    }
    if (value == "correct_tone_count") {
        return TutorialRequirementKind::CorrectToneCount;
    }
    if (value == "score_value") {
        return TutorialRequirementKind::ScoreValue;
    }
    if (value == "final_judgement") {
        return TutorialRequirementKind::FinalJudgement;
    }
    return TutorialRequirementKind::None;
}

CombatJudgement parseCombatJudgementFloor(const std::string& value) {
    if (value == "perfect") {
        return CombatJudgement::Perfect;
    }
    if (value == "good") {
        return CombatJudgement::Good;
    }
    if (value == "okay" || value == "ok") {
        return CombatJudgement::Okay;
    }
    return CombatJudgement::Flop;
}

std::optional<TutorialScenarioActionKind> parseActionKind(const json& stepJson) {
    if (!stepJson.is_object()) {
        return std::nullopt;
    }

    const std::string value = stepJson.value("action", "");
    if (value == "character_skill") {
        return TutorialScenarioActionKind::CharacterSkill;
    }
    if (value == "character_ultimate") {
        return TutorialScenarioActionKind::CharacterUltimate;
    }
    if (value == "boss_skill") {
        return TutorialScenarioActionKind::BossSkill;
    }
    return std::nullopt;
}

bool parseTutorialRequirement(const json& requirementJson, AbilityTutorialDefinition& outRequirement) {
    if (!requirementJson.is_object()) {
        return false;
    }

    outRequirement = AbilityTutorialDefinition{};
    outRequirement.requirementKind = parseTutorialRequirementKind(
        requirementJson.value("requirement", "")
    );
    outRequirement.minimumJudgement = parseCombatJudgementFloor(
        requirementJson.value("minimumJudgement", "okay")
    );
    outRequirement.minimumValue = std::max(0, requirementJson.value("minimumValue", 0));

    if (const auto presentationValueIt = requirementJson.find("presentationValue");
        presentationValueIt != requirementJson.end() && presentationValueIt->is_number_integer()) {
        outRequirement.presentationValue = presentationValueIt->get<int>();
    }

    if (const auto rollsIt = requirementJson.find("resolvedRolls");
        rollsIt != requirementJson.end() && rollsIt->is_array()) {
        for (const json& rollValue : *rollsIt) {
            if (rollValue.is_number_integer()) {
                outRequirement.resolvedRolls.push_back(rollValue.get<int>());
            }
        }
    }

    if (const auto targetsIt = requirementJson.find("resolvedTargetIndices");
        targetsIt != requirementJson.end() && targetsIt->is_array()) {
        for (const json& targetValue : *targetsIt) {
            if (targetValue.is_number_integer()) {
                outRequirement.resolvedTargetIndices.push_back(targetValue.get<int>());
            }
        }
    }

    return true;
}

bool parsePartyMemberState(const json& memberJson, TutorialScenarioPartyMemberState& outMember) {
    if (!memberJson.is_object()) {
        return false;
    }

    outMember = TutorialScenarioPartyMemberState{};
    outMember.characterKey = memberJson.value("characterKey", "");
    outMember.abilityKitId = memberJson.value("abilityKitId", "");

    if (const auto hpIt = memberJson.find("hp"); hpIt != memberJson.end() && hpIt->is_number_integer()) {
        outMember.hp = hpIt->get<int>();
    }
    if (const auto hpPercentIt = memberJson.find("hpPercent");
        hpPercentIt != memberJson.end() && hpPercentIt->is_number_integer()) {
        outMember.hpPercent = hpPercentIt->get<int>();
    }
    if (const auto aliveIt = memberJson.find("alive"); aliveIt != memberJson.end() && aliveIt->is_boolean()) {
        outMember.alive = aliveIt->get<bool>();
    }
    if (const auto shieldIt = memberJson.find("shield");
        shieldIt != memberJson.end() && shieldIt->is_number_integer()) {
        outMember.shield = shieldIt->get<int>();
    }
    if (const auto ultimateIt = memberJson.find("ultimateCharge");
        ultimateIt != memberJson.end() && ultimateIt->is_number_integer()) {
        outMember.ultimateCharge = ultimateIt->get<int>();
    }
    if (const auto speedIt = memberJson.find("speedBuff");
        speedIt != memberJson.end() && speedIt->is_number_integer()) {
        outMember.speedBuff = speedIt->get<int>();
    }
    if (const auto atkIt = memberJson.find("atkBuff");
        atkIt != memberJson.end() && atkIt->is_number_integer()) {
        outMember.atkBuff = atkIt->get<int>();
    }
    if (const auto damageIt = memberJson.find("damageBuff");
        damageIt != memberJson.end() && damageIt->is_number_integer()) {
        outMember.damageBuff = damageIt->get<int>();
    }

    return !outMember.characterKey.empty();
}

bool parseBattleStatePatch(const json& patchJson, TutorialScenarioBattleStatePatch& outPatch) {
    outPatch = TutorialScenarioBattleStatePatch{};
    if (!patchJson.is_object()) {
        return true;
    }

    if (const auto partyIt = patchJson.find("party"); partyIt != patchJson.end() && partyIt->is_array()) {
        for (const json& memberJson : *partyIt) {
            TutorialScenarioPartyMemberState member;
            if (parsePartyMemberState(memberJson, member)) {
                outPatch.party.push_back(std::move(member));
            }
        }
    }

    if (const auto bossHpIt = patchJson.find("bossHp");
        bossHpIt != patchJson.end() && bossHpIt->is_number_integer()) {
        outPatch.bossHp = bossHpIt->get<int>();
    }
    if (const auto bossHpPercentIt = patchJson.find("bossHpPercent");
        bossHpPercentIt != patchJson.end() && bossHpPercentIt->is_number_integer()) {
        outPatch.bossHpPercent = bossHpPercentIt->get<int>();
    }
    if (const auto bossPhaseIt = patchJson.find("bossPhaseIndex");
        bossPhaseIt != patchJson.end() && bossPhaseIt->is_number_integer()) {
        outPatch.bossPhaseIndex = bossPhaseIt->get<int>();
    }
    if (const auto tonesIt = patchJson.find("luotianyiCorrectTones");
        tonesIt != patchJson.end() && tonesIt->is_number_integer()) {
        outPatch.luotianyiCorrectTones = tonesIt->get<int>();
    }
    if (const auto tallyIt = patchJson.find("tetoHealingTally");
        tallyIt != patchJson.end() && tallyIt->is_number_integer()) {
        outPatch.tetoHealingTally = tallyIt->get<int>();
    }
    if (const auto transformedIt = patchJson.find("sailorVenusTransformed");
        transformedIt != patchJson.end() && transformedIt->is_boolean()) {
        outPatch.sailorVenusTransformed = transformedIt->get<bool>();
    }
    if (const auto turnsIt = patchJson.find("sailorVenusTurnsRemaining");
        turnsIt != patchJson.end() && turnsIt->is_number_integer()) {
        outPatch.sailorVenusTurnsRemaining = turnsIt->get<int>();
    }
    if (const auto spaceIt = patchJson.find("sailorVenusSpaceTally");
        spaceIt != patchJson.end() && spaceIt->is_number_integer()) {
        outPatch.sailorVenusSpaceTally = spaceIt->get<int>();
    }
    if (const auto finisherIt = patchJson.find("sailorVenusFinisherQueued");
        finisherIt != patchJson.end() && finisherIt->is_boolean()) {
        outPatch.sailorVenusFinisherQueued = finisherIt->get<bool>();
    }

    return true;
}

bool parseScenarioStep(const json& stepJson, TutorialScenarioStep& outStep) {
    if (!stepJson.is_object()) {
        return false;
    }

    outStep = TutorialScenarioStep{};
    outStep.promptKey = stepJson.value("promptKey", "");
    outStep.actionKind = parseActionKind(stepJson);
    outStep.actorKey = stepJson.value("actorKey", "");
    outStep.failurePromptKey = stepJson.value("failurePromptKey", "");
    outStep.abilityKitId = stepJson.value("abilityKitId", "");
    outStep.targetCharacterKey = stepJson.value("targetCharacterKey", "");
    outStep.suppressBossWarning = stepJson.value("suppressBossWarning", false);

    if (const auto requirementIt = stepJson.find("requirement");
        requirementIt != stepJson.end() && requirementIt->is_object()) {
        AbilityTutorialDefinition requirement;
        if (parseTutorialRequirement(*requirementIt, requirement)) {
            outStep.tutorialOverride = requirement;
        }
    }
    if (const auto presentationValueIt = stepJson.find("presentationValue");
        presentationValueIt != stepJson.end() && presentationValueIt->is_number_integer()) {
        outStep.presentationValue = presentationValueIt->get<int>();
    }
    if (const auto rollsIt = stepJson.find("resolvedRolls");
        rollsIt != stepJson.end() && rollsIt->is_array()) {
        for (const json& rollValue : *rollsIt) {
            if (rollValue.is_number_integer()) {
                outStep.resolvedRolls.push_back(rollValue.get<int>());
            }
        }
    }
    if (const auto targetsIt = stepJson.find("resolvedTargetIndices");
        targetsIt != stepJson.end() && targetsIt->is_array()) {
        for (const json& targetValue : *targetsIt) {
            if (targetValue.is_number_integer()) {
                outStep.resolvedTargetIndices.push_back(targetValue.get<int>());
            }
        }
    }
    if (const auto bossPhaseIt = stepJson.find("bossPhaseIndex");
        bossPhaseIt != stepJson.end() && bossPhaseIt->is_number_integer()) {
        outStep.bossPhaseIndex = bossPhaseIt->get<int>();
    }
    if (const auto preStateIt = stepJson.find("preState"); preStateIt != stepJson.end()) {
        (void)parseBattleStatePatch(*preStateIt, outStep.preState);
    }
    if (const auto postStateIt = stepJson.find("postState"); postStateIt != stepJson.end()) {
        (void)parseBattleStatePatch(*postStateIt, outStep.postState);
    }

    return !outStep.promptKey.empty() || outStep.actionKind.has_value();
}

const json* scenarioGroupForType(const json& root, TutorialScenarioType type) {
    const char* key = type == TutorialScenarioType::UnlockDrill ? "unlockDrills" : "bossPreviews";
    const auto it = root.find(key);
    if (it == root.end() || !it->is_object()) {
        return nullptr;
    }
    return &(*it);
}

bool loadScenarioRoot(json& outRoot) {
    return loader::readJsonRoot(loader::resolveAssetPath(kScenarioPath), outRoot, "tutorial scenarios");
}

bool parseScenario(const json& scenarioJson,
                   TutorialScenarioType type,
                   const std::string& key,
                   TutorialScenarioDefinition& outScenario) {
    if (!scenarioJson.is_object()) {
        return false;
    }

    TutorialScenarioDefinition scenario;
    scenario.type = type;
    scenario.key = key;
    scenario.enabled = scenarioJson.value("enabled", true);
    scenario.stageKey = scenarioJson.value("stageKey", "");

    if (const auto lineupIt = scenarioJson.find("lineup");
        lineupIt != scenarioJson.end() && lineupIt->is_array()) {
        for (const json& lineupEntry : *lineupIt) {
            if (lineupEntry.is_string()) {
                scenario.lineup.push_back(lineupEntry.get<std::string>());
            }
        }
    }

    if (const auto initialStateIt = scenarioJson.find("initialState"); initialStateIt != scenarioJson.end()) {
        (void)parseBattleStatePatch(*initialStateIt, scenario.initialState);
    }

    if (const auto stepsIt = scenarioJson.find("steps"); stepsIt != scenarioJson.end() && stepsIt->is_array()) {
        for (const json& stepJson : *stepsIt) {
            TutorialScenarioStep step;
            if (parseScenarioStep(stepJson, step)) {
                scenario.steps.push_back(std::move(step));
            }
        }
    }

    outScenario = std::move(scenario);
    return true;
}

} // namespace

bool loadScenario(TutorialScenarioType type,
                  const std::string& key,
                  TutorialScenarioDefinition& outScenario) {
    outScenario = TutorialScenarioDefinition{};

    json root;
    if (!loadScenarioRoot(root)) {
        return false;
    }

    const json* group = scenarioGroupForType(root, type);
    if (group == nullptr) {
        return false;
    }

    const auto it = group->find(key);
    if (it == group->end()) {
        return false;
    }

    if (!parseScenario(it.value(), type, key, outScenario)) {
        std::cerr << "[Tutorial] Invalid scenario entry: " << key << "\n";
        return false;
    }

    return true;
}

bool loadUnlockDrillScenario(const std::string& characterKey,
                             TutorialScenarioDefinition& outScenario) {
    return loadScenario(TutorialScenarioType::UnlockDrill, characterKey, outScenario);
}

bool loadBossPreviewScenario(const std::string& battleKey,
                             TutorialScenarioDefinition& outScenario) {
    return loadScenario(TutorialScenarioType::BossPreview, battleKey, outScenario);
}

bool hasEnabledBossPreviewScenario(const std::string& battleKey) {
    TutorialScenarioDefinition scenario;
    return loadBossPreviewScenario(battleKey, scenario) && scenario.enabled;
}

} // namespace battle::tutorial
