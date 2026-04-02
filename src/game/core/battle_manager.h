#ifndef BATTLE_MANAGER_H
#define BATTLE_MANAGER_H

#include <string>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

#include "combat_feedback.h"
#include "presentation_tuning_profile.h"

namespace battle {

using namespace std;

struct BossDefinition {
    std::string key;
    std::string title;
    std::string assets;
    std::string voiceHit;
    int spd = 0;
    int atk = 0;
    int hp = 0;
    std::string ability;
    std::string standardAbility;
    std::string skillAbility;
    std::string ultimate;
    int ultimatePoints = 0;
    int startingOrbs = 1;
    std::string bgm;
    float bgmVolume = 1.0f;
};

struct CharacterDefinition {
    std::string key;
    std::string title;
    std::string assets;
    std::string characterClass;
    int spd = 0;
    int atk = 0;
    int hp = 0;
    std::string ability;
    std::string standardAbility;
    std::string skillAbility;
    std::string ultimate;
    int ultimatePoints = 0;
    int startingOrbs = 1;
    int baseShield = 0; // New: base shield value for shield abilities
};

struct BattleSpecialRules {
    bool playerDamageHealsBoss = false;
    bool autoRevivePartyOnBossDamage = false;
    bool revivePartyToFull = false;
    bool bossSelfKnockoutIsDefeat = false;
};

struct BattleDefinition {
    std::string key;
    int id = -1;
    int storyOrder = -1;
    std::string name;
    std::string description;
    std::string type;
    std::string bossKey;
    std::string stageKey;
    std::string storyScript;
    std::string victoryStoryScript;
    std::string defeatStoryScript;
    std::string nextStoryScript;
    bool selectorVisible = false;
    bool isLineupFixed = true;
    int partySize = 4;
    std::vector<std::string> lineup;
    std::vector<std::string> lockedLineup;
    BattleSpecialRules specialRules;
};

struct BattleState {
    BossDefinition boss;
    std::vector<CharacterDefinition> party;
};

enum class AbilityType {
    Attack,
    Heal,
    Shield,
    Buff,
    Debuff
};

enum class TargetRule {
    SingleEnemy,
    AllEnemies,
    SingleAlly,
    AllAllies,
    Self
};

enum class InteractionType {
    None,
    Rhythm,
    Parry
};

struct AbilityDefinition {
    std::string id;
    std::string name;
    std::string instructionHint;
    AbilityType type = AbilityType::Attack;
    TargetRule targetRule = TargetRule::SingleEnemy;
    float multiplier = 1.0f;
    int flatHeal = 0;
    int speedBuff = 0;
    int atkBuff = 0;   // ATK % buff applied to targets (negative = nerf)
    float actionAdvance = 0.0f;
    float selfHpCostPercentOfMax = 0.0f;
    float selfHpCostPercentIncreasePerUse = 0.0f;
    float selfHpCostPercentMax = 0.0f;
    bool reviveDeadAllies = false;
    InteractionType interactionType = InteractionType::None;
    std::string presentationId;
};

enum class BattleResolvedOutcome {
    None,
    Victory,
    Defeat
};

struct PresentationContext {
    std::string abilityId;
    std::string abilityName;
    std::string presentationId;
    InteractionType interactionType = InteractionType::None;
    int casterIndex = -1;
    int targetIndex = -1;
    int presentationValue = 0;
    bool isBoss = false;
    bool isUltimate = false;
    PresentationTuningProfile tuningProfile{};
};

struct AbilityExecutionContext {
    const AbilityDefinition* ability = nullptr;
    int casterPartyIndex = -1;
    bool isBossCaster = false;
    bool playerDamageHealsBoss = false;
    int baseDamage = 0;
    int baseHeal = 0;
    float presentationMultiplier = 1.0f;
    float comboMultiplier = 1.0f;
    int bossMaxHp = 1;
};

struct BattleComboState {
    int comboCount = 0;
    float damageBonusFraction = 0.0f;
};

struct ComboResolution {
    bool applied = false;
    CombatJudgement judgement = CombatJudgement::Flop;
    int previousComboCount = 0;
    int comboCount = 0;
    int comboBreakHealPerAlly = 0;
    float damageBonusFraction = 0.0f;
    bool brokeCombo = false;
};

class BattleCharacter {
public:
    explicit BattleCharacter(const CharacterDefinition& definition, int partyIndex);

    const CharacterDefinition& definition() const;
    int partyIndex() const;

    int hp() const;
    int maxHp() const;
    bool isAlive() const;
    void receiveDamage(int amount);
    void receiveHealing(int amount);
    void revive(int amount);

