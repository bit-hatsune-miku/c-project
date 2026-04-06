#include "battle_loader.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace battle::loader {
bool loadBossDefinition(const std::string& bossKey, BossDefinition& outBoss);
bool loadCharacterDefinition(const std::string& characterKey, CharacterDefinition& outCharacter);

namespace {

InputPromptType parseInputPromptType(const std::string& promptType) {
    if (promptType == "space") {
        return InputPromptType::Space;
    }
    if (promptType == "wild") {
        return InputPromptType::Wild;
    }
    if (promptType == "custom") {
        return InputPromptType::Custom;
    }
    if (promptType == "arrows") {
        return InputPromptType::Arrows;
    }
    if (promptType == "upDown") {
        return InputPromptType::UpDown;
    }
    if (promptType == "leftRight") {
        return InputPromptType::LeftRight;
    }
    if (promptType == "spamSpace") {
        return InputPromptType::SpamSpace;
    }
    return InputPromptType::None;
}

SpecialDamageSource parseSpecialDamageSource(const std::string& source) {
    if (source == "team_shield") {
        return SpecialDamageSource::TeamShield;
    }
    if (source == "applied_heal") {
        return SpecialDamageSource::AppliedHeal;
    }
    if (source == "stored_healing_tally") {
        return SpecialDamageSource::StoredHealingTally;
    }
    return SpecialDamageSource::None;
}

ActionAdvanceMode parseActionAdvanceMode(const std::string& mode) {
    if (mode == "base_av_delta") {
        return ActionAdvanceMode::BaseActionValueDelta;
    }
    return ActionAdvanceMode::RemainingFraction;
}

std::string getUnitAbilityReferenceId(const json& unitJson, const char* slotName) {
    if (!unitJson.is_object() || slotName == nullptr) {
        return {};
    }

    const std::string slot(slotName);
    if (slot == "skill") {
        return unitJson.value("skillAbility", unitJson.value("ability", ""));
    }
    if (slot == "ultimate") {
        return unitJson.value("ultimate", "");
    }
    if (slot == "standard") {
        return unitJson.value("standardAbility", "");
    }

    return {};
}

bool parseCharacterAbilityKitDefinition(const json& kitJson, CharacterAbilityKitDefinition& outKit) {
    if (!kitJson.is_object()) {
        return false;
    }

    outKit = CharacterAbilityKitDefinition{};
    outKit.standardAbility = kitJson.value("standardAbility", "");
    outKit.skillAbility = kitJson.value("skillAbility", kitJson.value("ability", ""));
    outKit.ultimate = kitJson.value("ultimate", "");
    outKit.assetId = kitJson.value("assetId", "");
    return true;
}

bool parseNestedUnitAbilities(const json& unitJson,
                              const std::string& unitKey,
                              std::unordered_map<std::string, AbilityDefinition>& outAbilities) {
    if (!unitJson.is_object() || !unitJson.contains("abilities")) {
        return true;
    }

    const json& abilitiesJson = unitJson.at("abilities");
    if (!abilitiesJson.is_object()) {
        std::cerr << "[Battle] Invalid nested abilities for unit: " << unitKey << "\n";
        return false;
    }

    for (auto it = abilitiesJson.begin(); it != abilitiesJson.end(); ++it) {
        if (!it.value().is_object()) {
            std::cerr << "[Battle] Invalid nested ability entry '" << it.key()
                      << "' for unit: " << unitKey << "\n";
            return false;
        }

        const std::string fallbackId = getUnitAbilityReferenceId(unitJson, it.key().c_str());
        const std::string abilityId = it.value().value("id", fallbackId);
        if (abilityId.empty()) {
            std::cerr << "[Battle] Nested ability '" << it.key()
                      << "' for unit '" << unitKey << "' is missing an id reference\n";
            return false;
        }

        AbilityDefinition def;
        if (!parseAbilityDefinition(it.value(), abilityId, def)) {
            std::cerr << "[Battle] Failed parsing nested ability '" << abilityId
                      << "' for unit: " << unitKey << "\n";
            return false;
        }

        outAbilities[abilityId] = std::move(def);
    }

    return true;
}

/**
 * @brief Loads nested ability definitions from a JSON asset and merges them into an ability map.
 *
 * Reads the JSON at the given relative asset path, expects a top-level object where each entry
 * contains nested ability definitions for a unit, and parses those nested abilities into the
 * provided output map (overwriting any existing entries with the same id).
 *
 * @param relativePath Path to the JSON asset, relative to the assets root.
 * @param label Optional label used in error messages when reading/parsing the file (may be null).
 * @param outAbilities Map that will be populated with parsed AbilityDefinition entries keyed by id.
 * @return true if the file was read and all nested abilities were parsed and merged successfully, false otherwise.
 */
bool loadNestedAbilityDefinitionsFromFile(const std::string& relativePath,
                                          const char* label,
                                          std::unordered_map<std::string, AbilityDefinition>& outAbilities) {
    json root;
    if (!readJsonRoot(resolveAssetPath(relativePath), root, label)) {
        return false;
    }
    if (!root.is_object()) {
        std::cerr << "[Battle] Invalid " << label << " JSON root\n";
        return false;
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!parseNestedUnitAbilities(it.value(), it.key(), outAbilities)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Populates a PresentationTuningProfile from a JSON tuning object.
 *
 * Initializes \p outProfile to defaults, sets its profileId and phaseIndex,
 * and copies optional numeric entries from \p tuningJson:
 * - "intParams": integer entries copied into outProfile.intParams
 * - "floatParams": numeric entries copied into outProfile.floatParams
 *
 * Entries with the wrong JSON type are ignored. If \p tuningJson is not an
 * object, outProfile is still initialized with the provided profileId and phaseIndex.
 *
 * @param tuningJson JSON object containing optional "intParams" and "floatParams".
 * @param profileId Identifier to assign to the profile.
 * @param phaseIndex Phase index to assign to the profile.
 * @param outProfile Destination profile that will be initialized and populated.
 */
void parsePresentationTuningProfile(const json& tuningJson,
                                    const std::string& profileId,
                                    int phaseIndex,
                                    PresentationTuningProfile& outProfile) {
    outProfile = PresentationTuningProfile{};
    outProfile.profileId = profileId;
    outProfile.phaseIndex = phaseIndex;

    if (!tuningJson.is_object()) {
        return;
    }

    if (const auto intParamsIt = tuningJson.find("intParams");
        intParamsIt != tuningJson.end() && intParamsIt->is_object()) {
        for (auto it = intParamsIt->begin(); it != intParamsIt->end(); ++it) {
            if (!it.value().is_number_integer()) {
                continue;
            }
            outProfile.intParams[it.key()] = it.value().get<int>();
        }
    }

    if (const auto floatParamsIt = tuningJson.find("floatParams");
        floatParamsIt != tuningJson.end() && floatParamsIt->is_object()) {
        for (auto it = floatParamsIt->begin(); it != floatParamsIt->end(); ++it) {
            if (!it.value().is_number()) {
                continue;
            }
            outProfile.floatParams[it.key()] = it.value().get<float>();
        }
    }
}

/**
 * @brief Populates a boss phase definition from JSON.
 *
 * Initializes outPhase to defaults; if phaseJson is an object, marks the phase as configured
 * and reads optional fields `atkBonusPercent`, `bgm`, and `bgmVolume`. It also sets
 * outPhase.tuningProfile from a nested `presentation` object when present, otherwise
 * assigns a default profileId derived from bossKey and phaseIndex and sets the profile's phaseIndex.
 *
 * @param phaseJson JSON object describing the phase; ignored if not an object (outPhase remains defaults).
 * @param bossKey Base key of the boss used to synthesize a default tuning profile id when needed.
 * @param phaseIndex Zero-based index of the phase (used in the synthesized profile id and stored in the profile).
 * @param outPhase Destination structure that will be overwritten with parsed/defaulted phase data.
 */
void parseBossPhaseDefinition(const json& phaseJson,
                              const std::string& bossKey,
                              int phaseIndex,
                              BossDefinition::PhaseDefinition& outPhase) {
    outPhase = BossDefinition::PhaseDefinition{};
    if (!phaseJson.is_object()) {
        return;
    }

    outPhase.configured = true;
    outPhase.atkBonusPercent = phaseJson.value("atkBonusPercent", 0);
    outPhase.bgm = phaseJson.value("bgm", "");
    if (const auto bgmVolumeIt = phaseJson.find("bgmVolume");
        bgmVolumeIt != phaseJson.end() && bgmVolumeIt->is_number()) {
        outPhase.bgmVolume = bgmVolumeIt->get<float>();
    }
    outPhase.phaseChangeVoice = phaseJson.value("phaseChangeVoice", "");

    if (const auto presentationIt = phaseJson.find("presentation");
        presentationIt != phaseJson.end()) {
        parsePresentationTuningProfile(
            *presentationIt,
            bossKey + ".phase" + std::to_string(phaseIndex + 1),
            phaseIndex,
            outPhase.tuningProfile
        );
    } else {
        outPhase.tuningProfile = PresentationTuningProfile{};
        outPhase.tuningProfile.profileId = bossKey + ".phase" + std::to_string(phaseIndex + 1);
        outPhase.tuningProfile.phaseIndex = phaseIndex;
    }
}

/**
 * @brief Parses a JSON object into a BossDefinition and validates top-level structure.
 *
 * Parses fields from `bossJson` into `outBoss`, populating identifiers, assets, stats,
 * ability references, music settings, and optional phase data. If a `"phases"` object
 * is present it will parse up to `"phase1"`, `"phase2"`, and `"phase3"` and set
 * `outBoss.hasPhaseData` when any phase is configured.
 *
 * @param bossJson JSON value expected to be an object containing boss data.
 * @param key The lookup key used as the boss definition's `key` and default `title`.
 * @param outBoss Output structure that will be overwritten with the parsed boss definition.
 * @return true if `bossJson` is an object and parsing succeeded; `outBoss` is not modified on failure.
 */
bool parseBossDefinition(const json& bossJson, const std::string& key, BossDefinition& outBoss) {
    if (!bossJson.is_object()) {
        return false;
    }

    outBoss = BossDefinition{};
    outBoss.key = key;
    outBoss.title = bossJson.value("title", key);
    outBoss.assets = bossJson.value("assets", "");
    outBoss.voiceHit = bossJson.value("voiceHit", "");
    outBoss.voiceSpeakerId = bossJson.value("voiceSpeakerId", "");
    outBoss.spd = bossJson.value("spd", 0);
    outBoss.atk = bossJson.value("atk", 0);
    outBoss.hp = bossJson.value("hp", 0);
    outBoss.skillAbility = bossJson.value("skillAbility", bossJson.value("ability", ""));
    outBoss.standardAbility = bossJson.value("standardAbility", "BossStandardAttack");
    outBoss.ultimate = bossJson.value("ultimate", outBoss.skillAbility);
    outBoss.ultimatePoints = bossJson.value("ultimatePoints", 6);
    outBoss.startingOrbs = bossJson.value("startingOrbs", 1);
    outBoss.bgm = bossJson.value("bgm", "");
    outBoss.bgmVolume = bossJson.value("bgmVolume", 1.0f);
    outBoss.ability = outBoss.skillAbility;

    if (const auto phasesIt = bossJson.find("phases"); phasesIt != bossJson.end() && phasesIt->is_object()) {
        static constexpr std::array<const char*, 3> kPhaseKeys = {"phase1", "phase2", "phase3"};
        bool anyConfigured = false;
        for (size_t phaseIndex = 0; phaseIndex < kPhaseKeys.size(); ++phaseIndex) {
            const auto phaseIt = phasesIt->find(kPhaseKeys[phaseIndex]);
            if (phaseIt == phasesIt->end()) {
                continue;
            }
            parseBossPhaseDefinition(*phaseIt, key, static_cast<int>(phaseIndex), outBoss.phases[phaseIndex]);
            anyConfigured = anyConfigured || outBoss.phases[phaseIndex].configured;
        }
        outBoss.hasPhaseData = anyConfigured;
    }
    return true;
}

bool parseCharacterDefinition(const json& characterJson,
                              const std::string& key,
                              CharacterDefinition& outCharacter) {
    if (!characterJson.is_object()) {
        return false;
    }

    outCharacter = CharacterDefinition{};
    outCharacter.key = key;
    outCharacter.title = characterJson.value("title", key);
    outCharacter.assets = characterJson.value("assets", "");
    outCharacter.voiceAssetId = characterJson.value("voiceAssetId", "");
    outCharacter.voiceSpeakerId = characterJson.value("voiceSpeakerId", "");
    outCharacter.characterClass = characterJson.value("class", "");
    outCharacter.isSinger = characterJson.value("isSinger", false);
    outCharacter.spd = characterJson.value("spd", 0);
    outCharacter.atk = characterJson.value("atk", 0);
    outCharacter.hp = characterJson.value("hp", 0);
    outCharacter.standardAbility = characterJson.value("standardAbility", "BasicAttack");
    outCharacter.skillAbility = characterJson.value("skillAbility", characterJson.value("ability", ""));
    outCharacter.ability = outCharacter.skillAbility;
    outCharacter.ultimate = characterJson.value("ultimate", "");
    outCharacter.ultimatePoints = characterJson.value("ultimatePoints", 0);
    outCharacter.startingOrbs = characterJson.value("startingOrbs", 0);
    outCharacter.baseShield = characterJson.value("baseShield", 0);

    if (const auto kitsIt = characterJson.find("abilityKits");
        kitsIt != characterJson.end()) {
        if (!kitsIt->is_object()) {
            return false;
        }

        for (auto it = kitsIt->begin(); it != kitsIt->end(); ++it) {
            CharacterAbilityKitDefinition kit;
            if (!parseCharacterAbilityKitDefinition(it.value(), kit)) {
                return false;
            }
            outCharacter.abilityKits[it.key()] = std::move(kit);
        }
    }

    return true;
}

std::vector<std::string> collectUniqueBattleCharacterKeys(const BattleDefinition& battle) {
    std::vector<std::string> merged;
    merged.reserve(battle.lockedLineup.size() + battle.lineup.size());

    const auto appendIfMissing = [&merged](const std::string& key) {
        if (key.empty()) {
            return;
        }
        if (std::find(merged.begin(), merged.end(), key) == merged.end()) {
            merged.push_back(key);
        }
    };

    for (const std::string& key : battle.lockedLineup) {
        appendIfMissing(key);
    }
    for (const std::string& key : battle.lineup) {
        appendIfMissing(key);
    }

    return merged;
}

int resolveBattlePartySize(const json& battleJson, const BattleDefinition& battle) {
    const int configuredPartySize = battleJson.value("partySize", -1);
    if (configuredPartySize > 0) {
        return configuredPartySize;
    }

    const std::vector<std::string> merged = collectUniqueBattleCharacterKeys(battle);
    if (battle.isLineupFixed) {
        return std::max(1, static_cast<int>(merged.size()));
    }
    if (!battle.lockedLineup.empty()) {
        return std::max(1, static_cast<int>(battle.lockedLineup.size()));
    }

    return 4;
}

bool isBattleDefinitionValid(const BattleDefinition& battle) {
    if (battle.key.empty() || battle.bossKey.empty()) {
        return false;
    }
    if (battle.partySize < 1) {
        return false;
    }

    const std::vector<std::string> merged = collectUniqueBattleCharacterKeys(battle);
    if (battle.isLineupFixed && merged.empty()) {
        return false;
    }

    BossDefinition bossDefinition;
    if (!loadBossDefinition(battle.bossKey, bossDefinition)) {
        return false;
    }

    for (const std::string& key : merged) {
        CharacterDefinition characterDefinition;
        if (!loadCharacterDefinition(key, characterDefinition)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Parses a battle JSON object into a BattleDefinition and validates it.
 *
 * Populates `outBattle` with fields from `battleJson`, applying defaults for missing values,
 * reading optional `buffs` and `specialRules`, resolving the party size, and enforcing consistency
 * rules. If `battleJson` is not an object or the resulting definition fails validation, no
 * assignment to `outBattle` is performed.
 *
 * @param battleKey Optional override for the battle's key; if empty the function uses `battleJson["key"]`.
 * @param battleJson JSON object containing battle fields (e.g., id, name, bossKey, lineup, buffs).
 * @param outBattle Destination for the parsed and validated BattleDefinition.
 * @return `true` if the JSON was parsed and validated successfully, `false` otherwise.
 */
bool fillBattleDefinitionFromJson(const std::string& battleKey,
                                  const json& battleJson,
                                  BattleDefinition& outBattle) {
    if (!battleJson.is_object()) {
        return false;
    }

    BattleDefinition battle;
    battle.key = battleKey.empty() ? battleJson.value("key", "") : battleKey;
    battle.id = battleJson.value("id", -1);
    battle.storyOrder = battleJson.value("storyOrder", battle.id);
    battle.name = battleJson.value("name", battle.key);
    battle.description = battleJson.value("description", "");
    battle.type = battleJson.value("type", "");
    battle.bossKey = battleJson.value("bossKey", "");
    battle.stageKey = battleJson.value("stageKey", "");
    battle.storyScript = battleJson.value("storyScript", "");
    battle.victoryStoryScript = battleJson.value("victoryStoryScript", "");
    battle.defeatStoryScript = battleJson.value("defeatStoryScript", "");
    battle.nextStoryScript = battleJson.value("nextStoryScript", "");
    battle.selectorVisible = battleJson.value("selectorVisible", false);
    battle.isLineupFixed = battleJson.value("isLineupFixed", true);
    battle.lineup = battleJson.value("lineup", std::vector<std::string>{});
    battle.lockedLineup = battleJson.value("lockedLineup", std::vector<std::string>{});
    if (battleJson.contains("buffs") && battleJson.at("buffs").is_object()) {
        const json& buffsJson = battleJson.at("buffs");
        battle.buffs.hp = buffsJson.value("hp", 0);
        battle.buffs.atk = buffsJson.value("atk", 0);
        battle.buffs.spd = buffsJson.value("spd", 0);
    }
    if (battleJson.contains("specialRules") && battleJson.at("specialRules").is_object()) {
        const json& specialRulesJson = battleJson.at("specialRules");
        battle.specialRules.playerDamageHealsBoss =
            specialRulesJson.value("playerDamageHealsBoss", false);
        battle.specialRules.autoRevivePartyOnBossDamage =
            specialRulesJson.value("autoRevivePartyOnBossDamage", false);
        battle.specialRules.revivePartyToFull =
            specialRulesJson.value("revivePartyToFull", false);
        battle.specialRules.bossSelfKnockoutIsDefeat =
            specialRulesJson.value("bossSelfKnockoutIsDefeat", false);
    }
    battle.partySize = resolveBattlePartySize(battleJson, battle);

    if (!battle.isLineupFixed && battle.lineup.empty()) {
        battle.lineup = battle.lockedLineup;
    }

    if (!isBattleDefinitionValid(battle)) {
        return false;
    }

    outBattle = std::move(battle);
    return true;
}

const json* findBattleJsonByKey(const json& root, const std::string& battleKey) {
    if (root.is_object() && root.contains("battles")) {
        const json& battlesJson = root.at("battles");
        if (battlesJson.is_object()) {
            auto it = battlesJson.find(battleKey);
            if (it != battlesJson.end()) {
                return &it.value();
            }
        } else if (battlesJson.is_array()) {
            for (const json& entry : battlesJson) {
                if (entry.is_object() && entry.value("key", "") == battleKey) {
                    return &entry;
                }
            }
        }
    } else if (root.is_array()) {
        for (const json& entry : root) {
            if (entry.is_object() && entry.value("key", "") == battleKey) {
                return &entry;
            }
        }
    }

    return nullptr;
}

const json* findBattleJsonById(const json& root, int battleId, std::string& outBattleKey) {
    if (root.is_object() && root.contains("battles")) {
        const json& battlesJson = root.at("battles");
        if (battlesJson.is_object()) {
            for (auto it = battlesJson.begin(); it != battlesJson.end(); ++it) {
                if (it.value().is_object() && it.value().value("id", -1) == battleId) {
                    outBattleKey = it.key();
                    return &it.value();
                }
            }
        } else if (battlesJson.is_array()) {
            for (const json& entry : battlesJson) {
                if (entry.is_object() && entry.value("id", -1) == battleId) {
                    outBattleKey = entry.value("key", "");
                    return &entry;
                }
            }
        }
    } else if (root.is_array()) {
        for (const json& entry : root) {
            if (entry.is_object() && entry.value("id", -1) == battleId) {
                outBattleKey = entry.value("key", "");
                return &entry;
            }
        }
    }

    return nullptr;
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

bool readJsonRoot(const std::string& path, json& outRoot, const char* label) {
    std::string contents;
    if (!readJsonFile(path, contents)) {
        if (label != nullptr) {
            std::cerr << "[Battle] Could not read " << label << " JSON: " << path << "\n";
        }
        return false;
    }

    try {
        outRoot = json::parse(contents);
        return true;
    } catch (const json::exception& e) {
        if (label != nullptr) {
            std::cerr << "[Battle] Failed parsing " << label << " JSON: " << e.what() << "\n";
        }
        return false;
    }
}

bool loadBossDefinition(const std::string& bossKey, BossDefinition& outBoss) {
    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/boss.json"), root, "boss")) {
        return false;
    }
    if (!root.contains(bossKey)) {
        std::cerr << "[Battle] Boss key not found: " << bossKey << "\n";
        return false;
    }

    if (!parseBossDefinition(root.at(bossKey), bossKey, outBoss)) {
        std::cerr << "[Battle] Invalid boss entry for key: " << bossKey << "\n";
        return false;
    }

    return true;
}

bool loadCharacterDefinition(const std::string& characterKey, CharacterDefinition& outCharacter) {
    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/characters.json"), root, "character")) {
        return false;
    }
    if (!root.contains(characterKey)) {
        std::cerr << "[Battle] Character key not found: " << characterKey << "\n";
        return false;
    }

    if (!parseCharacterDefinition(root.at(characterKey), characterKey, outCharacter)) {
        std::cerr << "[Battle] Invalid character entry for key: " << characterKey << "\n";
        return false;
    }

    return true;
}

bool loadAllCharacterDefinitions(std::vector<CharacterDefinition>& outCharacters) {
    outCharacters.clear();

    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/characters.json"), root, "character")) {
        return false;
    }
    if (!root.is_object()) {
        std::cerr << "[Battle] Invalid character JSON root\n";
        return false;
    }

    outCharacters.reserve(root.size());
    for (auto it = root.begin(); it != root.end(); ++it) {
        CharacterDefinition character;
        if (!parseCharacterDefinition(it.value(), it.key(), character)) {
            std::cerr << "[Battle] Invalid character entry for key: " << it.key() << "\n";
            return false;
        }
        outCharacters.push_back(std::move(character));
    }

    std::sort(outCharacters.begin(), outCharacters.end(), [](const CharacterDefinition& lhs,
                                                             const CharacterDefinition& rhs) {
        if (lhs.title != rhs.title) {
            return lhs.title < rhs.title;
        }
        return lhs.key < rhs.key;
    });

    return true;
}

bool loadBattleDefinition(const std::string& battleKey, BattleDefinition& outBattle) {
    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/battles.json"), root, "battle")) {
        return false;
    }

    const json* battleJson = findBattleJsonByKey(root, battleKey);
    if (battleJson == nullptr) {
        std::cerr << "[Battle] Battle key not found: " << battleKey << "\n";
        return false;
    }

    if (!fillBattleDefinitionFromJson(battleKey, *battleJson, outBattle)) {
        std::cerr << "[Battle] Invalid battle definition: " << battleKey << "\n";
        return false;
    }

    return true;
}

bool loadBattleDefinitionById(int battleId, BattleDefinition& outBattle) {
    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/battles.json"), root, "battle")) {
        return false;
    }

    std::string battleKey;
    const json* battleJson = findBattleJsonById(root, battleId, battleKey);
    if (battleJson == nullptr) {
        std::cerr << "[Battle] Battle id not found: " << battleId << "\n";
        return false;
    }

    if (!fillBattleDefinitionFromJson(battleKey, *battleJson, outBattle)) {
        std::cerr << "[Battle] Invalid battle definition for id: " << battleId << "\n";
        return false;
    }

    return true;
}

/**
 * @brief Loads all battle definitions from assets/combat/battles.json into the provided vector.
 *
 * Populates outBattles with parsed BattleDefinition objects found in the JSON root (either the
 * top-level array or the object under the "battles" key), then sorts the collection by
 * storyOrder, id, and key.
 *
 * @param outBattles Destination vector that will be cleared and replaced with the loaded battles.
 * @return true if the JSON file was read and every battle entry was parsed successfully; false if
 *         the file cannot be read, the JSON root/container is invalid, or any battle definition
 *         fails validation/parsing.
 */
bool loadAllBattleDefinitions(std::vector<BattleDefinition>& outBattles) {
    outBattles.clear();

    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/battles.json"), root, "battle")) {
        return false;
    }

    const json* battlesJson = nullptr;
    if (root.is_object() && root.contains("battles")) {
        battlesJson = &root.at("battles");
    } else if (root.is_array()) {
        battlesJson = &root;
    } else {
        std::cerr << "[Battle] Invalid battles JSON root\n";
        return false;
    }

    if (battlesJson->is_object()) {
        outBattles.reserve(battlesJson->size());
        for (auto it = battlesJson->begin(); it != battlesJson->end(); ++it) {
            BattleDefinition battle;
            if (!fillBattleDefinitionFromJson(it.key(), it.value(), battle)) {
                std::cerr << "[Battle] Invalid battle definition: " << it.key() << "\n";
                return false;
            }
            outBattles.push_back(std::move(battle));
        }
    } else if (battlesJson->is_array()) {
        outBattles.reserve(battlesJson->size());
        for (const json& entry : *battlesJson) {
            BattleDefinition battle;
            if (!fillBattleDefinitionFromJson(entry.value("key", ""), entry, battle)) {
                std::cerr << "[Battle] Invalid battle definition in array\n";
                return false;
            }
            outBattles.push_back(std::move(battle));
        }
    } else {
        std::cerr << "[Battle] Invalid battles container\n";
        return false;
    }

    std::sort(outBattles.begin(), outBattles.end(), [](const BattleDefinition& lhs, const BattleDefinition& rhs) {
        if (lhs.storyOrder != rhs.storyOrder) {
            return lhs.storyOrder < rhs.storyOrder;
        }
        if (lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }
        return lhs.key < rhs.key;
    });
    return true;
}

bool loadAbilityDefinition(const std::string& abilityId, AbilityDefinition& outAbility) {
    std::unordered_map<std::string, AbilityDefinition> abilities;
    if (!loadAllAbilities(abilities)) {
        return false;
    }

    const auto it = abilities.find(abilityId);
    if (it == abilities.end()) {
        return false;
    }

    outAbility = it->second;
    return true;
}

bool parseAbilityDefinition(const json& abilityJson,
                            const std::string& abilityId,
                            AbilityDefinition& outAbility) {
    if (!abilityJson.is_object()) {
        return false;
    }

    outAbility = AbilityDefinition{};
    outAbility.id = abilityId;
    outAbility.name = abilityJson.value("name", abilityId);
    outAbility.statusName = abilityJson.value("statusName", "");
    outAbility.instructionHint = abilityJson.value("instructionHint", "");
    outAbility.multiplier = abilityJson.value("multiplier", 1.0f);
    outAbility.flatHeal = abilityJson.value("flatHeal", 0);
    outAbility.baseShield = abilityJson.value("baseShield", 0);
    outAbility.orbGain = std::max(0, abilityJson.value("orbGain", 1));
    outAbility.amountPercentOfCasterMaxHp.reset();
    if (const auto percentIt = abilityJson.find("amountPercentOfCasterMaxHp");
        percentIt != abilityJson.end() && percentIt->is_number()) {
        outAbility.amountPercentOfCasterMaxHp = percentIt->get<float>();
    }
    outAbility.speedBuff = abilityJson.value("speedBuff", 0);
    outAbility.atkBuff = abilityJson.value("atkBuff", 0);
    outAbility.damageBuff = abilityJson.value("damageBuff", 0);
    outAbility.actionAdvance = abilityJson.value("actionAdvance", 0.0f);
    outAbility.actionAdvanceMode = parseActionAdvanceMode(
        abilityJson.value("actionAdvanceMode", "remaining_fraction")
    );
    outAbility.selfHpCostPercentOfMax = abilityJson.value("selfHpCostPercentOfMax", 0.0f);
    outAbility.selfHpCostPercentIncreasePerUse = abilityJson.value("selfHpCostPercentIncreasePerUse", 0.0f);
    outAbility.selfHpCostPercentMax = abilityJson.value("selfHpCostPercentMax", 0.0f);
    outAbility.reviveDeadAllies = abilityJson.value("reviveDeadAllies", false);
    outAbility.specialDamageSource = parseSpecialDamageSource(
        abilityJson.value("specialDamageSource", "")
    );
    outAbility.presentationId = abilityJson.value("presentationId", "");
    outAbility.inputPromptType = parseInputPromptType(abilityJson.value("inputPromptType", "none"));

    if (const auto keysIt = abilityJson.find("inputPromptKeys");
        keysIt != abilityJson.end() && keysIt->is_array()) {
        for (const json& keyValue : *keysIt) {
            if (keyValue.is_string()) {
                outAbility.inputPromptKeys.push_back(keyValue.get<std::string>());
            }
        }
    }

    const std::string typeStr = abilityJson.value("type", "attack");
    if (typeStr == "heal") {
        outAbility.type = AbilityType::Heal;
    } else if (typeStr == "buff") {
        outAbility.type = AbilityType::Buff;
    } else if (typeStr == "debuff") {
        outAbility.type = AbilityType::Debuff;
    } else if (typeStr == "shield") {
        outAbility.type = AbilityType::Shield;
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

    json root;
    if (!readJsonRoot(resolveAssetPath("assets/combat/abilities.json"), root, "abilities")) {
        return false;
    }
    if (!root.is_object()) {
        std::cerr << "[Battle] Invalid abilities JSON root\n";
        return false;
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        AbilityDefinition def;
        if (!parseAbilityDefinition(it.value(), it.key(), def)) {
            continue;
        }
        outAbilities[it.key()] = def;
    }

    if (!loadNestedAbilityDefinitionsFromFile("assets/combat/characters.json", "character", outAbilities)) {
        return false;
    }
    if (!loadNestedAbilityDefinitionsFromFile("assets/combat/boss.json", "boss", outAbilities)) {
        return false;
    }

    std::cout << "[Battle] Loaded " << outAbilities.size() << " abilities.\n";
    return true;
}

} // namespace battle::loader
