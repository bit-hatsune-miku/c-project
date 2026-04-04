# Creating a Playable Character

This document describes how the combat system currently expects a playable character to be authored.

It is based on the current implementation in:

- `assets/combat/characters.json`
- `assets/combat/battles.json`
- `src/game/core/battle_loader.cpp`
- `src/game/core/battle_manager.cpp`
- `src/game/presentation/presentation_registry.cpp`
- `src/game/render/battle_asset_loading.cpp`

Use this as the source of truth for what works now, not for what we may want later.

## Overview

A playable character is made of four parts:

1. Data in `assets/combat/characters.json`
2. Art/audio assets referenced by the character's `assets` key
3. One or more ability definitions, usually nested inside the character entry
4. Optional custom presentation code, registered by `presentationId`

If you also want the character to be selectable or appear in a real fight, add them to a battle in `assets/combat/battles.json`.

## Required Files

At minimum, a new playable character should have:

- A character entry in `assets/combat/characters.json`
- A battle sprite at `assets/combat/sprites/<assets>.png`
- A HUD icon at `assets/combat/icons/<assets>.png`

Recommended voice files:

- `assets/combat/voices/<assets>/ability.wav`
- `assets/combat/voices/<assets>/ultimate.wav`
- `assets/combat/voices/<assets>/hit.wav`
- `assets/combat/voices/<assets>/dead.wav`

Optional additional voice files already used by the runtime:

- `assets/combat/voices/<assets>/ability2.wav`
- `assets/combat/voices/<assets>/ready.wav`
- `assets/combat/voices/<assets>/healed.wav`
- `assets/combat/voices/<assets>/idle.wav`

Important:

- The main SDL battle loaders in `src/game/render/battle_asset_loading.cpp` currently only try `.png` for sprites and icons.
- If you use `.webp` or another extension, the current battle UI will usually not load it unless you extend the loader.

## Character JSON Schema

Playable characters live in `assets/combat/characters.json`.

Minimal example:

```json
{
  "my_character": {
    "title": "My Character",
    "assets": "my_character",
    "class": "DPS",
    "spd": 140,
    "atk": 24,
    "hp": 52,
    "standardAbility": "BasicAttack",
    "skillAbility": "MySkill",
    "ability": "MySkill",
    "ultimate": "MyUltimate",
    "abilities": {
      "skill": {
        "id": "MySkill",
        "name": "My Skill",
        "instructionHint": "Optional hint shown during the presentation.",
        "type": "attack",
        "targetRule": "single_enemy",
        "multiplier": 1.2,
        "interactionType": "none",
        "presentationId": "my_skill_presentation"
      },
      "ultimate": {
        "id": "MyUltimate",
        "name": "My Ultimate",
        "type": "attack",
        "targetRule": "single_enemy",
        "multiplier": 2.0,
        "interactionType": "none",
        "presentationId": "my_ultimate_presentation"
      }
    },
    "ultimatePoints": 3,
    "startingOrbs": 1
  }
}
```

## Root Fields

Supported root-level character fields:

- `title`: displayed character name
- `assets`: asset stem used for sprite/icon lookup and most voice lookup
- `class`: display text only
- `spd`: base speed
- `atk`: base attack
- `hp`: max HP
- `standardAbility`: compatibility field; currently not meaningfully separate from skill
- `skillAbility`: the character's regular turn ability id
- `ability`: alias/fallback for `skillAbility`
- `ultimate`: the character's ultimate ability id
- `ultimatePoints`: number of orbs required to unlock the ultimate
- `startingOrbs`: starting ultimate charge
- `baseShield`: legacy character-level shield fallback; prefer ability-level support fields instead
- `abilities`: nested ability definitions for `skill`, `ultimate`, or `standard`

Recommended practice:

- Keep `skillAbility` and `ability` identical.
- Always define both a real regular ability and a real ultimate.
- Use `assets` as a clean stable key, because a lot of asset lookup is built around it.

## Ability Schema

Nested character abilities are parsed by `parseAbilityDefinition()` in `src/game/core/battle_loader.cpp`.

Supported fields:

- `id`
- `name`
- `instructionHint`
- `type`
- `targetRule`
- `multiplier`
- `flatHeal`
- `baseShield`
- `amountPercentOfCasterMaxHp`
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

Anything else falls back to `attack`.

Supported `targetRule` values:

- `single_enemy`
- `all_enemies`
- `single_ally`
- `all_allies`
- `self`

Anything else falls back to `single_enemy`.

Supported `interactionType` values:

- `none`
- `rhythm`
- `parry`

Anything else falls back to `none`.

## What Each Ability Field Actually Does

Current runtime behavior:

- `multiplier`
  - For `attack`: scales outgoing damage
  - For `debuff`: currently also deals damage if `multiplier > 0`
  - For `heal` / `shield`: only scales the presentation result, not the base authored amount
- `amountPercentOfCasterMaxHp`
  - Default support authoring field for `heal` and `shield`
  - Base amount is resolved from the caster's current max HP before presentation scaling
