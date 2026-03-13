#include "battle_manager.h"
#include "ability_system.h"
#include "battle_loader.h"
#include "../render/battle_ui.h"
#include "turn_system.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace battle {
namespace {
} // namespace

BattleCharacter::BattleCharacter(const CharacterDefinition& definition, int partyIndex)
    : definition_(definition)
    , partyIndex_(partyIndex)
    , hp_(definition.hp)
    , ultimateCharge_(0)
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
    ui::updateCharacterHp(partyIndex_, hp_, definition_.hp);
    ui::triggerCharacterDamageFlash(partyIndex_);
}

void BattleCharacter::receiveHealing(int amount) {
    hp_ += std::max(0, amount);
    if (hp_ > definition_.hp) {
        hp_ = definition_.hp;
    }
    ui::updateCharacterHp(partyIndex_, hp_, definition_.hp);
}

int BattleCharacter::ultimateCharge() const {
    return ultimateCharge_;
}

void BattleCharacter::gainUltimatePoint() {
    ++ultimateCharge_;
}

bool BattleCharacter::canUseUltimate() const {
    return ultimateCharge_ >= std::max(1, definition_.ultimatePoints);
}

void BattleCharacter::consumeUltimate() {
    ultimateCharge_ = 0;
}

bool BattleManager::initialize(const std::string& bossKey, const std::vector<std::string>& characterKeys) {
    initialized_ = false;
    state_ = BattleState{};
    turnState_ = TurnState{};
    bossCurrentHp_ = 0;
    characters_.clear();
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
    for (size_t i = 0; i < state_.party.size(); ++i) {
        characters_.emplace_back(state_.party[i], static_cast<int>(i));
    }

    if (!buildInitialTurnState()) {
        return false;
    }

    if (!loader::loadAllAbilities(abilities_)) {
        std::cerr << "[Battle] Warning: Failed to load abilities.\n";
    }

    // Initialize HP transition states
    ui::updateBossHp(bossCurrentHp_, state_.boss.hp);
    for (size_t i = 0; i < characters_.size(); ++i) {
        ui::updateCharacterHp(static_cast<int>(i), characters_[i].hp(), characters_[i].maxHp());
    }

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
    std::cout << "  ability: " << state_.boss.ability << "\n\n";

    std::cout << "Party (" << state_.party.size() << "):\n";
    for (size_t i = 0; i < state_.party.size(); ++i) {
        const CharacterDefinition& c = state_.party[i];
        std::cout << "  [" << (i + 1) << "] key: " << c.key << "\n";
        std::cout << "      title: " << c.title << "\n";
        std::cout << "      assets: " << c.assets << "\n";
        std::cout << "      class: " << c.characterClass << "\n";
        std::cout << "      spd: " << c.spd << " | atk: " << c.atk << " | hp: " << c.hp << "\n";
        std::cout << "      ability: " << c.ability << "\n";
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

    const AbilityDefinition* abilityDef = getAbility(character.definition().ability);
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
    ui::updateBossHp(bossCurrentHp_, state_.boss.hp);
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
        queueExtraTurnForCharacter(character.partyIndex());
    }

    if (event.actingActorIndex >= turnState_.actors.size()) {
        return false;
    }
    turnState_.actors[event.actingActorIndex].currentActionValue = turnState_.actors[event.actingActorIndex].baseActionValue;

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

    if (turnState_.actors[next.actingActorIndex].type != ParticipantType::Character) {
        return false;
    }

    const TurnEvent event = advanceToNextTurnEvent();
    if (!event.valid) {
        return false;
    }

    executeTurn(event.actingActorIndex);
    return true;
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

        const TurnEvent event = advanceToNextTurnEvent();
        if (!event.valid) {
            break;
        }

        executeTurn(event.actingActorIndex);
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
    ++simulatedActions_;

    std::ostringstream prefix;
    prefix << "  (" << simulatedActions_ << ") ";
    if (actor.type == ParticipantType::Boss) {
        prefix << "Boss(" << actor.key << ")";
    } else {
        prefix << "P" << (actor.partyIndex + 1) << "(" << actor.key << ")";
    }
    if (actor.isExtraTurn) {
        prefix << "[EXTRA]";
    }

    if (actor.type == ParticipantType::Boss) {
        // Boss ability execution through ability system
        const AbilityDefinition* bossAbilityDef = getAbility(state_.boss.ability);
        
        if (bossAbilityDef != nullptr) {
            PresentationContext presContext;
            presContext.abilityId = bossAbilityDef->id;
            presContext.presentationId = bossAbilityDef->presentationId;
            presContext.interactionType = bossAbilityDef->interactionType;
            presContext.casterIndex = -1;
            presContext.targetIndex = -1;
            presContext.isBoss = true;
            
            const float multiplier = runPresentationInteraction(presContext);
            
            // Boss attacks all living characters
            if (bossAbilityDef->targetRule == TargetRule::AllEnemies) {
                int hitCount = 0;
                std::cout << prefix.str() << " uses " << bossAbilityDef->name;
                if (multiplier != 1.0f) {
                    std::cout << " (x" << multiplier << ")";
                }
                std::cout << " hitting all:\n";
                
                for (BattleCharacter& c : characters_) {
                    if (c.isAlive()) {
                        const int finalDamage = normalizeDamage(static_cast<int>(
                            state_.boss.atk * bossAbilityDef->multiplier * multiplier
                        ));
                        c.receiveDamage(finalDamage);
                        std::cout << "      P" << (c.partyIndex() + 1) << " takes " 
                                  << finalDamage << " dmg | hp=" << c.hp() << "\n";
                        ++hitCount;
                    }
                }
                
                if (hitCount == 0) {
                    std::cout << "      (no valid targets)\n";
                }
            } else {
                // Single target attack
                const int targetIndex = firstLivingCharacterPartyIndex();
                if (targetIndex >= 0) {
                    const int finalDamage = normalizeDamage(static_cast<int>(
                        state_.boss.atk * bossAbilityDef->multiplier * multiplier
                    ));
                    
                    characters_[static_cast<size_t>(targetIndex)].receiveDamage(finalDamage);
                    std::cout << prefix.str() << " uses " << bossAbilityDef->name
                              << " on P" << (targetIndex + 1);
                    if (multiplier != 1.0f) {
                        std::cout << " (x" << multiplier << ")";
                    }
                    std::cout << " for " << finalDamage << " dmg"
                              << " | target hp=" << characters_[static_cast<size_t>(targetIndex)].hp()
                              << "\n";
                } else {
                    std::cout << prefix.str() << " has no valid target.\n";
                }
            }
        } else {
            // Fallback if ability not found
            const int targetIndex = firstLivingCharacterPartyIndex();
            if (targetIndex >= 0) {
                const int damage = normalizeDamage(state_.boss.atk);
                characters_[static_cast<size_t>(targetIndex)].receiveDamage(damage);
                std::cout << prefix.str() << " attacks P" << (targetIndex + 1)
                          << " for " << damage << " dmg"
                          << " | target hp=" << characters_[static_cast<size_t>(targetIndex)].hp()
                          << "\n";
            } else {
                std::cout << prefix.str() << " has no valid target.\n";
            }
        }
    } else {
        if (actor.partyIndex < 0 || static_cast<size_t>(actor.partyIndex) >= characters_.size()) {
            return;
        }

        BattleCharacter& character = characters_[static_cast<size_t>(actor.partyIndex)];
        if (!character.isAlive()) {
            std::cout << prefix.str() << " is down and cannot act.\n";
        } else if (actor.isExtraTurn) {
            // Ultimate execution through ability system
            const AbilityDefinition* ultDef = getAbility(character.definition().ultimate);
            
            if (ultDef != nullptr) {
                PresentationContext presContext;
                presContext.abilityId = ultDef->id;
                presContext.presentationId = ultDef->presentationId;
                presContext.interactionType = ultDef->interactionType;
                presContext.casterIndex = character.partyIndex();
                presContext.targetIndex = -1;
                presContext.isBoss = false;
                
                const float multiplier = runPresentationInteraction(presContext);
                
                AbilityExecutionContext execContext;
                execContext.ability = ultDef;
                execContext.casterPartyIndex = character.partyIndex();
                execContext.isBossCaster = false;
                execContext.baseDamage = character.definition().atk;
                execContext.baseHeal = ultDef->flatHeal;
                execContext.presentationMultiplier = multiplier;
                execContext.bossMaxHp = state_.boss.hp;
                
                executeAbilityEffect(execContext);
                
                character.consumeUltimate();
                std::cout << prefix.str() << " casts ULTIMATE " << ultDef->name;
                if (multiplier != 1.0f) {
                    std::cout << " (x" << multiplier << ")";
                }
                
                if (ultDef->type == AbilityType::Attack) {
                    const int finalDamage = normalizeDamage(static_cast<int>(
                        execContext.baseDamage * ultDef->multiplier * multiplier
                    ));
                    std::cout << " for " << finalDamage << " dmg";
                } else if (ultDef->type == AbilityType::Heal) {
                    std::cout << " healing for " << ultDef->flatHeal << " hp";
                }
                std::cout << " | boss hp=" << bossCurrentHp_ << "\n";
            } else {
                // Fallback
                const int damage = normalizeDamage(character.definition().atk * 2);
                bossCurrentHp_ = std::max(0, bossCurrentHp_ - damage);
                ui::updateBossHp(bossCurrentHp_, state_.boss.hp);
                character.consumeUltimate();
                std::cout << prefix.str() << " casts ULTIMATE " << character.definition().ultimate
                          << " for " << damage << " dmg"
                          << " | boss hp=" << bossCurrentHp_ << "\n";
            }
        } else {
            // Regular ability execution through ability system
            const AbilityDefinition* abilityDef = getAbility(character.definition().ability);
            
            if (abilityDef != nullptr) {
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
                
                character.gainUltimatePoint();

                std::cout << prefix.str() << " casts " << abilityDef->name;
                if (multiplier != 1.0f) {
                    std::cout << " (x" << multiplier << ")";
                }
                
                if (abilityDef->type == AbilityType::Attack) {
                    const int finalDamage = normalizeDamage(static_cast<int>(
                        execContext.baseDamage * abilityDef->multiplier * multiplier
                    ));
                    std::cout << " for " << finalDamage << " dmg";
                } else if (abilityDef->type == AbilityType::Heal) {
                    std::cout << " healing for " << abilityDef->flatHeal << " hp";
                }
                
                std::cout << " | ult=" << character.ultimateCharge() << "/"
                          << std::max(1, character.definition().ultimatePoints)
                          << " | boss hp=" << bossCurrentHp_ << "\n";

                if (character.canUseUltimate()) {
                    queueExtraTurnForCharacter(character.partyIndex());
                    std::cout << "      -> extra turn queued for " << character.definition().title << "\n";
                }
            } else {
                // Fallback
                const int damage = normalizeDamage(character.definition().atk);
                bossCurrentHp_ = std::max(0, bossCurrentHp_ - damage);
                ui::updateBossHp(bossCurrentHp_, state_.boss.hp);
                character.gainUltimatePoint();

                std::cout << prefix.str() << " casts ability " << character.definition().ability
                          << " for " << damage << " dmg"
                          << " | ult=" << character.ultimateCharge() << "/"
                          << std::max(1, character.definition().ultimatePoints)
                          << " | boss hp=" << bossCurrentHp_ << "\n";

                if (character.canUseUltimate()) {
                    queueExtraTurnForCharacter(character.partyIndex());
                    std::cout << "      -> extra turn queued for " << character.definition().title << "\n";
                }
            }
        }
    }

    // Resolve actor timeline slot.
    if (actorIndex >= turnState_.actors.size()) {
        return;
    }

    if (turnState_.actors[actorIndex].isExtraTurn) {
        turnState_.actors.erase(turnState_.actors.begin() + actorIndex);
    } else {
        turnState_.actors[actorIndex].currentActionValue = turnState_.actors[actorIndex].baseActionValue;
    }
}

void BattleManager::queueExtraTurnForCharacter(int partyIndex) {
    if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= characters_.size()) {
        return;
    }

    const BattleCharacter& c = characters_[static_cast<size_t>(partyIndex)];
    turn::queueExtraTurnForCharacter(turnState_, c.definition(), partyIndex);
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
