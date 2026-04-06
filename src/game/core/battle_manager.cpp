#include "battle_manager.h"
#include <cstddef>

namespace battle {
// Returns the shield value for a party member at the given index, or 0 if out of range.
int BattleManager::getCharacterShield(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return 0;
    }
    return characters_[static_cast<size_t>(partyIndex)].getShield();
}
} // namespace battle

#include "ability_system.h"
#include "battle_loader.h"
#include "../presentation/ability_presentation.h"
#include "turn_system.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

namespace battle {
constexpr const char* kDefaultCharacterStandardAbilityId = "BasicAttack";
constexpr const char* kDefaultBossStandardAbilityId = "BossStandardAttack";

std::string getCharacterRegularAbilityId(const CharacterDefinition& definition) {
    if (!definition.skillAbility.empty()) {
        return definition.skillAbility;
    }
    if (!definition.ability.empty()) {
        return definition.ability;
    }
    return definition.standardAbility.empty() ? std::string{kDefaultCharacterStandardAbilityId} : definition.standardAbility;
}

std::string getBossNormalAbilityId(const BossDefinition& definition) {
    if (!definition.skillAbility.empty()) {
        return definition.skillAbility;
    }
    if (!definition.ability.empty()) {
        return definition.ability;
    }
    return definition.standardAbility.empty() ? std::string{kDefaultBossStandardAbilityId} : definition.standardAbility;
}

namespace {

constexpr float kActionValueEpsilon = 0.0001f;

std::string getCharacterStandardAbilityId(const CharacterDefinition& definition) {
    if (!definition.standardAbility.empty()) {
        return definition.standardAbility;
    }
    return getCharacterRegularAbilityId(definition);
}

int resolveAbilityOrbGain(const AbilityDefinition* ability) {
    if (ability == nullptr) {
        return 1;
    }

    if (ability->id == "LoveAndBeautyShock" ||
        ability->id == "CrescentBeam" ||
        ability->id == "VenusLoveMeChain") {
        return 0;
    }

    return std::max(0, ability->orbGain);
}

void grantUltimatePointForAction(BattleCharacter& character,
                                 const AbilityDefinition* ability,
                                 bool grantsUltimatePointOnAction) {
    if (!grantsUltimatePointOnAction) {
        return;
    }

    character.gainUltimatePoint(resolveAbilityOrbGain(ability));
}

bool bossAbilityUsesFocusedPartyPresentation(const AbilityDefinition& ability) {
    if (ability.targetRule != TargetRule::AllEnemies) {
        return false;
    }

    return ability.presentationId == "aesthetic_warning" ||
           ability.presentationId == "teto_baguette_attack" ||
           ability.presentationId == "pompom_dino_attack" ||
           ability.presentationId == "huafei_hostage_grab";
}

bool canPreviewActorExecuteQueuedAction(const BattleManager& manager,
                                        const BattleCharacter& previewCharacter,
                                        const TurnActor& previewActor) {
    if (!previewCharacter.isAlive()) {
        return false;
    }

    const std::string abilityId = manager.resolveCharacterAbilityId(
        previewActor.partyIndex,
        previewActor.extraTurnAction,
        previewActor.abilityKitOverride
    );
    if (abilityId.empty()) {
        return false;
    }

    if (previewActor.extraTurnAction == BattleAction::Ultimate) {
        return previewCharacter.canUseUltimate();
    }

    return true;
}

bool consumeInvalidPreviewCharacterTurn(const BattleManager& manager,
                                        const std::vector<BattleCharacter>& characters,
                                        TurnState& turnState,
                                        const TurnEvent& next) {
    if (!next.valid || next.actingActorIndex >= turnState.actors.size()) {
        return false;
    }

    const TurnActor& previewActor = turnState.actors[next.actingActorIndex];
    if (previewActor.type != ParticipantType::Character ||
        previewActor.partyIndex < 0 ||
        static_cast<size_t>(previewActor.partyIndex) >= characters.size()) {
        return false;
    }

    const BattleCharacter& previewCharacter = characters[static_cast<size_t>(previewActor.partyIndex)];
    if (!previewActor.isExtraTurn && previewCharacter.isAlive()) {
        return false;
    }

    if (previewActor.isExtraTurn &&
        canPreviewActorExecuteQueuedAction(manager, previewCharacter, previewActor)) {
        return false;
    }

    TurnEvent consumed = turn::advanceToNextTurnEvent(turnState);
    if (!consumed.valid || consumed.actingActorIndex >= turnState.actors.size()) {
        return false;
    }

    turnState.actors[consumed.actingActorIndex].currentActionValue =
        turnState.actors[consumed.actingActorIndex].baseActionValue;
    turnState.actors[consumed.actingActorIndex].priority = 0;
    return true;
}

TurnActor makePrimaryCharacterTurnActor(const BattleCharacter& character) {
    const CharacterDefinition& definition = character.definition();
    TurnActor actor;
    actor.type = ParticipantType::Character;
    actor.key = definition.key;
    actor.assetId = definition.assets;
    actor.title = definition.title;
    actor.partyIndex = character.partyIndex();
    actor.priority = 0;
    actor.isExtraTurn = false;
    actor.spd = character.effectiveSpd();
    actor.baseActionValue = turn::actionValueFromSpeed(actor.spd);
    actor.currentActionValue = actor.baseActionValue;
    return actor;
}

} // namespace

BattleCharacter::BattleCharacter(const CharacterDefinition& definition, int partyIndex)
    : definition_(definition)
    , partyIndex_(partyIndex)
    , hp_(definition.hp)
    , ultimateCharge_(std::clamp(definition.startingOrbs, 0, std::max(1, definition.ultimatePoints)))
    , shield_(0)
{}

const CharacterDefinition& BattleCharacter::definition() const {
    return definition_;
}

int BattleCharacter::partyIndex() const {
    return partyIndex_;
}

int BattleCharacter::hp() const {
    return hp_;
}

int BattleCharacter::maxHp() const {
    return definition_.hp;
}

bool BattleCharacter::isAlive() const {
    return hp_ > 0;
}

void BattleCharacter::receiveDamage(int amount) {
    int dmg = std::max(0, amount);
    if (shield_ > 0) {
        int absorbed = std::min(shield_, dmg);
        shield_ -= absorbed;
        dmg -= absorbed;
    }
    if (dmg > 0) {
        hp_ -= dmg;
        if (hp_ < 0) hp_ = 0;
    }
}

void BattleCharacter::receiveHealing(int amount) {
    if (!isAlive()) {
        return;
    }
    hp_ += std::max(0, amount);
    if (hp_ > definition_.hp) {
        hp_ = definition_.hp;
    }
}

void BattleCharacter::revive(int amount) {
    const int restoredHp = std::max(0, amount);
    if (restoredHp <= 0) {
        return;
    }
    if (isAlive()) {
        receiveHealing(restoredHp);
        return;
    }
    hp_ = std::min(definition_.hp, std::max(1, restoredHp));
}

int BattleCharacter::ultimateCharge() const {
    return ultimateCharge_;
}

void BattleCharacter::gainUltimatePoint(int amount) {
    ultimateCharge_ = std::clamp(
        ultimateCharge_ + std::max(0, amount),
        0,
        std::max(1, definition_.ultimatePoints)
    );
}

bool BattleCharacter::canUseSkill() const {
    return !definition_.ability.empty() || !definition_.skillAbility.empty() || !definition_.standardAbility.empty();
}

bool BattleCharacter::canUseUltimate() const {
    return ultimateCharge_ >= std::max(1, definition_.ultimatePoints);
}

void BattleCharacter::consumeUltimatePoint(int amount) {
    ultimateCharge_ = std::max(0, ultimateCharge_ - std::max(0, amount));
}

void BattleCharacter::consumeUltimate() {
    ultimateCharge_ = 0;
}

int BattleCharacter::effectiveSpd() const {
    return std::max(1, definition_.spd + spdBuffBonus_);
}

int BattleCharacter::spdBuffBonus() const {
    return spdBuffBonus_;
}

void BattleCharacter::setSpdBuffBonus(int amount) {
    spdBuffBonus_ = amount;
}

int BattleCharacter::effectiveAtk() const {
    // atkBuffBonus_ is a signed percentage (e.g. 50 = +50%, -20 = -20%)
    const float scaled = static_cast<float>(definition_.atk) * (1.0f + static_cast<float>(atkBuffBonus_) / 100.0f);
    return std::max(0, static_cast<int>(scaled));
}

int BattleCharacter::atkBuffBonus() const {
    return atkBuffBonus_;
}

/**
 * @brief Sets the character's attack modifier as a signed percentage.
 *
 * Positive values increase the character's attack, negative values decrease it.
 *
 * @param percentBonus Signed percentage to apply to the character's base attack.
 */
void BattleCharacter::setAtkBuffBonus(int percentBonus) {
    atkBuffBonus_ = percentBonus;
}

int BattleCharacter::damageBuffBonus() const {
    return damageBuffBonus_;
}

float BattleCharacter::damageBuffMultiplier() const {
    return std::max(0.0f, 1.0f + (static_cast<float>(damageBuffBonus_) / 100.0f));
}

void BattleCharacter::setDamageBuffBonus(int percentBonus) {
    damageBuffBonus_ = percentBonus;
}

/**
 * @brief Initializes the battle using the specified boss identifier and party members.
 *
 * Constructs a battle configuration with both the battle key and boss key set to @p bossKey,
 * applies @p progression to the provided characters, and initializes runtime battle state for the resulting party.
 *
 * @param bossKey Identifier of the boss definition to load.
 * @param characterKeys Identifiers of character definitions to include in the party.
 * @param progression Player progression values applied to each character during loading.
 * @return true if initialization completed successfully, false otherwise.
 */
bool BattleManager::initialize(const std::string& bossKey,
                               const std::vector<std::string>& characterKeys,
                               const PlayerProgression& progression) {
    BattleDefinition battleDefinition;
    battleDefinition.key = bossKey;
    battleDefinition.bossKey = bossKey;
    return initialize(battleDefinition, characterKeys, progression);
}

/**
 * @brief Initializes the battle manager with a battle definition, party composition, and player progression.
 *
 * Loads and validates the boss and character definitions, applies progression bonuses to each character,
 * resets and populates runtime battle state (boss HP, phase/ultimate tracking, characters, turn state, abilities, presentations, telemetry, and related runtime structures).
 *
 * @param battleDefinition Definition describing the battle to initialize (must include a valid boss key).
 * @param characterKeys Ordered list of character definition keys representing the player party.
 * @param progression Player progression data applied to each loaded character prior to building the party.
 * @return bool `true` if initialization completed successfully and the manager is ready; `false` if required data is missing or any load/build step fails (e.g., missing boss key, failing to load boss or any character, or failing to build the initial turn state).
 */
bool BattleManager::initialize(const BattleDefinition& battleDefinition,
                               const std::vector<std::string>& characterKeys,
                               const PlayerProgression& progression) {
    initialized_ = false;
    battleDefinition_ = battleDefinition;
    state_ = BattleState{};
    turnState_ = TurnState{};
    bossCurrentHp_ = 0;
    bossAtkBuffBonus_ = 0;
    bossPhaseIndex_ = 0;
    pendingBossPhaseTransition_.reset();
    bossUltimateCharge_ = 0;
    characters_.clear();
    recentActionEvents_.clear();
    telemetry_ = BattleTelemetry{};
    simulatedActions_ = 0;
    activePartyBuffs_.clear();
    activeBossDebuffs_.clear();
    activeCharacterAbilityKits_.clear();
    bossAbilityUseCounts_.clear();
    nextManualUltimatePriority_ = 1000;
    luotianyiCorrectTones_ = 0;
    comboState_ = BattleComboState{};
    forcedOutcome_.reset();
    currentActionOutgoingDamage_ = 0;
    pendingSplitAttackActorKey_.clear();
    tetoHealingTally_ = 0;
    sailorVenusState_ = SailorVenusState{};

    if (battleDefinition_.bossKey.empty()) {
        std::cerr << "[Battle] Missing boss key.\n";
        return false;
    }

    if (!loader::loadBossDefinition(battleDefinition_.bossKey, state_.boss)) {
        return false;
    }

    state_.party.reserve(characterKeys.size());
    for (const std::string& key : characterKeys) {
        CharacterDefinition character;
        if (!loader::loadCharacterDefinition(key, character)) {
            return false;
        }
        applyCharacterProgressionBonuses(character, progression);
        state_.party.push_back(character);
    }

    bossCurrentHp_ = state_.boss.hp;
    bossAtkBuffBonus_ = currentBossPhaseDefinition().atkBonusPercent;
    bossUltimateCharge_ = std::clamp(state_.boss.startingOrbs, 0, std::max(1, state_.boss.ultimatePoints));
    activeCharacterAbilityKits_.assign(state_.party.size(), "");
    for (size_t i = 0; i < state_.party.size(); ++i) {
        characters_.emplace_back(state_.party[i], static_cast<int>(i));
        if (state_.party[i].key == "sailorVenus") {
            sailorVenusState_.partyIndex = static_cast<int>(i);
        }
    }

    if (!buildInitialTurnState()) {
        return false;
    }

    if (!loader::loadAllAbilities(abilities_)) {
        std::cerr << "[Battle] Warning: Failed to load abilities.\n";
    }

    registerAllPresentations();

    initialized_ = true;
    return true;
}

void BattleManager::printBattleSummary() const {
    if (!initialized_) {
        std::cerr << "[Battle] Battle not initialized.\n";
        return;
    }

    std::cout << "===== Battle Initialized =====\n";
    std::cout << "Boss:\n";
    std::cout << "  key: " << state_.boss.key << "\n";
    std::cout << "  title: " << state_.boss.title << "\n";
    std::cout << "  assets: " << state_.boss.assets << "\n";
    std::cout << "  spd: " << state_.boss.spd << " | atk: " << state_.boss.atk << " | hp: " << state_.boss.hp << "\n";
    std::cout << "  standard: " << state_.boss.standardAbility << "\n";
    std::cout << "  skill: " << state_.boss.skillAbility << "\n";
    std::cout << "  ultimate: " << state_.boss.ultimate << " (points: " << state_.boss.ultimatePoints << ")\n\n";

    std::cout << "Party (" << state_.party.size() << "):\n";
    for (size_t i = 0; i < state_.party.size(); ++i) {
        const CharacterDefinition& c = state_.party[i];
        std::cout << "  [" << (i + 1) << "] key: " << c.key << "\n";
        std::cout << "      title: " << c.title << "\n";
        std::cout << "      assets: " << c.assets << "\n";
        std::cout << "      class: " << c.characterClass << "\n";
        std::cout << "      spd: " << c.spd << " | atk: " << c.atk << " | hp: " << c.hp << "\n";
        std::cout << "      standard: " << c.standardAbility << "\n";
        std::cout << "      skill: " << c.skillAbility << "\n";
        std::cout << "      ultimate: " << c.ultimate << " (points: " << c.ultimatePoints << ")\n";
    }

    std::cout << "\nTurn Actors (initial action values):\n";
    for (size_t i = 0; i < turnState_.actors.size(); ++i) {
        const TurnActor& actor = turnState_.actors[i];
        std::cout << "  [" << i << "] "
                  << (actor.type == ParticipantType::Boss ? "Boss" : "Character")
                  << " - " << actor.title
                  << " | priority: " << actor.priority
                  << " | extra: " << (actor.isExtraTurn ? "yes" : "no")
                  << " | spd: " << actor.spd
                  << " | baseAV: " << actor.baseActionValue
                  << " | currentAV: " << actor.currentActionValue
                  << "\n";
    }

    std::cout << "\nPseudo battle log:\n";
    BattleManager simulation = *this;
    simulation.runPseudoBattle(25);
}

const BattleState& BattleManager::getBattleState() const {
    return state_;
}

const BattleDefinition& BattleManager::getBattleDefinition() const {
    return battleDefinition_;
}

const BattleSpecialRules& BattleManager::getSpecialRules() const {
    return battleDefinition_.specialRules;
}

const TurnState& BattleManager::getTurnState() const {
    return turnState_;
}

int BattleManager::getPreviewNextActorIndex() const {
    const TurnEvent event = peekNextTurnEvent();
    if (!event.valid) {
        return -1;
    }
    return static_cast<int>(event.actingActorIndex);
}

int BattleManager::getBossCurrentHp() const {
    return bossCurrentHp_;
}

/**
 * @brief Retrieves the boss's maximum hit points.
 *
 * @return int The boss's configured maximum HP.
 */
int BattleManager::getBossMaxHp() const {
    return state_.boss.hp;
}

/**
 * @brief Computes the boss's effective attack after applying the boss attack buff percentage.
 *
 * Calculates boss attack as boss base attack multiplied by (1 + bossAtkBuffBonus_ / 100),
 * and clamps the result to be at least 0.
 *
 * @return int The boss's effective attack value (>= 0).
 */
int BattleManager::getBossEffectiveAtk() const {
    const float scaled = static_cast<float>(state_.boss.atk) *
        (1.0f + static_cast<float>(bossAtkBuffBonus_) / 100.0f);
    return std::max(0, static_cast<int>(scaled));
}

/**
 * @brief Provides the current boss phase index.
 *
 * @return int The current boss phase index (0-based).
 */
int BattleManager::getBossPhaseIndex() const {
    return bossPhaseIndex_;
}

/**
 * @brief Retrieves and clears any pending boss phase transition.
 *
 * If a boss phase transition was queued, returns that transition and removes it
 * from the pending state; otherwise returns an empty optional.
 *
 * @return std::optional<BossPhaseTransition> The pending transition if present, `std::nullopt` otherwise.
 */
std::optional<BossPhaseTransition> BattleManager::consumeBossPhaseTransition() {
    const std::optional<BossPhaseTransition> transition = pendingBossPhaseTransition_;
    pendingBossPhaseTransition_.reset();
    return transition;
}

/**
 * @brief Retrieves the background music identifier currently active for the boss.
 *
 * If the current boss phase defines a BGM override, that identifier is returned;
 * otherwise the boss's base BGM identifier from the battle state is returned.
 *
 * @return std::string The BGM identifier to use for the current boss (phase-specific if present, otherwise the boss default).
 */
std::string BattleManager::getCurrentBossBgm() const {
    const BossDefinition::PhaseDefinition& phase = currentBossPhaseDefinition();
    if (!phase.bgm.empty()) {
        return phase.bgm;
    }
    return state_.boss.bgm;
}

/**
 * @brief Gets the boss background-music volume for the currently active boss phase.
 *
 * @return float The phase-specific BGM volume if defined for the current boss phase; otherwise the boss definition's default BGM volume.
 */
float BattleManager::getCurrentBossBgmVolume() const {
    const BossDefinition::PhaseDefinition& phase = currentBossPhaseDefinition();
    if (phase.bgmVolume.has_value()) {
        return *phase.bgmVolume;
    }
    return state_.boss.bgmVolume;
}

/**
 * @brief Current ultimate charge of the boss.
 *
 * @return int The boss's current ultimate charge (0 or greater).
 */
int BattleManager::getBossUltimateCharge() const {
    return bossUltimateCharge_;
}

int BattleManager::getBossUltimateRequired() const {
    return std::max(1, state_.boss.ultimatePoints);
}

int BattleManager::getCharacterCurrentHp(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return 0;
    }
    return characters_[static_cast<size_t>(partyIndex)].hp();
}

