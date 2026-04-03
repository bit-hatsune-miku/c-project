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
