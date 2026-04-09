#ifndef BATTLE_MANAGER_H
#define BATTLE_MANAGER_H

#include <string>
#include <iostream>
#include <array>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

#include "combat_feedback.h"
#include "player_progression.h"
#include "presentation_tuning_profile.h"

namespace battle {

using namespace std;

struct BossDefinition {
    struct PhaseDefinition {
        bool configured = false;
        int atkBonusPercent = 0;
        std::string bgm;
        std::optional<float> bgmVolume;
        PresentationTuningProfile tuningProfile{};
        std::string phaseChangeVoice;
    };

    std::string key;
    std::string title;
    std::string assets;
    std::string voiceHit;
    std::string voiceSpeakerId;
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
    std::array<PhaseDefinition, 3> phases{};
    bool hasPhaseData = false;
};

struct CharacterAbilityKitDefinition {
    std::string standardAbility;
    std::string skillAbility;
    std::string ultimate;
    std::string assetId;
};

struct CharacterDefinition {
    std::string key;
    std::string title;
    std::string assets;
    std::string voiceAssetId;
    std::string voiceSpeakerId;
    std::string characterClass;
    bool isSinger = false;
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
    std::unordered_map<std::string, CharacterAbilityKitDefinition> abilityKits;
};

std::string getCharacterRegularAbilityId(const CharacterDefinition& definition);
std::string getBossNormalAbilityId(const BossDefinition& definition);

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
    FlatStatBonuses buffs;
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

enum class InputPromptType {
    None,
    Space,
    Wild,
    Custom,
    Arrows,
    UpDown,
    LeftRight,
    SpamSpace
};

enum class SpecialDamageSource {
    None,
    TeamShield,
    AppliedHeal,
    StoredHealingTally
};

enum class ActionAdvanceMode {
    RemainingFraction,
    BaseActionValueDelta
};

struct AbilityDefinition {
    std::string id;
    std::string name;
    std::string statusName;
    std::string instructionHint;
    AbilityType type = AbilityType::Attack;
    TargetRule targetRule = TargetRule::SingleEnemy;
    float multiplier = 1.0f;
    int flatHeal = 0;
    int baseShield = 0;
    std::optional<float> amountPercentOfCasterMaxHp;
    int speedBuff = 0;
    int atkBuff = 0;   // ATK % buff applied to targets (negative = nerf)
    int damageBuff = 0; // DMG % buff applied to outgoing damage (negative = nerf)
    int casterTurnDuration = 1;
    int bossAtkBuff = 0; // Boss ATK % modifier applied for bossTurnDuration boss turns.
    int bossDamageTakenBuff = 0; // Boss DMG TAKEN % modifier applied for bossTurnDuration boss turns.
    int bossTurnDuration = 1;
    int orbGain = 1;
    float actionAdvance = 0.0f;
    ActionAdvanceMode actionAdvanceMode = ActionAdvanceMode::RemainingFraction;
    float selfHpCostPercentOfMax = 0.0f;
    float selfHpCostPercentIncreasePerUse = 0.0f;
    float selfHpCostPercentMax = 0.0f;
    bool reviveDeadAllies = false;
    InteractionType interactionType = InteractionType::None;
    SpecialDamageSource specialDamageSource = SpecialDamageSource::None;
    std::string presentationId;
    InputPromptType inputPromptType = InputPromptType::None;
    std::vector<std::string> inputPromptKeys;
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
    std::vector<int> resolvedRolls;
    std::vector<int> resolvedTargetIndices;
    bool isBoss = false;
    bool isUltimate = false;
    bool suppressPrompt = false;
    bool suppressHint = false;
    bool suppressBossWarning = false;
    PresentationTuningProfile tuningProfile{};
};

struct AbilityExecutionContext {
    const AbilityDefinition* ability = nullptr;
    int casterPartyIndex = -1;
    int targetPartyIndex = -1;
    bool isBossCaster = false;
    bool playerDamageHealsBoss = false;
    int baseDamage = 0;
    int baseHeal = 0;
    float presentationMultiplier = 1.0f;
    float comboMultiplier = 1.0f;
    float damageBuffMultiplier = 1.0f;
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

/**
 * Initialize a BattleCharacter from a character definition and party slot.
 * @param definition CharacterDefinition describing base stats and identifiers.
 * @param partyIndex Index of this character in the player's party (0-based).
 */
/**
 * Access the stored CharacterDefinition.
 * @returns Reference to the character's definition.
 */
/**
 * Index of this character within the player's party.
 * @returns 0-based party index.
 */
/**
 * Current HP of the character.
 * @returns Current hit points.
 */
/**
 * Maximum HP of the character as defined by its definition and any modifiers.
 * @returns Maximum hit points.
 */
/**
 * Whether the character is alive (hp > 0).
 * @returns `true` if hp is greater than zero, `false` otherwise.
 */
/**
 * Apply damage to the character, reducing shield first then HP; clamps HP to a minimum of 0.
 * @param amount Amount of incoming damage to apply (assumed non-negative).
 */
/**
 * Restore HP to the character up to its maximum; negative or zero amounts have no effect.
 * @param amount Amount of healing to apply.
 */
/**
 * Revive a defeated character and set its HP to the supplied amount (clamped to [1, maxHp]).
 * @param amount HP value to set on revival.
 */
/**
 * Current ultimate charge value for the character.
 * @returns Current ultimate charge.
 */
/**
 * Increase the character's ultimate charge by the given amount.
 * @param amount Points to add to the ultimate charge (default 1).
 */
/**
 * Whether the character can use their skill action according to current charge/state.
 * @returns `true` if the skill is available, `false` otherwise.
 */
/**
 * Whether the character can use their ultimate action according to current charge/state.
 * @returns `true` if the ultimate is available, `false` otherwise.
 */
/**
 * Decrease the character's ultimate charge by the given amount.
 * @param amount Points to consume from the ultimate charge (default 1).
 */
/**
 * Consume the character's ultimate (perform the ultimate's charge consumption side-effect).
 */
/**
 * Effective speed value including base speed and any speed buff bonuses.
 * @returns Effective speed used for turn ordering.
 */
/**
 * Current additive speed buff applied to the character.
 * @returns Speed buff amount.
 */
/**
 * Set the character's speed buff bonus to the specified amount.
 * @param amount New speed buff value.
 */
/**
 * Effective attack value including base attack and any percent ATK buff/nerf.
 * @returns Effective attack used for damage calculations.
 */
/**
 * Current ATK buff expressed as a percent (e.g. 50 = +50%, -20 = -20%).
 * @returns ATK buff percent.
 */
/**
 * Set the character's ATK buff/nerf percent.
 * @param percentBonus ATK percent to apply (positive to buff, negative to nerf).
 */
/**
 * Current shield value that will absorb incoming damage before HP.
 * @returns Current shield amount.
 */
/**
 * Increase the character's shield by the given amount (no-op for non-positive amounts).
 * @param amount Amount of shield to add (must be positive to take effect).
 */
/**
 * Reduce the character's shield by the given amount and clamp to zero.
 * @param amount Amount of shield to remove.
 */
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
    int damageBuffBonus() const;
    float damageBuffMultiplier() const;
    void setDamageBuffBonus(int percentBonus);

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
    int damageBuffBonus_ = 0; // DMG buff/nerf in percent (e.g. 50 = +50%, -20 = -20%)
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
    std::string abilityKitOverride;
    int partyIndex = -1; // 0 = left-most character. -1 for boss.
    int priority = 0; // Higher comes first on same action value.
    bool isExtraTurn = false;
    BattleAction extraTurnAction = BattleAction::Skill;
    bool autoExecute = false;
    bool grantsUltimatePointOnAction = true;
    int spd = 1;
    float baseActionValue = 10000.0f;
    float currentActionValue = 10000.0f;
    int phaseTransitionFromIndex = -1;
    int phaseTransitionToIndex = -1;