int BattleManager::getCharacterMaxHp(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return 1;
    }
    return characters_[static_cast<size_t>(partyIndex)].maxHp();
}

int BattleManager::getCharacterEffectiveAtk(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return 0;
    }
    return characters_[static_cast<size_t>(partyIndex)].effectiveAtk();
}

float BattleManager::getCharacterDamageBuffMultiplier(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return 1.0f;
    }
    return characters_[static_cast<size_t>(partyIndex)].damageBuffMultiplier();
}

bool BattleManager::isCharacterAlive(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return false;
    }
    return characters_[static_cast<size_t>(partyIndex)].isAlive();
}

/**
 * @brief Attempts to revive the party member at the given index and update turn participation.
 *
 * Revives the character by the specified amount (minimum restored HP determined by the character definition),
 * and, if the character's HP increases, synchronizes that character's turn participation and ultimate-turn queue.
 *
 * @param partyIndex Index of the party member to revive; must be within party bounds.
 * @param amount Amount of HP to restore (treated as non-negative).
 * @return true if the character's HP increased as a result of the revive, false if the index is invalid or no HP was restored.
 */
bool BattleManager::reviveCharacter(int partyIndex, int amount) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return false;
    }

    BattleCharacter& character = characters_[static_cast<size_t>(partyIndex)];
    const int hpBefore = character.hp();
    character.revive(amount);
    if (character.hp() <= hpBefore) {
        return false;
    }

    syncCharacterTurnParticipation(partyIndex);
    syncCharacterUltimateTurn(partyIndex);
    return true;
}

/**
 * @brief Determines whether a manual ultimate turn can be queued for the specified party member.
 *
 * Validates the battle state and the target character, then reports whether queuing an extra-turn
 * ultimate is possible, already queued, unavailable, or blocked by the ultimate meter.
 *
 * @param partyIndex Zero-based index of the party member to check.
 * @return ManualUltimateRequestResult `Queued` if the ultimate can be queued,
 * `Unavailable` if the index is invalid, the battle is over, the character is dead, or the character has no ultimate configured,
 * `AlreadyQueued` if an extra-turn ultimate for that character is already queued,
 * `MeterNotReady` if the character's ultimate meter is not full.
 */
ManualUltimateRequestResult BattleManager::previewManualUltimateTurnRequest(int partyIndex) const {
    if (isBattleOver() ||
        partyIndex < 0 ||
        static_cast<size_t>(partyIndex) >= characters_.size()) {
        return ManualUltimateRequestResult::Unavailable;
    }

    const BattleCharacter& character = characters_[static_cast<size_t>(partyIndex)];
    if (!character.isAlive() ||
        resolveCharacterAbilityId(partyIndex, BattleAction::Ultimate).empty()) {
        return ManualUltimateRequestResult::Unavailable;
    }

    if (hasQueuedExtraTurn(partyIndex, BattleAction::Ultimate, false)) {
        return ManualUltimateRequestResult::AlreadyQueued;
    }

    if (!character.canUseUltimate()) {
        return ManualUltimateRequestResult::MeterNotReady;
    }

    return ManualUltimateRequestResult::Queued;
}

/**
 * @brief Requests queuing a manual ultimate extra turn for the specified party member.
 *
 * If the request is accepted, an extra-turn ultimate actor is queued for that character.
 *
 * @param partyIndex Index of the party member to request the manual ultimate for.
 * @return ManualUltimateRequestResult `Queued` if the ultimate turn was queued;
 *         `Unavailable` if the battle state or character prevents manual ultimates;
 *         `AlreadyQueued` if an equivalent extra-turn ultimate is already queued for that member;
 *         `MeterNotReady` if the character's ultimate meter is not ready.
 */
ManualUltimateRequestResult BattleManager::requestManualUltimateTurn(int partyIndex) {
    const ManualUltimateRequestResult result = previewManualUltimateTurnRequest(partyIndex);
    if (result != ManualUltimateRequestResult::Queued) {
        return result;
    }

    queueExtraTurnForCharacter(partyIndex);
    return ManualUltimateRequestResult::Queued;
}

/**
 * @brief Retrieves the current ultimate charge for a party member.
 *
 * Returns the character's stored ultimate charge value, or `0` if the provided
 * `partyIndex` is negative or outside the current party bounds.
 *
 * @param partyIndex Index of the party member to query.
 * @return int The character's ultimate charge, or `0` for an invalid index.
 */
int BattleManager::getCharacterUltimateCharge(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return 0;
    }
    return characters_[static_cast<size_t>(partyIndex)].ultimateCharge();
}

int BattleManager::getCharacterUltimateRequired(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return 1;
    }
    return std::max(1, characters_[static_cast<size_t>(partyIndex)].definition().ultimatePoints);
}

bool BattleManager::setCharacterAbilityKit(int partyIndex, const std::string& kitId) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= state_.party.size()) {
        return false;
    }

    if (kitId.empty()) {
        clearCharacterAbilityKit(partyIndex);
        return true;
    }

    const CharacterDefinition& definition = state_.party[static_cast<size_t>(partyIndex)];
    if (definition.abilityKits.find(kitId) == definition.abilityKits.end()) {
        return false;
    }

    activeCharacterAbilityKits_[static_cast<size_t>(partyIndex)] = kitId;
    syncCharacterTurnParticipation(partyIndex);
    syncCharacterUltimateTurn(partyIndex);
    refreshTurnActorAssets(partyIndex);
    return true;
}

void BattleManager::clearCharacterAbilityKit(int partyIndex) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= activeCharacterAbilityKits_.size()) {
        return;
    }

    activeCharacterAbilityKits_[static_cast<size_t>(partyIndex)].clear();
    syncCharacterTurnParticipation(partyIndex);
    syncCharacterUltimateTurn(partyIndex);
    refreshTurnActorAssets(partyIndex);
}

std::string BattleManager::resolveCharacterAbilityId(int partyIndex,
                                                     BattleAction action,
                                                     const std::string& turnAbilityKitOverride) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= state_.party.size()) {
        return {};
    }

    const std::string activeKitId =
        static_cast<size_t>(partyIndex) < activeCharacterAbilityKits_.size()
        ? activeCharacterAbilityKits_[static_cast<size_t>(partyIndex)]
        : std::string{};
    return resolveCharacterAbilityId(
        state_.party[static_cast<size_t>(partyIndex)],
        action,
        activeKitId,
        turnAbilityKitOverride
    );
}

std::string BattleManager::resolveCharacterAssetId(int partyIndex,
                                                   const std::string& turnAbilityKitOverride) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= state_.party.size()) {
        return {};
    }

    const std::string activeKitId =
        static_cast<size_t>(partyIndex) < activeCharacterAbilityKits_.size()
        ? activeCharacterAbilityKits_[static_cast<size_t>(partyIndex)]
        : std::string{};
    return resolveCharacterAssetId(
        state_.party[static_cast<size_t>(partyIndex)],
        activeKitId,
        turnAbilityKitOverride
    );
}

std::string BattleManager::resolveCharacterVoiceAssetId(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= state_.party.size()) {
        return {};
    }

    const CharacterDefinition& definition = state_.party[static_cast<size_t>(partyIndex)];
    if (!definition.voiceAssetId.empty()) {
        return definition.voiceAssetId;
    }
    if (!definition.assets.empty()) {
        return definition.assets;
    }
    return definition.key;
}

void BattleManager::addSailorVenusSpaceTally(int partyIndex, int amount) {
    if (amount <= 0 ||
        !isSailorVenusTransformed(partyIndex)) {
        return;
    }

    sailorVenusState_.spaceTally += amount;
}

void BattleManager::addLuotianyiCorrectTones(int amount) {
    if (amount <= 0) {
        return;
    }
    luotianyiCorrectTones_ += amount;
}

/**
 * @brief Retrieves the number of correctly played tones for Luotianyi.
 *
 * The returned value is the stored count clamped to a minimum of 0.
 *
 * @return int Non-negative count of correctly played tones for Luotianyi.
 */
int BattleManager::getLuotianyiCorrectTones() const {
    return std::max(0, luotianyiCorrectTones_);
}

/**
 * @brief Retrieves the current combo tracking state for the battle.
 *
 * @return const BattleComboState& The current combo state containing combo count and damage bonus fraction.
 */
const BattleComboState& BattleManager::getComboState() const {
    return comboState_;
}

/**
 * @brief Updates combo state from a presentation feedback event and applies any resulting effects.
 *
 * Processes the given presentation feedback when it is valid and marked combo-eligible:
 * - Classifies the judgement and increments or resets the combo counter accordingly.
 * - On a combo break, computes and applies heal-per-ally to all alive characters (if > 0) and synchronizes turn participation.
 * - Refreshes derived combo state values (e.g., damage bonus fraction).
 *
 * @param feedback Presentation feedback to evaluate; only processed when `feedback.valid()` and `feedback.comboEligible` are true.
 * @return ComboResolution Result object describing whether the feedback was applied, the judgement, the previous and resulting combo counts,
 *         whether a combo was broken, heal-per-ally applied on break, and the updated damage bonus fraction.
 */
ComboResolution BattleManager::applyPresentationFeedback(bool isBossCaster,
                                                         const PresentationFeedbackEvent& feedback) {
    ComboResolution result;
    (void)isBossCaster;
    if (!feedback.valid() || !feedback.comboEligible) {
        return result;
    }

    result.applied = true;
    result.judgement = classifyCombatJudgement(feedback.signal);
    result.previousComboCount = comboState_.comboCount;

    if (judgementNaturallyIncreasesCombo(result.judgement)) {
        ++comboState_.comboCount;
    } else {
        result.brokeCombo = comboState_.comboCount > 0;
        result.comboBreakHealPerAlly = comboBreakHealPerAlly(comboState_.comboCount);
        if (result.comboBreakHealPerAlly > 0) {
            bool healed = false;
            for (BattleCharacter& character : characters_) {
                if (!character.isAlive()) {
                    continue;
                }
                character.receiveHealing(result.comboBreakHealPerAlly);
                healed = true;
            }
            if (healed) {
                syncAllCharacterTurnParticipation();
            }
        }
        comboState_.comboCount = 0;
    }

    refreshComboState();
    result.comboCount = comboState_.comboCount;
    result.damageBonusFraction = comboState_.damageBonusFraction;
    return result;
}

/**
 * @brief Determine the battle's resolved outcome from current state.
 *
 * @return The forced outcome if present; otherwise `Victory` if the boss's current HP is less than or equal to 0, `Defeat` if no party members are alive, or `None` if the battle is still ongoing.
 */
BattleResolvedOutcome BattleManager::computeDerivedOutcome() const {
    if (forcedOutcome_.has_value()) {
        return *forcedOutcome_;
    }

    if (bossCurrentHp_ <= 0) {
        return BattleResolvedOutcome::Victory;
    }

    return firstLivingCharacterPartyIndex() < 0
        ? BattleResolvedOutcome::Defeat
        : BattleResolvedOutcome::None;
}

BattleResolvedOutcome BattleManager::outcome() const {
    return computeDerivedOutcome();
}

/**
 * @brief Indicates whether player damage should heal the boss.
 *
 * @return `true` if damage dealt by player characters heals the boss, `false` otherwise.
 */
bool BattleManager::playerDamageHealsBoss() const {
    return battleDefinition_.specialRules.playerDamageHealsBoss;
}

/**
 * @brief Accesses the accumulated telemetry for the current battle.
 *
 * @return const BattleTelemetry& Collected telemetry data (read-only) for the active battle.
 */
const BattleTelemetry& BattleManager::getBattleTelemetry() const {
    return telemetry_;
}

/**
 * @brief Applies damage to the boss, reducing its current HP and initiating any required phase transition.
 *
 * Decreases bossCurrentHp_ by the specified amount (clamped at zero). If `amount` is less than or equal to zero
 * or the boss is already at zero HP, no change is performed. After applying damage, evaluates and applies a boss
 * phase transition if the new HP warrants one.
 *
 * @param amount Damage to apply to the boss; values less than or equal to zero are ignored.
 */
void BattleManager::applyBossDamage(int amount) {
    if (amount <= 0 || bossCurrentHp_ <= 0) {
        return;
    }
    bossCurrentHp_ = std::max(0, bossCurrentHp_ - amount);
    applyBossPhaseTransitionIfNeeded();
}

/**
 * @brief Heals the boss by the specified amount, clamped to the boss's maximum HP.
 *
 * If `amount` is less than or equal to zero or the boss is already at zero HP, the call has no effect.
 *
 * @param amount Amount of HP to restore to the boss; values <= 0 are ignored. The boss's HP will not exceed its defined maximum.
 */
void BattleManager::applyBossHealing(int amount) {
    if (amount <= 0 || bossCurrentHp_ <= 0) {
        return;
    }
    bossCurrentHp_ = std::min(state_.boss.hp, bossCurrentHp_ + amount);
}

