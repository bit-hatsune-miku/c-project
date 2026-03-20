#ifndef BATTLE_LOADER_H
#define BATTLE_LOADER_H

#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "battle_manager.h"

namespace battle::loader {

bool readJsonRoot(const std::string& path, nlohmann::json& outRoot, const char* label);

bool readJsonFile(const std::string& path, std::string& outContents);
std::string resolveAssetPath(const std::string& relativePath);

bool loadBossDefinition(const std::string& bossKey, BossDefinition& outBoss);
bool loadCharacterDefinition(const std::string& characterKey, CharacterDefinition& outCharacter);

bool loadBattleDefinition(const std::string& battleKey, BattleDefinition& outBattle);
bool loadBattleDefinitionById(int battleId, BattleDefinition& outBattle);

bool loadAbilityDefinition(const std::string& abilityId, AbilityDefinition& outAbility);
bool parseAbilityDefinition(const nlohmann::json& abilityJson,
                            const std::string& abilityId,
                            AbilityDefinition& outAbility);
bool loadAllAbilities(std::unordered_map<std::string, AbilityDefinition>& outAbilities);

} // namespace battle::loader

#endif // BATTLE_LOADER_H
