
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
#include <iostream>
#include <sstream>

namespace battle {
namespace {

constexpr const char* kDefaultCharacterStandardAbilityId = "BasicAttack";
constexpr const char* kDefaultBossStandardAbilityId = "BossStandardAttack";

std::string getCharacterRegularAbilityId(const CharacterDefinition& definition) {
    if (!definition.ability.empty()) {
        return definition.ability;
    }
    if (!definition.skillAbility.empty()) {
        return definition.skillAbility;
    }
    return definition.standardAbility.empty() ? std::string{kDefaultCharacterStandardAbilityId} : definition.standardAbility;
}

std::string getBossStandardAbilityId(const BossDefinition& definition) {
    return definition.standardAbility.empty() ? std::string{kDefaultBossStandardAbilityId} : definition.standardAbility;
}

std::string getBossNormalAbilityId(const BossDefinition& definition) {
    if (!definition.ability.empty()) {
        return definition.ability;
    }
    if (!definition.skillAbility.empty()) {
        return definition.skillAbility;
    }
    return getBossStandardAbilityId(definition);
}

bool consumeInvalidPreviewCharacterTurn(const std::vector<BattleCharacter>& characters,
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

    if (previewActor.isExtraTurn && previewCharacter.isAlive()) {
        switch (previewActor.extraTurnAction) {
            case BattleAction::Standard:
            case BattleAction::Skill:
                if (previewCharacter.canUseSkill()) {
                    return false;
                }
                break;
            case BattleAction::Ultimate:
                if (previewCharacter.canUseUltimate()) {
                    return false;
                }
                break;
        }
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

void BattleCharacter::setAtkBuffBonus(int percentBonus) {
    atkBuffBonus_ = percentBonus;
}

bool BattleManager::initialize(const std::string& bossKey, const std::vector<std::string>& characterKeys) {
    initialized_ = false;
    state_ = BattleState{};
    turnState_ = TurnState{};
    bossCurrentHp_ = 0;
    bossUltimateCharge_ = 0;
    characters_.clear();
    recentActionEvents_.clear();
    bossStatus_ = BossStatusState{};
    simulatedActions_ = 0;
    activePartyBuffs_.clear();
    luotianyiCorrectTones_ = 0;

    if (bossKey.empty()) {
        std::cerr << "[Battle] Missing boss key.\n";
        return false;
    }

    // if (characterKeys.empty() || characterKeys.size() > 4) {
    //     std::cerr << "[Battle] Party size must be between 1 and 4.\n";
    //     return false;
    // }

    if (!loader::loadBossDefinition(bossKey, state_.boss)) {
        return false;
    }

    state_.party.reserve(characterKeys.size());
    for (const std::string& key : characterKeys) {
        CharacterDefinition character;
        if (!loader::loadCharacterDefinition(key, character)) {
            return false;
        }
        state_.party.push_back(character);
    }

    bossCurrentHp_ = state_.boss.hp;
    bossUltimateCharge_ = std::clamp(state_.boss.startingOrbs, 0, std::max(1, state_.boss.ultimatePoints));
    for (size_t i = 0; i < state_.party.size(); ++i) {
        characters_.emplace_back(state_.party[i], static_cast<int>(i));
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

int BattleManager::getBossMaxHp() const {
    return state_.boss.hp;
}

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

bool BattleManager::isCharacterAlive(int partyIndex) const {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return false;
    }
    return characters_[static_cast<size_t>(partyIndex)].isAlive();
}

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

void BattleManager::addLuotianyiCorrectTones(int amount) {
    if (amount <= 0) {
        return;
    }
    luotianyiCorrectTones_ += amount;
}

int BattleManager::getLuotianyiCorrectTones() const {
    return std::max(0, luotianyiCorrectTones_);
}

void BattleManager::applyPresentationHitDamage(bool isBossCaster,
                                               int perHitDamage,
                                               int hitEvents,
                                               int targetPartyIndex) {
    if (perHitDamage <= 0 || hitEvents <= 0 || isBattleOver()) {
        return;
    }

    const int totalDamage = std::max(1, perHitDamage) * std::max(1, hitEvents);
    if (isBossCaster) {
        if (targetPartyIndex >= 0 && static_cast<size_t>(targetPartyIndex) < characters_.size()) {
            BattleCharacter& target = characters_[static_cast<size_t>(targetPartyIndex)];
            if (!target.isAlive()) {
                return;
            }

            target.receiveDamage(totalDamage);
            syncCharacterTurnParticipation(targetPartyIndex);
            syncCharacterUltimateTurn(targetPartyIndex);
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
            removeBuffsFromDefeatedCharacters();
            presentationHitDamageApplied_ = true;
        }
        return;
    }

    if (bossCurrentHp_ > 0) {
        bossCurrentHp_ = std::max(0, bossCurrentHp_ - totalDamage);
        presentationHitDamageApplied_ = true;
    }
}

void BattleManager::applyPresentationHealing(bool isBossCaster, int perHitHeal, int hitEvents, bool reviveDeadAllies) {
    if (perHitHeal <= 0 || hitEvents <= 0 || isBattleOver()) {
        return;
    }

    const int totalHeal = std::max(1, perHitHeal) * std::max(1, hitEvents);

    if (isBossCaster) {
        if (bossCurrentHp_ <= 0) {
            return;
        }
        bossCurrentHp_ = std::min(state_.boss.hp, bossCurrentHp_ + totalHeal);
        presentationHealingApplied_ = true;
        return;
    }

    bool applied = false;
    for (BattleCharacter& c : characters_) {
        const int hpBefore = c.hp();
        if (c.isAlive()) {
            c.receiveHealing(totalHeal);
        } else if (reviveDeadAllies) {
            c.revive(totalHeal);
        }
        if (c.hp() > hpBefore) {
            applied = true;
        }
    }

    if (applied) {
        syncAllCharacterTurnParticipation();
        presentationHealingApplied_ = true;
    }
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
        if (!character.isAlive()) {
            return false;
        }

        switch (actor.extraTurnAction) {
            case BattleAction::Standard:
            case BattleAction::Skill:
                return action == actor.extraTurnAction && character.canUseSkill();
            case BattleAction::Ultimate:
                return action == BattleAction::Ultimate && character.canUseUltimate();
        }
    }

    switch (action) {
        case BattleAction::Standard:
            return character.isAlive() && character.canUseSkill();
        case BattleAction::Skill:
            return character.isAlive() && character.canUseSkill();
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

    const BattleCharacter& character = characters_[static_cast<size_t>(actor.partyIndex)];
    if (!character.isAlive()) {
        return false;
    }

    const AbilityDefinition* abilityDef = getAbility(getCharacterRegularAbilityId(character.definition()));

    if (abilityDef == nullptr || abilityDef->type != AbilityType::Attack) {
        return false;
    }

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
        character.effectiveAtk() * abilityDef->multiplier * multiplier
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

void BattleManager::applyBossSplitHitDamage(int damage) {
    if (damage <= 0 || isBattleOver()) {
        return;
    }

    bossCurrentHp_ = std::max(0, bossCurrentHp_ - damage);
}

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

    character.gainUltimatePoint();
    if (character.canUseUltimate()) {
        bool alreadyQueued = false;
        for (const TurnActor& actorCandidate : turnState_.actors) {
            if (actorCandidate.type == ParticipantType::Character &&
                actorCandidate.isExtraTurn &&
                actorCandidate.partyIndex == character.partyIndex()) {
                alreadyQueued = true;
                break;
            }
        }
        if (!alreadyQueued) {
            queueExtraTurnForCharacter(character.partyIndex());
        }
    }

    if (event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }
    turnState_.actors[event.actingActorIndex].currentActionValue = turnState_.actors[event.actingActorIndex].baseActionValue;
    turnState_.actors[event.actingActorIndex].priority = 0;
    syncCharacterUltimateTurn(character.partyIndex());

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

bool BattleManager::processAutomaticTurns() {
    bool progressed = false;

    while (!isBattleOver()) {
        const TurnEvent next = peekNextTurnEvent();
        if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
            break;
        }

        const TurnActor& nextActor = turnState_.actors[next.actingActorIndex];
        if (nextActor.type == ParticipantType::Boss) {
            if (!resolveBossAction()) {
                break;
            }
            progressed = true;
            continue;
        }

        if (!(nextActor.isExtraTurn && nextActor.autoExecute)) {
            break;
        }

        if (nextActor.partyIndex < 0 || static_cast<size_t>(nextActor.partyIndex) >= characters_.size()) {
            break;
        }

        BattleCharacter& character = characters_[static_cast<size_t>(nextActor.partyIndex)];
        if (!character.isAlive()) {
            if (!consumeInvalidPreviewCharacterTurn(characters_, turnState_, next)) {
                break;
            }
            progressed = true;
            continue;
        }

        const TurnEvent event = advanceToNextTurnEvent();
        if (!event.valid || event.actingActorIndex >= turnState_.actors.size()) {
            break;
        }

        if (!executeCharacterAction(event.actingActorIndex, character, nextActor.extraTurnAction)) {
            break;
        }
        progressed = true;
    }

    return progressed;
}

bool BattleManager::isBattleOver() const {
    return bossCurrentHp_ <= 0 || firstLivingCharacterPartyIndex() < 0;
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

void BattleManager::executeTurn(size_t actorIndex) {
    if (actorIndex >= turnState_.actors.size()) {
        return;
    }

    TurnActor actor = turnState_.actors[actorIndex];
    if (actor.type == ParticipantType::Boss) {
        executeBossAction(actorIndex, BattleAction::Standard);
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
            executeCharacterAction(actorIndex, character, actor.extraTurnAction);
        } else {
            executeCharacterAction(actorIndex, character, BattleAction::Skill);
        }
    }
}

void BattleManager::queueExtraTurnForCharacter(int partyIndex) {
    queueExtraTurnForCharacter(
        partyIndex,
        BattleAction::Ultimate,
        false,
        false,
        100
    );
}

void BattleManager::queueExtraTurnForCharacter(int partyIndex,
                                               BattleAction action,
                                               bool autoExecute,
                                               bool grantsUltimatePointOnAction,
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
        priority
    );
}

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

bool BattleManager::resolvePlayerAction(BattleAction action) {
    if (isBattleOver()) {
        return false;
    }

    while (!isBattleOver()) {
        const TurnEvent next = peekNextTurnEvent();
        if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
            return false;
        }

        if (!consumeInvalidPreviewCharacterTurn(characters_, turnState_, next)) {
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
        return consumeInvalidPreviewCharacterTurn(characters_, turnState_, next) && resolvePlayerAction(action);
    }

    if (((action == BattleAction::Standard || action == BattleAction::Skill) && !character.canUseSkill()) ||
        (action == BattleAction::Ultimate && !character.canUseUltimate())) {
        return false;
    }

    const TurnEvent event = advanceToNextTurnEvent();
    if (!event.valid || event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }

    return executeCharacterAction(
        event.actingActorIndex,
        characters_[static_cast<size_t>(previewActor.partyIndex)],
        action
    );
}

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

    return executeBossAction(event.actingActorIndex, BattleAction::Standard);
}

bool BattleManager::executeCharacterAction(size_t actorIndex, BattleCharacter& character, BattleAction action) {
    if (actorIndex >= turnState_.actors.size()) {
        return false;
    }

    ++simulatedActions_;

    const bool wasExtraTurn = turnState_.actors[actorIndex].isExtraTurn;
    const bool grantsUltimatePointOnAction = turnState_.actors[actorIndex].grantsUltimatePointOnAction;
    if (!wasExtraTurn) {
        expireBuffsFromCaster(character.partyIndex());
    }

    const std::string abilityId = (action == BattleAction::Ultimate)
        ? character.definition().ultimate
        : getCharacterRegularAbilityId(character.definition());
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

    if (abilityDef == nullptr) {
        const int fallbackDamage = normalizeDamage(
            character.effectiveAtk() * ((action == BattleAction::Ultimate) ? 2 : 1)
        );
        bossCurrentHp_ = std::max(0, bossCurrentHp_ - fallbackDamage);
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
        }
        presContext.isBoss = false;
        presContext.isUltimate = action == BattleAction::Ultimate;

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
            execContext.baseDamage = character.effectiveAtk();
            execContext.baseHeal = abilityDef->flatHeal;
            execContext.presentationMultiplier = presentationMultiplier;
            execContext.bossMaxHp = state_.boss.hp;
            executeAbilityEffect(execContext);
        }
    }
    actionEvent.bossHpAfter = bossCurrentHp_;
    actionEvent.abilityVoicesHandledDuringPresentation = consumePresentationAbilityAudioPlayed();
    actionEvent.hitVoicesHandledDuringPresentation = consumePresentationHitAudioPlayed();