    bool isBossPhaseIntroTurn() const {
        return type == ParticipantType::Boss &&
            isExtraTurn &&
            phaseTransitionFromIndex >= 0 &&
            phaseTransitionToIndex >= 0;
    }
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
    float consumedActionValue = 0.0f;
    int totalOutgoingDamage = 0;
    std::vector<int> targetPartyIndices;
    std::vector<int> targetHpBefore;
    std::vector<int> targetHpAfter;
    std::vector<int> targetShieldBefore;
    std::vector<int> targetShieldAfter;
};

struct BattleTelemetry {
    float totalActionValueConsumed = 0.0f;
    std::unordered_map<std::string, int> characterDamageByKey;
};

struct BossPhaseTransition {
    bool valid = false;
    int fromPhaseIndex = 0;
    int toPhaseIndex = 0;
};

enum class BattleStatusBadgeTarget {
    PartyMember,
    Boss
};

enum class BattleStatusBadgeCategory {
    Shield,
    Buff,
    Debuff
};

enum class BattleStatusBadgeValueKind {
    None,
    Flat,
    Percent,
    Charges
};

struct BattleStatusBadge {
    BattleStatusBadgeTarget target = BattleStatusBadgeTarget::PartyMember;
    int targetPartyIndex = -1;
    int sourcePartyIndex = -1;
    BattleStatusBadgeCategory category = BattleStatusBadgeCategory::Buff;
    BattleStatusBadgeValueKind valueKind = BattleStatusBadgeValueKind::None;
    std::string abilityId;
    std::string sourceKey;
    std::string sourceAssetId;
    std::string statusName;
    std::string statLabel;
    int value = 0;
};

/**
 * Retrieve the current shield value for the party member at the given index.
 * @param partyIndex 0-based index of the party member.
 * @returns The shield value for the specified party member, or 0 if the index is out of range.
 */
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
    bool initialize(const BattleDefinition& battleDefinition,
                    const std::vector<std::string>& characterKeys,
                    const PlayerProgression& progression = {});
    bool initialize(const std::string& bossKey,
                    const std::vector<std::string>& characterKeys,
                    const PlayerProgression& progression = {});
    void printBattleSummary() const;
    const BattleState& getBattleState() const;
    const BattleDefinition& getBattleDefinition() const;
    const BattleSpecialRules& getSpecialRules() const;
    const TurnState& getTurnState() const;
    int getPreviewNextActorIndex() const;
    int getBossCurrentHp() const;
    int getBossMaxHp() const;
    int getBossEffectiveAtk() const;
    int getBossPhaseIndex() const;
    void setBossTitle(const std::string& title);
    std::optional<BossPhaseTransition> consumeBossPhaseTransition();
    std::string getCurrentBossBgm() const;
    float getCurrentBossBgmVolume() const;
    int getBossUltimateCharge() const;
    int getBossUltimateRequired() const;
    int getCharacterCurrentHp(int partyIndex) const;
    int getCharacterMaxHp(int partyIndex) const;
    int getCharacterEffectiveAtk(int partyIndex) const;
    float getCharacterDamageBuffMultiplier(int partyIndex) const;
    bool isCharacterAlive(int partyIndex) const;
    bool reviveCharacter(int partyIndex, int amount);
    ManualUltimateRequestResult previewManualUltimateTurnRequest(int partyIndex) const;
    ManualUltimateRequestResult requestManualUltimateTurn(int partyIndex);
    int getCharacterUltimateCharge(int partyIndex) const;
    int getCharacterUltimateRequired(int partyIndex) const;
    bool setCharacterAbilityKit(int partyIndex, const std::string& kitId);
    void clearCharacterAbilityKit(int partyIndex);
    std::string resolveCharacterAbilityId(int partyIndex,
                                          BattleAction action,
                                          const std::string& turnAbilityKitOverride = {}) const;
    std::string resolveCharacterAssetId(int partyIndex,
                                        const std::string& turnAbilityKitOverride = {}) const;
    std::string resolveCharacterVoiceAssetId(int partyIndex) const;
    void addSailorVenusSpaceTally(int partyIndex, int amount);
    void addLuotianyiCorrectTones(int amount);
    int getLuotianyiCorrectTones() const;
    const BattleComboState& getComboState() const;
    ComboResolution applyPresentationFeedback(bool isBossCaster, const PresentationFeedbackEvent& feedback);
    void applyPresentationHitDamage(bool isBossCaster,
                                    int perHitDamage,
                                    int hitEvents,
                                    int targetPartyIndex = -1,
                                    int sourcePartyIndex = -1);
    int applyPresentationHealing(bool isBossCaster,
                                 int perHitHeal,
                                 int hitEvents,
                                 TargetRule targetRule = TargetRule::AllAllies,
                                 int targetPartyIndex = -1,
                                 bool reviveDeadAllies = false,
                                 bool recordForTetoTally = true);
    int applyConvertedPlayerSpecialDamageToBoss(int rawAmount,
                                                float abilityMultiplier,
                                                bool markPresentationResolved = false,
                                                int sourcePartyIndex = -1);
    int applyCurrentTeamShieldDamageToBoss(bool markPresentationResolved = false,
                                           float abilityMultiplier = 1.0f,
                                           int sourcePartyIndex = -1);
    int getTetoHealingTally() const;
    int consumeTetoHealingTally();
    bool consumePresentationHitDamageApplied();
    bool consumePresentationHealingApplied();
    void markPresentationAbilityAudioPlayed();
    bool consumePresentationAbilityAudioPlayed();
    void markPresentationHitAudioPlayed();
    bool consumePresentationHitAudioPlayed();
    const AbilityDefinition* findAbilityDefinition(const std::string& abilityId) const;
    std::vector<BattleStatusBadge> getActiveStatusBadges() const;
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
    bool processNextAutomaticTurn();
    bool processAutomaticTurns();
    bool isBattleOver() const;
    BattleResolvedOutcome outcome() const;
    bool playerDamageHealsBoss() const;
    const BattleTelemetry& getBattleTelemetry() const;

private:
    struct ActivePartyBuff {
        std::string abilityId;
        int sourcePartyIndex = -1;
        int targetPartyIndex = -1;
        int speedBuff = 0;
        int atkBuff = 0; // ATK % buff/nerf (signed)
        int damageBuff = 0; // DMG % buff/nerf (signed)
        int turnsRemaining = 1;
    };

