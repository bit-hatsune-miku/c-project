#ifndef BATTLE_SYNERGY_PAIRS_H
#define BATTLE_SYNERGY_PAIRS_H

#include <string>
#include <vector>

namespace battle {

struct SynergyPairDefinition {
    std::string key;
    std::vector<std::string> members;
    std::string combinedUltimateAbilityId;
    std::string designatedCasterKey;
};

namespace synergy {

bool loadAllPairDefinitions(std::vector<SynergyPairDefinition>& outPairs);

} // namespace synergy

} // namespace battle

#endif // BATTLE_SYNERGY_PAIRS_H