    if (abilityDef != nullptr &&
        abilityDef->id == "MeiCiDuXiangZhuang" &&
        bossCurrentHp_ > 0) {
        applyJiafeiUltimateDebuff();
    }

    if (action == BattleAction::Ultimate) {
        character.consumeUltimate();
    } else if (grantsUltimatePointOnAction) {
        character.gainUltimatePoint(1);
    }

    if (wasExtraTurn) {
        turnState_.actors.erase(turnState_.actors.begin() + static_cast<long>(actorIndex));
    } else {
        turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
        turnState_.actors[actorIndex].priority = 0;
    }

    if (abilityDef != nullptr && abilityDef->type == AbilityType::Buff) {
        applyPartyBuffFromAbility(character.partyIndex(), *abilityDef, presentationMultiplier);
        if (abilityDef->actionAdvance > 0.0f) {
            applyAllAlliesActionAdvance(abilityDef->actionAdvance);
        }
    }

    syncCharacterUltimateTurn(character.partyIndex());
    tryQueueJiafeiFollowUp(actionEvent);
    recentActionEvents_.push_back(std::move(actionEvent));
    return true;
}

bool BattleManager::executeBossAction(size_t actorIndex, BattleAction action) {
    if (actorIndex >= turnState_.actors.size()) {
        return false;
    }

    ++simulatedActions_;

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
    actionEvent.targetPartyIndices.clear();
    actionEvent.targetHpBefore.clear();
    actionEvent.targetHpAfter.clear();

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
        int presentationTargetHpBefore = -1;

        // For Jiafei boss parry interaction, pick a random alive character to focus.
        if (abilityDef->id == "AestheticWarning" && abilityDef->targetRule == TargetRule::AllEnemies) {
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
            }
        } else if (abilityDef->id == "AestheticWarning" && presContext.targetIndex >= 0) {
            // Jiafei handles all damage through presentation hit events.
        } else if (abilityDef->targetRule == TargetRule::AllEnemies) {
            for (BattleCharacter& c : characters_) {
                if (!c.isAlive()) {
                    continue;
                }
                actionEvent.targetPartyIndices.push_back(c.partyIndex());
                actionEvent.targetHpBefore.push_back(c.hp());
                const int finalDamage = normalizeDamage(static_cast<int>(
                    state_.boss.atk * abilityDef->multiplier * multiplier
                ));
                c.receiveDamage(finalDamage);
                actionEvent.targetHpAfter.push_back(c.hp());
            }
            syncAllCharacterTurnParticipation();
        } else {
            const int targetIndex = firstLivingCharacterPartyIndex();
            if (targetIndex >= 0) {
                actionEvent.targetPartyIndices.push_back(targetIndex);
                actionEvent.targetHpBefore.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
                const int finalDamage = normalizeDamage(static_cast<int>(
                    state_.boss.atk * abilityDef->multiplier * multiplier
                ));
                characters_[static_cast<size_t>(targetIndex)].receiveDamage(finalDamage);
                actionEvent.targetHpAfter.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
                syncCharacterTurnParticipation(targetIndex);
            }
        }
    } else {
        const int targetIndex = firstLivingCharacterPartyIndex();
        if (targetIndex >= 0) {
            actionEvent.targetPartyIndices.push_back(targetIndex);
            actionEvent.targetHpBefore.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
            characters_[static_cast<size_t>(targetIndex)].receiveDamage(normalizeDamage(state_.boss.atk));
            actionEvent.targetHpAfter.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
            syncCharacterTurnParticipation(targetIndex);
        }
    }

    removeBuffsFromDefeatedCharacters();

    actionEvent.abilityVoicesHandledDuringPresentation = consumePresentationAbilityAudioPlayed();
    actionEvent.hitVoicesHandledDuringPresentation = consumePresentationHitAudioPlayed();

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