/**
 * @brief Applies player-originating offense to the boss, recording the outgoing damage and
 *        either healing or damaging the boss according to special rules.
 *
 * If `amount` is less than or equal to zero this call does nothing. The function
 * increments the manager's outgoing-damage counter for the current action; if the
 * special rule `playerDamageHealsBoss()` is active the boss is healed by `amount`,
 * otherwise the boss takes `amount` damage (which may trigger phase transitions).
 *
 * @param amount Positive damage amount to apply; values less than or equal to zero are ignored.
 */
void BattleManager::applyPlayerOffenseToBoss(int amount) {
    if (amount <= 0) {
        return;
    }

    currentActionOutgoingDamage_ += amount;

    if (playerDamageHealsBoss()) {
        applyBossHealing(amount);
        return;
    }

    applyBossDamage(amount);
}

void BattleManager::addToTetoHealingTally(int amount) {
    if (amount <= 0 || findCharacterPartyIndexByKey("teto") < 0) {
        return;
    }

    tetoHealingTally_ += amount;
}

int BattleManager::resolveSupportTargetPartyIndex(TargetRule targetRule, int requestedTargetPartyIndex) const {
    if (targetRule != TargetRule::SingleAlly) {
        return -1;
    }

    if (requestedTargetPartyIndex >= 0 &&
        static_cast<size_t>(requestedTargetPartyIndex) < characters_.size() &&
        characters_[static_cast<size_t>(requestedTargetPartyIndex)].isAlive()) {
        return requestedTargetPartyIndex;
    }

    int bestPartyIndex = -1;
    int bestHp = std::numeric_limits<int>::max();
    for (const BattleCharacter& character : characters_) {
        if (!character.isAlive()) {
            continue;
        }
        if (character.hp() < bestHp) {
            bestHp = character.hp();
            bestPartyIndex = character.partyIndex();
        }
    }

    return bestPartyIndex;
}

int BattleManager::applyHealingToTargets(int perTargetHeal,
                                         TargetRule targetRule,
                                         int requestedTargetPartyIndex,
                                         bool reviveDeadAllies,
                                         bool recordForTetoTally,
                                         bool* outAnyTargetResolved,
                                         bool* outAnyActualHpChange) {
    if (outAnyTargetResolved != nullptr) {
        *outAnyTargetResolved = false;
    }
    if (outAnyActualHpChange != nullptr) {
        *outAnyActualHpChange = false;
    }
    if (perTargetHeal <= 0) {
        return 0;
    }

    int totalRequestedHealing = 0;
    const auto applyToTarget = [&](BattleCharacter& target) {
        const int hpBefore = target.hp();
        bool targetResolved = false;
        bool actualHpChange = false;

        if (target.isAlive()) {
            target.receiveHealing(perTargetHeal);
            targetResolved = true;
            actualHpChange = target.hp() > hpBefore;
            totalRequestedHealing += perTargetHeal;
        } else if (reviveDeadAllies && perTargetHeal > 0) {
            target.revive(perTargetHeal);
            targetResolved = true;
            actualHpChange = target.hp() > hpBefore;
            totalRequestedHealing += perTargetHeal;
        }

        if (targetResolved && outAnyTargetResolved != nullptr) {
            *outAnyTargetResolved = true;
        }
        if (actualHpChange && outAnyActualHpChange != nullptr) {
            *outAnyActualHpChange = true;
        }
    };

    if (targetRule == TargetRule::AllAllies) {
        for (BattleCharacter& target : characters_) {
            applyToTarget(target);
        }
    } else if (targetRule == TargetRule::SingleAlly) {
        const int targetPartyIndex = resolveSupportTargetPartyIndex(targetRule, requestedTargetPartyIndex);
        if (targetPartyIndex >= 0 && static_cast<size_t>(targetPartyIndex) < characters_.size()) {
            applyToTarget(characters_[static_cast<size_t>(targetPartyIndex)]);
        }
    }

    if (recordForTetoTally && totalRequestedHealing > 0) {
        addToTetoHealingTally(totalRequestedHealing);
    }

    return totalRequestedHealing;
}

int BattleManager::applyShieldToTargets(int perTargetShield,
                                        TargetRule targetRule,
                                        int requestedTargetPartyIndex) {
    if (perTargetShield <= 0) {
        return 0;
    }

    int applications = 0;
    if (targetRule == TargetRule::AllAllies) {
        for (BattleCharacter& target : characters_) {
            if (!target.isAlive()) {
                continue;
            }
            target.addShield(perTargetShield);
            ++applications;
        }
    } else if (targetRule == TargetRule::SingleAlly) {
        const int targetPartyIndex = resolveSupportTargetPartyIndex(targetRule, requestedTargetPartyIndex);
        if (targetPartyIndex >= 0 && static_cast<size_t>(targetPartyIndex) < characters_.size()) {
            BattleCharacter& target = characters_[static_cast<size_t>(targetPartyIndex)];
            if (target.isAlive()) {
                target.addShield(perTargetShield);
                ++applications;
            }
        }
    }

    return applications;
}

int BattleManager::currentLivingPartyShieldTotal() const {
    int totalShield = 0;
    for (const BattleCharacter& character : characters_) {
        if (!character.isAlive()) {
            continue;
        }
        totalShield += std::max(0, character.getShield());
    }
    return totalShield;
}

int BattleManager::applyConvertedPlayerSpecialDamageToBoss(int rawAmount,
                                                           float abilityMultiplier,
                                                           bool markPresentationResolved,
                                                           int sourcePartyIndex) {
    if (isBattleOver()) {
        return 0;
    }

    if (rawAmount <= 0 || abilityMultiplier <= 0.0f) {
        return 0;
    }

    if (markPresentationResolved) {
        presentationHitDamageApplied_ = true;
    }

    const int finalDamage = std::max(
        1,
        static_cast<int>(std::lround(
            static_cast<float>(rawAmount) *
            abilityMultiplier *
            getCharacterDamageBuffMultiplier(sourcePartyIndex) *
            comboDamageMultiplier(comboState_.comboCount)
        )));
    applyPlayerOffenseToBoss(finalDamage);
    return finalDamage;
}

int BattleManager::applyCurrentTeamShieldDamageToBoss(bool markPresentationResolved,
                                                      float abilityMultiplier,
                                                      int sourcePartyIndex) {
    const int totalShield = currentLivingPartyShieldTotal();
    return applyConvertedPlayerSpecialDamageToBoss(
        totalShield,
        abilityMultiplier,
        markPresentationResolved,
        sourcePartyIndex
    );
}

int BattleManager::getTetoHealingTally() const {
    return std::max(0, tetoHealingTally_);
}

int BattleManager::consumeTetoHealingTally() {
    const int tally = getTetoHealingTally();
    tetoHealingTally_ = 0;
    return tally;
}

/**
 * @brief Normalizes combo count and updates the corresponding damage bonus fraction.
 *
 * Ensures `comboState_.comboCount` is at least 0, then recomputes
 * `comboState_.damageBonusFraction` using `comboDamageBonusFraction(comboState_.comboCount)`.
 */
void BattleManager::refreshComboState() {
    comboState_.comboCount = std::max(0, comboState_.comboCount);
    comboState_.damageBonusFraction = comboDamageBonusFraction(comboState_.comboCount);
}

/**
 * @brief Revives defeated party members when the battle rule enables auto-revive on boss damage.
 *
 * If the special rule `autoRevivePartyOnBossDamage` is enabled, revives each dead party
 * member either to full HP or to 1 HP depending on `revivePartyToFull`. After reviving,
 * updates turn participation and ultimate-turn queue state for all characters.
 */
void BattleManager::reviveDefeatedPartyMembersIfNeeded() {
    if (!battleDefinition_.specialRules.autoRevivePartyOnBossDamage) {
        return;
    }

    for (BattleCharacter& character : characters_) {
        if (character.isAlive()) {
            continue;
        }

        const int reviveAmount = battleDefinition_.specialRules.revivePartyToFull
            ? character.maxHp()
            : 1;
        character.revive(reviveAmount);
    }

    syncAllCharacterTurnParticipation();
}

void BattleManager::applyBossAbilitySelfCost(const AbilityDefinition& ability) {
    if (ability.selfHpCostPercentOfMax <= 0.0f || bossCurrentHp_ <= 0 || state_.boss.hp <= 0) {
        return;
    }

    const std::string usageKey = ability.id.empty() ? ability.name : ability.id;
    const int priorUses =
        usageKey.empty() ? 0 : bossAbilityUseCounts_[usageKey];
    float effectivePercent =
        std::max(0.0f, ability.selfHpCostPercentOfMax) +
        (std::max(0.0f, ability.selfHpCostPercentIncreasePerUse) * static_cast<float>(priorUses));
    if (ability.selfHpCostPercentMax > 0.0f) {
        effectivePercent = std::min(effectivePercent, ability.selfHpCostPercentMax);
    }

    const float clampedPercent = std::max(0.0f, effectivePercent);
    const int selfDamage = std::max(
        1,
        static_cast<int>(std::lround((clampedPercent / 100.0f) * static_cast<float>(state_.boss.hp)))
    );
    applyBossDamage(selfDamage);
    if (!usageKey.empty()) {
        bossAbilityUseCounts_[usageKey] = priorUses + 1;
    }
    if (bossCurrentHp_ <= 0 && battleDefinition_.specialRules.bossSelfKnockoutIsDefeat) {
        forcedOutcome_ = BattleResolvedOutcome::Defeat;
    }
}

/**
 * @brief Applies presentation-driven hit damage to the boss or player characters.
 *
 * Applies damage produced by a presentation event. When the caster is the boss, damage is
 * delivered to a specific party member if `targetPartyIndex` is valid; otherwise it is
 * applied to all alive characters. When the caster is a player character, damage is
 * applied to the boss and scaled by the current combo multiplier. On successful application
 * the function synchronizes turn participation/ultimate queues for affected characters,
 * triggers revive/cleanup rules, and marks the presentation hit-as-damage flag.
 *
 * @param isBossCaster True if the boss is the source of the damage; false if a character is.
 * @param perHitDamage Base damage per hit (values ≤ 0 are ignored).
 * @param hitEvents Number of hit events (values ≤ 0 are ignored).
 * @param targetPartyIndex If >= 0 and valid, the zero-based party index to target when the boss
 *        is the caster; otherwise targets all alive characters.
 * @param sourcePartyIndex Party index of the player caster when known; currently informational.
 */
void BattleManager::applyPresentationHitDamage(bool isBossCaster,
                                               int perHitDamage,
                                               int hitEvents,
                                               int targetPartyIndex,
                                               int sourcePartyIndex) {
    if (perHitDamage <= 0 || hitEvents <= 0 || isBattleOver()) {
        return;
    }

    int totalDamage = std::max(1, perHitDamage) * std::max(1, hitEvents);
    if (isBossCaster) {
        if (targetPartyIndex >= 0 && static_cast<size_t>(targetPartyIndex) < characters_.size()) {
            BattleCharacter& target = characters_[static_cast<size_t>(targetPartyIndex)];
            if (!target.isAlive()) {
                return;
            }

            target.receiveDamage(totalDamage);
            syncCharacterTurnParticipation(targetPartyIndex);
            syncCharacterUltimateTurn(targetPartyIndex);
            reviveDefeatedPartyMembersIfNeeded();
            removeBuffsFromDefeatedCharacters();
            presentationHitDamageApplied_ = true;
            return;
        }

        bool applied = false;
        for (BattleCharacter& c : characters_) {
            if (!c.isAlive()) {
                continue;
            }
            c.receiveDamage(totalDamage);
            applied = true;
        }
        if (applied) {
            syncAllCharacterTurnParticipation();
            reviveDefeatedPartyMembersIfNeeded();
            removeBuffsFromDefeatedCharacters();
            presentationHitDamageApplied_ = true;
        }
        return;
    }

    if (bossCurrentHp_ > 0) {
        (void)sourcePartyIndex;
        applyPlayerOffenseToBoss(totalDamage);
        presentationHitDamageApplied_ = true;
    }
}

int BattleManager::applyPresentationHealing(bool isBossCaster,
                                            int perHitHeal,
                                            int hitEvents,
                                            TargetRule targetRule,
                                            int targetPartyIndex,
                                            bool reviveDeadAllies,
                                            bool recordForTetoTally) {
    if (perHitHeal <= 0 || hitEvents <= 0 || isBattleOver()) {
        return 0;
    }

    const int totalHeal = std::max(1, perHitHeal) * std::max(1, hitEvents);

    if (isBossCaster) {
        if (bossCurrentHp_ <= 0) {
            return 0;
        }
        applyBossHealing(totalHeal);
        presentationHealingApplied_ = true;
        return totalHeal;
    }

    bool anyTargetResolved = false;
    const int totalRequestedHealing = applyHealingToTargets(
        totalHeal,
        targetRule,
        targetPartyIndex,
        reviveDeadAllies,
        recordForTetoTally,
        &anyTargetResolved,
        nullptr
    );

    if (anyTargetResolved) {
        syncAllCharacterTurnParticipation();
        presentationHealingApplied_ = true;
    }

    return totalRequestedHealing;
}

bool BattleManager::consumePresentationHitDamageApplied() {
    const bool applied = presentationHitDamageApplied_;
    presentationHitDamageApplied_ = false;
    return applied;
}

bool BattleManager::consumePresentationHealingApplied() {
    const bool applied = presentationHealingApplied_;
    presentationHealingApplied_ = false;
    return applied;
}

void BattleManager::markPresentationAbilityAudioPlayed() {
    presentationAbilityAudioPlayed_ = true;
}

bool BattleManager::consumePresentationAbilityAudioPlayed() {
    const bool played = presentationAbilityAudioPlayed_;
    presentationAbilityAudioPlayed_ = false;
    return played;
}

void BattleManager::markPresentationHitAudioPlayed() {
    presentationHitAudioPlayed_ = true;
}

bool BattleManager::consumePresentationHitAudioPlayed() {
    const bool played = presentationHitAudioPlayed_;
    presentationHitAudioPlayed_ = false;
    return played;
}

const AbilityDefinition* BattleManager::findAbilityDefinition(const std::string& abilityId) const {
    return getAbility(abilityId);
}

