#include "player_progression.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <unordered_map>
#include <unordered_set>

#include "battle_loader.h"

namespace battle {
namespace {

constexpr std::size_t kDefaultPartyLineupSize = 4;
constexpr std::array<const char*, 2> kStarterRoster = {"miku", "cupcakke"};
constexpr int kStageBuffPickCount = 4;

/**
 * @brief Checks whether a string ends with a given suffix.
 *
 * @param value The string to examine.
 * @param suffix The suffix to test for at the end of `value`.
 * @return `true` if `value` ends with `suffix`, `false` otherwise.
 */
bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<std::string> uniqueValidKeys(const std::vector<std::string>& source,
                                         const std::unordered_set<std::string>& validKeys,
                                         const std::unordered_set<std::string>* allowedKeys = nullptr) {
    std::vector<std::string> result;
    result.reserve(source.size());

    std::unordered_set<std::string> seen;
    for (const std::string& key : source) {
        if (validKeys.find(key) == validKeys.end()) {
            continue;
        }
        if (allowedKeys != nullptr && allowedKeys->find(key) == allowedKeys->end()) {
            continue;
        }
        if (!seen.insert(key).second) {
            continue;
        }
        result.push_back(key);
    }

    return result;
}

std::vector<std::string> starterRosterKeys(const std::unordered_set<std::string>& validKeys) {
    std::vector<std::string> starter;
    starter.reserve(kStarterRoster.size());
    for (const char* key : kStarterRoster) {
        if (validKeys.find(key) != validKeys.end()) {
            starter.emplace_back(key);
        }
    }
    return starter;
}

std::unordered_map<std::string, CharacterDefinition>
characterDefinitionsByKey(const std::vector<CharacterDefinition>& characters) {
    std::unordered_map<std::string, CharacterDefinition> result;
    result.reserve(characters.size());
    for (const CharacterDefinition& character : characters) {
        result.emplace(character.key, character);
    }
    return result;
}

std::unordered_map<std::string, std::string>
characterKeyByAsset(const std::vector<CharacterDefinition>& characters) {
    std::unordered_map<std::string, std::string> result;
    result.reserve(characters.size());
    for (const CharacterDefinition& character : characters) {
        if (!character.assets.empty()) {
            result.emplace(character.assets, character.key);
        }
    }
    return result;
}

std::string resolveUnlockableCharacterKey(
    const std::string& bossKey,
    const std::unordered_map<std::string, CharacterDefinition>& charactersByKey,
    const std::unordered_map<std::string, std::string>& characterKeyByAssetMap) {
    if (bossKey.empty()) {
        return std::string();
    }

    if (charactersByKey.find(bossKey) != charactersByKey.end()) {
        return bossKey;
    }

    if (endsWith(bossKey, "Boss")) {
        const std::string trimmedKey = bossKey.substr(0, bossKey.size() - 4);
        if (charactersByKey.find(trimmedKey) != charactersByKey.end()) {
            return trimmedKey;
        }
    }

    BossDefinition boss;
    if (!loader::loadBossDefinition(bossKey, boss)) {
        return std::string();
    }

    if (charactersByKey.find(boss.key) != charactersByKey.end()) {
        return boss.key;
    }

    if (charactersByKey.find(boss.assets) != charactersByKey.end()) {
        return boss.assets;
    }

    const auto assetIt = characterKeyByAssetMap.find(boss.assets);
    if (assetIt != characterKeyByAssetMap.end()) {
        return assetIt->second;
    }

    return std::string();
}

/**
 * Derives character keys unlocked by the given sequence of cleared battles.
 *
 * For each battle key (in encounter order) this attempts to load the battle definition,
 * resolve the unlockable character key from the battle's boss, and append the resolved
 * key to the result if it is non-empty and has not already been included.
 *
 * @param clearedBattleKeys Sequence of cleared battle keys in encounter order.
 * @param charactersByKey Map from character key to its definition used for direct lookups.
 * @param characterKeyByAssetMap Map from character asset identifier to character key used
 *        when a boss references character assets.
 * @return std::vector<std::string> Unique unlocked character keys in encounter order;
 *         entries are omitted for battles that fail to load or that do not resolve to a character.
 */
std::vector<std::string> unlockedCharacterKeysFromClearedBattles(
    const std::vector<std::string>& clearedBattleKeys,
    const std::unordered_map<std::string, CharacterDefinition>& charactersByKey,
    const std::unordered_map<std::string, std::string>& characterKeyByAssetMap) {
    std::vector<std::string> unlocked;
    unlocked.reserve(clearedBattleKeys.size());

    std::unordered_set<std::string> seen;
    for (const std::string& battleKey : clearedBattleKeys) {
        BattleDefinition battle;
        if (!loader::loadBattleDefinition(battleKey, battle)) {
            continue;
        }

        const std::string unlockedCharacterKey =
            resolveUnlockableCharacterKey(battle.bossKey, charactersByKey, characterKeyByAssetMap);
        if (unlockedCharacterKey.empty() || !seen.insert(unlockedCharacterKey).second) {
            continue;
        }

        unlocked.push_back(unlockedCharacterKey);
    }

    return unlocked;
}

/**
 * @brief Normalize a stage buff allocation to valid character keys and the per-stage pick limit.
 *
 * Filters out entries with non-positive counts or keys not present in `validKeys`, ranks remaining
 * entries by descending count (tie-broken by ascending key), and assigns each entry a clamped
 * count such that the summed picks do not exceed `kStageBuffPickCount`.
 *
 * @param allocation Mapping of character key to requested pick count.
 * @param validKeys Set of allowed character keys; entries not in this set are ignored.
 * @return StageBuffAllocation Mapping of character key to normalized (clamped and possibly reduced)
 *         pick count. Entries with zero picks are omitted.
 */
StageBuffAllocation normalizeStageAllocation(const StageBuffAllocation& allocation,
                                            const std::unordered_set<std::string>& validKeys) {
    std::vector<std::pair<std::string, int>> entries;
    entries.reserve(allocation.size());
    for (const auto& [characterKey, count] : allocation) {
        if (count <= 0 || validKeys.find(characterKey) == validKeys.end()) {
            continue;
        }
        entries.emplace_back(characterKey, count);
    }

    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
        }
        return lhs.first < rhs.first;
    });

    StageBuffAllocation normalized;
    int remaining = kStageBuffPickCount;
    for (const auto& [characterKey, count] : entries) {
        if (remaining <= 0) {
            break;
        }
        const int clampedCount = std::min(count, remaining);
        if (clampedCount <= 0) {
            continue;
        }
        normalized[characterKey] = clampedCount;
        remaining -= clampedCount;
    }

    return normalized;
}