void BattleManager::executeAbilityEffect(const AbilityExecutionContext& context) {
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

    // presentationMultiplier carries the ATK buff ratio from the presentation:
    //   > 1.0 → buff  (e.g. 1.6 → +60% if atkBuff == 100 base)
    //   < 1.0 and >= 0 → partial buff
    //   < 0 → nerf    (the presentation returns negative multipliers for 0-correct)
    // The ability JSON stores the max-correct ATK buff percent in atkBuff.
    // We multiply by presentationMultiplier so partial results scale down.
    const int scaledAtkBuff = (ability.atkBuff != 0)
        ? static_cast<int>(std::lround(static_cast<float>(ability.atkBuff) * presentationMultiplier))
        : 0;

    if (scaledSpeedBuff <= 0 && scaledAtkBuff == 0) {
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
                return;
            }
        }

        activePartyBuffs_.push_back(ActivePartyBuff{
            ability.id,
            sourcePartyIndex,
            targetPartyIndex,
            scaledSpeedBuff,
            scaledAtkBuff
        });
    };

    if (ability.targetRule == TargetRule::Self) {
        refreshOrInsertBuff(sourcePartyIndex);
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
        return;
    }

    activePartyBuffs_.erase(newEnd, activePartyBuffs_.end());
    refreshCharacterBuffBonuses();
    refreshAllTurnActorSpeeds();
}

