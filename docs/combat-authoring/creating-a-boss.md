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
3. Sprite/icon/audio assets
4. Usually a custom boss presentation

## Required Files

At minimum, a new boss should have:

- A boss entry in `assets/combat/boss.json`
- A sprite at `assets/combat/sprites/<assets>.png`
- An icon at `assets/combat/icons/<assets>.png`
- A battle entry in `assets/combat/battles.json`

Recommended voice files:

- `assets/combat/voices/<bossKey>/ability.wav`
- `assets/combat/voices/<assets>/hit.wav`
- `assets/combat/voices/<assets>/dead.wav`

You can also set:

- `voiceHit` directly in `boss.json`

Important:

- Boss hit and death voice lookup are not identical.
- `voiceHit` only overrides hit voice lookup.
- Dead voice is still resolved from combat voice folders, usually by `assets` or `bossKey`.

## Boss JSON Schema

Bosses live in `assets/combat/boss.json`.

Minimal example:

```json
{
  "myBoss": {
    "title": "My Boss",
    "assets": "myBoss",
    "voiceHit": "assets/combat/voices/myBoss/hit.wav",
    "spd": 180,
    "atk": 40,
    "hp": 500,
    "standardAbility": "BossStandardAttack",
    "skillAbility": "MyBossAttack",
    "ability": "MyBossAttack",
    "ultimate": "MyBossAttack",
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
    },
    "ultimatePoints": 6,
    "startingOrbs": 1,
    "bgm": "myBossTheme",
    "bgmVolume": 0.3
  }
}
```

## Root Fields

Supported boss fields:

- `title`
- `assets`
- `voiceHit`
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
- `abilities`

## Real Boss Behavior vs Compatibility Fields

This is the most important boss-specific authoring note.

The current system still stores:

- `standardAbility`
- `skillAbility`
- `ultimate`

But actual boss turn execution in `BattleManager::executeBossAction()` always resolves the boss's normal ability through:

- `ability`
- then `skillAbility`
- then `standardAbility`

And `resolveBossAction()` only runs the boss's standard turn path.

Practical authoring rule:

- Treat the boss as having one real combat ability
- Set `ability`, `skillAbility`, and `ultimate` to the same real boss skill unless you are intentionally preserving compatibility for other systems
- Keep `standardAbility` filled for compatibility, but do not rely on it for actual design

## Boss Ability Schema

Boss nested abilities use the same parser as playable characters.

Supported fields:

- `id`
- `name`
- `instructionHint`
- `type`
- `targetRule`
- `multiplier`
- `flatHeal`
- `speedBuff`
- `atkBuff`
- `actionAdvance`
- `reviveDeadAllies`
- `interactionType`
- `presentationId`

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
- Put `hit.wav` and `dead.wav` under `assets/combat/voices/<assets>/`
- Optionally set `voiceHit` to an explicit path if hit audio should come from somewhere else

Why this split is safest:

- Boss ability voice playback often tries `<bossKey>` first
- Boss hit/death fallback usually tries `assets`, then `bossKey`

If you only provide one folder, use both names if possible to avoid surprises.

## BGM

Boss BGM is loaded by name, not full path.

If `bgm` is:

- `lyooBoss`

the runtime looks for:

- `assets/combat/bgm/lyooBoss.wav`
- then `assets/combat/bgm/lyooBoss.mp3`

`bgmVolume` is a direct float multiplier.

## Splash Art Behavior

Boss splash art is effectively automatic.

During presentation playback:

- if the boss presentation provides `getSplashConfig()`, that config is used
- otherwise the runtime creates a default splash using the boss sprite texture

The splash title now uses the ability `name`, not the raw ability id.

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
- `ability`, `skillAbility`, and `ultimate` point to the same real boss skill
- Boss nested `abilities.skill.id` matches those ids
- `presentationId` is registered
- Voice files exist in the folders the runtime actually checks
- Battle entry exists in `assets/combat/battles.json`
- `stageKey` is set, even if it is only `default_stage` for now

## Known Gotchas

- Bosses do not really use separate standard/skill/ultimate combat behaviors right now
- Generic single-target boss behavior targets the first living ally, not a random ally
- `voiceHit` does not configure death voice
- Ability ids are global; duplicate ids overwrite earlier entries during ability loading
- Icons and sprites are safest as `.png`
- If your boss mechanic changes game rules, expect `BattleManager` work in addition to JSON and presentation work