std::vector<BattleStatusBadge> BattleManager::getActiveStatusBadges() const {
    std::vector<BattleStatusBadge> badges;
    badges.reserve(characters_.size() + activePartyBuffs_.size() * 3 + activeBossDebuffs_.size());

    for (size_t i = 0; i < characters_.size(); ++i) {
        const BattleCharacter& character = characters_[i];
        if (!character.isAlive() || character.getShield() <= 0) {
            continue;
        }

        BattleStatusBadge badge;
        badge.target = BattleStatusBadgeTarget::PartyMember;
        badge.targetPartyIndex = static_cast<int>(i);
        badge.sourcePartyIndex = static_cast<int>(i);
        badge.category = BattleStatusBadgeCategory::Shield;
        badge.abilityId = "shield";
        badge.statusName = "Shield";
        badge.value = character.getShield();
        badges.push_back(std::move(badge));
    }

    for (const ActivePartyBuff& buff : activePartyBuffs_) {
        if (buff.sourcePartyIndex < 0 ||
            buff.targetPartyIndex < 0 ||
            static_cast<size_t>(buff.sourcePartyIndex) >= characters_.size() ||
            static_cast<size_t>(buff.targetPartyIndex) >= characters_.size()) {
            continue;
        }

        const BattleCharacter& source = characters_[static_cast<size_t>(buff.sourcePartyIndex)];
        const BattleCharacter& target = characters_[static_cast<size_t>(buff.targetPartyIndex)];
        if (!source.isAlive() || !target.isAlive()) {
            continue;
        }

        const AbilityDefinition* ability = getAbility(buff.abilityId);
        const std::string statusName =
            (ability != nullptr && !ability->statusName.empty())
                ? ability->statusName
                : ((ability != nullptr && !ability->name.empty()) ? ability->name : buff.abilityId);

        const auto appendBadge = [&](int value, BattleStatusBadgeValueKind valueKind, const char* statLabel) {
            if (value == 0) {
                return;
            }

            BattleStatusBadge badge;
            badge.target = BattleStatusBadgeTarget::PartyMember;
            badge.targetPartyIndex = buff.targetPartyIndex;
            badge.sourcePartyIndex = buff.sourcePartyIndex;
            badge.category = value < 0 ? BattleStatusBadgeCategory::Debuff : BattleStatusBadgeCategory::Buff;
            badge.valueKind = valueKind;
            badge.abilityId = buff.abilityId;
            badge.sourceKey = source.definition().key;
            badge.sourceAssetId = resolveCharacterAssetId(source.partyIndex());
            badge.statusName = statusName;
            badge.statLabel = statLabel == nullptr ? std::string() : std::string(statLabel);
            badge.value = value;
            badges.push_back(std::move(badge));
        };

        appendBadge(buff.speedBuff, BattleStatusBadgeValueKind::Flat, "SPD");
        appendBadge(buff.atkBuff, BattleStatusBadgeValueKind::Percent, "ATK");
        appendBadge(buff.damageBuff, BattleStatusBadgeValueKind::Percent, "DMG");
    }

    const int tetoPartyIndex = findCharacterPartyIndexByKey("teto");
    if (tetoPartyIndex >= 0 &&
        static_cast<size_t>(tetoPartyIndex) < characters_.size() &&
        characters_[static_cast<size_t>(tetoPartyIndex)].isAlive() &&
        tetoHealingTally_ > 0) {
        const BattleCharacter& teto = characters_[static_cast<size_t>(tetoPartyIndex)];
        BattleStatusBadge badge;
        badge.target = BattleStatusBadgeTarget::PartyMember;
        badge.targetPartyIndex = tetoPartyIndex;
        badge.sourcePartyIndex = tetoPartyIndex;
        badge.category = BattleStatusBadgeCategory::Buff;
        badge.valueKind = BattleStatusBadgeValueKind::Flat;
        badge.abilityId = "teto_healing_tally";
        badge.sourceKey = teto.definition().key;
        badge.sourceAssetId = resolveCharacterAssetId(teto.partyIndex());
        badge.statLabel = "HEAL";
        badge.value = tetoHealingTally_;
        badges.push_back(std::move(badge));
    }

    if (isSailorVenusTransformed(sailorVenusState_.partyIndex) &&
        static_cast<size_t>(sailorVenusState_.partyIndex) < characters_.size() &&
        characters_[static_cast<size_t>(sailorVenusState_.partyIndex)].isAlive()) {
        const BattleCharacter& venus = characters_[static_cast<size_t>(sailorVenusState_.partyIndex)];
        const auto appendVenusBadge = [&](int value,
                                          BattleStatusBadgeValueKind valueKind,
                                          const char* statLabel) {
            BattleStatusBadge badge;
            badge.target = BattleStatusBadgeTarget::PartyMember;
            badge.targetPartyIndex = venus.partyIndex();
            badge.sourcePartyIndex = venus.partyIndex();
            badge.category = BattleStatusBadgeCategory::Buff;
            badge.valueKind = valueKind;
            badge.abilityId = "VenusTransformation";
            badge.sourceKey = venus.definition().key;
            badge.sourceAssetId = resolveCharacterAssetId(venus.partyIndex());
            badge.statusName = "Sailor Venus";
            badge.statLabel = statLabel == nullptr ? std::string() : std::string(statLabel);
            badge.value = value;
            badges.push_back(std::move(badge));
        };

        appendVenusBadge(65, BattleStatusBadgeValueKind::Flat, "SPD");
        appendVenusBadge(40, BattleStatusBadgeValueKind::Percent, "ATK");
    }

    if (bossCurrentHp_ > 0) {
        for (const ActiveBossDebuff& debuff : activeBossDebuffs_) {
            if (debuff.charges <= 0 ||
                debuff.sourcePartyIndex < 0 ||
                static_cast<size_t>(debuff.sourcePartyIndex) >= characters_.size()) {
                continue;
            }

            const BattleCharacter& source = characters_[static_cast<size_t>(debuff.sourcePartyIndex)];
            if (!source.isAlive()) {
                continue;
            }

            const AbilityDefinition* ability = getAbility(debuff.abilityId);
            const std::string statusName =
                (ability != nullptr && !ability->statusName.empty())
                    ? ability->statusName
                    : ((ability != nullptr && !ability->name.empty()) ? ability->name : debuff.abilityId);

            BattleStatusBadge badge;
            badge.target = BattleStatusBadgeTarget::Boss;
            badge.sourcePartyIndex = debuff.sourcePartyIndex;
            badge.category = BattleStatusBadgeCategory::Debuff;
            badge.valueKind = BattleStatusBadgeValueKind::Charges;
            badge.abilityId = debuff.abilityId;
            badge.sourceKey = source.definition().key;
            badge.sourceAssetId = resolveCharacterAssetId(source.partyIndex());
            badge.statusName = statusName;
            badge.value = debuff.charges;
            badges.push_back(std::move(badge));
        }
    }

    const auto categoryOrder = [](BattleStatusBadgeCategory category) {
        switch (category) {
            case BattleStatusBadgeCategory::Shield:
                return 0;
            case BattleStatusBadgeCategory::Buff:
                return 1;
            case BattleStatusBadgeCategory::Debuff:
                return 2;
        }
        return 3;
    };

    std::stable_sort(badges.begin(), badges.end(), [&](const BattleStatusBadge& lhs, const BattleStatusBadge& rhs) {
        if (lhs.target != rhs.target) {
            return lhs.target < rhs.target;
        }
        if (lhs.targetPartyIndex != rhs.targetPartyIndex) {
            return lhs.targetPartyIndex < rhs.targetPartyIndex;
        }
        const int lhsCategoryOrder = categoryOrder(lhs.category);
        const int rhsCategoryOrder = categoryOrder(rhs.category);
        if (lhsCategoryOrder != rhsCategoryOrder) {
            return lhsCategoryOrder < rhsCategoryOrder;
        }
        if (lhs.sourcePartyIndex != rhs.sourcePartyIndex) {
            return lhs.sourcePartyIndex < rhs.sourcePartyIndex;
        }
        if (lhs.abilityId != rhs.abilityId) {
            return lhs.abilityId < rhs.abilityId;
        }
        return lhs.statLabel < rhs.statLabel;
    });

    return badges;
}

const std::vector<BattleActionEvent>& BattleManager::getRecentActionEvents() const {
    return recentActionEvents_;
}

void BattleManager::clearRecentActionEvents() {
    recentActionEvents_.clear();
}

bool BattleManager::isPlayerActionReady(BattleAction action) const {
    if (isBattleOver()) {
        return false;
    }

    const TurnEvent next = peekNextTurnEvent();
    if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    const TurnActor& actor = turnState_.actors[next.actingActorIndex];
    if (actor.type != ParticipantType::Character || actor.partyIndex < 0 ||
        static_cast<size_t>(actor.partyIndex) >= characters_.size()) {
        return false;
    }

    const BattleCharacter& character = characters_[static_cast<size_t>(actor.partyIndex)];

    if (actor.autoExecute) {
        return false;
    }

    if (actor.isExtraTurn) {
        return action == actor.extraTurnAction &&
            canCharacterUseAction(actor.partyIndex, actor.extraTurnAction, actor.abilityKitOverride);
    }

    switch (action) {
        case BattleAction::Standard:
            return canCharacterUseAction(actor.partyIndex, BattleAction::Skill);
        case BattleAction::Skill:
            return canCharacterUseAction(actor.partyIndex, BattleAction::Skill);
        case BattleAction::Ultimate:
            return false;
    }
    return false;
}

bool BattleManager::executePlayerAction(BattleAction action) {
    return resolvePlayerAction(action);
}

bool BattleManager::executePlayerStandardTurn() {
    return resolvePlayerAction(BattleAction::Skill);
}

bool BattleManager::executePlayerSkillTurn() {
    return resolvePlayerAction(BattleAction::Skill);
}

bool BattleManager::executePlayerUltimateTurn() {
    return resolvePlayerAction(BattleAction::Ultimate);
}

/**
 * @brief Prepare an evenly split per-hit damage plan for the current primary character's attack.
 *
 * If successful, computes total damage for the character's regular attack (including presentation and combo multipliers),
 * divides it into `hitCount` integer chunks (distributing any remainder to earlier hits), and stores them in
 * `outHitDamages`.
 *
 * @param hitCount Number of hits to split the attack into; must be > 0.
 * @param outHitDamages Output vector that will be cleared and then filled with `hitCount` positive damage values.
 * @return true if a plan was prepared and `outHitDamages` contains `hitCount` damage values, `false` otherwise.
 *
 * Side effects: on success sets `pendingSplitAttackActorKey_` to the acting character's definition key.
 */
bool BattleManager::prepareCurrentPlayerSplitAttackPlan(int hitCount, std::vector<int>& outHitDamages) {
    outHitDamages.clear();
    if (isBattleOver() || hitCount <= 0) {
        return false;
    }

    const TurnEvent next = peekNextTurnEvent();
    if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    const TurnActor& actor = turnState_.actors[next.actingActorIndex];
    if (actor.type != ParticipantType::Character || actor.isExtraTurn) {
        return false;
    }
    if (actor.partyIndex < 0 || static_cast<size_t>(actor.partyIndex) >= characters_.size()) {
        return false;
    }

    BattleCharacter& character = characters_[static_cast<size_t>(actor.partyIndex)];
    if (!character.isAlive()) {
        return false;
    }

    pendingSplitAttackActorKey_ = character.definition().key;

    const AbilityDefinition* abilityDef = getAbility(
        resolveCharacterAbilityId(actor.partyIndex, BattleAction::Skill)
    );

    if (abilityDef == nullptr || abilityDef->type != AbilityType::Attack) {
        return false;
    }

    grantUltimatePointForAction(character, abilityDef, true);
    syncCharacterUltimateTurn(character.partyIndex());

    PresentationContext presContext;
    presContext.abilityId = abilityDef->id;
    presContext.abilityName = abilityDef->name;
    presContext.presentationId = abilityDef->presentationId;
    presContext.interactionType = abilityDef->interactionType;
    presContext.casterIndex = character.partyIndex();
    presContext.targetIndex = -1;
    presContext.isBoss = false;

    const float multiplier = runPresentationInteraction(presContext);
    const int totalDamage = normalizeDamage(static_cast<int>(
        character.effectiveAtk() *
        abilityDef->multiplier *
        character.damageBuffMultiplier() *
        multiplier *
        comboDamageMultiplier(comboState_.comboCount)
    ));

    const int base = totalDamage / hitCount;
    int remainder = totalDamage % hitCount;
    outHitDamages.reserve(static_cast<size_t>(hitCount));
    for (int i = 0; i < hitCount; ++i) {
        int chunk = base;
        if (remainder > 0) {
            ++chunk;
            --remainder;
        }
        outHitDamages.push_back(chunk);
    }

    return true;
}

/**
 * @brief Applies a single split-attack hit's damage to the boss.
 *
 * If `damage` is less than or equal to zero or the battle is over, no action is taken.
 * On the first outgoing damage of a pending split-attack (when `currentActionOutgoingDamage_ == 0`
 * and `pendingSplitAttackActorKey_` is non-empty) the damage is attributed to telemetry for that
 * character key before being applied.
 *
 * @param damage Damage amount to apply to the boss; values ≤ 0 are ignored.
 */
void BattleManager::applyBossSplitHitDamage(int damage) {
    if (damage <= 0 || isBattleOver()) {
        return;
    }

    if (currentActionOutgoingDamage_ == 0 && !pendingSplitAttackActorKey_.empty()) {
        telemetry_.characterDamageByKey[pendingSplitAttackActorKey_] += damage;
    }
    applyPlayerOffenseToBoss(damage);
}

/**
 * @brief Commits a prepared split-attack as the character's primary turn and applies resulting state updates.
 *
 * Advances the turn preview into an executed primary character turn for the pending split-attack,
 * records consumed action value into telemetry, resets the consumed actor's action value and priority,
 * synchronizes ultimate-turn queuing, and clears the pending split-attack actor key.
 *
 * @return `true` if the split-attack turn was successfully committed and state was updated; `false` if the battle is over,
 * the preview/actor was invalid or out of range, the actor is an extra turn, advancing the turn failed, or the character is not alive.
 */