/**
 * @brief Produce a normalized map of per-battle stage buff allocations filtered to valid characters and loadable battles.
 *
 * For each entry in `assignments`, the function skips empty battle keys and battles whose definitions cannot be loaded,
 * normalizes the allocation to contain only valid character keys with counts clamped to the configured pick limit,
 * and includes the entry in the result only if the normalized allocation is non-empty.
 *
 * @param assignments Map from battle key to stage buff allocation to normalize.
 * @param validKeys Set of character keys considered valid for allocations.
 * @return StageBuffAssignments Map of battle key -> normalized stage buff allocation containing only non-empty, validated entries.
 */
StageBuffAssignments normalizeStageBuffAssignments(const StageBuffAssignments& assignments,
                                                   const std::unordered_set<std::string>& validKeys) {
    StageBuffAssignments normalized;
    for (const auto& [battleKey, allocation] : assignments) {
        if (battleKey.empty()) {
            continue;
        }

        BattleDefinition battleDefinition;
        if (!loader::loadBattleDefinition(battleKey, battleDefinition)) {
            continue;
        }

        const StageBuffAllocation normalizedAllocation = normalizeStageAllocation(allocation, validKeys);
        if (!normalizedAllocation.empty()) {
            normalized[battleKey] = normalizedAllocation;
        }
    }
    return normalized;
}

} /**
 * @brief Loads all available character keys into the provided vector.
 *
 * Fills outCharacterKeys with every character definition key returned by the character loader, preserving the loader's order. If loading fails, outCharacterKeys is cleared.
 *
 * @param[out] outCharacterKeys Vector to receive the character keys.
 * @return true if character definitions were successfully loaded and keys were written to outCharacterKeys, false if loading failed (outCharacterKeys will be empty).
 */

bool loadAllCharacterKeys(std::vector<std::string>& outCharacterKeys) {
    outCharacterKeys.clear();

    std::vector<CharacterDefinition> characters;
    if (!loader::loadAllCharacterDefinitions(characters)) {
        return false;
    }

    outCharacterKeys.reserve(characters.size());
    for (const CharacterDefinition& character : characters) {
        outCharacterKeys.push_back(character.key);
    }
    return true;
}

