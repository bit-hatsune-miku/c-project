#ifndef BATTLE_UI_H
#define BATTLE_UI_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "../core/battle_manager.h"
#include "battle_hint.h"

namespace battle::ui {

struct BattleHudFontCache;

struct HudCharacterModel {
    std::string key;
    std::string assetId;
    std::string title;
    int currentHp = 0;
    int maxHp = 1;
    int ultimateCharge = 0;
    int ultimateRequired = 1;
};

struct BattleHudModel {
    std::string bossTitle;
    int bossCurrentHp = 0;
    int bossMaxHp = 1;
    std::vector<HudCharacterModel> characters;
    TurnState turnState;
    int activeActorIndex = -1;
};

class BattleHud {
public:
    BattleHud();
    BattleHud(const BattleHud&) = delete;
    BattleHud& operator=(const BattleHud&) = delete;
    ~BattleHud();

    void syncFromManager(const BattleManager& manager);
    void draw(SDL_Renderer* renderer,
              int screenW,
              int screenH,
              const std::map<std::string, SDL_Texture*>& iconByAsset);
    void reset();

    void setHint(const std::string& text, Uint32 displayMs = 0);
    void clearHint();

private:
    struct HpTransition {
        bool active = false;
        float elapsed = 0.0f;
        float duration = 0.5f;
        int fromHp = 0;
        int toHp = 0;
        int maxHp = 1;
        Uint64 lastTickMs = 0;
    };

    struct DamageFlashState {
        bool active = false;
        float elapsed = 0.0f;
        float duration = 1.0f;
        Uint64 lastTickMs = 0;
    };

    void updateBossTransition(int newHp, int maxHp);
    void updateCharacterTransition(int charIndex, int newHp, int maxHp);
    void triggerCharacterDamageFlash(int charIndex);
    int getDisplayedHp(HpTransition& transition, int fallbackCurrentHp);
    float getDamageFlashAlpha(int charIndex, int& shakeOffsetX);
    void drawTurnOrder(SDL_Renderer* renderer, const std::map<std::string, SDL_Texture*>& iconByAsset);
    void drawBossHeader(SDL_Renderer* renderer, int screenW);
    void drawHint(SDL_Renderer* renderer, int screenW);
    void drawCharacterStatus(SDL_Renderer* renderer,
                             int screenW,
                             int screenH,
                             const std::map<std::string, SDL_Texture*>& iconByAsset);

    BattleHudModel model_;
    bool initialized_ = false;
    HpTransition bossHpTransition_;
    std::vector<HpTransition> characterHpTransitions_;
    std::vector<DamageFlashState> characterDamageFlashes_;
    BattleHint hint_;

#ifdef BATTLE_ENABLE_TTF
    std::unique_ptr<BattleHudFontCache> fontCache_;
#endif
};

} // namespace battle::ui

#endif // BATTLE_UI_H