    int ultimateCharge() const;
    void gainUltimatePoint(int amount = 1);
    bool canUseSkill() const;
    bool canUseUltimate() const;
    void consumeUltimatePoint(int amount = 1);
    void consumeUltimate();
    int effectiveSpd() const;
    int spdBuffBonus() const;
    void setSpdBuffBonus(int amount);
    int effectiveAtk() const;
    int atkBuffBonus() const;
    void setAtkBuffBonus(int percentBonus);

    // Shield support
    int getShield() const { return shield_; }
    void addShield(int amount) {
        if (amount > 0) {
            shield_ += amount;
            std::cout << "[Shield] " << definition_.key << " gained " << amount << " shield (now " << shield_ << ")\n";
        }
    }
    void reduceShield(int amount) {
        int before = shield_;
        shield_ = std::max(0, shield_ - amount);
        if (before != shield_) {
            std::cout << "[Shield] " << definition_.key << " lost " << (before - shield_) << " shield (now " << shield_ << ")\n";
        }
    }

private:
    CharacterDefinition definition_;
    int partyIndex_ = 0;
    int hp_ = 0;
    int ultimateCharge_ = 0;
    int shield_ = 0;
    int spdBuffBonus_ = 0;
    int atkBuffBonus_ = 0; // ATK buff/nerf in percent (e.g. 50 = +50%, -20 = -20%)
};

enum class BattleAction {
    Standard,
    Skill,
    Ultimate
};

enum class ParticipantType {
    Boss,
    Character
};

struct TurnActor {
    ParticipantType type = ParticipantType::Character;
    std::string key;
    std::string assetId;
    std::string title;
    int partyIndex = -1; // 0 = left-most character. -1 for boss.
    int priority = 0; // Higher comes first on same action value.
    bool isExtraTurn = false;
    BattleAction extraTurnAction = BattleAction::Skill;
    bool autoExecute = false;
    bool grantsUltimatePointOnAction = true;
    int spd = 1;
    float baseActionValue = 10000.0f;
    float currentActionValue = 10000.0f;
};

struct TurnEvent {
    float consumedActionValue = 0.0f;
    bool valid = false;
    size_t actingActorIndex = 0;
};

struct TurnState {
    std::vector<TurnActor> actors;
};

struct BattleActionEvent {
    ParticipantType actorType = ParticipantType::Character;
    std::string actorKey;
    std::string actorTitle;
    int actorPartyIndex = -1;
    BattleAction action = BattleAction::Standard;
    std::string abilityId;
    std::string abilityName;
    InteractionType interactionType = InteractionType::None;
    int bossHpBefore = 0;
    int bossHpAfter = 0;
    bool abilityVoicesHandledDuringPresentation = false;
    bool hitVoicesHandledDuringPresentation = false;
    std::vector<int> targetPartyIndices;
    std::vector<int> targetHpBefore;
    std::vector<int> targetHpAfter;
};

enum class ManualUltimateRequestResult {
    Queued,
    MeterNotReady,
    AlreadyQueued,
    Unavailable
};

class BattleManager {
public:
    // Returns the shield value for a party member at the given index, or 0 if out of range.
    int getCharacterShield(int partyIndex) const;
public:
    bool initialize(const BattleDefinition& battleDefinition, const std::vector<std::string>& characterKeys);
    bool initialize(const std::string& bossKey, const std::vector<std::string>& characterKeys);
    void printBattleSummary() const;
    const BattleState& getBattleState() const;
    const BattleDefinition& getBattleDefinition() const;
    const BattleSpecialRules& getSpecialRules() const;
    const TurnState& getTurnState() const;
    int getPreviewNextActorIndex() const;
    int getBossCurrentHp() const;
    int getBossMaxHp() const;
    int getBossUltimateCharge() const;
    int getBossUltimateRequired() const;
    int getCharacterCurrentHp(int partyIndex) const;
    int getCharacterMaxHp(int partyIndex) const;
    bool isCharacterAlive(int partyIndex) const;
    bool reviveCharacter(int partyIndex, int amount);
    ManualUltimateRequestResult previewManualUltimateTurnRequest(int partyIndex) const;
    ManualUltimateRequestResult requestManualUltimateTurn(int partyIndex);
    int getCharacterUltimateCharge(int partyIndex) const;
    int getCharacterUltimateRequired(int partyIndex) const;
    void addLuotianyiCorrectTones(int amount);
    int getLuotianyiCorrectTones() const;
    const BattleComboState& getComboState() const;
    ComboResolution applyPresentationFeedback(bool isBossCaster, const PresentationFeedbackEvent& feedback);
    void applyPresentationHitDamage(bool isBossCaster,
                                    int perHitDamage,
                                    int hitEvents,
                                    int targetPartyIndex = -1);
    void applyPresentationHealing(bool isBossCaster, int perHitHeal, int hitEvents, bool reviveDeadAllies = false);
    bool consumePresentationHitDamageApplied();
    bool consumePresentationHealingApplied();
    void markPresentationAbilityAudioPlayed();
    bool consumePresentationAbilityAudioPlayed();
    void markPresentationHitAudioPlayed();
    bool consumePresentationHitAudioPlayed();
    const AbilityDefinition* findAbilityDefinition(const std::string& abilityId) const;
    const std::vector<BattleActionEvent>& getRecentActionEvents() const;
    void clearRecentActionEvents();
    bool isPlayerActionReady(BattleAction action) const;
    bool executePlayerAction(BattleAction action);
    bool executePlayerStandardTurn();
    bool executePlayerSkillTurn();
    bool executePlayerUltimateTurn();
    bool prepareCurrentPlayerSplitAttackPlan(int hitCount, std::vector<int>& outHitDamages);
    void applyBossSplitHitDamage(int damage);
    bool commitCurrentPlayerSplitAttackTurn();
    bool executePlayerTurn();
    bool processAutomaticTurns();
    bool isBattleOver() const;
    BattleResolvedOutcome outcome() const;
    bool playerDamageHealsBoss() const;

private:
    struct ActivePartyBuff {
        std::string abilityId;
        int sourcePartyIndex = -1;
        int targetPartyIndex = -1;
        int speedBuff = 0;
        int atkBuff = 0; // ATK % buff/nerf (signed)
    };

