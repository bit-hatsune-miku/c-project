#ifndef BATTLE_PLAYER_PROGRESSION_H
#define BATTLE_PLAYER_PROGRESSION_H

#include <string>
#include <vector>

namespace battle {

struct PlayerProgression {
    std::vector<std::string> unlockedCharacterKeys;
    std::vector<std::string> currentPartyLineup;
    std::vector<std::string> clearedBattleKeys;
};

enum class ProgressionFallbackPolicy {
    StarterRoster,
    FullRoster
};

PlayerProgression makeDefaultPlayerProgression();
bool loadAllCharacterKeys(std::vector<std::string>& outCharacterKeys);
std::vector<std::string> sanitizeCharacterKeyList(const std::vector<std::string>& keys);
void normalizePlayerProgression(PlayerProgression& progression,
                                ProgressionFallbackPolicy fallbackPolicy = ProgressionFallbackPolicy::StarterRoster);

} // namespace battle

#endif // BATTLE_PLAYER_PROGRESSION_H
