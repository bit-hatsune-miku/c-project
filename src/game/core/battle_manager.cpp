#include "battle_manager.h"
#include "ability_system.h"
#include "battle_loader.h"
#include "../presentation/ability_presentation.h"
#include "turn_system.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace battle {
namespace {

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

    if (!previewActor.isExtraTurn && characters[static_cast<size_t>(previewActor.partyIndex)].isAlive()) {
        return false;
    }

    TurnEvent consumed = turn::advanceToNextTurnEvent(turnState);
    if (!consumed.valid || consumed.actingActorIndex >= turnState.actors.size()) {
        return false;
    }

    turnState.actors[consumed.actingActorIndex].currentActionValue =
        turnState.actors[consumed.actingActorIndex].baseActionValue;
    return true;
}

} // namespace

BattleCharacter::BattleCharacter(const CharacterDefinition& definition, int partyIndex)
    : definition_(definition)
    , partyIndex_(partyIndex)
    , hp_(definition.hp)
    , ultimateCharge_(std::clamp(definition.startingOrbs, 0, std::max(1, definition.ultimatePoints)))
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
    hp_ -= std::max(0, amount);
    if (hp_ < 0) {
        hp_ = 0;
    }
}

void BattleCharacter::receiveHealing(int amount) {
    hp_ += std::max(0, amount);
    if (hp_ > definition_.hp) {
        hp_ = definition_.hp;
    }
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
    return ultimateCharge_ >= 1;
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

bool BattleManager::initialize(const std::string& bossKey, const std::vector<std::string>& characterKeys) {
    initialized_ = false;
    state_ = BattleState{};
    turnState_ = TurnState{};
    bossCurrentHp_ = 0;
    bossUltimateCharge_ = 0;
    characters_.clear();
    recentActionEvents_.clear();
    simulatedActions_ = 0;

    if (bossKey.empty()) {
        std::cerr << "[Battle] Missing boss key.\n";
        return false;
    }

    if (characterKeys.empty() || characterKeys.size() > 4) {
        std::cerr << "[Battle] Party size must be between 1 and 4.\n";
        return false;
    }

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
    switch (action) {
        case BattleAction::Standard:
            return character.isAlive();
        case BattleAction::Skill:
            return character.isAlive() && !actor.isExtraTurn && character.canUseSkill();
        case BattleAction::Ultimate:
            return character.isAlive() && !actor.isExtraTurn && character.canUseUltimate();
    }
    return false;
}

bool BattleManager::executePlayerAction(BattleAction action) {
    return resolvePlayerAction(action);
}

bool BattleManager::executePlayerStandardTurn() {
    return resolvePlayerAction(BattleAction::Standard);
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

    const AbilityDefinition* abilityDef = getAbility(character.definition().standardAbility);
    if (abilityDef == nullptr || abilityDef->type != AbilityType::Attack) {
        return false;
    }

    PresentationContext presContext;
    presContext.abilityId = abilityDef->id;
    presContext.presentationId = abilityDef->presentationId;
    presContext.interactionType = abilityDef->interactionType;
    presContext.casterIndex = character.partyIndex();
    presContext.targetIndex = -1;
    presContext.isBoss = false;

    const float multiplier = runPresentationInteraction(presContext);
    const int totalDamage = normalizeDamage(static_cast<int>(
        character.definition().atk * abilityDef->multiplier * multiplier
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

    if (event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }
    turnState_.actors[event.actingActorIndex].currentActionValue = turnState_.actors[event.actingActorIndex].baseActionValue;

    return true;
}

bool BattleManager::executePlayerTurn() {
    // Auto-select the best available action, matching the AI's executeTurn() priority:
    //   Ultimate (full orbs) → Skill (≥1 orb) → Standard fallback.
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
    const BattleCharacter& character = characters_[static_cast<size_t>(actor.partyIndex)];
    if (!actor.isExtraTurn && character.canUseUltimate()) {
        return resolvePlayerAction(BattleAction::Ultimate);
    }
    if (!actor.isExtraTurn && character.canUseSkill()) {
        return resolvePlayerAction(BattleAction::Skill);
    }
    return resolvePlayerAction(BattleAction::Standard);
}

bool BattleManager::processAutomaticTurns() {
    bool progressed = false;

    while (!isBattleOver()) {
        const TurnEvent next = peekNextTurnEvent();
        if (!next.valid || next.actingActorIndex >= turnState_.actors.size()) {
            break;
        }

        if (turnState_.actors[next.actingActorIndex].type != ParticipantType::Boss) {
            break;
        }
        if (!resolveBossAction()) {
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
        if (canUseBossAction(BattleAction::Ultimate)) {
            executeBossAction(actorIndex, BattleAction::Ultimate);
        } else if (canUseBossAction(BattleAction::Skill)) {
            executeBossAction(actorIndex, BattleAction::Skill);
        } else {
            executeBossAction(actorIndex, BattleAction::Standard);
        }
    } else {
        if (actor.partyIndex < 0 || static_cast<size_t>(actor.partyIndex) >= characters_.size()) {
            return;
        }

        BattleCharacter& character = characters_[static_cast<size_t>(actor.partyIndex)];
        if (!character.isAlive()) {
            return;
        }

        if (character.canUseUltimate()) {
            executeCharacterAction(actorIndex, character, BattleAction::Ultimate);
        } else if (character.canUseSkill()) {
            executeCharacterAction(actorIndex, character, BattleAction::Skill);
        } else {
            executeCharacterAction(actorIndex, character, BattleAction::Standard);
        }
    }

    // Resolve actor timeline slot.
    if (actorIndex >= turnState_.actors.size()) {
        return;
    }

    turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
}

void BattleManager::queueExtraTurnForCharacter(int partyIndex) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return;
    }

    const BattleCharacter& c = characters_[static_cast<size_t>(partyIndex)];
    turn::queueExtraTurnForCharacter(turnState_, c.definition(), partyIndex);
}

bool BattleManager::canUseBossAction(BattleAction action) const {
    switch (action) {
        case BattleAction::Standard:
            return true;
        case BattleAction::Skill:
            return bossUltimateCharge_ >= 1 && !state_.boss.skillAbility.empty();
        case BattleAction::Ultimate:
            return bossUltimateCharge_ >= std::max(1, state_.boss.ultimatePoints) && !state_.boss.ultimate.empty();
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

    if (previewActor.isExtraTurn && action != BattleAction::Standard) {
        return false;
    }

    BattleCharacter& character = characters_[static_cast<size_t>(previewActor.partyIndex)];
    if (!character.isAlive()) {
        return consumeInvalidPreviewCharacterTurn(characters_, turnState_, next) && resolvePlayerAction(action);
    }

    if ((action == BattleAction::Skill && !character.canUseSkill()) ||
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

    if (canUseBossAction(BattleAction::Ultimate)) {
        return executeBossAction(event.actingActorIndex, BattleAction::Ultimate);
    }
    if (canUseBossAction(BattleAction::Skill)) {
        return executeBossAction(event.actingActorIndex, BattleAction::Skill);
    }
    return executeBossAction(event.actingActorIndex, BattleAction::Standard);
}

bool BattleManager::executeCharacterAction(size_t actorIndex, BattleCharacter& character, BattleAction action) {
    if (actorIndex >= turnState_.actors.size()) {
        return false;
    }

    ++simulatedActions_;

    const std::string abilityId =
        (action == BattleAction::Standard) ? character.definition().standardAbility :
        (action == BattleAction::Skill) ? character.definition().skillAbility :
        character.definition().ultimate;
    const AbilityDefinition* abilityDef = getAbility(abilityId);
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

    if (abilityDef == nullptr) {
        const int fallbackDamage = normalizeDamage(
            character.definition().atk *
            ((action == BattleAction::Ultimate) ? 2 : (action == BattleAction::Skill ? 1 : 1))
        );
        bossCurrentHp_ = std::max(0, bossCurrentHp_ - fallbackDamage);
    } else {
        PresentationContext presContext;
        presContext.abilityId = abilityDef->id;
        presContext.presentationId = abilityDef->presentationId;
        presContext.interactionType = abilityDef->interactionType;
        presContext.casterIndex = character.partyIndex();
        presContext.targetIndex = -1;
        presContext.isBoss = false;

        const float multiplier = runPresentationInteraction(presContext);

        AbilityExecutionContext execContext;
        execContext.ability = abilityDef;
        execContext.casterPartyIndex = character.partyIndex();
        execContext.isBossCaster = false;
        execContext.baseDamage = character.definition().atk;
        execContext.baseHeal = abilityDef->flatHeal;
        execContext.presentationMultiplier = multiplier;
        execContext.bossMaxHp = state_.boss.hp;
        executeAbilityEffect(execContext);
    }
    actionEvent.bossHpAfter = bossCurrentHp_;

    if (action == BattleAction::Standard) {
        character.gainUltimatePoint(1);
    } else if (action == BattleAction::Skill) {
        character.consumeUltimatePoint(1);
    } else {
        character.consumeUltimate();
    }

    turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
    recentActionEvents_.push_back(std::move(actionEvent));
    return true;
}

bool BattleManager::executeBossAction(size_t actorIndex, BattleAction action) {
    if (actorIndex >= turnState_.actors.size()) {
        return false;
    }

    ++simulatedActions_;

    const std::string abilityId =
        (action == BattleAction::Standard) ? state_.boss.standardAbility :
        (action == BattleAction::Skill) ? state_.boss.skillAbility :
        state_.boss.ultimate;
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
    actionEvent.targetPartyIndices.clear();
    actionEvent.targetHpBefore.clear();
    actionEvent.targetHpAfter.clear();

    if (abilityDef != nullptr) {
        PresentationContext presContext;
        presContext.abilityId = abilityDef->id;
        presContext.presentationId = abilityDef->presentationId;
        presContext.interactionType = abilityDef->interactionType;
        presContext.casterIndex = -1;
        presContext.targetIndex = -1;
        presContext.isBoss = true;

        const float multiplier = runPresentationInteraction(presContext);

        if (abilityDef->targetRule == TargetRule::AllEnemies) {
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
            }
        }
    } else {
        const int targetIndex = firstLivingCharacterPartyIndex();
        if (targetIndex >= 0) {
            actionEvent.targetPartyIndices.push_back(targetIndex);
            actionEvent.targetHpBefore.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
            characters_[static_cast<size_t>(targetIndex)].receiveDamage(normalizeDamage(state_.boss.atk));
            actionEvent.targetHpAfter.push_back(characters_[static_cast<size_t>(targetIndex)].hp());
        }
    }

    if (action == BattleAction::Standard) {
        bossUltimateCharge_ = std::clamp(
            bossUltimateCharge_ + 1,
            0,
            std::max(1, state_.boss.ultimatePoints)
        );
    } else if (action == BattleAction::Skill) {
        bossUltimateCharge_ = std::max(0, bossUltimateCharge_ - 1);
    } else {
        bossUltimateCharge_ = 0;
    }

    turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
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
}

float BattleManager::runPresentationInteraction(const PresentationContext& context) {
    return ability::runPresentationInteraction(context);
}

} // namespace battle
