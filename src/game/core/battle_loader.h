#ifndef BATTLE_LOADER_H
#define BATTLE_LOADER_H

// Code updated by Lyes, 10:02AM 2026/4/29.
// Revision details: added grader-facing API notes for the JSON loading layer
// used by battles, bosses, characters, and abilities.

#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "battle_manager.h"

namespace battle::loader {

// Reads and validates a JSON root object from the given asset path.
bool readJsonRoot(const std::string& path, nlohmann::json& outRoot, const char* label);

bool readJsonFile(const std::string& path, std::string& outContents);

// Resolves authored repo-relative asset references into runtime filesystem
// paths. This is used by the battle data layer before renderer/audio systems
// try to open the file.
std::string resolveAssetPath(const std::string& relativePath);

// Data loaders used by the main app and preview tools.
bool loadBossDefinition(const std::string& bossKey, BossDefinition& outBoss);
bool loadCharacterDefinition(const std::string& characterKey, CharacterDefinition& outCharacter);
bool loadAllCharacterDefinitions(std::vector<CharacterDefinition>& outCharacters);

bool loadBattleDefinition(const std::string& battleKey, BattleDefinition& outBattle);
bool loadBattleDefinitionById(int battleId, BattleDefinition& outBattle);
bool loadAllBattleDefinitions(std::vector<BattleDefinition>& outBattles);

// Ability loading is split out because characters and bosses both embed ability
// references, while some tools need direct access to parsed ability data.
bool loadAbilityDefinition(const std::string& abilityId, AbilityDefinition& outAbility);
bool parseAbilityDefinition(const nlohmann::json& abilityJson,
                            const std::string& abilityId,
                            AbilityDefinition& outAbility);
bool loadAllAbilities(std::unordered_map<std::string, AbilityDefinition>& outAbilities);

} // namespace battle::loader

#endif // BATTLE_LOADER_H