void BattleManager::refreshCharacterBuffBonuses() {
    std::vector<int> speedTotals(characters_.size(), 0);
    std::vector<int> atkTotals(characters_.size(), 0);
    for (const ActivePartyBuff& buff : activePartyBuffs_) {
        if (buff.targetPartyIndex < 0 || static_cast<size_t>(buff.targetPartyIndex) >= speedTotals.size()) {
            continue;
        }
        speedTotals[static_cast<size_t>(buff.targetPartyIndex)] += buff.speedBuff;
        atkTotals[static_cast<size_t>(buff.targetPartyIndex)]   += buff.atkBuff;
    }

    for (size_t i = 0; i < characters_.size(); ++i) {
        characters_[i].setSpdBuffBonus(speedTotals[i]);
        characters_[i].setAtkBuffBonus(atkTotals[i]);
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

void BattleManager::refreshAllTurnActorSpeeds() {
    for (size_t i = 0; i < characters_.size(); ++i) {
        refreshTurnActorSpeed(static_cast<int>(i));
    }
}

void BattleManager::applyAllAlliesActionAdvance(float fraction) {
    const float clampedFraction = std::clamp(fraction, 0.0f, 1.0f);
    if (clampedFraction <= 0.0f) {
        return;
    }

    struct ActionAdvanceCandidate {
        size_t actorIndex = 0;
        float originalActionValue = 0.0f;
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
        actor.currentActionValue = std::max(0.0f, actor.currentActionValue * (1.0f - clampedFraction));
        if (std::fabs(actor.currentActionValue) <= 0.0001f) {
            actor.currentActionValue = 0.0f;
        }
        actor.priority = (actor.currentActionValue <= 0.0001f)
            ? (kActionAdvancePriorityBase + static_cast<int>(candidates.size() - rank))
            : 0;
    }
}

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
        return;
    }

    turnState_.actors.push_back(
        makePrimaryCharacterTurnActor(character)
    );
}