bool BattleManager::commitCurrentPlayerSplitAttackTurn() {
    if (isBattleOver()) {
        return false;
    }

    const TurnEvent next = peekNextTurnEvent();
    if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    const TurnActor previewActor = turnState_.actors[next.actingActorIndex];
    if (previewActor.type != ParticipantType::Character || previewActor.isExtraTurn) {
        return false;
    }
    if (previewActor.partyIndex < 0 || static_cast<size_t>(previewActor.partyIndex) >= characters_.size()) {
        return false;
    }

    const TurnEvent event = advanceToNextTurnEvent();
    if (!event.valid || event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    TurnActor actor = turnState_.actors[event.actingActorIndex];
    if (actor.type != ParticipantType::Character || actor.isExtraTurn) {
        return false;
    }
    if (actor.partyIndex < 0 || static_cast<size_t>(actor.partyIndex) >= characters_.size()) {
        return false;
    }

    BattleCharacter& character = characters_[static_cast<size_t>(actor.partyIndex)];
    if (!character.isAlive()) {
        return false;
    }

    telemetry_.totalActionValueConsumed += event.consumedActionValue;

    if (event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }
    turnState_.actors[event.actingActorIndex].currentActionValue = turnState_.actors[event.actingActorIndex].baseActionValue;
    turnState_.actors[event.actingActorIndex].priority = 0;
    syncCharacterUltimateTurn(character.partyIndex());
    pendingSplitAttackActorKey_.clear();

    return true;
}

bool BattleManager::executePlayerTurn() {
    if (isBattleOver()) {
        return false;
    }
    const TurnEvent next = peekNextTurnEvent();
    if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    const TurnActor& actor = turnState_.actors[next.actingActorIndex];
    if (actor.type != ParticipantType::Character ||
        actor.partyIndex < 0 ||
        static_cast<size_t>(actor.partyIndex) >= characters_.size()) {
        return false;
    }

    if (actor.isExtraTurn) {
        return resolvePlayerAction(actor.extraTurnAction);
    }

    return resolvePlayerAction(BattleAction::Skill);
}

bool BattleManager::processNextAutomaticTurn() {
    if (isBattleOver()) {
        return false;
    }

    const TurnEvent next = peekNextTurnEvent();
    if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    const TurnActor& nextActor = turnState_.actors[next.actingActorIndex];
    if (nextActor.type == ParticipantType::Boss) {
        return resolveBossAction();
    }

    if (!(nextActor.isExtraTurn && nextActor.autoExecute)) {
        return false;
    }

    if (nextActor.partyIndex < 0 || static_cast<size_t>(nextActor.partyIndex) >= characters_.size()) {
        return false;
    }

    BattleCharacter& character = characters_[static_cast<size_t>(nextActor.partyIndex)];
    if (!character.isAlive()) {
        return consumeInvalidPreviewCharacterTurn(*this, characters_, turnState_, next);
    }

    const TurnEvent event = advanceToNextTurnEvent();
    if (!event.valid || event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    return executeCharacterAction(event.actingActorIndex,
                                  character,
                                  nextActor.extraTurnAction,
                                  event.consumedActionValue);
}

/**
 * @brief Advances and executes any pending automatic turns until no further auto-executable actions remain.
 *
 * Processes the turn queue, executing boss primary turns and character auto-execute extra turns in sequence.
 * The function will consume invalid preview character turns for dead characters, execute valid automatic actions,
 * and may schedule a boss phase transition as a side effect. Processing stops when the battle is over, the next
 * previewed turn is not an auto-executable extra character turn, the turn preview is invalid, or a boss phase
 * transition becomes pending.
 *
 * @return true if at least one automatic turn was executed or an invalid preview turn was consumed; `false` if no progress was made.
 */
bool BattleManager::processAutomaticTurns() {
    bool progressed = false;

    while (processNextAutomaticTurn()) {
        progressed = true;
        if (pendingBossPhaseTransition_.has_value()) {
            break;
        }
    }

    return progressed;
}

bool BattleManager::isBattleOver() const {
    return outcome() != BattleResolvedOutcome::None;
}

bool BattleManager::buildInitialTurnState() {
    return turn::buildInitialTurnState(state_, turnState_);
}

TurnEvent BattleManager::peekNextTurnEvent() const {
    return turn::peekNextTurnEvent(turnState_);
}

TurnEvent BattleManager::advanceToNextTurnEvent() {
    return turn::advanceToNextTurnEvent(turnState_);
}

void BattleManager::runPseudoBattle(int maxActions) {
    simulatedActions_ = 0;
    const int clampedActions = std::max(1, maxActions);

    while (simulatedActions_ < clampedActions) {
        if (bossCurrentHp_ <= 0) {
            std::cout << "  [End] Boss defeated in " << simulatedActions_ << " actions.\n";
            break;
        }

        if (firstLivingCharacterPartyIndex() < 0) {
            std::cout << "  [End] Party wiped in " << simulatedActions_ << " actions.\n";
            break;
        }

        TurnEvent event = advanceToNextTurnEvent();
        if (!event.valid) {
            std::cout << "  [End] No valid turn available.\n";
            break;
        }

        executeTurn(event.actingActorIndex);
    }

    if (bossCurrentHp_ > 0 && firstLivingCharacterPartyIndex() >= 0 && simulatedActions_ >= clampedActions) {
        std::cout << "  [End] Reached action cap (" << clampedActions << ").\n";
    }
}

/**
 * @brief Executes the pending turn for the actor at the given turn actor index.
 *
 * Validates the index and, if the actor is a boss, executes the boss standard action.
 * For character actors, verifies the party index and that the character is alive; if the
 * actor is invalid or the character is dead, removes an extra-turn actor or resets a
 * primary actor's action value and priority. For valid character turns, executes the
 * actor's extra-turn action for extra turns or the character's regular skill for primary turns.
 *
 * @param actorIndex Index of the turn actor in `turnState_.actors` to execute.
 */
void BattleManager::executeTurn(size_t actorIndex) {
    if (actorIndex >= turnState_.actors.size()) {
        return;
    }

    TurnActor actor = turnState_.actors[actorIndex];
    if (actor.type == ParticipantType::Boss) {
        executeBossAction(actorIndex, BattleAction::Standard, 0.0f);
    } else {
        if (actor.partyIndex < 0 || static_cast<size_t>(actor.partyIndex) >= characters_.size()) {
            if (actorIndex < turnState_.actors.size()) {
                if (turnState_.actors[actorIndex].isExtraTurn) {
                    turnState_.actors.erase(turnState_.actors.begin() + actorIndex);
                } else {
                    turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
                    turnState_.actors[actorIndex].priority = 0;
                }
            }
            return;
        }

        BattleCharacter& character = characters_[static_cast<size_t>(actor.partyIndex)];
        if (!character.isAlive()) {
            if (actorIndex < turnState_.actors.size()) {
                if (turnState_.actors[actorIndex].isExtraTurn) {
                    turnState_.actors.erase(turnState_.actors.begin() + actorIndex);
                } else {
                    turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
                    turnState_.actors[actorIndex].priority = 0;
                }
            }
            return;
        }

        // Legacy single-ability behavior:
        // - Regular turn: use the character's only non-ultimate ability.
        // - Extra turn: cast ultimate automatically.
        if (actor.isExtraTurn) {
            executeCharacterAction(actorIndex, character, actor.extraTurnAction, 0.0f);
        } else {
            executeCharacterAction(actorIndex, character, BattleAction::Skill, 0.0f);
        }
    }
}

/**
 * @brief Queues a manual extra turn for the specified party member that uses their Ultimate action.
 *
 * The queued actor represents an extra (non-primary) turn targeted at the character at
 * `partyIndex`. The extra turn will use `BattleAction::Ultimate`, will not be marked
 * `autoExecute`, and will not grant an ultimate point on action. The turn's priority is
 * assigned from `nextManualUltimatePriority_` and that priority counter is decremented.
 *
 * @param partyIndex Index of the party member to receive the extra Ultimate turn.
 */
void BattleManager::queueExtraTurnForCharacter(int partyIndex) {
    queueExtraTurnForCharacter(
        partyIndex,
        BattleAction::Ultimate,
        false,
        false,
        {},
        nextManualUltimatePriority_--
    );
}

/**
 * @brief Enqueues an extra (secondary) turn for a party character in the turn state.
 *
 * If `partyIndex` is out of range the call is a no-op.
 *
 * @param partyIndex Index of the party member to receive the extra turn.
 * @param action The action the extra turn will perform (Standard, Skill, Ultimate).
 * @param autoExecute If true, the extra turn will be executed automatically without player input.
 * @param grantsUltimatePointOnAction If true, the character gains an ultimate point when this extra turn is executed.
 * @param priority Priority value used to order extra turns relative to other actors (higher runs earlier).
 */
void BattleManager::queueExtraTurnForCharacter(int partyIndex,
                                               BattleAction action,
                                               bool autoExecute,
                                               bool grantsUltimatePointOnAction,
                                               const std::string& abilityKitOverride,
                                               int priority) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return;
    }

    const BattleCharacter& c = characters_[static_cast<size_t>(partyIndex)];
    turn::queueExtraTurnForCharacter(
        turnState_,
        c.definition(),
        partyIndex,
        c.effectiveSpd(),
        action,
        autoExecute,
        grantsUltimatePointOnAction,
        resolveCharacterAssetId(partyIndex, abilityKitOverride),
        abilityKitOverride,
        priority
    );
}

/**
 * @brief Schedules a boss phase-intro extra turn that will trigger a phase transition.
 *
 * If an existing boss phase-intro actor is already queued, this call does nothing.
 *
 * @param fromPhaseIndex Index of the boss phase being transitioned from.
 * @param toPhaseIndex Index of the boss phase being transitioned to.
 */
void BattleManager::queueBossPhaseIntroTurn(int fromPhaseIndex, int toPhaseIndex) {
    constexpr int kBossPhaseIntroPriority = 5000;

    for (const TurnActor& actor : turnState_.actors) {
        if (actor.isBossPhaseIntroTurn()) {
            return;
        }
    }

    TurnActor extra;
    extra.type = ParticipantType::Boss;
    extra.key = state_.boss.key;
    extra.assetId = state_.boss.assets;
    extra.title = state_.boss.title;
    extra.partyIndex = -1;
    extra.priority = kBossPhaseIntroPriority;
    extra.isExtraTurn = true;
    extra.extraTurnAction = BattleAction::Standard;
    extra.autoExecute = true;
    extra.grantsUltimatePointOnAction = false;
    extra.spd = std::max(1, state_.boss.spd);
    extra.baseActionValue = turn::actionValueFromSpeed(extra.spd);
    extra.currentActionValue = 0.0f;
    extra.phaseTransitionFromIndex = fromPhaseIndex;
    extra.phaseTransitionToIndex = toPhaseIndex;
    turnState_.actors.push_back(extra);
}

/**
 * @brief Determines whether the boss is allowed to perform the specified action type.
 *
 * @return `true` if the action is `Standard`, `false` otherwise.
 */
bool BattleManager::canUseBossAction(BattleAction action) const {
    switch (action) {
        case BattleAction::Standard:
            return true;
        case BattleAction::Skill:
            return false;
        case BattleAction::Ultimate:
            return false;
    }
    return false;
}

/**
 * @brief Attempt to resolve and execute a player-initiated action against the current turn preview.
 *
 * Validates battle/preview state and the preview actor's eligibility for the requested action,
 * consumes invalid preview character turns when necessary, advances the turn queue, and executes
 * the resolved character action if all checks pass.
 *
 * @param action The player action to perform (Standard, Skill, or Ultimate).
 * @return true if the action was executed and applied to the resolved character turn, false otherwise.
 */
bool BattleManager::resolvePlayerAction(BattleAction action) {
    if (isBattleOver()) {
        return false;
    }

    while (!isBattleOver()) {
        const TurnEvent next = peekNextTurnEvent();
        if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
            return false;
        }

        if (!consumeInvalidPreviewCharacterTurn(*this, characters_, turnState_, next)) {
            break;
        }
    }

    const TurnEvent next = peekNextTurnEvent();
    if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    const TurnActor& previewActor = turnState_.actors[next.actingActorIndex];
    if (previewActor.type != ParticipantType::Character || previewActor.partyIndex < 0 ||
        static_cast<size_t>(previewActor.partyIndex) >= characters_.size()) {
        return false;
    }

    if (previewActor.autoExecute) {
        return false;
    }
    if (previewActor.isExtraTurn && action != previewActor.extraTurnAction) {
        return false;
    }
    if (!previewActor.isExtraTurn && action == BattleAction::Ultimate) {
        return false;
    }

    BattleCharacter& character = characters_[static_cast<size_t>(previewActor.partyIndex)];
    if (!character.isAlive()) {
        return consumeInvalidPreviewCharacterTurn(*this, characters_, turnState_, next) && resolvePlayerAction(action);
    }

    const BattleAction resolvedAction =
        (action == BattleAction::Standard) ? BattleAction::Skill : action;
    if (!canCharacterUseAction(previewActor.partyIndex, resolvedAction, previewActor.abilityKitOverride)) {
        return false;
    }

    const TurnEvent event = advanceToNextTurnEvent();
    if (!event.valid || event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    return executeCharacterAction(event.actingActorIndex,
                                  characters_[static_cast<size_t>(previewActor.partyIndex)],
                                  action,
                                  event.consumedActionValue);
}

/**
 * @brief Advance the turn preview to the next event and execute the boss's scheduled standard action if present.
 *
 * This checks that the battle is ongoing and the next scheduled turn actor is the boss, advances the turn
 * state to consume that event, and then executes the boss's standard action using the consumed action value.
 *
 * @return `true` if a boss action was executed, `false` otherwise.
 */
bool BattleManager::resolveBossAction() {
    if (isBattleOver()) {
        return false;
    }

    const TurnEvent next = peekNextTurnEvent();
    if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }
    if (turnState_.actors[next.actingActorIndex].type != ParticipantType::Boss) {
        return false;
    }

    const TurnEvent event = advanceToNextTurnEvent();
    if (!event.valid || event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    return executeBossAction(event.actingActorIndex, BattleAction::Standard, event.consumedActionValue);
}

/**
 * @brief Executes a character's action (standard/skill/ultimate) for the specified turn actor.
 *
 * Processes presentation interactions and ability effects, applies resulting damage/healing/buffs,
 * updates telemetry and ultimate charges, updates or removes the turn actor, and records a
 * BattleActionEvent in recentActionEvents_.
 *
 * @param actorIndex Index of the turn actor in turnState_.actors to execute.
 * @param character Reference to the BattleCharacter performing the action.
 * @param action The BattleAction being performed (Standard/Skill/Ultimate).
 * @param consumedActionValue Non-negative action value consumed for this execution; used for telemetry
 *        and passed through to presentation/execution logic.
 * @return bool `true` if the action was executed and recorded; `false` if actorIndex was out of range.
 */
bool BattleManager::executeCharacterAction(size_t actorIndex,
                                          BattleCharacter& character,
                                          BattleAction action,
                                          float consumedActionValue) {
    if (actorIndex >= turnState_.actors.size()) {
        return false;
    }

    ++simulatedActions_;
    telemetry_.totalActionValueConsumed += std::max(0.0f, consumedActionValue);
    currentActionOutgoingDamage_ = 0;
    pendingSplitAttackActorKey_.clear();
    presentationHitDamageApplied_ = false;
    presentationHealingApplied_ = false;
    presentationAbilityAudioPlayed_ = false;
    presentationHitAudioPlayed_ = false;

    const TurnActor& turnActor = turnState_.actors[actorIndex];
    const bool wasExtraTurn = turnActor.isExtraTurn;
    const bool grantsUltimatePointOnAction = turnActor.grantsUltimatePointOnAction;
    if (!wasExtraTurn) {
        expireBuffsFromCaster(character.partyIndex());
    }

    const BattleAction abilityAction =
        (!wasExtraTurn && action == BattleAction::Standard) ? BattleAction::Skill : action;
    const std::string abilityId = resolveCharacterAbilityId(
        character.partyIndex(),
        abilityAction,
        turnActor.abilityKitOverride
    );
    const AbilityDefinition* abilityDef = getAbility(abilityId);
    float presentationMultiplier = 1.0f;
    BattleActionEvent actionEvent;
    actionEvent.actorType = ParticipantType::Character;
    actionEvent.actorKey = character.definition().key;
    actionEvent.actorTitle = character.definition().title;
    actionEvent.actorPartyIndex = character.partyIndex();
    actionEvent.action = action;
    actionEvent.abilityId = abilityId;
    actionEvent.abilityName = abilityDef != nullptr ? abilityDef->name : abilityId;
    actionEvent.interactionType = abilityDef != nullptr ? abilityDef->interactionType : InteractionType::None;
    actionEvent.bossHpBefore = bossCurrentHp_;
    actionEvent.bossHpAfter = bossCurrentHp_;
    actionEvent.hitVoicesHandledDuringPresentation = false;
    actionEvent.consumedActionValue = std::max(0.0f, consumedActionValue);
    actionEvent.targetPartyIndices.clear();
    actionEvent.targetHpBefore.clear();
    actionEvent.targetHpAfter.clear();
    actionEvent.targetShieldBefore.clear();
    actionEvent.targetShieldAfter.clear();

    std::vector<int> supportTargetPartyIndices;
    std::vector<int> supportHpBefore;
    std::vector<int> supportShieldBefore;
    if (abilityDef != nullptr &&
        (abilityDef->type == AbilityType::Heal ||
         abilityDef->type == AbilityType::Shield ||
         abilityDef->type == AbilityType::Buff)) {
        supportTargetPartyIndices = collectSupportTargetPartyIndices(
            character.partyIndex(),
            *abilityDef,
            -1
        );
        supportHpBefore.reserve(characters_.size());
        supportShieldBefore.reserve(characters_.size());
        for (const BattleCharacter& target : characters_) {
            supportHpBefore.push_back(target.hp());
            supportShieldBefore.push_back(target.getShield());
        }
    }

    grantUltimatePointForAction(character, abilityDef, grantsUltimatePointOnAction);
    if (grantsUltimatePointOnAction) {
        syncCharacterUltimateTurn(character.partyIndex());
    }

    if (abilityDef == nullptr) {
        const int fallbackDamage = normalizeDamage(static_cast<int>(std::lround(
            static_cast<float>(character.effectiveAtk() * ((abilityAction == BattleAction::Ultimate) ? 2 : 1)) *
            character.damageBuffMultiplier()
        )));
        applyPlayerOffenseToBoss(fallbackDamage);
    } else {
        PresentationContext presContext;
        presContext.abilityId = abilityDef->id;
        presContext.abilityName = abilityDef->name;
        presContext.presentationId = abilityDef->presentationId;
        presContext.interactionType = abilityDef->interactionType;
        presContext.casterIndex = character.partyIndex();
        presContext.targetIndex = -1;
        if (abilityDef->id == "QuanYuTianXia") {
            presContext.presentationValue = getLuotianyiCorrectTones();
        } else if (abilityDef->id == "VenusLoveMeChain") {
            presContext.presentationValue = getSailorVenusSpaceTally();
        } else if (character.definition().key == "zhouShen") {
            presContext.presentationValue = getSingerCountInParty();
        }
        presContext.isBoss = false;
        presContext.isUltimate = abilityAction == BattleAction::Ultimate;

        presentationMultiplier = runPresentationInteraction(presContext);
        const bool presentationHitApplied = consumePresentationHitDamageApplied();
        const bool presentationHealingApplied = consumePresentationHealingApplied();

        const bool presentationAppliedPrimaryEffect =
            (presentationHealingApplied && abilityDef->type == AbilityType::Heal) ||
            (presentationHitApplied && abilityDef->type != AbilityType::Heal);

        if (!presentationAppliedPrimaryEffect) {
            AbilityExecutionContext execContext;
            execContext.ability = abilityDef;
            execContext.casterPartyIndex = character.partyIndex();
            execContext.isBossCaster = false;
            execContext.playerDamageHealsBoss = playerDamageHealsBoss();
            execContext.baseDamage = character.effectiveAtk();
            execContext.baseHeal = abilityDef->flatHeal;
            execContext.presentationMultiplier = presentationMultiplier;
            execContext.comboMultiplier = comboDamageMultiplier(comboState_.comboCount);
            execContext.damageBuffMultiplier = character.damageBuffMultiplier();
            execContext.bossMaxHp = state_.boss.hp;
            executeAbilityEffect(execContext);
        }
    }
    actionEvent.bossHpAfter = bossCurrentHp_;
    actionEvent.totalOutgoingDamage = currentActionOutgoingDamage_;
    actionEvent.abilityVoicesHandledDuringPresentation = consumePresentationAbilityAudioPlayed();
    actionEvent.hitVoicesHandledDuringPresentation = consumePresentationHitAudioPlayed();
    if (!character.definition().key.empty() && currentActionOutgoingDamage_ > 0) {
        telemetry_.characterDamageByKey[character.definition().key] += currentActionOutgoingDamage_;
    }
    currentActionOutgoingDamage_ = 0;

    if (abilityDef != nullptr &&
        abilityDef->id == "MeiCiDuXiangZhuang" &&
        bossCurrentHp_ > 0) {
        applyJiafeiUltimateDebuff(character.partyIndex(), *abilityDef);
    }

    if (!supportHpBefore.empty()) {
        for (int targetPartyIndex : supportTargetPartyIndices) {
            if (targetPartyIndex < 0 || static_cast<size_t>(targetPartyIndex) >= characters_.size()) {
                continue;
            }

            const BattleCharacter& target = characters_[static_cast<size_t>(targetPartyIndex)];
            actionEvent.targetPartyIndices.push_back(targetPartyIndex);
            actionEvent.targetHpBefore.push_back(supportHpBefore[static_cast<size_t>(targetPartyIndex)]);
            actionEvent.targetHpAfter.push_back(target.hp());
            actionEvent.targetShieldBefore.push_back(supportShieldBefore[static_cast<size_t>(targetPartyIndex)]);
            actionEvent.targetShieldAfter.push_back(target.getShield());
        }
    }

    if (action == BattleAction::Ultimate) {
        character.consumeUltimate();
    }

    if (wasExtraTurn) {
        turnState_.actors.erase(turnState_.actors.begin() + static_cast<long>(actorIndex));
    } else {
        turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
        turnState_.actors[actorIndex].priority = 0;
    }

    if (abilityDef != nullptr && abilityDef->type == AbilityType::Buff) {
        applyPartyBuffFromAbility(character.partyIndex(), *abilityDef, presentationMultiplier);
    }
    if (abilityDef != nullptr && abilityDef->actionAdvance > 0.0f) {
        applyAllAlliesActionAdvance(abilityDef->actionAdvance, abilityDef->actionAdvanceMode);
    }
    if (abilityDef != nullptr) {
        if (abilityDef->id == "VenusTransformation") {
            activateSailorVenusTransformation(character.partyIndex());
        } else if (abilityDef->id == "LoveAndBeautyShock") {
            consumeSailorVenusTurnAndQueueFinisherIfNeeded(character.partyIndex());
        } else if (abilityDef->id == "VenusLoveMeChain") {
            revertSailorVenusTransformation(true);
        }
    }

    syncCharacterUltimateTurn(character.partyIndex());
    tryQueueJiafeiFollowUp(actionEvent);
    tryQueueZhouShenFollowUp(actionEvent);
    tryQueueSailorVenusFollowUp(actionEvent);
    recentActionEvents_.push_back(std::move(actionEvent));
    return true;
}

