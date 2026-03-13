#include "battle_loader.h"

#include <fstream>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace battle::loader {
namespace {

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
    outCharacter.startingOrbs = characterJson.value("startingOrbs", 1);
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
    const std::string path = resolveAssetPath("assets/combat/boss.json");
    std::string contents;
    if (!readJsonFile(path, contents)) {
        std::cerr << "[Battle] Could not read boss JSON: " << path << "\n";
        return false;
    }

    try {
        const json root = json::parse(contents);
        if (!root.contains(bossKey)) {
            std::cerr << "[Battle] Boss key not found: " << bossKey << "\n";
            return false;
        }

        if (!parseBossDefinition(root.at(bossKey), bossKey, outBoss)) {
            std::cerr << "[Battle] Invalid boss entry for key: " << bossKey << "\n";
            return false;
        }

        return true;
    } catch (const json::exception& e) {
        std::cerr << "[Battle] Failed parsing boss JSON: " << e.what() << "\n";
        return false;
    }
}

bool loadCharacterDefinition(const std::string& characterKey, CharacterDefinition& outCharacter) {
    const std::string path = resolveAssetPath("assets/combat/characters.json");
    std::string contents;
    if (!readJsonFile(path, contents)) {
        std::cerr << "[Battle] Could not read character JSON: " << path << "\n";
        return false;
    }

    try {
        const json root = json::parse(contents);
        if (!root.contains(characterKey)) {
            std::cerr << "[Battle] Character key not found: " << characterKey << "\n";
            return false;
        }

        if (!parseCharacterDefinition(root.at(characterKey), characterKey, outCharacter)) {
            std::cerr << "[Battle] Invalid character entry for key: " << characterKey << "\n";
            return false;
        }

        return true;
    } catch (const json::exception& e) {
        std::cerr << "[Battle] Failed parsing character JSON: " << e.what() << "\n";
        return false;
    }
}

bool loadAbilityDefinition(const std::string& abilityId, AbilityDefinition& outAbility) {
    const std::string path = resolveAssetPath("assets/combat/abilities.json");
    std::string contents;
    if (!readJsonFile(path, contents)) {
        return false;
    }

    try {
        const json root = json::parse(contents);
        if (!root.contains(abilityId)) {
            return false;
        }

        return parseAbilityDefinition(root[abilityId], abilityId, outAbility);
    } catch (const json::exception&) {
        return false;
    }
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

    const std::string path = resolveAssetPath("assets/combat/abilities.json");
    std::string contents;
    if (!readJsonFile(path, contents)) {
        std::cerr << "[Battle] Could not read abilities JSON: " << path << "\n";
        return false;
    }

    try {
        const json root = json::parse(contents);
        for (auto it = root.begin(); it != root.end(); ++it) {
            AbilityDefinition def;
            if (!parseAbilityDefinition(it.value(), it.key(), def)) {
                continue;
            }
            outAbilities[it.key()] = def;
        }

        std::cout << "[Battle] Loaded " << outAbilities.size() << " abilities.\n";
        return true;
    } catch (const json::exception& e) {
        std::cerr << "[Battle] Failed parsing abilities JSON: " << e.what() << "\n";
        return false;
    }
}

} // namespace battle::loader