- `flatHeal`
  - Legacy fallback base heal amount for `heal` when `amountPercentOfCasterMaxHp` is not set
- `baseShield`
  - Legacy fallback base shield amount for `shield` when `amountPercentOfCasterMaxHp` is not set
- `speedBuff`
  - Applied by `buff` abilities through `applyPartyBuffFromAbility()`
- `atkBuff`
  - Applied by `buff` abilities through `applyPartyBuffFromAbility()`
- `actionAdvance`
  - Applies after a `buff` ability and advances all allies toward their turns
- `reviveDeadAllies`
  - Only meaningful on `heal` abilities
- `instructionHint`
  - Shown automatically during the presentation by `BattleSessionCore`
- `presentationId`
  - Used to construct a custom `AbilityPresentation`

Support authoring rule:

- New healers and shielders should use `amountPercentOfCasterMaxHp`.
- `flatHeal`, ability-level `baseShield`, and root-level `CharacterDefinition.baseShield` exist only as legacy fallbacks.

## Current Turn Logic

This is the part most likely to surprise you.

The current playable-turn flow in `src/game/core/battle_manager.cpp` behaves like this:

- A normal player turn uses the character's regular ability
- The regular ability is resolved from `ability`, then `skillAbility`, then `standardAbility`
- In practice, `standardAbility` and `skillAbility` are not really distinct right now
- When a character reaches full ultimate charge, the system queues an extra turn for the ultimate
- That ultimate turn is a separate turn actor in the action-value system

Practical consequence:

- Treat `skillAbility` as the real "normal button"
- Treat `ultimate` as a separate extra-turn action
- Do not rely on a separate basic-attack-vs-skill distinction unless you are planning engine changes too

## Buff Behavior

Buffs are implemented in `BattleManager`.

Current rules:

- Buffs are party buffs on allies, not generic status-effect objects
- A buff from the same source character and same ability does not stack on the same target
- Reapplying the same buff refreshes/replaces the existing values
- Buffs expire when the source character starts a new non-extra turn
- Buffs are removed if the source dies
- Buffs are removed if the target dies

Speed-specific behavior:

- Speed buffs update the turn actor's action value using `oldAV * oldSpd / newSpd`
- This means a speed buff can immediately move a character closer to their turn if they have not acted yet

## Damage / Healing Timing

Two patterns are supported:

1. Passive presentation:
   - presentation plays
   - runtime applies the normal effect after the presentation ends

2. Timed presentation:
   - presentation emits hit/heal events during playback
   - runtime applies damage/healing exactly when those events occur

If your character attack is multi-hit or needs precise timing, make the presentation emit hit events.

## Authoring Checklist

Before testing a new character, make sure all of these are true:

- Character exists in `assets/combat/characters.json`
- `assets` matches real sprite/icon files
- `skillAbility`, `ability`, and nested `abilities.skill.id` all match
- `ultimate` and nested `abilities.ultimate.id` match
- Every custom `presentationId` is registered in `src/game/presentation/presentation_registry.cpp`
- New presentation `.cpp` and `.h` files are added to `BATTLE_PRESENTATION_SOURCES` in `CMakeLists.txt`
- Ability ids are globally unique across all characters, bosses, and `assets/combat/abilities.json`

That last point matters because all abilities are merged into one `unordered_map`, so duplicate ids overwrite each other.

## Adding the Character to a Battle

To actually use the character, add them to a battle in `assets/combat/battles.json`.

Example:

```json
{
  "battles": {
    "my_test_battle": {
      "id": 200,
      "name": "My Test Battle",
      "bossKey": "lyooBoss",
      "isLineupFixed": true,
      "lineup": ["my_character", "miku", "lyoo"],
      "stageKey": "default_stage"
    }
  }
}
```

Notes:

- If `isLineupFixed` is `true`, the fixed lineup must contain at least one valid character
- If you explicitly set `partySize`, keep it consistent with the final merged fixed lineup
- If you omit `partySize`, the loader usually resolves it correctly for fixed battles

## Testing Tips

Fastest current ways to test:

- Run an existing demo executable with an explicit battle key:
  - `./build/bin/demo my_test_battle`
- Or add a dedicated demo target in `CMakeLists.txt`
- If you add a dedicated demo target, also map its compile definition in `src/demo.cpp`

## Known Gotchas

- `ultimatePoints` is effectively treated as at least `1` by runtime clamps
- A "character with no ultimate at all" is not a clean supported case right now
- `standardAbility` is mostly compatibility/UI plumbing right now
- Sprites and icons are safest as `.png`
- Voice lookup for playable characters is centered on `assets`, not `title`
- Special mechanics like Jiafei follow-ups, Luotianyi tone tracking, or Wechatalipay shield-sum damage required engine code, not just JSON

If your new character needs a mechanic that is not expressible with `attack/heal/shield/buff/debuff + presentation multiplier`, plan on touching `src/game/core/battle_manager.cpp` as well.