void BattleManager::syncAllCharacterTurnParticipation() {
    for (size_t i = 0; i < characters_.size(); ++i) {
        const int partyIndex = static_cast<int>(i);
        syncCharacterTurnParticipation(partyIndex);
        syncCharacterUltimateTurn(partyIndex);
    }
}

void BattleManager::syncCharacterUltimateTurn(int partyIndex) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return;
    }

    const BattleCharacter& character = characters_[static_cast<size_t>(partyIndex)];
    const bool shouldHaveExtraTurn = character.isAlive() && character.canUseUltimate();
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

        if (!shouldHaveExtraTurn || hasExtraTurn) {
            turnState_.actors.erase(turnState_.actors.begin() + static_cast<long>(i));
            continue;
        }

        hasExtraTurn = true;
        ++i;
    }

    if (shouldHaveExtraTurn && !hasExtraTurn) {
        queueExtraTurnForCharacter(partyIndex);
    }
}

int BattleManager::findCharacterPartyIndexByKey(const std::string& characterKey) const {
    for (const BattleCharacter& character : characters_) {
        if (character.definition().key == characterKey) {
            return character.partyIndex();
        }
    }
    return -1;
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

void BattleManager::applyJiafeiUltimateDebuff() {
    bossStatus_.magicEggSpinningMachineCharges = 2;
}

void BattleManager::consumeJiafeiUltimateDebuff() {
    bossStatus_.magicEggSpinningMachineCharges = 0;
}

void BattleManager::tryQueueJiafeiFollowUp(const BattleActionEvent& actionEvent) {
    if (bossStatus_.magicEggSpinningMachineCharges <= 0 ||
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
        false,
        200
    );

    --bossStatus_.magicEggSpinningMachineCharges;
    if (bossStatus_.magicEggSpinningMachineCharges <= 0) {
        consumeJiafeiUltimateDebuff();
    }
}

} // namespace battle
