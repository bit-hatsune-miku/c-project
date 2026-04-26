#include "synergy_pairs.h"

#include <iostream>

#include <nlohmann/json.hpp>

#include "battle_loader.h"

namespace battle::synergy {
namespace {

using json = nlohmann::json;

constexpr char kSynergyPairPath[] = "assets/combat/synergy_pairs.json";

bool parsePairDefinition(const json& pairJson, SynergyPairDefinition& outPair) {
    if (!pairJson.is_object()) {
        return false;
    }

    SynergyPairDefinition pair;
    pair.key = pairJson.value("key", "");
    pair.combinedUltimateAbilityId = pairJson.value("combinedUltimateAbilityId", "");
    pair.designatedCasterKey = pairJson.value("designatedCasterKey", "");

    if (const auto membersIt = pairJson.find("members");
        membersIt != pairJson.end() && membersIt->is_array()) {
        for (const json& member : *membersIt) {
            if (member.is_string()) {
                pair.members.push_back(member.get<std::string>());
            }
        }
    }

    if (pair.key.empty() ||
        pair.members.size() != 2 ||
        pair.members[0].empty() ||
        pair.members[1].empty() ||
        pair.combinedUltimateAbilityId.empty() ||
        pair.designatedCasterKey.empty()) {
        return false;
    }

    outPair = std::move(pair);
    return true;
}

} // namespace

bool loadAllPairDefinitions(std::vector<SynergyPairDefinition>& outPairs) {
    outPairs.clear();

    json root;
    if (!loader::readJsonRoot(loader::resolveAssetPath(kSynergyPairPath), root, "synergy pairs")) {
        return false;
    }

    if (!root.is_array()) {
        std::cerr << "[Synergy] Invalid synergy pair root.\n";
        return false;
    }

    outPairs.reserve(root.size());
    for (const json& pairJson : root) {
        SynergyPairDefinition pair;
        if (!parsePairDefinition(pairJson, pair)) {
            std::cerr << "[Synergy] Invalid pair entry in " << kSynergyPairPath << "\n";
            return false;
        }
        outPairs.push_back(std::move(pair));
    }

    return true;
}

} // namespace battle::synergy