    struct ActiveBossDebuff {
        std::string abilityId;
        int sourcePartyIndex = -1;
        int charges = 0;
        int atkPercent = 0;
        int damageTakenPercent = 0;
        int bossTurnsRemaining = 0;
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
                                    const std::string& abilityKitOverride,
                                    int priority);
    void queueBossPhaseIntroTurn(int fromPhaseIndex, int toPhaseIndex);
    int firstLivingCharacterPartyIndex() const;
    int findCharacterPartyIndexByKey(const std::string& characterKey) const;
    bool isSingerPartyMember(int partyIndex) const;
    int getSingerCountInParty() const;
    bool hasQueuedExtraTurn(int partyIndex, BattleAction action, bool autoExecute) const;
    static int normalizeDamage(int value);
    const AbilityDefinition* getAbility(const std::string& abilityId) const;
    const CharacterAbilityKitDefinition* findCharacterAbilityKit(const CharacterDefinition& definition,
                                                                 const std::string& kitId) const;
    std::string resolveCharacterAbilityId(const CharacterDefinition& definition,
                                          BattleAction action,
                                          const std::string& activeKitId,
                                          const std::string& turnAbilityKitOverride) const;
    std::string resolveCharacterAssetId(const CharacterDefinition& definition,
                                        const std::string& activeKitId,
                                        const std::string& turnAbilityKitOverride) const;
    bool canCharacterUseAction(int partyIndex,
                               BattleAction action,
                               const std::string& turnAbilityKitOverride = {}) const;
    void executeAbilityEffect(const AbilityExecutionContext& context);
    float runPresentationInteraction(const PresentationContext& context);
    BattleResolvedOutcome computeDerivedOutcome() const;
    void applyBossDamage(int amount);
    void applyBossHealing(int amount);
    void applyPlayerOffenseToBoss(int amount);
    int resolveSupportTargetPartyIndex(TargetRule targetRule, int requestedTargetPartyIndex) const;
    int applyHealingToTargets(int perTargetHeal,
                              TargetRule targetRule,
                              int requestedTargetPartyIndex,
                              bool reviveDeadAllies,
                              bool recordForTetoTally,
                              bool* outAnyTargetResolved = nullptr,
                              bool* outAnyActualHpChange = nullptr);
    int applyShieldToTargets(int perTargetShield,
                             TargetRule targetRule,
                             int requestedTargetPartyIndex);
    void addToTetoHealingTally(int amount);
    int currentLivingPartyShieldTotal() const;
    void applyBossPhaseTransitionIfNeeded();
    const BossDefinition::PhaseDefinition& currentBossPhaseDefinition() const;
    void applyBossAbilitySelfCost(const AbilityDefinition& ability);
    void reviveDefeatedPartyMembersIfNeeded();
    bool canUseBossAction(BattleAction action) const;
    bool resolvePlayerAction(BattleAction action);
    bool resolveBossAction();
    bool executeCharacterAction(size_t actorIndex,
                                BattleCharacter& character,
                                BattleAction action,
                                float consumedActionValue);
    bool executeBossAction(size_t actorIndex, BattleAction action, float consumedActionValue);
    void applyJiafeiUltimateDebuff(int sourcePartyIndex, const AbilityDefinition& ability);
    void consumeJiafeiUltimateDebuff();
    void tryQueueJiafeiFollowUp(const BattleActionEvent& actionEvent);
    void tryQueueZhouShenFollowUp(const BattleActionEvent& actionEvent);
    void tryQueueSailorVenusFollowUp(const BattleActionEvent& actionEvent);
    void syncCharacterTurnParticipation(int partyIndex);
    void syncAllCharacterTurnParticipation();
    void syncCharacterUltimateTurn(int partyIndex);
    void applyPartyBuffFromAbility(int sourcePartyIndex,
                                   const AbilityDefinition& ability,
                                   float presentationMultiplier);
    void expireBuffsFromCaster(int sourcePartyIndex);
    void removeBuffsFromDefeatedCharacters();
    void removeBossDebuffsFromDefeatedCharacters();
    void refreshCharacterBuffBonuses();
    void refreshTurnActorSpeed(int partyIndex);
    void refreshTurnActorAssets(int partyIndex);
    void refreshAllTurnActorSpeeds();
    void applyAllAlliesActionAdvance(float fraction, ActionAdvanceMode mode);
    void applyCharacterActionAdvance(int partyIndex, float fraction, ActionAdvanceMode mode);
    void advanceBossActionByFraction(float fraction);
    void refreshComboState();
    void applyBossDebuffCharges(int sourcePartyIndex,
                                const AbilityDefinition& ability,
                                int charges);
    void applyBossTimedModifier(int sourcePartyIndex,
                                const AbilityDefinition& ability,
                                int atkPercent,
                                int damageTakenPercent,
                                int bossTurnsRemaining);
    int getBossDebuffCharges(const std::string& abilityId) const;
    void consumeBossDebuffCharge(const std::string& abilityId);
    void tickBossDebuffsForBossTurnEnd();
    int totalBossAtkModifierPercent() const;
    int totalBossDamageTakenModifierPercent() const;
    std::vector<int> resolveRandomLivingPartyTargets(int hitCount);
    std::vector<int> collectSupportTargetPartyIndices(int sourcePartyIndex,
                                                      const AbilityDefinition& ability,
                                                      int requestedTargetPartyIndex) const;
    bool isSailorVenusPartyIndex(int partyIndex) const;
    bool isSailorVenusTransformed(int partyIndex) const;
    void activateSailorVenusTransformation(int partyIndex);
    void revertSailorVenusTransformation(bool advanceNextAction);
    void consumeSailorVenusTurnAndQueueFinisherIfNeeded(int partyIndex);
    int getSailorVenusSpaceTally() const;
    bool isPomPomPartyIndex(int partyIndex) const;
    std::vector<int> resolvePomPomPulls(int pullCount);
    void applyPomPomPullBuffs(int sourcePartyIndex, const std::vector<int>& pullResults);
    void grantPomPomUltimateTeamOrbs(int sourcePartyIndex);
    void grantHuafeiUltimateTeamOrbs(int sourcePartyIndex);
    void clearPomPomBuffs();
    void tickPomPomBuffsForTurnStart(int partyIndex, bool isExtraTurn);
    void removePomPomBuffsFromDefeatedCharacters();
    int pomPomDamageBuffTotalForTarget(int targetPartyIndex) const;

