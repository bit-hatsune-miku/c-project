#ifndef BATTLE_PARTY_STAGING_H
#define BATTLE_PARTY_STAGING_H

#include <vector>

#include "battle_scene_types.h"

namespace battle::render {

void computeDefaultPartyCharacterPositions(std::vector<SceneEntity>& entities,
                                           int partySize,
                                           bool bossActing,
                                           int actingPartyIndex);

} // namespace battle::render

#endif
