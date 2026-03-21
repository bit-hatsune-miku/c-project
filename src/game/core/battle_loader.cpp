#include "battle_loader.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace battle::loader {
namespace {

bool readJsonRoot(const std::string& path, json& outRoot, const char* label) {
    std::string contents;
    if (!readJsonFile(path, contents)) {
        if (label != nullptr) {
            std::cerr << "[Battle] Could not read " << label << " JSON: " << path << "\n";
        }
        return false;
    }

    try {
        outRoot = json::parse(contents);
        return true;
    } catch (const json::exception& e) {
        if (label != nullptr) {
            std::cerr << "[Battle] Failed parsing " << label << " JSON: " << e.what() << "\n";
        }
        return false;
    }
}

std::string getUnitAbilityReferenceId(const json& unitJson, const char* slotName) {
    if (!unitJson.is_object() || slotName == nullptr) {
        return std::string();
    }

    const std::string slot(slotName);
    if (slot == "skill") {
        return unitJson.value("skillAbility", unitJson.value("ability", ""));
    }
    if (slot == "ultimate") {
        return unitJson.value("ultimate", "");
    }
    if (slot == "standard") {
        return unitJson.value("standardAbility", "");
    }

    return std::string();
}

bool parseNestedUnitAbilities(const json& unitJson,
                              const std::string& unitKey,
                              std::unordered_map<std::string, AbilityDefinition>& outAbilities) {
    if (!unitJson.is_object() || !unitJson.contains("abilities")) {
        return true;
    }

    const json& abilitiesJson = unitJson.at("abilities");
    if (!abilitiesJson.is_object()) {
        std::cerr << "[Battle] Invalid nested abilities for unit: " << unitKey << "\n";
        return false;
    }

    for (auto it = abilitiesJson.begin(); it != abilitiesJson.end(); ++it) {
        if (!it.value().is_object()) {
            std::cerr << "[Battle] Invalid nested ability entry '" << it.key()
                      << "' for unit: " << unitKey << "\n";
            return false;
        }

        const std::string fallbackId = getUnitAbilityReferenceId(unitJson, it.key().c_str());
        const std::string abilityId = it.value().value("id", fallbackId);
        if (abilityId.empty()) {
            std::cerr << "[Battle] Nested ability '" << it.key()
                      << "' for unit '" << unitKey << "' is missing an id reference\n";
            return false;
        }

        AbilityDefinition def;
        if (!parseAbilityDefinition(it.value(), abilityId, def)) {
            std::cerr << "[Battle] Failed parsing nested ability '" << abilityId
                      << "' for unit: " << unitKey << "\n";
            return false;
        }

        outAbilities[abilityId] = std::move(def);
    }

    return true;
}

bool loadNestedAbilityDefinitionsFromFile(const std::string& relativePath,
                                          const char* label,
                                          std::unordered_map<std::string, AbilityDefinition>& outAbilities) {
    json root;
    if (!readJsonRoot(resolveAssetPath(relativePath), root, label)) {
        return false;
    }
    if (!root.is_object()) {
        std::cerr << "[Battle] Invalid " << label << " JSON root\n";
        return false;
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!parseNestedUnitAbilities(it.value(), it.key(), outAbilities)) {
            return false;
        }
    }

    return true;
}

bool parseBossDefinition(const json& bossJson, const std::string& key, BossDefinition& outBoss) {
    if (!bossJson.is_object()) {
        return false;
    }

    outBoss.key = key;
    outBoss.title = bossJson.value("title", key);
    outBoss.assets = bossJson.value("assets", "");
    outBoss.spd = bossJson.value("spd", 0);
    outBoss.atk = bossJson.value("atk", 0);
    outBoss.hp = bossJson.value("hp", 0);
    outBoss.skillAbility = bossJson.value("skillAbility", bossJson.value("ability", ""));
    outBoss.standardAbility = bossJson.value("standardAbility", "BossStandardAttack");
    outBoss.ultimate = bossJson.value("ultimate", outBoss.skillAbility);
    outBoss.ultimatePoints = bossJson.value("ultimatePoints", 6);
    outBoss.startingOrbs = bossJson.value("startingOrbs", 1);
    outBoss.bgm = bossJson.value("bgm", "");
    outBoss.bgmVolume = bossJson.value("bgmVolume", 1.0f);
    outBoss.ability = outBoss.skillAbility;
    return true;
}

bool parseCharacterDefinition(const json& characterJson, const std::string& key, CharacterDefinition& outCharacter) {
    if (!characterJson.is_object()) {
        return false;
    }

    outCharacter.key = key;
    outCharacter.title = characterJson.value("title", key);
    outCharacter.assets = characterJson.value("assets", "");
    outCharacter.characterClass = characterJson.value("class", "");
    outCharacter.spd = characterJson.value("spd", 0);
    outCharacter.atk = characterJson.value("atk", 0);
    outCharacter.hp = characterJson.value("hp", 0);
    outCharacter.standardAbility = characterJson.value("standardAbility", "BasicAttack");
    outCharacter.skillAbility = characterJson.value("skillAbility", characterJson.value("ability", ""));
    outCharacter.ability = outCharacter.skillAbility;
    outCharacter.ultimate = characterJson.value("ultimate", "");
    outCharacter.ultimatePoints = characterJson.value("ultimatePoints", 0);
    outCharacter.startingOrbs = characterJson.value("startingOrbs", 0);
    return true;
}

