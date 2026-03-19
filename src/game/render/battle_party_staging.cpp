#include "battle_party_staging.h"

#include <algorithm>

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
                                           const std::vector<bool>& livingPartyMembers,
                                           bool bossActing,
                                           int actingPartyIndex) {
    std::vector<int> visibleSlotByPartyIndex(livingPartyMembers.size(), -1);
    int visiblePartySize = 0;
    for (size_t i = 0; i < livingPartyMembers.size(); ++i) {
        if (!livingPartyMembers[i]) {
            continue;
        }
        visibleSlotByPartyIndex[i] = visiblePartySize;
        ++visiblePartySize;
    }

    const int actingVisibleSlot =
        actingPartyIndex >= 0 &&
        static_cast<size_t>(actingPartyIndex) < visibleSlotByPartyIndex.size()
            ? visibleSlotByPartyIndex[static_cast<size_t>(actingPartyIndex)]
            : -1;

    for (SceneEntity& entity : entities) {
        if (entity.isBoss) {
            entity.lineupVisible = true;
            continue;
        }

        if (entity.partyIndex < 0 ||
            static_cast<size_t>(entity.partyIndex) >= livingPartyMembers.size() ||
            !livingPartyMembers[static_cast<size_t>(entity.partyIndex)]) {
            entity.lineupVisible = false;
            continue;
        }

        entity.lineupVisible = true;
        entity.worldY = bossActing || actingVisibleSlot < 0 ? kBossTurnCharacterBaseY : kDuelCharacterBaseY;
        entity.worldZ = 0.0f;

        const int pi = entity.partyIndex;
        const int visibleSlot = visibleSlotByPartyIndex[static_cast<size_t>(pi)];
        if (bossActing || actingVisibleSlot < 0) {
            const float totalWidth = std::max(0, visiblePartySize - 1) * kBossTurnCharacterSpacingWorld;
            const float startX = kDuelBossSlotX - totalWidth * 0.5f;
            entity.worldX = startX + visibleSlot * kBossTurnCharacterSpacingWorld;
        } else {
            const int relativeSlot = visibleSlot - actingVisibleSlot;
            entity.worldX = kDuelCharacterSlotX + relativeSlot * kCharacterSpacingWorld;
        }
    }
}

} // namespace battle::render
