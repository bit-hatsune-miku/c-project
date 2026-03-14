#include "battle_party_staging.h"

namespace battle::render {
namespace {

constexpr float kCharacterGapWorld = 1200.0f;
constexpr float kDuelCharacterSlotX = -0.5f * kCharacterGapWorld;
constexpr float kCharacterSpacingWorld = 400.0f;
constexpr float kBossTurnCharacterSpacingWorld = 260.0f;
constexpr float kDuelBossSlotX = 0.0f;
constexpr float kDuelCharacterBaseY = 300.0f;
constexpr float kBossTurnCharacterBaseY = 420.0f;

} // namespace

void computeDefaultPartyCharacterPositions(std::vector<SceneEntity>& entities,
                                           int partySize,
                                           bool bossActing,
                                           int actingPartyIndex) {
    for (SceneEntity& entity : entities) {
        if (entity.isBoss) {
            continue;
        }

        entity.visible = true;
        entity.worldY = bossActing || actingPartyIndex < 0 ? kBossTurnCharacterBaseY : kDuelCharacterBaseY;
        entity.worldZ = 0.0f;

        const int pi = entity.partyIndex;
        if (bossActing || actingPartyIndex < 0) {
            const float totalWidth = (partySize - 1) * kBossTurnCharacterSpacingWorld;
            const float startX = kDuelBossSlotX - totalWidth * 0.5f;
            entity.worldX = startX + pi * kBossTurnCharacterSpacingWorld;
        } else {
            const int relativeSlot = pi - actingPartyIndex;
            entity.worldX = kDuelCharacterSlotX + relativeSlot * kCharacterSpacingWorld;
        }
    }
}

} // namespace battle::render