bool containsDuplicateNonEmptyKeys(const std::vector<std::string>& keys) {
    std::vector<std::string> seen;
    seen.reserve(keys.size());
    for (const std::string& key : keys) {
        if (key.empty()) {
            return true;
        }
        if (std::find(seen.begin(), seen.end(), key) != seen.end()) {
            return true;
        }
        seen.push_back(key);
    }
    return false;
}

bool isBattleDefinitionValid(const BattleDefinition& battle) {
    if (battle.key.empty() || battle.bossKey.empty()) {
        return false;
    }
    if (battle.partySize < 1 || battle.partySize > 4) {
        return false;
    }
    if (containsDuplicateNonEmptyKeys(battle.lockedLineup) ||
        containsDuplicateNonEmptyKeys(battle.lineup)) {
        return false;
    }
    if (static_cast<int>(battle.lockedLineup.size()) > battle.partySize) {
        return false;
    }

    std::vector<std::string> merged = battle.lockedLineup;
    for (const std::string& key : battle.lineup) {
        if (std::find(merged.begin(), merged.end(), key) == merged.end()) {
            merged.push_back(key);
        }
    }
    if (battle.isLineupFixed && static_cast<int>(merged.size()) != battle.partySize) {
        return false;
    }

    return true;
}

} // namespace

bool readJsonFile(const std::string& path, std::string& outContents) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    outContents.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

std::string resolveAssetPath(const std::string& relativePath) {
    const std::vector<std::string> candidates = {
        relativePath,
        "../" + relativePath,
        "../../" + relativePath
    };

    for (const std::string& candidate : candidates) {
        std::ifstream test(candidate);
        if (test.good()) {
            return candidate;
        }
    }

    return relativePath;
}

bool loadBossDefinition(const std::string& bossKey, BossDefinition& outBoss) {
    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/boss.json"), root, "boss")) {
        return false;
    }
    if (!root.contains(bossKey)) {
        std::cerr << "[Battle] Boss key not found: " << bossKey << "\n";
        return false;
    }

    if (!parseBossDefinition(root.at(bossKey), bossKey, outBoss)) {
        std::cerr << "[Battle] Invalid boss entry for key: " << bossKey << "\n";
        return false;
    }

    return true;
}

bool loadCharacterDefinition(const std::string& characterKey, CharacterDefinition& outCharacter) {
    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/characters.json"), root, "character")) {
        return false;
    }
    if (!root.contains(characterKey)) {
        std::cerr << "[Battle] Character key not found: " << characterKey << "\n";
        return false;
    }

    if (!parseCharacterDefinition(root.at(characterKey), characterKey, outCharacter)) {
        std::cerr << "[Battle] Invalid character entry for key: " << characterKey << "\n";
        return false;
    }

    return true;
}

bool loadAllCharacterDefinitions(std::vector<CharacterDefinition>& outCharacters) {
    outCharacters.clear();

    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/characters.json"), root, "character")) {
        return false;
    }
    if (!root.is_object()) {
        std::cerr << "[Battle] Invalid character JSON root\n";
        return false;
    }

    outCharacters.reserve(root.size());
    for (auto it = root.begin(); it != root.end(); ++it) {
        CharacterDefinition character;
        if (!parseCharacterDefinition(it.value(), it.key(), character)) {
            std::cerr << "[Battle] Invalid character entry for key: " << it.key() << "\n";
            return false;
        }
        outCharacters.push_back(std::move(character));
    }

    std::sort(outCharacters.begin(), outCharacters.end(), [](const CharacterDefinition& lhs, const CharacterDefinition& rhs) {
        if (lhs.title != rhs.title) {
            return lhs.title < rhs.title;
        }
        return lhs.key < rhs.key;
    });

    return true;
}

bool loadBattleDefinition(const std::string& battleKey, BattleDefinition& outBattle) {
    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/battles.json"), root, "battle")) {
        return false;
    }

    const json* battlesJson = nullptr;
    if (root.is_array()) {
        battlesJson = &root;
    } else if (root.is_object() && root.contains("battles") && root.at("battles").is_array()) {
        battlesJson = &root.at("battles");
    }

    if (battlesJson == nullptr) {
        std::cerr << "[Battle] Invalid battles JSON root\n";
        return false;
    }

    for (const json& battleJson : *battlesJson) {
        if (!battleJson.is_object()) {
            continue;
        }

        const std::string parsedKey = battleJson.value("key", "");
        if (parsedKey != battleKey) {
            continue;
        }

        outBattle = BattleDefinition{};
        outBattle.key = parsedKey;
        outBattle.id = battleJson.value("id", -1);
        outBattle.name = battleJson.value("name", parsedKey);
        outBattle.description = battleJson.value("description", "");
        outBattle.bossKey = battleJson.value("bossKey", "");
        outBattle.isLineupFixed = battleJson.value("isLineupFixed", true);
        const int partySizeRaw = battleJson.value("partySize", 4);
        outBattle.partySize = partySizeRaw;
        outBattle.lineup = battleJson.value("lineup", std::vector<std::string>{});
        outBattle.lockedLineup = battleJson.value("lockedLineup", std::vector<std::string>{});

        if (!isBattleDefinitionValid(outBattle)) {
            std::cerr << "[Battle] Invalid battle definition: " << battleKey << "\n";
            return false;
        }
        outBattle.partySize = std::clamp(partySizeRaw, 1, 4);
        return true;
    }

    std::cerr << "[Battle] Battle key not found: " << battleKey << "\n";
    return false;
}

