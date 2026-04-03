#ifndef BATTLE_PLAYER_PROGRESSION_H
#define BATTLE_PLAYER_PROGRESSION_H

#include <string>
#include <unordered_map>
#include <vector>

namespace battle {

struct CharacterDefinition;

struct FlatStatBonuses {
    int hp = 0;
    int atk = 0;
    int spd = 0;
};

using StageBuffAllocation = std::unordered_map<std::string, int>;
using StageBuffAssignments = std::unordered_map<std::string, StageBuffAllocation>;

struct PlayerProgression {
    std::vector<std::string> unlockedCharacterKeys;
    std::vector<std::string> currentPartyLineup;
    std::vector<std::string> clearedBattleKeys;
    StageBuffAssignments stageBuffAssignments;
};

/**
 * Create a default PlayerProgression populated with the expected initial values.
 * @returns A PlayerProgression initialized to the game's default progression state.
 */
/**
 * Load all known character keys.
 * @param[out] outCharacterKeys Receives the list of all character keys when the function returns.
 * @returns `true` if the keys were loaded successfully, `false` otherwise.
 */
/**
 * Produce a sanitized list of character keys suitable for storing or comparing.
 * The result removes or normalizes invalid, duplicate, or otherwise malformed entries.
 * @param keys Input list of character keys to sanitize.
 * @returns A new vector containing the sanitized character keys.
 */
/**
 * Check whether a character is present in the progression's unlocked character list.
 * @param progression The player progression to query.
 * @param characterKey Key identifying the character to check.
 * @returns `true` if `characterKey` exists in `progression.unlockedCharacterKeys`, `false` otherwise.
 */
/**
 * Check whether a battle has been recorded as cleared in the progression.
 * @param progression The player progression to query.
 * @param battleKey Key identifying the battle to check.
 * @returns `true` if `battleKey` exists in `progression.clearedBattleKeys`, `false` otherwise.
 */
/**
 * Compute flat stat bonuses for a character based on the given progression.
 * @param progression The player progression used to compute bonuses.
 * @param characterKey Key identifying the character whose bonuses are computed.
 * @param excludedBattleKey If non-empty, contributions originating from this battle key are ignored.
 * @returns A FlatStatBonuses struct with aggregated `hp`, `atk`, and `spd` bonuses for the character.
 */
/**
 * Apply progression-derived flat bonuses and other progression effects to a CharacterDefinition.
 * @param character The CharacterDefinition to modify in-place.
 * @param progression The player progression supplying bonuses and assignments.
 * @param excludedBattleKey If non-empty, progression contributions from this battle key are ignored when applying bonuses.
 */
/**
 * Replace the stage/battle buff allocation for a specific battle key.
 * If an allocation for `battleKey` already exists it will be overwritten.
 * @param progression The player progression to modify.
 * @param battleKey Key identifying the battle/stage whose allocation is being replaced.
 * @param allocation Mapping of buff identifiers to integer allocation values to store for `battleKey`.
 */
/**
 * Normalize a PlayerProgression to conform to expected structure and valid values.
 * Missing or invalid data may be replaced according to `fallbackPolicy`.
 * @param progression The PlayerProgression to normalize in-place.
 * @param fallbackPolicy Policy determining how to populate or replace missing/invalid progression data (default: StarterRoster).
 */
enum class ProgressionFallbackPolicy {
    StarterRoster,
    FullRoster
};

PlayerProgression makeDefaultPlayerProgression();
bool loadAllCharacterKeys(std::vector<std::string>& outCharacterKeys);
std::vector<std::string> sanitizeCharacterKeyList(const std::vector<std::string>& keys);
bool hasUnlockedCharacter(const PlayerProgression& progression, const std::string& characterKey);
bool hasClearedBattle(const PlayerProgression& progression, const std::string& battleKey);
FlatStatBonuses characterFlatStatBonuses(const PlayerProgression& progression,
                                         const std::string& characterKey,
                                         const std::string& excludedBattleKey = std::string());
void applyCharacterProgressionBonuses(CharacterDefinition& character,
                                     const PlayerProgression& progression,
                                     const std::string& excludedBattleKey = std::string());
void replaceStageBuffAllocation(PlayerProgression& progression,
                                const std::string& battleKey,
                                const StageBuffAllocation& allocation);
void normalizePlayerProgression(PlayerProgression& progression,
                                ProgressionFallbackPolicy fallbackPolicy = ProgressionFallbackPolicy::StarterRoster);

} // namespace battle

#endif // BATTLE_PLAYER_PROGRESSION_H