    struct BossStatusState {
        int magicEggSpinningMachineCharges = 0;
    };

    bool buildInitialTurnState();
    TurnEvent peekNextTurnEvent() const;
    TurnEvent advanceToNextTurnEvent();
    void runPseudoBattle(int maxActions);
    void executeTurn(size_t actorIndex);
    void queueExtraTurnForCharacter(int partyIndex);
    void queueExtraTurnForCharacter(int partyIndex,
                                    BattleAction action,
                                    bool autoExecute,
                                    bool grantsUltimatePointOnAction,
                                    int priority);
    int firstLivingCharacterPartyIndex() const;
    int findCharacterPartyIndexByKey(const std::string& characterKey) const;
    bool hasQueuedExtraTurn(int partyIndex, BattleAction action, bool autoExecute) const;
    static int normalizeDamage(int value);
    const AbilityDefinition* getAbility(const std::string& abilityId) const;
    void executeAbilityEffect(const AbilityExecutionContext& context);
    float runPresentationInteraction(const PresentationContext& context);
    BattleResolvedOutcome computeDerivedOutcome() const;
    void applyBossDamage(int amount);
    void applyBossHealing(int amount);
    void applyPlayerOffenseToBoss(int amount);
    void applyBossAbilitySelfCost(const AbilityDefinition& ability);
    void reviveDefeatedPartyMembersIfNeeded();
    bool canUseBossAction(BattleAction action) const;
    bool resolvePlayerAction(BattleAction action);
    bool resolveBossAction();
    bool executeCharacterAction(size_t actorIndex, BattleCharacter& character, BattleAction action);
    bool executeBossAction(size_t actorIndex, BattleAction action);
    void applyJiafeiUltimateDebuff();
    void consumeJiafeiUltimateDebuff();
    void tryQueueJiafeiFollowUp(const BattleActionEvent& actionEvent);
    void syncCharacterTurnParticipation(int partyIndex);
    void syncAllCharacterTurnParticipation();
    void syncCharacterUltimateTurn(int partyIndex);
    void applyPartyBuffFromAbility(int sourcePartyIndex,
                                   const AbilityDefinition& ability,
                                   float presentationMultiplier);
    void expireBuffsFromCaster(int sourcePartyIndex);
    void removeBuffsFromDefeatedCharacters();
    void refreshCharacterBuffBonuses();
    void refreshTurnActorSpeed(int partyIndex);
    void refreshAllTurnActorSpeeds();
    void applyAllAlliesActionAdvance(float fraction);
    void refreshComboState();

    BattleState state_;
    BattleDefinition battleDefinition_{};
    TurnState turnState_;
    int bossCurrentHp_ = 0;
    int bossUltimateCharge_ = 0;
    std::vector<BattleCharacter> characters_;
    int simulatedActions_ = 0;
    bool initialized_ = false;
    std::unordered_map<std::string, AbilityDefinition> abilities_;
    std::vector<BattleActionEvent> recentActionEvents_;
    bool presentationHitDamageApplied_ = false;
    bool presentationHealingApplied_ = false;
    bool presentationAbilityAudioPlayed_ = false;
    bool presentationHitAudioPlayed_ = false;
    std::optional<BattleResolvedOutcome> forcedOutcome_;
    BossStatusState bossStatus_;
    int luotianyiCorrectTones_ = 0;
    BattleComboState comboState_{};
    std::vector<ActivePartyBuff> activePartyBuffs_;
    std::unordered_map<std::string, int> bossAbilityUseCounts_;
    int nextManualUltimatePriority_ = 1000;
};

} // namespace battle

#endif // BATTLE_MANAGER_H
