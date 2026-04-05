# Creating a Boss

This document describes how bosses are authored in the current combat system.

Relevant implementation files:

- `assets/combat/boss.json`
- `assets/combat/battles.json`
- `src/game/core/battle_loader.cpp`
- `src/game/core/battle_manager.cpp`
- `src/game/presentation/presentation_runtime.cpp`
- `src/game/demo_battle_session.cpp`
- `src/game/app_battle_session.cpp`

## Overview

A boss is made of:

1. A boss entry in `assets/combat/boss.json`
2. A battle entry in `assets/combat/battles.json`
3. Sprite, icon, BGM, and voice assets
4. Usually a custom boss presentation

## Required Files

At minimum, a new boss should have:

- A boss entry in `assets/combat/boss.json`
- A sprite at `assets/combat/sprites/<assets>.png`
- An icon at `assets/combat/icons/<assets>.png`
- A battle entry in `assets/combat/battles.json`

Safe recommended voice files:

- `assets/combat/voices/<bossKey>/ability.wav`
- `assets/combat/voices/<assets>/hit.wav`
- `assets/combat/voices/<assets>/dead.wav`

Optional additional voice files:

- `assets/combat/voices/<bossKey>/ultimate.wav`
- `assets/combat/voices/<bossKey>/ready.wav`
- `assets/combat/voices/<bossKey>/special.wav`
- `assets/combat/voices/<assets>/healed.wav`

Notes:

- `voiceHit` can also be set directly in `boss.json`.
- `phaseChangeVoice` is not a clip name lookup. It is a direct path stored in phase data.
- `.png` is still the safest art format for boss sprites and icons.

## Boss JSON Schema

Bosses live in `assets/combat/boss.json`.

Minimal example:

```json
{
  "myBoss": {
    "title": "My Boss",
    "assets": "myBoss",
    "voiceSpeakerId": "my_boss_voice",
    "voiceHit": "assets/combat/voices/myBoss/hit.wav",
    "spd": 180,
    "atk": 40,
    "hp": 500,
    "standardAbility": "BossStandardAttack",
    "skillAbility": "MyBossAttack",
    "ability": "MyBossAttack",
    "ultimate": "MyBossAttack",
    "ultimatePoints": 6,
    "startingOrbs": 1,
    "bgm": "myBossTheme",
    "bgmVolume": 0.3,
    "phases": {
      "phase1": {
        "bgm": "myBossTheme",
        "presentation": {
          "profileId": "myBoss.phase1"
        }
      },
      "phase2": {
        "atkBonusPercent": 20,
        "bgm": "myBossThemePhase2",
        "bgmVolume": 0.32,
        "phaseChangeVoice": "assets/combat/voices/myBoss/phase2change.wav",
        "presentation": {
          "profileId": "myBoss.phase2"
        }
      },
      "phase3": {
        "atkBonusPercent": 40,
        "bgm": "myBossThemePhase3",
        "phaseChangeVoice": "assets/combat/voices/myBoss/phase3change.wav",
        "presentation": {
          "profileId": "myBoss.phase3"
        }
      }
    },
    "abilities": {
      "skill": {
        "id": "MyBossAttack",
        "name": "My Boss Attack",
        "instructionHint": "Optional input hint.",
        "type": "attack",
        "targetRule": "all_enemies",
        "multiplier": 1.2,
        "interactionType": "parry",
        "presentationId": "my_boss_attack"
      }
    }
  }
}
```

## Root Fields

Supported boss fields:

- `title`
- `assets`
- `voiceHit`
- `voiceSpeakerId`
- `spd`
- `atk`
- `hp`
- `standardAbility`
- `skillAbility`
- `ability`
- `ultimate`
- `ultimatePoints`
- `startingOrbs`
- `bgm`
- `bgmVolume`
- `phases`
- `abilities`

## Real Boss Behavior vs Compatibility Fields

This is the most important boss-specific authoring note.

The current system still stores:

- `standardAbility`
- `skillAbility`
- `ultimate`

But actual boss turn execution in `BattleManager::executeBossAction()` resolves the boss's normal action through:

- `ability`
- then `skillAbility`
- then `standardAbility`

Practical authoring rule:

- Treat the boss as having one real combat ability.
- Set `ability`, `skillAbility`, and `ultimate` to the same real boss skill unless you are intentionally preserving compatibility for other systems.
- Keep `standardAbility` filled for compatibility, but do not rely on it as a distinct design slot.

## Boss Ability Schema

Boss nested abilities use the same parser as playable characters.

Supported fields:

- `id`
- `name`
- `statusName`
- `instructionHint`
- `type`
- `targetRule`
- `multiplier`
- `flatHeal`
- `baseShield`
- `amountPercentOfCasterMaxHp`
- `speedBuff`
- `atkBuff`
- `orbGain`
- `actionAdvance`
- `selfHpCostPercentOfMax`
- `selfHpCostPercentIncreasePerUse`
- `selfHpCostPercentMax`
- `reviveDeadAllies`
- `interactionType`
- `presentationId`
- `inputPromptType`
- `inputPromptKeys`