/**
 * @brief Executes the boss's action for the turn actor at the given index, applying ability effects,
 * presentation interaction, damage/healing to characters or boss, self-costs, buff cleanup, telemetry,
 * and recording the resulting action event.
 *
 * If the turn actor is a boss phase intro turn this will queue the pending phase transition and remove
 * the intro actor instead of performing a normal ability resolution.
 *
 * @param actorIndex Index into `turnState_.actors` identifying the boss turn actor to execute.
 * @param action The `BattleAction` (e.g., Standard, Ultimate) to resolve for the boss actor.
 * @param consumedActionValue The action value consumed for this execution (used for telemetry and event data).
 * @return true if the boss action (or phase-intro handling) was performed and recorded; `false` if `actorIndex`
 *         is out of range and no action was executed.
 */
bool BattleManager::executeBossAction(size_t actorIndex, BattleAction action, float consumedActionValue) {
    if (actorIndex >= turnState_.actors.size()) {
        return false;
    }

    if (turnState_.actors[actorIndex].isBossPhaseIntroTurn()) {
        pendingBossPhaseTransition_ = BossPhaseTransition{
            true,
            turnState_.actors[actorIndex].phaseTransitionFromIndex,
            turnState_.actors[actorIndex].phaseTransitionToIndex
        };
        turnState_.actors.erase(turnState_.actors.begin() + static_cast<long>(actorIndex));
        return true;
    }

    ++simulatedActions_;
    telemetry_.totalActionValueConsumed += std::max(0.0f, consumedActionValue);
    presentationHitDamageApplied_ = false;
    presentationHealingApplied_ = false;
    presentationAbilityAudioPlayed_ = false;
    presentationHitAudioPlayed_ = false;

    const std::string abilityId = getBossNormalAbilityId(state_.boss);
    const AbilityDefinition* abilityDef = getAbility(abilityId);
    BattleActionEvent actionEvent;
    actionEvent.actorType = ParticipantType::Boss;
    actionEvent.actorKey = state_.boss.key;
    actionEvent.actorTitle = state_.boss.title;
    actionEvent.actorPartyIndex = -1;
    actionEvent.action = action;
    actionEvent.abilityId = abilityId;
    actionEvent.abilityName = abilityDef != nullptr ? abilityDef->name : abilityId;
    actionEvent.interactionType = abilityDef != nullptr ? abilityDef->interactionType : InteractionType::None;
    actionEvent.bossHpBefore = bossCurrentHp_;
    actionEvent.bossHpAfter = bossCurrentHp_;
    actionEvent.hitVoicesHandledDuringPresentation = false;
    actionEvent.consumedActionValue = std::max(0.0f, consumedActionValue);
    actionEvent.targetPartyIndices.clear();
    actionEvent.targetHpBefore.clear();
    actionEvent.targetHpAfter.clear();
    actionEvent.targetShieldBefore.clear();
    actionEvent.targetShieldAfter.clear();

    bool presentationHitApplied = false;
    if (abilityDef != nullptr) {
        PresentationContext presContext;
        presContext.abilityId = abilityDef->id;
        presContext.abilityName = abilityDef->name;
        presContext.presentationId = abilityDef->presentationId;
        presContext.interactionType = abilityDef->interactionType;
        presContext.casterIndex = -1;
        presContext.targetIndex = -1;
        presContext.isBoss = true;
        presContext.isUltimate = action == BattleAction::Ultimate;
        presContext.tuningProfile = currentBossPhaseDefinition().tuningProfile;
        int presentationTargetHpBefore = -1;

        // Some boss presentations focus one random alive ally and drive all damage through the presentation itself.
        if (bossAbilityUsesFocusedPartyPresentation(*abilityDef)) {
            std::vector<int> alive;
            for (const BattleCharacter& c : characters_) {
                if (c.isAlive()) {
                    alive.push_back(c.partyIndex());
                }
            }
            if (!alive.empty()) {
                const int index = std::rand() % static_cast<int>(alive.size());
                presContext.targetIndex = alive[static_cast<size_t>(index)];
                if (static_cast<size_t>(presContext.targetIndex) < characters_.size()) {
                    presentationTargetHpBefore =
                        characters_[static_cast<size_t>(presContext.targetIndex)].hp();
                }
            }
        }

        const float multiplier = runPresentationInteraction(presContext);
        presentationHitApplied = consumePresentationHitDamageApplied();

        if (presentationHitApplied && abilityDef->type == AbilityType::Attack) {
            if (presContext.targetIndex >= 0 &&
                static_cast<size_t>(presContext.targetIndex) < characters_.size() &&
                presentationTargetHpBefore >= 0) {
                actionEvent.targetPartyIndices.push_back(presContext.targetIndex);
                actionEvent.targetHpBefore.push_back(presentationTargetHpBefore);
                actionEvent.targetHpAfter.push_back(
                    characters_[static_cast<size_t>(presContext.targetIndex)].hp());
                const int currentShield = characters_[static_cast<size_t>(presContext.targetIndex)].getShield();
                actionEvent.targetShieldBefore.push_back(currentShield);
                actionEvent.targetShieldAfter.push_back(currentShield);
            }
        } else if (bossAbilityUsesFocusedPartyPresentation(*abilityDef) && presContext.targetIndex >= 0) {
            // Focused boss presentations handle all damage through presentation hit events.
        } else if (abilityDef->targetRule == TargetRule::AllEnemies) {
            for (BattleCharacter& c : characters_) {
                if (!c.isAlive()) {
                    continue;
                }
                actionEvent.targetPartyIndices.push_back(c.partyIndex());
                actionEvent.targetHpBefore.push_back(c.hp());
                actionEvent.targetShieldBefore.push_back(c.getShield());
                const int finalDamage = normalizeDamage(static_cast<int>(
                    getBossEffectiveAtk() * abilityDef->multiplier * multiplier
                ));
                c.receiveDamage(finalDamage);
                actionEvent.targetHpAfter.push_back(c.hp());
                actionEvent.targetShieldAfter.push_back(c.getShield());
            }
            syncAllCharacterTurnParticipation();
        } else {
            const int targetIndex = firstLivingCharacterPartyIndex();
            if (targetIndex >= 0) {
                actionEvent.targetPartyIndices.push_back(targetIndex);
                actionEvent.targetHpBefore.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
                actionEvent.targetShieldBefore.push_back(characters_[static_cast<size_t>(targetIndex)].getShield());
                const int finalDamage = normalizeDamage(static_cast<int>(
                    getBossEffectiveAtk() * abilityDef->multiplier * multiplier
                ));
                characters_[static_cast<size_t>(targetIndex)].receiveDamage(finalDamage);
                actionEvent.targetHpAfter.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
                actionEvent.targetShieldAfter.push_back(characters_[static_cast<size_t>(targetIndex)].getShield());
                syncCharacterTurnParticipation(targetIndex);
            }
        }
    } else {
        const int targetIndex = firstLivingCharacterPartyIndex();
        if (targetIndex >= 0) {
            actionEvent.targetPartyIndices.push_back(targetIndex);
            actionEvent.targetHpBefore.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
            actionEvent.targetShieldBefore.push_back(characters_[static_cast<size_t>(targetIndex)].getShield());
            characters_[static_cast<size_t>(targetIndex)].receiveDamage(normalizeDamage(getBossEffectiveAtk()));
            actionEvent.targetHpAfter.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
            actionEvent.targetShieldAfter.push_back(characters_[static_cast<size_t>(targetIndex)].getShield());
            syncCharacterTurnParticipation(targetIndex);
        }
    }

    reviveDefeatedPartyMembersIfNeeded();
    if (abilityDef != nullptr) {
        applyBossAbilitySelfCost(*abilityDef);
    }
    removeBuffsFromDefeatedCharacters();

    actionEvent.abilityVoicesHandledDuringPresentation = consumePresentationAbilityAudioPlayed();
    actionEvent.hitVoicesHandledDuringPresentation = consumePresentationHitAudioPlayed();
    actionEvent.bossHpAfter = bossCurrentHp_;

    turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
    turnState_.actors[actorIndex].priority = 0;
    recentActionEvents_.push_back(std::move(actionEvent));
    return true;
}

int BattleManager::firstLivingCharacterPartyIndex() const {
    for (const BattleCharacter& c : characters_) {
        if (c.isAlive()) {
            return c.partyIndex();
        }
    }
    return -1;
}

int BattleManager::normalizeDamage(int value) {
    return std::max(1, value);
}

const AbilityDefinition* BattleManager::getAbility(const std::string& abilityId) const {
    auto it = abilities_.find(abilityId);
    if (it != abilities_.end()) {
        return &it->second;
    }
    return nullptr;
}

const CharacterAbilityKitDefinition* BattleManager::findCharacterAbilityKit(
    const CharacterDefinition& definition,
    const std::string& kitId) const {
    if (kitId.empty()) {
        return nullptr;
    }

    const auto it = definition.abilityKits.find(kitId);
    if (it != definition.abilityKits.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string BattleManager::resolveCharacterAbilityId(const CharacterDefinition& definition,
                                                     BattleAction action,
                                                     const std::string& activeKitId,
                                                     const std::string& turnAbilityKitOverride) const {
    const auto slotOverride = [action](const CharacterAbilityKitDefinition* kit) -> std::string {
        if (kit == nullptr) {
            return {};
        }

        switch (action) {
            case BattleAction::Standard:
                return kit->standardAbility;
            case BattleAction::Skill:
                return kit->skillAbility;
            case BattleAction::Ultimate:
                return kit->ultimate;
        }

        return {};
    };

    if (const CharacterAbilityKitDefinition* turnKit =
            findCharacterAbilityKit(definition, turnAbilityKitOverride)) {
        const std::string overriddenAbility = slotOverride(turnKit);
        if (!overriddenAbility.empty()) {
            return overriddenAbility;
        }
    }

    if (const CharacterAbilityKitDefinition* activeKit =
            findCharacterAbilityKit(definition, activeKitId)) {
        const std::string overriddenAbility = slotOverride(activeKit);
        if (!overriddenAbility.empty()) {
            return overriddenAbility;
        }
    }

    switch (action) {
        case BattleAction::Standard:
            return getCharacterStandardAbilityId(definition);
        case BattleAction::Skill:
            return getCharacterRegularAbilityId(definition);
        case BattleAction::Ultimate:
            return definition.ultimate;
    }

    return {};
}

std::string BattleManager::resolveCharacterAssetId(const CharacterDefinition& definition,
                                                   const std::string& activeKitId,
                                                   const std::string& turnAbilityKitOverride) const {
    if (const CharacterAbilityKitDefinition* turnKit =
            findCharacterAbilityKit(definition, turnAbilityKitOverride);
        turnKit != nullptr && !turnKit->assetId.empty()) {
        return turnKit->assetId;
    }

    if (const CharacterAbilityKitDefinition* activeKit =
            findCharacterAbilityKit(definition, activeKitId);
        activeKit != nullptr && !activeKit->assetId.empty()) {
        return activeKit->assetId;
    }

    if (!definition.assets.empty()) {
        return definition.assets;
    }
    return definition.key;
}

bool BattleManager::canCharacterUseAction(int partyIndex,
                                          BattleAction action,
                                          const std::string& turnAbilityKitOverride) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return false;
    }

    const BattleCharacter& character = characters_[static_cast<size_t>(partyIndex)];
    if (!character.isAlive()) {
        return false;
    }

    const std::string abilityId = resolveCharacterAbilityId(partyIndex, action, turnAbilityKitOverride);
    if (abilityId.empty()) {
        return false;
    }

    if (action == BattleAction::Ultimate) {
        return character.canUseUltimate();
    }

    return true;
}

void BattleManager::executeAbilityEffect(const AbilityExecutionContext& context) {
    if (context.ability == nullptr) {
        return;
    }

    const AbilityDefinition& ability = *context.ability;
    if (!context.isBossCaster) {
        if (ability.specialDamageSource == SpecialDamageSource::TeamShield) {
            (void)applyCurrentTeamShieldDamageToBoss(
                false,
                ability.multiplier,
                context.casterPartyIndex
            );
            syncAllCharacterTurnParticipation();
            return;
        }

        if (ability.type == AbilityType::Heal || ability.type == AbilityType::Shield) {
            const int casterMaxHp = (context.casterPartyIndex >= 0 &&
                                     static_cast<size_t>(context.casterPartyIndex) < characters_.size())
                ? characters_[static_cast<size_t>(context.casterPartyIndex)].maxHp()
                : 1;

            if (ability.type == AbilityType::Heal) {
                const int baseHeal = std::max(0, context.baseHeal > 0 ? context.baseHeal : ability.flatHeal);
                const int healAmount = ability::resolveSupportAmount(
                    ability,
                    casterMaxHp,
                    context.presentationMultiplier,
                    baseHeal
                );
                const int totalRequestedHealing = applyHealingToTargets(
                    healAmount,
                    ability.targetRule,
                    context.targetPartyIndex,
                    ability.reviveDeadAllies,
                    ability.specialDamageSource != SpecialDamageSource::StoredHealingTally,
                    nullptr,
                    nullptr
                );
                if (ability.specialDamageSource == SpecialDamageSource::AppliedHeal) {
                    (void)applyConvertedPlayerSpecialDamageToBoss(
                        totalRequestedHealing,
                        ability.multiplier,
                        false,
                        context.casterPartyIndex
                    );
                } else if (ability.specialDamageSource == SpecialDamageSource::StoredHealingTally) {
                    (void)applyConvertedPlayerSpecialDamageToBoss(
                        consumeTetoHealingTally(),
                        ability.multiplier,
                        false,
                        context.casterPartyIndex
                    );
                }
                syncAllCharacterTurnParticipation();
                return;
            }

            const int shieldAmount = ability::resolveSupportAmount(
                ability,
                casterMaxHp,
                context.presentationMultiplier,
                ability.baseShield > 0 ? ability.baseShield :
                    (context.casterPartyIndex >= 0 &&
                     static_cast<size_t>(context.casterPartyIndex) < characters_.size()
                        ? characters_[static_cast<size_t>(context.casterPartyIndex)].definition().baseShield
                        : 0)
            );
            (void)applyShieldToTargets(shieldAmount, ability.targetRule, context.targetPartyIndex);
            syncAllCharacterTurnParticipation();
            return;
        }
    }

    ability::executeAbilityEffect(context, bossCurrentHp_, characters_);
    syncAllCharacterTurnParticipation();
}