/**
 * @brief Determines whether a character is present in a player's unlocked characters.
 *
 * @param progression Player progression data to check.
 * @param characterKey Character key to look for.
 * @return true if `characterKey` appears in `progression.unlockedCharacterKeys`, false otherwise.
 */
bool hasUnlockedCharacter(const PlayerProgression& progression, const std::string& characterKey) {
    return std::find(progression.unlockedCharacterKeys.begin(),
                     progression.unlockedCharacterKeys.end(),
                     characterKey) != progression.unlockedCharacterKeys.end();
}

/**
 * @brief Determines whether a battle key is present in the player's cleared battles.
 *
 * @param progression Player progression to inspect.
 * @param battleKey Identifier of the battle to check.
 * @return true if `battleKey` appears in `progression.clearedBattleKeys`, false otherwise.
 */
bool hasClearedBattle(const PlayerProgression& progression, const std::string& battleKey) {
    return std::find(progression.clearedBattleKeys.begin(),
                     progression.clearedBattleKeys.end(),
                     battleKey) != progression.clearedBattleKeys.end();
}

bool hasCompletedTutorial(const PlayerProgression& progression, const std::string& tutorialKey) {
    return std::find(progression.completedTutorialKeys.begin(),
                     progression.completedTutorialKeys.end(),
                     tutorialKey) != progression.completedTutorialKeys.end();
}

/**
 * @brief Compute the total flat stat bonuses for a character from stored stage buff allocations.
 *
 * @param progression Player progression data containing per-battle `stageBuffAssignments`.
 * @param characterKey Character key to compute bonuses for; an empty key produces zeroed bonuses.
 * @param excludedBattleKey If non-empty, skips allocations from this battle key when summing bonuses.
 * @return FlatStatBonuses Sum of each battle's `buffs.hp`, `buffs.atk`, and `buffs.spd` multiplied by
 * the allocated count for `characterKey`; returns zeroed bonuses when `characterKey` is empty or no
 * applicable allocations are found.
 */
FlatStatBonuses characterFlatStatBonuses(const PlayerProgression& progression,
                                         const std::string& characterKey,
                                         const std::string& excludedBattleKey) {
    FlatStatBonuses bonuses;
    if (characterKey.empty()) {
        return bonuses;
    }

    for (const auto& [battleKey, allocation] : progression.stageBuffAssignments) {
        if (!excludedBattleKey.empty() && battleKey == excludedBattleKey) {
            continue;
        }

        const auto allocationIt = allocation.find(characterKey);
        if (allocationIt == allocation.end() || allocationIt->second <= 0) {
            continue;
        }

        BattleDefinition battleDefinition;
        if (!loader::loadBattleDefinition(battleKey, battleDefinition)) {
            continue;
        }

        bonuses.hp += battleDefinition.buffs.hp * allocationIt->second;
        bonuses.atk += battleDefinition.buffs.atk * allocationIt->second;
        bonuses.spd += battleDefinition.buffs.spd * allocationIt->second;
    }

    return bonuses;
}

/**
 * @brief Applies flat progression stat bonuses to a character.
 *
 * Adds cumulative flat HP, ATK, and SPD bonuses computed from the player's
 * stage buff assignments to the provided CharacterDefinition.
 *
 * @param character CharacterDefinition to modify; HP, ATK, and SPD fields are incremented.
 * @param progression Player progression data used to compute bonuses.
 * @param excludedBattleKey If non-empty, stage buffs from this battle key are ignored when computing bonuses.
 */
void applyCharacterProgressionBonuses(CharacterDefinition& character,
                                     const PlayerProgression& progression,
                                     const std::string& excludedBattleKey) {
    const FlatStatBonuses bonuses =
        characterFlatStatBonuses(progression, character.key, excludedBattleKey);
    character.hp += bonuses.hp;
    character.atk += bonuses.atk;
    character.spd += bonuses.spd;
}

/**
 * @brief Replace or remove the stage buff allocation for a battle in player progression.
 *
 * Updates progression.stageBuffAssignments for the given battle key: if a valid set of
 * character keys can be loaded, the provided allocation is normalized (filtered to valid
 * characters, clamped to the per-battle pick limit) and stored; if the normalized allocation
 * is empty the mapping is removed. If character keys cannot be loaded, the raw allocation
 * is stored as-is unless it is empty, in which case the mapping is removed. An empty
 * battleKey is ignored.
 *
 * @param progression Player progression object to modify.
 * @param battleKey Key identifying the battle whose allocation should be replaced.
 * @param allocation New stage buff allocation to store (subject to normalization and validation).
 */
