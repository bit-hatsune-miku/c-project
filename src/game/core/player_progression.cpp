#include "player_progression.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

#include "battle_loader.h"

namespace battle {
namespace {

constexpr std::size_t kDefaultPartyLineupSize = 4;
constexpr std::array<const char*, 2> kStarterRoster = {"miku", "cupcakke"};

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

} // namespace

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

bool hasUnlockedCharacter(const PlayerProgression& progression, const std::string& characterKey) {
    return std::find(progression.unlockedCharacterKeys.begin(),
                     progression.unlockedCharacterKeys.end(),
                     characterKey) != progression.unlockedCharacterKeys.end();
}

bool hasClearedBattle(const PlayerProgression& progression, const std::string& battleKey) {
    return std::find(progression.clearedBattleKeys.begin(),
                     progression.clearedBattleKeys.end(),
                     battleKey) != progression.clearedBattleKeys.end();
}

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

void normalizePlayerProgression(PlayerProgression& progression, ProgressionFallbackPolicy fallbackPolicy) {
    std::vector<std::string> allCharacterKeys;
    if (!loadAllCharacterKeys(allCharacterKeys)) {
        progression.unlockedCharacterKeys = sanitizeCharacterKeyList(progression.unlockedCharacterKeys);
        progression.currentPartyLineup = sanitizeCharacterKeyList(progression.currentPartyLineup);
        progression.clearedBattleKeys = uniqueKeySequence(progression.clearedBattleKeys);
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
}

} // namespace battle