float BattleManager::runPresentationInteraction(const PresentationContext& context) {
    return ability::runPresentationInteraction(context);
}

void BattleManager::applyPartyBuffFromAbility(int sourcePartyIndex,
                                              const AbilityDefinition& ability,
                                              float presentationMultiplier) {
    if (sourcePartyIndex < 0 || static_cast<size_t>(sourcePartyIndex) >= characters_.size()) {
        return;
    }

    const int scaledSpeedBuff = std::max(0, static_cast<int>(std::lround(
        static_cast<float>(ability.speedBuff) * std::max(0.0f, presentationMultiplier)
    )));

    // presentationMultiplier carries the presentation's resolved scaling for persistent buffs:
    //   > 1.0 → stronger positive buff
    //   < 1.0 and >= 0 → partial buff
    //   < 0 → nerf
    // The authored JSON stores the max-correct buff value and we scale it by the presentation
    // multiplier so partial results map cleanly onto both ATK% and DMG%.
    const int scaledAtkBuff = (ability.atkBuff != 0)
        ? static_cast<int>(std::lround(static_cast<float>(ability.atkBuff) * presentationMultiplier))
        : 0;
    const int scaledDamageBuff = (ability.damageBuff != 0)
        ? static_cast<int>(std::lround(static_cast<float>(ability.damageBuff) * presentationMultiplier))
        : 0;

    if (scaledSpeedBuff <= 0 && scaledAtkBuff == 0 && scaledDamageBuff == 0) {
        return;
    }

    auto refreshOrInsertBuff = [&](int targetPartyIndex) {
        if (targetPartyIndex < 0 || static_cast<size_t>(targetPartyIndex) >= characters_.size()) {
            return;
        }
        if (!characters_[static_cast<size_t>(targetPartyIndex)].isAlive()) {
            return;
        }

        for (ActivePartyBuff& buff : activePartyBuffs_) {
            if (buff.abilityId == ability.id &&
                buff.sourcePartyIndex == sourcePartyIndex &&
                buff.targetPartyIndex == targetPartyIndex) {
                buff.speedBuff = scaledSpeedBuff;
                buff.atkBuff   = scaledAtkBuff;
                buff.damageBuff = scaledDamageBuff;
                return;
            }
        }

        activePartyBuffs_.push_back(ActivePartyBuff{
            ability.id,
            sourcePartyIndex,
            targetPartyIndex,
            scaledSpeedBuff,
            scaledAtkBuff,
            scaledDamageBuff
        });
    };

    if (ability.targetRule == TargetRule::Self) {
        refreshOrInsertBuff(sourcePartyIndex);
    } else if (ability.targetRule == TargetRule::SingleAlly) {
        refreshOrInsertBuff(resolveSupportTargetPartyIndex(ability.targetRule, -1));
    } else if (ability.targetRule == TargetRule::AllAllies) {
        for (const BattleCharacter& character : characters_) {
            refreshOrInsertBuff(character.partyIndex());
        }
    }

    removeBuffsFromDefeatedCharacters();
    refreshCharacterBuffBonuses();
    refreshAllTurnActorSpeeds();
}

void BattleManager::expireBuffsFromCaster(int sourcePartyIndex) {
    if (sourcePartyIndex < 0) {
        return;
    }

    const auto newEnd = std::remove_if(
        activePartyBuffs_.begin(),
        activePartyBuffs_.end(),
        [sourcePartyIndex](const ActivePartyBuff& buff) {
            return buff.sourcePartyIndex == sourcePartyIndex;
        }
    );
    if (newEnd == activePartyBuffs_.end()) {
        return;
    }

    activePartyBuffs_.erase(newEnd, activePartyBuffs_.end());
    refreshCharacterBuffBonuses();
    refreshAllTurnActorSpeeds();
}

void BattleManager::removeBuffsFromDefeatedCharacters() {
    if (isSailorVenusPartyIndex(sailorVenusState_.partyIndex) &&
        static_cast<size_t>(sailorVenusState_.partyIndex) < characters_.size() &&
        !characters_[static_cast<size_t>(sailorVenusState_.partyIndex)].isAlive()) {
        revertSailorVenusTransformation(false);
    }

    const auto newEnd = std::remove_if(
        activePartyBuffs_.begin(),
        activePartyBuffs_.end(),
        [this](const ActivePartyBuff& buff) {
            const bool validSource =
                buff.sourcePartyIndex >= 0 &&
                static_cast<size_t>(buff.sourcePartyIndex) < characters_.size() &&
                characters_[static_cast<size_t>(buff.sourcePartyIndex)].isAlive();
            const bool validTarget =
                buff.targetPartyIndex >= 0 &&
                static_cast<size_t>(buff.targetPartyIndex) < characters_.size() &&
                characters_[static_cast<size_t>(buff.targetPartyIndex)].isAlive();
            return !validSource || !validTarget;
        }
    );
    if (newEnd == activePartyBuffs_.end()) {
        removeBossDebuffsFromDefeatedCharacters();
        return;
    }

    activePartyBuffs_.erase(newEnd, activePartyBuffs_.end());
    refreshCharacterBuffBonuses();
    refreshAllTurnActorSpeeds();
    removeBossDebuffsFromDefeatedCharacters();
}

void BattleManager::removeBossDebuffsFromDefeatedCharacters() {
    const auto newEnd = std::remove_if(
        activeBossDebuffs_.begin(),
        activeBossDebuffs_.end(),
        [this](const ActiveBossDebuff& debuff) {
            if (debuff.charges <= 0 || bossCurrentHp_ <= 0) {
                return true;
            }
            if (debuff.sourcePartyIndex < 0 ||
                static_cast<size_t>(debuff.sourcePartyIndex) >= characters_.size()) {
                return true;
            }
            return !characters_[static_cast<size_t>(debuff.sourcePartyIndex)].isAlive();
        }
    );

    if (newEnd == activeBossDebuffs_.end()) {
        return;
    }

    activeBossDebuffs_.erase(newEnd, activeBossDebuffs_.end());
}

void BattleManager::refreshCharacterBuffBonuses() {
    std::vector<int> speedTotals(characters_.size(), 0);
    std::vector<int> atkTotals(characters_.size(), 0);
    std::vector<int> damageTotals(characters_.size(), 0);
    for (const ActivePartyBuff& buff : activePartyBuffs_) {
        if (buff.targetPartyIndex < 0 || static_cast<size_t>(buff.targetPartyIndex) >= speedTotals.size()) {
            continue;
        }
        speedTotals[static_cast<size_t>(buff.targetPartyIndex)] += buff.speedBuff;
        atkTotals[static_cast<size_t>(buff.targetPartyIndex)]   += buff.atkBuff;
        damageTotals[static_cast<size_t>(buff.targetPartyIndex)] += buff.damageBuff;
    }

    if (isSailorVenusTransformed(sailorVenusState_.partyIndex) &&
        static_cast<size_t>(sailorVenusState_.partyIndex) < characters_.size()) {
        speedTotals[static_cast<size_t>(sailorVenusState_.partyIndex)] += 65;
        atkTotals[static_cast<size_t>(sailorVenusState_.partyIndex)] += 40;
    }

    for (size_t i = 0; i < characters_.size(); ++i) {
        characters_[i].setSpdBuffBonus(speedTotals[i]);
        characters_[i].setAtkBuffBonus(atkTotals[i]);
        characters_[i].setDamageBuffBonus(damageTotals[i]);
    }
}

void BattleManager::refreshTurnActorSpeed(int partyIndex) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return;
    }

    const int newSpd = characters_[static_cast<size_t>(partyIndex)].effectiveSpd();
    for (TurnActor& actor : turnState_.actors) {
        if (actor.type != ParticipantType::Character || actor.partyIndex != partyIndex) {
            continue;
        }

        const int oldSpd = std::max(1, actor.spd);
        if (oldSpd == newSpd) {
            continue;
        }

        actor.currentActionValue *= static_cast<float>(oldSpd) / static_cast<float>(newSpd);
        if (std::fabs(actor.currentActionValue) <= 0.0001f) {
            actor.currentActionValue = 0.0f;
        }
        actor.spd = newSpd;
        actor.baseActionValue = turn::actionValueFromSpeed(newSpd);
    }
}

void BattleManager::refreshTurnActorAssets(int partyIndex) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= state_.party.size()) {
        return;
    }

    for (TurnActor& actor : turnState_.actors) {
        if (actor.type != ParticipantType::Character || actor.partyIndex != partyIndex) {
            continue;
        }

        actor.assetId = resolveCharacterAssetId(partyIndex, actor.abilityKitOverride);
    }
}

void BattleManager::refreshAllTurnActorSpeeds() {
    for (size_t i = 0; i < characters_.size(); ++i) {
        refreshTurnActorSpeed(static_cast<int>(i));
    }
}

/**
 * @brief Advances the turn progress of all alive, primary ally actors by a fractional amount.
 *
 * Clamps `fraction` to the [0, 1] range and reduces each eligible character actor's
 * `currentActionValue` multiplicatively by `(1 - clampedFraction)`. Only primary
 * (non-extra) character actors with a valid party index and alive characters are affected.
 * Actors whose action value becomes effectively zero are assigned a non-zero `priority`
 * so they are ordered for immediate execution; others have their `priority` cleared.
 *
 * Candidates are evaluated in deterministic rank order: lower original action value first,
 * then higher original priority, then lower party index.
 *
 * @param fraction Fraction in [0,1] representing the portion of action progress to advance.
 */
void BattleManager::applyAllAlliesActionAdvance(float fraction, ActionAdvanceMode mode) {
    const float clampedFraction = std::clamp(fraction, 0.0f, 1.0f);
    if (clampedFraction <= 0.0f) {
        return;
    }

    struct ActionAdvanceCandidate {
        size_t actorIndex = 0;
        float originalActionValue = 0.0f;
        float baseActionValue = 0.0f;
        int originalPriority = 0;
        int partyIndex = -1;
    };

    std::vector<ActionAdvanceCandidate> candidates;
    for (size_t i = 0; i < turnState_.actors.size(); ++i) {
        const TurnActor& actor = turnState_.actors[i];
        if (actor.type != ParticipantType::Character ||
            actor.isExtraTurn ||
            actor.partyIndex < 0 ||
            static_cast<size_t>(actor.partyIndex) >= characters_.size() ||
            !characters_[static_cast<size_t>(actor.partyIndex)].isAlive()) {
            continue;
        }

        candidates.push_back(ActionAdvanceCandidate{
            i,
            actor.currentActionValue,
            actor.baseActionValue,
            actor.priority,
            actor.partyIndex
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const ActionAdvanceCandidate& lhs,
                                                       const ActionAdvanceCandidate& rhs) {
        if (std::fabs(lhs.originalActionValue - rhs.originalActionValue) > 0.0001f) {
            return lhs.originalActionValue < rhs.originalActionValue;
        }
        if (lhs.originalPriority != rhs.originalPriority) {
            return lhs.originalPriority > rhs.originalPriority;
        }
        return lhs.partyIndex < rhs.partyIndex;
    });

    constexpr int kActionAdvancePriorityBase = 50;
    for (size_t rank = 0; rank < candidates.size(); ++rank) {
        TurnActor& actor = turnState_.actors[candidates[rank].actorIndex];
        switch (mode) {
            case ActionAdvanceMode::BaseActionValueDelta:
                actor.currentActionValue = std::max(
                    0.0f,
                    actor.currentActionValue - (clampedFraction * std::max(0.0f, candidates[rank].baseActionValue))
                );
                break;
            case ActionAdvanceMode::RemainingFraction:
            default:
                actor.currentActionValue = std::max(0.0f, actor.currentActionValue * (1.0f - clampedFraction));
                break;
        }
        if (std::fabs(actor.currentActionValue) <= kActionValueEpsilon) {
            actor.currentActionValue = 0.0f;
        }
        actor.priority = (actor.currentActionValue <= 0.0001f)
            ? (kActionAdvancePriorityBase + static_cast<int>(candidates.size() - rank))
            : 0;
    }
}

void BattleManager::applyCharacterActionAdvance(int partyIndex, float fraction, ActionAdvanceMode mode) {
    const float clampedFraction = std::clamp(fraction, 0.0f, 1.0f);
    if (clampedFraction <= 0.0f ||
        partyIndex < 0 ||
        static_cast<size_t>(partyIndex) >= characters_.size() ||
        !characters_[static_cast<size_t>(partyIndex)].isAlive()) {
        return;
    }

    for (TurnActor& actor : turnState_.actors) {
        if (actor.type != ParticipantType::Character ||
            actor.isExtraTurn ||
            actor.partyIndex != partyIndex) {
            continue;
        }

        switch (mode) {
            case ActionAdvanceMode::BaseActionValueDelta:
                actor.currentActionValue = std::max(
                    0.0f,
                    actor.currentActionValue - (clampedFraction * std::max(0.0f, actor.baseActionValue))
                );
                break;
            case ActionAdvanceMode::RemainingFraction:
            default:
                actor.currentActionValue = std::max(0.0f, actor.currentActionValue * (1.0f - clampedFraction));
                break;
        }

        if (std::fabs(actor.currentActionValue) <= kActionValueEpsilon) {
            actor.currentActionValue = 0.0f;
        }
        actor.priority = actor.currentActionValue <= 0.0001f ? 120 : 0;
        return;
    }
}

/**
 * @brief Reduces the boss primary actor's remaining action value by a fraction.
 *
 * Clamps `fraction` to the range [0, 1]. If the clamped fraction is 0, no change is made.
 * Finds the first non-extra boss turn actor, multiplies its `currentActionValue` by
 * (1 - clamped fraction), clamps the result to a minimum of 0, and sets its `priority` to 0.
 * If the resulting `currentActionValue` is within `kActionValueEpsilon` of zero, it is set to 0.
 *
 * @param fraction Fraction of the boss's remaining action value to reduce (clamped to [0, 1]).
 */
void BattleManager::advanceBossActionByFraction(float fraction) {
    const float clampedFraction = std::clamp(fraction, 0.0f, 1.0f);
    if (clampedFraction <= 0.0f) {
        return;
    }

    for (TurnActor& actor : turnState_.actors) {
        if (actor.type != ParticipantType::Boss || actor.isExtraTurn) {
            continue;
        }

        actor.currentActionValue = std::max(0.0f, actor.currentActionValue * (1.0f - clampedFraction));
        if (std::fabs(actor.currentActionValue) <= kActionValueEpsilon) {
            actor.currentActionValue = 0.0f;
        }
        actor.priority = 0;
        return;
    }
}

/**
 * @brief Retrieves the currently active boss phase definition.
 *
 * If the battle state lacks phase data or the stored phase index is invalid, a stable static default phase definition is returned.
 *
 * @return const BossDefinition::PhaseDefinition& Reference to the active phase definition, or a static default when unavailable.
 */
const BossDefinition::PhaseDefinition& BattleManager::currentBossPhaseDefinition() const {
    static const BossDefinition::PhaseDefinition kDefaultPhase{};
    if (!state_.boss.hasPhaseData ||
        bossPhaseIndex_ < 0 ||
        static_cast<size_t>(bossPhaseIndex_) >= state_.boss.phases.size()) {
        return kDefaultPhase;
    }
    return state_.boss.phases[static_cast<size_t>(bossPhaseIndex_)];
}

/**
 * @brief Checks boss HP against phase thresholds and transitions to a higher boss phase when needed.
 *
 * When the boss has phase data and is still alive, evaluates current boss HP as a percentage of
 * the boss maximum to determine a target phase (HP ≤ 33% → phase 2, HP ≤ 66% → phase 1, otherwise phase 0).
 * If the target phase is greater than the current phase, updates the active phase index and the
 * boss attack buff for the new phase, advances the boss's action progress to the start of the new phase,
 * and queues a boss phase-introduction turn.
 */
void BattleManager::applyBossPhaseTransitionIfNeeded() {
    if (!state_.boss.hasPhaseData || bossCurrentHp_ <= 0 || state_.boss.hp <= 0) {
        return;
    }

    int targetPhaseIndex = 0;
    const long long hpScaled = static_cast<long long>(bossCurrentHp_) * 100LL;
    const long long maxScaled = static_cast<long long>(state_.boss.hp);
    if (hpScaled <= maxScaled * 33LL) {
        targetPhaseIndex = 2;
    } else if (hpScaled <= maxScaled * 66LL) {
        targetPhaseIndex = 1;
    }

    if (targetPhaseIndex <= bossPhaseIndex_) {
        return;
    }

    const int previousPhaseIndex = bossPhaseIndex_;
    bossPhaseIndex_ = targetPhaseIndex;
    bossAtkBuffBonus_ = currentBossPhaseDefinition().atkBonusPercent;
    advanceBossActionByFraction(1.0f);
    queueBossPhaseIntroTurn(previousPhaseIndex, bossPhaseIndex_);
}

/**
 * @brief Synchronizes a party member's participation in the turn actor list with their alive state.
 *
 * Ensures the turn state reflects whether the character at `partyIndex` should participate:
 * - If `partyIndex` is invalid this function does nothing.
 * - If the character is dead, removes their primary (non-extra) turn actors while preserving any extra-turn actors.
 * - If the character is alive, guarantees exactly one primary (non-extra) turn actor exists for that party member and removes any additional duplicate primary actors.
 *
 * @param partyIndex Index of the party member in the current character list.
 */
void BattleManager::syncCharacterTurnParticipation(int partyIndex) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return;
    }

    const BattleCharacter& character = characters_[static_cast<size_t>(partyIndex)];
    const bool shouldParticipate = character.isAlive();
    bool hasPrimaryTurn = false;

    for (size_t i = 0; i < turnState_.actors.size();) {
        const TurnActor& actor = turnState_.actors[i];
        const bool matchesCharacter =
            actor.type == ParticipantType::Character && actor.partyIndex == partyIndex;
        if (!matchesCharacter) {
            ++i;
            continue;
        }

        if (!shouldParticipate) {
            turnState_.actors.erase(turnState_.actors.begin() + static_cast<long>(i));
            continue;
        }

        if (actor.isExtraTurn) {
            ++i;
            continue;
        }

        if (!hasPrimaryTurn) {
            hasPrimaryTurn = true;
            ++i;
            continue;
        }

        turnState_.actors.erase(turnState_.actors.begin() + static_cast<long>(i));
    }

    if (!shouldParticipate || hasPrimaryTurn) {
        if (shouldParticipate) {
            refreshTurnActorAssets(partyIndex);
        }
        return;
    }

    turnState_.actors.push_back(
        makePrimaryCharacterTurnActor(character)
    );
    refreshTurnActorAssets(partyIndex);
}