void replaceStageBuffAllocation(PlayerProgression& progression,
                                const std::string& battleKey,
                                const StageBuffAllocation& allocation) {
    if (battleKey.empty()) {
        return;
    }

    std::vector<std::string> allCharacterKeys;
    if (!loadAllCharacterKeys(allCharacterKeys)) {
        if (allocation.empty()) {
            progression.stageBuffAssignments.erase(battleKey);
        } else {
            progression.stageBuffAssignments[battleKey] = allocation;
        }
        return;
    }

    const std::unordered_set<std::string> validKeys(allCharacterKeys.begin(), allCharacterKeys.end());
    const StageBuffAllocation normalizedAllocation = normalizeStageAllocation(allocation, validKeys);
    if (normalizedAllocation.empty()) {
        progression.stageBuffAssignments.erase(battleKey);
    } else {
        progression.stageBuffAssignments[battleKey] = normalizedAllocation;
    }
}

void markCompletedTutorial(PlayerProgression& progression, const std::string& tutorialKey) {
    if (tutorialKey.empty() || hasCompletedTutorial(progression, tutorialKey)) {
        return;
    }
    progression.completedTutorialKeys.push_back(tutorialKey);
}

std::string resolveUnlockCharacterKeyForBattle(const std::string& battleKey) {
    if (battleKey.empty()) {
        return std::string();
    }

    BattleDefinition battle;
    if (!loader::loadBattleDefinition(battleKey, battle)) {
        return std::string();
    }

    std::vector<CharacterDefinition> allCharacterDefinitions;
    if (!loader::loadAllCharacterDefinitions(allCharacterDefinitions)) {
        return std::string();
    }

    const auto charactersByKey = characterDefinitionsByKey(allCharacterDefinitions);
    const auto characterKeyByAssetMap = characterKeyByAsset(allCharacterDefinitions);
    return resolveUnlockableCharacterKey(battle.bossKey, charactersByKey, characterKeyByAssetMap);
}

/**
 * @brief Sanitizes a list of character keys by removing duplicates and ensuring validity.
 *
 * When the game's canonical character list is available, the result contains only keys
 * that exist in that list, with duplicates removed and original order preserved.
 * If the canonical list cannot be obtained, duplicates are removed and the original
 * order is preserved but keys are not validated.
 *
 * @param keys Input sequence of character keys to sanitize.
 * @return std::vector<std::string> A deduplicated sequence of character keys, filtered to valid characters when possible.
 */
std::vector<std::string> sanitizeCharacterKeyList(const std::vector<std::string>& keys) {
    std::vector<std::string> allCharacterKeys;
    if (!loadAllCharacterKeys(allCharacterKeys)) {
        std::vector<std::string> fallback;
        fallback.reserve(keys.size());
        std::unordered_set<std::string> seen;
        for (const std::string& key : keys) {
            if (seen.insert(key).second) {
                fallback.push_back(key);
            }
        }
        return fallback;
    }

    const std::unordered_set<std::string> validKeys(allCharacterKeys.begin(), allCharacterKeys.end());
    return uniqueValidKeys(keys, validKeys);
}

std::vector<std::string> uniqueKeySequence(const std::vector<std::string>& keys) {
    std::vector<std::string> result;
    result.reserve(keys.size());

    std::unordered_set<std::string> seen;
    for (const std::string& key : keys) {
        if (key.empty()) {
            continue;
        }
        if (seen.insert(key).second) {
            result.push_back(key);
        }
    }

    return result;
}

PlayerProgression makeDefaultPlayerProgression() {
    PlayerProgression progression;
    normalizePlayerProgression(progression, ProgressionFallbackPolicy::StarterRoster);
    return progression;
}

/**
 * @brief Normalize and sanitize a player's progression fields in place.
 *
 * Ensures `progression.unlockedCharacterKeys`, `progression.currentPartyLineup`,
 * `progression.clearedBattleKeys`, and `progression.stageBuffAssignments` are valid,
 * deduplicated, and consistent with available character and battle data.
 *
 * When character key loading fails, performs best-effort sanitization on the
 * unlocked list and current lineup and deduplicates cleared battles, then returns.
 * When character data is available, computes the final unlocked set by applying
 * a configurable fallback (starter roster or full roster), adding unlocks derived
 * from cleared battles, and optionally honoring explicit unlocked entries from
 * the input progression. The party lineup is then restricted to unlocked characters
 * and populated from fallbacks if empty. Finally, stage buff assignments are
 * normalized to valid keys and per-battle constraints.
 *
 * @param progression Player progression object to modify and normalize.
 * @param fallbackPolicy Policy that determines which fallback unlocked characters
 *                       and lineup to use when constructing the normalized progression
 *                       (e.g., starter roster vs full roster).
 */
