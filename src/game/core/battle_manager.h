#ifndef BATTLE_MANAGER_H
#define BATTLE_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector>

namespace battle {

struct BossDefinition {
    std::string key;
    std::string title;
    std::string assets;
    int spd = 0;
    int atk = 0;
    int hp = 0;
    std::string ability;
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
    std::string ultimate;
    int ultimatePoints = 0;
};

struct BattleState {
    BossDefinition boss;
    std::vector<CharacterDefinition> party;
};

enum class AbilityType {
    Attack,
    Heal,
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
    AbilityType type = AbilityType::Attack;
    TargetRule targetRule = TargetRule::SingleEnemy;
    float multiplier = 1.0f;
    int flatHeal = 0;
    InteractionType interactionType = InteractionType::None;
    std::string presentationId;
};

struct PresentationContext {
    std::string abilityId;
    std::string presentationId;
    InteractionType interactionType = InteractionType::None;
    int casterIndex = -1;
    int targetIndex = -1;
    bool isBoss = false;
};

struct AbilityExecutionContext {
    const AbilityDefinition* ability = nullptr;
    int casterPartyIndex = -1;
    bool isBossCaster = false;
    int baseDamage = 0;
    int baseHeal = 0;
    float presentationMultiplier = 1.0f;
    int bossMaxHp = 1;
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

    int ultimateCharge() const;
    void gainUltimatePoint();
    bool canUseUltimate() const;
    void consumeUltimate();

private:
    CharacterDefinition definition_;
    int partyIndex_ = 0;
    int hp_ = 0;
    int ultimateCharge_ = 0;
};

enum class ParticipantType {
    Boss,
    Character
};

struct TurnActor {
    ParticipantType type = ParticipantType::Character;
    std::string key;
    std::string title;
    int partyIndex = -1; // 0 = left-most character. -1 for boss.
    int priority = 0; // Higher comes first on same action value.
    bool isExtraTurn = false;
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

class BattleManager {
public:
    bool initialize(const std::string& bossKey, const std::vector<std::string>& characterKeys);
    void printBattleSummary() const;
    const BattleState& getBattleState() const;
    const TurnState& getTurnState() const;
    int getPreviewNextActorIndex() const;
    int getBossCurrentHp() const;
    int getBossMaxHp() const;
    int getCharacterCurrentHp(int partyIndex) const;
    int getCharacterMaxHp(int partyIndex) const;
    int getCharacterUltimateCharge(int partyIndex) const;
    int getCharacterUltimateRequired(int partyIndex) const;
    bool prepareCurrentPlayerSplitAttackPlan(int hitCount, std::vector<int>& outHitDamages);
    void applyBossSplitHitDamage(int damage);
    bool commitCurrentPlayerSplitAttackTurn();
    bool executePlayerTurn();
    bool processAutomaticTurns();
    bool isBattleOver() const;

private:
    bool buildInitialTurnState();
    TurnEvent peekNextTurnEvent() const;
    TurnEvent advanceToNextTurnEvent();
    void runPseudoBattle(int maxActions);
    void executeTurn(size_t actorIndex);
    void queueExtraTurnForCharacter(int partyIndex);
    int firstLivingCharacterPartyIndex() const;
    static int normalizeDamage(int value);
    const AbilityDefinition* getAbility(const std::string& abilityId) const;
    void executeAbilityEffect(const AbilityExecutionContext& context);
    float runPresentationInteraction(const PresentationContext& context);

    BattleState state_;
    TurnState turnState_;
    int bossCurrentHp_ = 0;
    std::vector<BattleCharacter> characters_;
    int simulatedActions_ = 0;
    bool initialized_ = false;
    std::unordered_map<std::string, AbilityDefinition> abilities_;
};

} // namespace battle

#endif // BATTLE_MANAGER_H