void BattleManager::syncAllCharacterTurnParticipation() {
    for (size_t i = 0; i < characters_.size(); ++i) {
        const int partyIndex = static_cast<int>(i);
        syncCharacterTurnParticipation(partyIndex);
        syncCharacterUltimateTurn(partyIndex);
    }
}

/**
 * @brief Synchronizes queued extra-turn ultimates for a single party member.
 *
 * Ensures at most one non-auto-executing extra-turn actor representing an ultimate remains
 * for the given party index when that character is alive and can use their ultimate;
 * otherwise removes all such queued ultimate extra-turn actors for that party member.
 *
 * @param partyIndex Index of the party member whose ultimate extra-turn queue should be synchronized; no-op if out of range.
 */
void BattleManager::syncCharacterUltimateTurn(int partyIndex) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return;
    }

    const BattleCharacter& character = characters_[static_cast<size_t>(partyIndex)];
    const bool shouldKeepExtraTurn =
        character.isAlive() &&
        character.canUseUltimate() &&
        !resolveCharacterAbilityId(partyIndex, BattleAction::Ultimate).empty();
    bool hasExtraTurn = false;

    for (size_t i = 0; i < turnState_.actors.size();) {
        const TurnActor& actor = turnState_.actors[i];
        const bool matchesExtraTurn =
            actor.type == ParticipantType::Character &&
            actor.partyIndex == partyIndex &&
            actor.isExtraTurn &&
            actor.extraTurnAction == BattleAction::Ultimate &&
            !actor.autoExecute;
        if (!matchesExtraTurn) {
            ++i;
            continue;
        }

        if (!shouldKeepExtraTurn || hasExtraTurn) {
            turnState_.actors.erase(turnState_.actors.begin() + static_cast<long>(i));
            continue;
        }

        hasExtraTurn = true;
        ++i;
    }
}

/**
 * @brief Finds the party index of a character by its definition key.
 *
 * @param characterKey Character definition key to locate.
 * @return int Party index of the matching character, or `-1` if no character with the given key is present.
 */
int BattleManager::findCharacterPartyIndexByKey(const std::string& characterKey) const {
    for (const BattleCharacter& character : characters_) {
        if (character.definition().key == characterKey) {
            return character.partyIndex();
        }
    }
    return -1;
}

bool BattleManager::isSingerPartyMember(int partyIndex) const {
    return partyIndex >= 0 &&
        static_cast<size_t>(partyIndex) < characters_.size() &&
        characters_[static_cast<size_t>(partyIndex)].definition().isSinger;
}

int BattleManager::getSingerCountInParty() const {
    int singerCount = 0;
    for (const BattleCharacter& character : characters_) {
        if (character.definition().isSinger) {
            ++singerCount;
        }
    }
    return singerCount;
}

bool BattleManager::hasQueuedExtraTurn(int partyIndex, BattleAction action, bool autoExecute) const {
    for (const TurnActor& actor : turnState_.actors) {
        if (actor.type != ParticipantType::Character ||
            actor.partyIndex != partyIndex ||
            !actor.isExtraTurn) {
            continue;
        }

        if (actor.extraTurnAction == action && actor.autoExecute == autoExecute) {
            return true;
        }
    }
    return false;
}

std::vector<int> BattleManager::collectSupportTargetPartyIndices(int sourcePartyIndex,
                                                                 const AbilityDefinition& ability,
                                                                 int requestedTargetPartyIndex) const {
    std::vector<int> targets;
    const auto appendIfValid = [this, &targets](int partyIndex) {
        if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
            return;
        }
        if (std::find(targets.begin(), targets.end(), partyIndex) == targets.end()) {
            targets.push_back(partyIndex);
        }
    };

    switch (ability.targetRule) {
        case TargetRule::Self:
            appendIfValid(sourcePartyIndex);
            break;
        case TargetRule::SingleAlly:
            appendIfValid(resolveSupportTargetPartyIndex(ability.targetRule, requestedTargetPartyIndex));
            break;
        case TargetRule::AllAllies:
            for (const BattleCharacter& character : characters_) {
                appendIfValid(character.partyIndex());
            }
            break;
        default:
            break;
    }

    return targets;
}

bool BattleManager::isSailorVenusPartyIndex(int partyIndex) const {
    return partyIndex >= 0 &&
        partyIndex == sailorVenusState_.partyIndex &&
        static_cast<size_t>(partyIndex) < characters_.size() &&
        characters_[static_cast<size_t>(partyIndex)].definition().key == "sailorVenus";
}

bool BattleManager::isSailorVenusTransformed(int partyIndex) const {
    return sailorVenusState_.transformed && isSailorVenusPartyIndex(partyIndex);
}

void BattleManager::activateSailorVenusTransformation(int partyIndex) {
    if (!isSailorVenusPartyIndex(partyIndex) ||
        static_cast<size_t>(partyIndex) >= characters_.size() ||
        !characters_[static_cast<size_t>(partyIndex)].isAlive()) {
        return;
    }

    sailorVenusState_.transformed = true;
    sailorVenusState_.turnsRemaining = 3;
    sailorVenusState_.spaceTally = 0;
    sailorVenusState_.finisherQueued = false;

    (void)setCharacterAbilityKit(partyIndex, "transformed");
    refreshCharacterBuffBonuses();
    refreshAllTurnActorSpeeds();
    refreshTurnActorAssets(partyIndex);
    applyCharacterActionAdvance(partyIndex, 1.0f, ActionAdvanceMode::RemainingFraction);
}

void BattleManager::revertSailorVenusTransformation(bool advanceNextAction) {
    if (!sailorVenusState_.transformed || sailorVenusState_.partyIndex < 0) {
        sailorVenusState_.spaceTally = 0;
        sailorVenusState_.turnsRemaining = 0;
        sailorVenusState_.finisherQueued = false;
        return;
    }

    const int partyIndex = sailorVenusState_.partyIndex;
    sailorVenusState_.transformed = false;
    sailorVenusState_.turnsRemaining = 0;
    sailorVenusState_.spaceTally = 0;
    sailorVenusState_.finisherQueued = false;

    clearCharacterAbilityKit(partyIndex);
    refreshCharacterBuffBonuses();
    refreshAllTurnActorSpeeds();
    refreshTurnActorAssets(partyIndex);
    if (advanceNextAction) {
        applyCharacterActionAdvance(partyIndex, 1.0f, ActionAdvanceMode::RemainingFraction);
    }
}

void BattleManager::consumeSailorVenusTurnAndQueueFinisherIfNeeded(int partyIndex) {
    if (!isSailorVenusTransformed(partyIndex) || sailorVenusState_.turnsRemaining <= 0) {
        return;
    }

    --sailorVenusState_.turnsRemaining;
    if (sailorVenusState_.turnsRemaining > 0 || sailorVenusState_.finisherQueued || bossCurrentHp_ <= 0) {
        return;
    }

    queueExtraTurnForCharacter(
        partyIndex,
        BattleAction::Skill,
        true,
        false,
        "loveMeChain",
        250
    );
    sailorVenusState_.finisherQueued = true;
}

int BattleManager::getSailorVenusSpaceTally() const {
    return std::max(0, sailorVenusState_.spaceTally);
}

void BattleManager::applyBossDebuffCharges(int sourcePartyIndex,
                                           const AbilityDefinition& ability,
                                           int charges) {
    if (charges <= 0 || sourcePartyIndex < 0 || static_cast<size_t>(sourcePartyIndex) >= characters_.size()) {
        return;
    }

    for (ActiveBossDebuff& debuff : activeBossDebuffs_) {
        if (debuff.abilityId == ability.id && debuff.sourcePartyIndex == sourcePartyIndex) {
            debuff.charges = charges;
            removeBossDebuffsFromDefeatedCharacters();
            return;
        }
    }

    activeBossDebuffs_.push_back(ActiveBossDebuff{ability.id, sourcePartyIndex, charges});
    removeBossDebuffsFromDefeatedCharacters();
}

int BattleManager::getBossDebuffCharges(const std::string& abilityId) const {
    for (const ActiveBossDebuff& debuff : activeBossDebuffs_) {
        if (debuff.abilityId == abilityId) {
            return debuff.charges;
        }
    }
    return 0;
}

void BattleManager::consumeBossDebuffCharge(const std::string& abilityId) {
    for (ActiveBossDebuff& debuff : activeBossDebuffs_) {
        if (debuff.abilityId != abilityId) {
            continue;
        }

        --debuff.charges;
        break;
    }

    removeBossDebuffsFromDefeatedCharacters();
}

void BattleManager::applyJiafeiUltimateDebuff(int sourcePartyIndex, const AbilityDefinition& ability) {
    applyBossDebuffCharges(sourcePartyIndex, ability, 2);
}

void BattleManager::consumeJiafeiUltimateDebuff() {
    activeBossDebuffs_.erase(
        std::remove_if(activeBossDebuffs_.begin(),
                       activeBossDebuffs_.end(),
                       [](const ActiveBossDebuff& debuff) {
                           return debuff.abilityId == "MeiCiDuXiangZhuang";
                       }),
        activeBossDebuffs_.end());
}

void BattleManager::tryQueueJiafeiFollowUp(const BattleActionEvent& actionEvent) {
    if (getBossDebuffCharges("MeiCiDuXiangZhuang") <= 0 ||
        bossCurrentHp_ <= 0 ||
        actionEvent.actorType != ParticipantType::Character ||
        actionEvent.actorKey == "jiafei" ||
        actionEvent.bossHpAfter >= actionEvent.bossHpBefore) {
        return;
    }

    const int jiafeiPartyIndex = findCharacterPartyIndexByKey("jiafei");
    if (jiafeiPartyIndex < 0 ||
        static_cast<size_t>(jiafeiPartyIndex) >= characters_.size() ||
        !characters_[static_cast<size_t>(jiafeiPartyIndex)].isAlive()) {
        return;
    }

    queueExtraTurnForCharacter(
        jiafeiPartyIndex,
        BattleAction::Skill,
        true,
        true,
        "followUp",
        200
    );

    consumeBossDebuffCharge("MeiCiDuXiangZhuang");
    if (getBossDebuffCharges("MeiCiDuXiangZhuang") <= 0) {
        consumeJiafeiUltimateDebuff();
    }
}

void BattleManager::tryQueueZhouShenFollowUp(const BattleActionEvent& actionEvent) {
    if (bossCurrentHp_ <= 0 ||
        actionEvent.actorType != ParticipantType::Character ||
        actionEvent.actorKey == "zhouShen" ||
        (actionEvent.action != BattleAction::Skill && actionEvent.action != BattleAction::Ultimate) ||
        !isSingerPartyMember(actionEvent.actorPartyIndex)) {
        return;
    }

    const int zhouShenPartyIndex = findCharacterPartyIndexByKey("zhouShen");
    if (zhouShenPartyIndex < 0 ||
        static_cast<size_t>(zhouShenPartyIndex) >= characters_.size() ||
        !characters_[static_cast<size_t>(zhouShenPartyIndex)].isAlive()) {
        return;
    }

    queueExtraTurnForCharacter(
        zhouShenPartyIndex,
        BattleAction::Skill,
        true,
        true,
        "followUp",
        200
    );
}

void BattleManager::tryQueueSailorVenusFollowUp(const BattleActionEvent& actionEvent) {
    if (!isSailorVenusTransformed(sailorVenusState_.partyIndex) ||
        bossCurrentHp_ <= 0 ||
        actionEvent.actorType != ParticipantType::Character ||
        actionEvent.actorPartyIndex < 0 ||
        actionEvent.actorPartyIndex == sailorVenusState_.partyIndex ||
        static_cast<size_t>(sailorVenusState_.partyIndex) >= characters_.size() ||
        !characters_[static_cast<size_t>(sailorVenusState_.partyIndex)].isAlive()) {
        return;
    }

    const AbilityDefinition* ability = getAbility(actionEvent.abilityId);
    if (ability == nullptr ||
        (ability->type != AbilityType::Heal &&
         ability->type != AbilityType::Shield &&
         ability->type != AbilityType::Buff)) {
        return;
    }

    if (std::find(actionEvent.targetPartyIndices.begin(),
                  actionEvent.targetPartyIndices.end(),
                  sailorVenusState_.partyIndex) == actionEvent.targetPartyIndices.end()) {
        return;
    }

    queueExtraTurnForCharacter(
        sailorVenusState_.partyIndex,
        BattleAction::Skill,
        true,
        false,
        "crescentBeam",
        225
    );
}

} // namespace battle