void normalizePlayerProgression(PlayerProgression& progression, ProgressionFallbackPolicy fallbackPolicy) {
    std::vector<std::string> allCharacterKeys;
    if (!loadAllCharacterKeys(allCharacterKeys)) {
        progression.unlockedCharacterKeys = sanitizeCharacterKeyList(progression.unlockedCharacterKeys);
        progression.currentPartyLineup = sanitizeCharacterKeyList(progression.currentPartyLineup);
        progression.clearedBattleKeys = uniqueKeySequence(progression.clearedBattleKeys);
        progression.completedTutorialKeys = uniqueKeySequence(progression.completedTutorialKeys);
        return;
    }

    std::vector<CharacterDefinition> allCharacterDefinitions;
    const bool loadedCharacterDefinitions = loader::loadAllCharacterDefinitions(allCharacterDefinitions);
    const std::unordered_set<std::string> validKeys(allCharacterKeys.begin(), allCharacterKeys.end());
    std::vector<std::string> fallbackUnlocked =
        (fallbackPolicy == ProgressionFallbackPolicy::FullRoster)
            ? allCharacterKeys
            : starterRosterKeys(validKeys);
    if (fallbackUnlocked.empty()) {
        fallbackUnlocked = allCharacterKeys;
    }

    const std::vector<std::string> explicitUnlocked =
        uniqueValidKeys(progression.unlockedCharacterKeys, validKeys);
    std::vector<std::string> clearedBattleUnlocks;
    if (loadedCharacterDefinitions) {
        const auto charactersByKey = characterDefinitionsByKey(allCharacterDefinitions);
        const auto characterKeyByAssetMap = characterKeyByAsset(allCharacterDefinitions);
        clearedBattleUnlocks = unlockedCharacterKeysFromClearedBattles(
            progression.clearedBattleKeys,
            charactersByKey,
            characterKeyByAssetMap
        );
    }

    std::vector<std::string> unlocked = uniqueValidKeys(fallbackUnlocked, validKeys);
    std::unordered_set<std::string> unlockedSet(unlocked.begin(), unlocked.end());

    const auto appendUnlockedKey = [&](const std::string& key) {
        if (validKeys.find(key) == validKeys.end()) {
            return;
        }
        if (unlockedSet.insert(key).second) {
            unlocked.push_back(key);
        }
    };

    for (const std::string& key : clearedBattleUnlocks) {
        appendUnlockedKey(key);
    }

    bool ignoreExplicitUnlocked = false;
    if (fallbackPolicy == ProgressionFallbackPolicy::StarterRoster &&
        explicitUnlocked.size() == allCharacterKeys.size() &&
        unlocked.size() < allCharacterKeys.size()) {
        ignoreExplicitUnlocked = true;
    }

    if (!ignoreExplicitUnlocked) {
        for (const std::string& key : explicitUnlocked) {
            appendUnlockedKey(key);
        }
    }

    std::vector<std::string> lineup =
        uniqueValidKeys(progression.currentPartyLineup, validKeys, &unlockedSet);

    std::vector<std::string> fallbackLineup;
    if (fallbackPolicy == ProgressionFallbackPolicy::StarterRoster) {
        fallbackLineup = uniqueValidKeys(starterRosterKeys(validKeys), validKeys, &unlockedSet);
    }
    if (fallbackLineup.empty()) {
        const std::size_t count = std::min(kDefaultPartyLineupSize, unlocked.size());
        fallbackLineup.assign(unlocked.begin(), unlocked.begin() + static_cast<std::ptrdiff_t>(count));
    }
    if (lineup.empty()) {
        lineup = fallbackLineup;
    }

    progression.unlockedCharacterKeys = std::move(unlocked);
    progression.currentPartyLineup = std::move(lineup);
    progression.clearedBattleKeys = uniqueKeySequence(progression.clearedBattleKeys);
    progression.completedTutorialKeys = uniqueKeySequence(progression.completedTutorialKeys);
    progression.stageBuffAssignments =
        normalizeStageBuffAssignments(progression.stageBuffAssignments, validKeys);
}

} // namespace battle