Supported `type` values:

- `attack`
- `heal`
- `shield`
- `buff`
- `debuff`

Supported `targetRule` values:

- `single_enemy`
- `all_enemies`
- `single_ally`
- `all_allies`
- `self`

Supported `interactionType` values:

- `none`
- `rhythm`
- `parry`

Supported `inputPromptType` values:

- `none`
- `space`
- `wild`
- `custom`
- `arrows`
- `leftRight`
- `spamSpace`

## Phases

Boss phase data is now authored directly in `boss.json`.

The current loader supports:

- `phases.phase1`
- `phases.phase2`
- `phases.phase3`

Each phase entry can contain:

- `atkBonusPercent`
- `bgm`
- `bgmVolume`
- `presentation`
- `phaseChangeVoice`

Notes:

- Phase data is optional.
- You can author only phase 2 and phase 3 if phase 1 uses the base boss defaults.
- `phaseChangeVoice` is optional. If it is missing, empty, or points to a file that does not exist, the transition still works and no voice is played.
- `phaseChangeVoice` is a direct file path, not an implicit `phase2change.wav` or `phase3change.wav` convention.

## Targeting Rules for Bosses

Generic boss targeting is currently simple:

- `all_enemies`: applies to all living party members
- `single_enemy`: hits the first living character in party order

If you want:

- random target selection
- focus on one ally during the presentation
- custom dodge logic against one chosen ally
- split-hit behavior tied to presentation events

you will probably need custom code in `src/game/core/battle_manager.cpp`, not just JSON.

Examples already in the codebase:

- Jiafei boss custom focused target logic
- Luotianyi boss custom interaction logic
- Wechatalipay boss custom UI minigame damage timing

## Voice Authoring Rules

Safest current voice layout for bosses:

- Put `ability.wav` under `assets/combat/voices/<bossKey>/`
- Put `hit.wav`, `dead.wav`, and `healed.wav` under `assets/combat/voices/<assets>/`
- Optionally set `voiceHit` to an explicit path if hit audio should come from somewhere else
- Put phase-change clips wherever you want, then reference them directly with `phaseChangeVoice`

Why this split is safest:

- Boss ability voice playback often tries `<bossKey>` first.
- Boss hit / death fallback usually tries `assets`, then `bossKey`.
- `voiceHit` only overrides hit voice lookup.
- Phase-change voice does not participate in the clip-name lookup rules; it uses the exact path authored in the phase entry.

## BGM

Boss BGM is loaded by name, not full path.

If `bgm` is:

- `lyooBoss`

the runtime looks for:

- `assets/combat/bgm/lyooBoss.wav`
- then `assets/combat/bgm/lyooBoss.mp3`

`bgmVolume` is a direct float multiplier.

Phase entries can override both `bgm` and `bgmVolume`.

## Splash Art Behavior

Boss splash art is effectively automatic.

During presentation playback:

- if the boss presentation provides `getSplashConfig()`, that config is used
- otherwise the runtime creates a default splash using the boss sprite texture

The splash title uses the resolved ability `name`, not the raw ability id.

## Adding the Boss to Battles

Bosses do nothing until they are referenced by `assets/combat/battles.json`.

Example:

```json
{
  "battles": {
    "my_boss_test": {
      "id": 300,
      "name": "My Boss Test",
      "bossKey": "myBoss",
      "isLineupFixed": true,
      "lineup": ["miku", "lyoo", "jiafei"],
      "stageKey": "default_stage"
    }
  }
}
```

## Recommended Boss Checklist

- Boss exists in `assets/combat/boss.json`
- `assets` matches a real sprite and icon
- `ability`, `skillAbility`, and `ultimate` point to the same real boss skill unless you intentionally need compatibility differences
- Boss nested `abilities.skill.id` matches those ids
- Any authored phases use `phase1`, `phase2`, and `phase3`
- Any `phaseChangeVoice` path points to a real file
- `presentationId` is registered
- Voice files exist in the folders the runtime actually checks
- Battle entry exists in `assets/combat/battles.json`
- `stageKey` is set, even if it is only `default_stage` for now

## Known Gotchas

- Bosses do not really use separate standard / skill / ultimate combat behaviors right now.
- Generic single-target boss behavior targets the first living ally, not a random ally.
- `voiceHit` does not configure death voice.
- `phaseChangeVoice` only plays on phase transition if the file is present.
- Ability ids are global; duplicate ids overwrite earlier entries during ability loading.
- `.png` is still the safest cross-runtime art format for boss sprites and icons.
- If your boss mechanic changes game rules, expect `BattleManager` work in addition to JSON and presentation work.
