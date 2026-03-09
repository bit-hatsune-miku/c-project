#ifndef BATTLE_UI_H
#define BATTLE_UI_H

#include <map>
#include <string>

#include <SDL2/SDL.h>

#include "battle_manager.h"

namespace battle::ui {

void drawTurnOrderUI(SDL_Renderer* renderer,
                     const TurnState& turnState,
                     const std::map<std::string, SDL_Texture*>& iconByAsset,
                     int activeActorIndex);

void drawCharacterStatusUI(SDL_Renderer* renderer,
                           int screenW,
                           int screenH,
                           const BattleManager& manager,
                           const BattleState& battleState,
                           const std::map<std::string, SDL_Texture*>& iconByAsset);

void drawBossHeaderUI(SDL_Renderer* renderer,
                      int screenW,
                      const BattleState& battleState,
                      int bossCurrentHp,
                      int bossMaxHp);

void updateCharacterHp(int charIndex, int newHp, int maxHp);
void updateBossHp(int newHp, int maxHp);

void shutdownFonts();

} // namespace battle::ui

#endif // BATTLE_UI_H