    BattleState state_;
    BattleDefinition battleDefinition_{};
    TurnState turnState_;
    int bossCurrentHp_ = 0;
    int bossAtkBuffBonus_ = 0;
    int bossPhaseIndex_ = 0;
    std::optional<BossPhaseTransition> pendingBossPhaseTransition_;
    int bossUltimateCharge_ = 0;
    std::vector<BattleCharacter> characters_;
    int simulatedActions_ = 0;
    bool initialized_ = false;
    std::unordered_map<std::string, AbilityDefinition> abilities_;
    std::vector<BattleActionEvent> recentActionEvents_;
    BattleTelemetry telemetry_{};
    bool presentationHitDamageApplied_ = false;
    bool presentationHealingApplied_ = false;
    bool presentationAbilityAudioPlayed_ = false;
    bool presentationHitAudioPlayed_ = false;
    std::optional<BattleResolvedOutcome> forcedOutcome_;
    int luotianyiCorrectTones_ = 0;
    BattleComboState comboState_{};
    std::vector<ActivePartyBuff> activePartyBuffs_;
    std::vector<ActiveBossDebuff> activeBossDebuffs_;
    std::vector<std::string> activeCharacterAbilityKits_;
    std::unordered_map<std::string, int> bossAbilityUseCounts_;
    int nextManualUltimatePriority_ = 1000;
    int currentActionOutgoingDamage_ = 0;
    std::string pendingSplitAttackActorKey_;
    int tetoHealingTally_ = 0;
    std::mt19937 battleRng_{std::random_device{}()};
    std::mt19937 pomPomRng_{std::random_device{}()};

    struct PomPomBuffStack {
        int targetPartyIndex = -1;
        int rarity = 3;
        int damageBuff = 0;
        int pomPomTurnsRemaining = 0;
    };

    struct PomPomState {
        int partyIndex = -1;
        int pullsSinceLastFourStar = 0;
        int pullsSinceLastFiveStar = 0;
        std::vector<PomPomBuffStack> activeBuffs;
    } pomPomState_{};

    struct SailorVenusState {
        int partyIndex = -1;
        bool transformed = false;
        int turnsRemaining = 0;
        int spaceTally = 0;
        bool finisherQueued = false;
    } sailorVenusState_{};
};

} // namespace battle

#endif // BATTLE_MANAGER_H
