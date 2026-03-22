// HACK: Provide shield access for demo HUD rendering
#include "../core/battle_manager.h"
const battle::BattleManager* g_lastBattleHudManager = nullptr;
extern "C" int getCharacterShield_HACK(const battle::BattleManager* mgr, int idx) {
    if (!mgr) return 0;
    // This is a hack: cast away const, access private vector
    struct ExposeCharacters : public battle::BattleManager {
        using battle::BattleManager::characters_;
    };
    const auto* ex = reinterpret_cast<const ExposeCharacters*>(mgr);
    if (idx < 0 || static_cast<size_t>(idx) >= ex->characters_.size()) return 0;
    return ex->characters_[static_cast<size_t>(idx)].getShield();
}