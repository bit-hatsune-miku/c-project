#ifndef BATTLE_FEEDBACK_H
#define BATTLE_FEEDBACK_H

#include <vector>

#include <SDL2/SDL.h>

#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include "camera_3d.h"
#include "../core/battle_manager.h"

namespace battle::render {

struct FeedbackEntityAnchor {
    bool isBoss = false;
    int partyIndex = -1;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    bool visible = true;
};

class BattleFeedbackSystem {
public:
    void reset(const BattleManager& manager);
    void shutdown();

    void syncFromManager(const BattleManager& manager, bool presentationPlaybackActive);
    void update(float deltaSeconds);

    void queuePresentationHitShakes(bool isBossCaster, int hitEvents, const BattleManager& manager);
    void queuePresentationHitFeedback(bool isBossCaster,
                                      int hitEvents,
                                      int perHitDamage,
                                      const BattleManager& manager);
    float getShakeOffsetX(bool isBoss, int partyIndex) const;

    void render(SDL_Renderer* renderer,
                const Camera3D& camera,
                const std::vector<FeedbackEntityAnchor>& anchors);

private:
    struct DamagePopup {
        bool onBoss = false;
        int partyIndex = -1;
        int damage = 0;
        float elapsed = 0.0f;
        float lifetime = 0.95f;
        float jitterX = 0.0f;
        float jitterY = 0.0f;
    };

    struct DamageShakeState {
        int queuedHits = 0;
        bool active = false;
        float elapsed = 0.0f;
        int directionSign = 1;
    };

    void spawnDamagePopup(bool onBoss, int partyIndex, int damage);
    void queueDamageShake(bool onBoss, int partyIndex, int hitCount = 1);
    void updateOneDamageShakeState(DamageShakeState& state, float deltaSeconds);

    std::vector<int> lastCharacterHp_;
    int lastBossHp_ = -1;
    bool suppressBossNextDeltaPopup_ = false;
    std::vector<bool> suppressCharacterNextDeltaPopup_;

    std::vector<DamagePopup> popups_;
    DamageShakeState bossShakeState_;
    std::vector<DamageShakeState> characterShakeStates_;

#ifdef BATTLE_ENABLE_TTF
    TTF_Font* font_ = nullptr;
#endif
};

} // namespace battle::render

#endif