bool loadBattleDefinitionById(int battleId, BattleDefinition& outBattle) {
    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/battles.json"), root, "battle")) {
        return false;
    }

    const json* battlesJson = nullptr;
    if (root.is_array()) {
        battlesJson = &root;
    } else if (root.is_object() && root.contains("battles") && root.at("battles").is_array()) {
        battlesJson = &root.at("battles");
    }

    if (battlesJson == nullptr) {
        std::cerr << "[Battle] Invalid battles JSON root\n";
        return false;
    }

    for (const json& battleJson : *battlesJson) {
        if (!battleJson.is_object() || battleJson.value("id", -1) != battleId) {
            continue;
        }

        const std::string battleKey = battleJson.value("key", "");
        if (battleKey.empty()) {
            std::cerr << "[Battle] Battle id " << battleId << " is missing a key\n";
            return false;
        }
        return loadBattleDefinition(battleKey, outBattle);
    }

    std::cerr << "[Battle] Battle id not found: " << battleId << "\n";
    return false;
}

bool loadAbilityDefinition(const std::string& abilityId, AbilityDefinition& outAbility) {
    std::unordered_map<std::string, AbilityDefinition> abilities;
    if (!loadAllAbilities(abilities)) {
        return false;
    }

    const auto it = abilities.find(abilityId);
    if (it == abilities.end()) {
        return false;
    }

    outAbility = it->second;
    return true;
}

bool parseAbilityDefinition(const json& abilityJson, const std::string& abilityId, AbilityDefinition& outAbility) {
    if (!abilityJson.is_object()) {
        return false;
    }

    outAbility = AbilityDefinition{};
    outAbility.id = abilityId;
    outAbility.name = abilityJson.value("name", abilityId);
    outAbility.multiplier = abilityJson.value("multiplier", 1.0f);
    outAbility.flatHeal = abilityJson.value("flatHeal", 0);
    outAbility.reviveDeadAllies = abilityJson.value("reviveDeadAllies", false);
    outAbility.presentationId = abilityJson.value("presentationId", "");

    const std::string typeStr = abilityJson.value("type", "attack");
    if (typeStr == "heal") {
        outAbility.type = AbilityType::Heal;
    } else if (typeStr == "buff") {
        outAbility.type = AbilityType::Buff;
    } else if (typeStr == "debuff") {
        outAbility.type = AbilityType::Debuff;
    } else {
        outAbility.type = AbilityType::Attack;
    }

    const std::string targetStr = abilityJson.value("targetRule", "single_enemy");
    if (targetStr == "all_enemies") {
        outAbility.targetRule = TargetRule::AllEnemies;
    } else if (targetStr == "single_ally") {
        outAbility.targetRule = TargetRule::SingleAlly;
    } else if (targetStr == "all_allies") {
        outAbility.targetRule = TargetRule::AllAllies;
    } else if (targetStr == "self") {
        outAbility.targetRule = TargetRule::Self;
    } else {
        outAbility.targetRule = TargetRule::SingleEnemy;
    }

    const std::string interactionStr = abilityJson.value("interactionType", "none");
    if (interactionStr == "rhythm") {
        outAbility.interactionType = InteractionType::Rhythm;
    } else if (interactionStr == "parry") {
        outAbility.interactionType = InteractionType::Parry;
    } else {
        outAbility.interactionType = InteractionType::None;
    }

    return true;
}

bool loadAllAbilities(std::unordered_map<std::string, AbilityDefinition>& outAbilities) {
    outAbilities.clear();

    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/abilities.json"), root, "abilities")) {
        return false;
    }
    if (!root.is_object()) {
        std::cerr << "[Battle] Invalid abilities JSON root\n";
        return false;
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        AbilityDefinition def;
        if (!parseAbilityDefinition(it.value(), it.key(), def)) {
            continue;
        }
        outAbilities[it.key()] = def;
    }

    if (!loadNestedAbilityDefinitionsFromFile("assets/combat/characters.json", "character", outAbilities)) {
        return false;
    }
    if (!loadNestedAbilityDefinitionsFromFile("assets/combat/boss.json", "boss", outAbilities)) {
        return false;
    }

    std::cout << "[Battle] Loaded " << outAbilities.size() << " abilities.\n";
    return true;
}

} // namespace battle::loader
