# Creating a Playable Character

This document describes how playable characters are authored in the current combat system.

It is based on the current implementation in:

- `assets/combat/characters.json`
- `assets/combat/battles.json`
- `src/game/core/battle_loader.cpp`
- `src/game/core/battle_manager.cpp`
- `src/game/presentation/presentation_registry.cpp`
- `src/game/render/battle_asset_loading.cpp`
- `src/game/app_battle_session.cpp`

Use this as the source of truth for what works now.

## Overview

A playable character is made of four parts:

1. Data in `assets/combat/characters.json`
2. Art and voice assets referenced by the character's `assets` key
3. One or more ability definitions, usually nested inside the character entry
4. Optional custom presentation code, registered by `presentationId`

If you also want the character to be selectable or appear in a real fight, add them to a battle in `assets/combat/battles.json`.

## Required Files

At minimum, a new playable character should have:

- A character entry in `assets/combat/characters.json`
- A battle sprite at `assets/combat/sprites/<assets>.png`
- A HUD icon at `assets/combat/icons/<assets>.png`

Safe recommended voice files:

- `assets/combat/voices/<assets>/ability.wav`
- `assets/combat/voices/<assets>/ultimate.wav`
- `assets/combat/voices/<assets>/hit.wav`
- `assets/combat/voices/<assets>/dead.wav`

Other voice files already used by the runtime:

- `assets/combat/voices/<assets>/ability2.wav`
- `assets/combat/voices/<assets>/ready.wav`
- `assets/combat/voices/<assets>/special.wav`
- `assets/combat/voices/<assets>/healed.wav`
- `assets/combat/voices/<assets>/revived.wav`
- `assets/combat/voices/<assets>/shielded.wav`
- `assets/combat/voices/<assets>/idle.wav`

Notes:

- Voice clips can also be `.opus`; `.wav` is just the safest example to document.
- Regular ability playback checks `ability` first and also accepts `skill` as a fallback clip name.
- Ultimate splash / activation playback checks `ready`, then `special`.
- `ability2` is only useful for presentations that intentionally trigger multiple voice cues.

## Art Recommendations

Current runtime-safe authoring defaults:

- Combat sprite:
  - `140x260` per frame
  - Animated sprite sheets should be horizontal strips where each frame is `140x260`
- HUD icon:
  - authored square
  - keep the character centered because the HUD crops into a `140x140` display box
  - `512x512` is a good export target
  - `256x256` is a practical minimum

Important asset format note:

- Some SDL loaders already try `.png` and `.webp` for sprites and icons.
- The GL world renderer still assumes `.png` for combat sprites.
- Because the game uses both paths, author new combat sprites and icons as `.png` unless you are intentionally extending the loaders.

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
    "ultimatePoints": 3,
    "startingOrbs": 1,
    "abilityKits": {
      "followUp": {
        "skillAbility": "MyFollowUpSkill"
      }
    },
    "abilities": {
      "skill": {
        "id": "MySkill",
        "name": "My Skill",
        "type": "attack",
        "targetRule": "single_enemy",
        "multiplier": 1.2,
        "orbGain": 1,
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
      },
      "followUpSkill": {
        "id": "MyFollowUpSkill",
        "name": "My Follow-Up Skill",
        "type": "attack",
        "targetRule": "single_enemy",
        "multiplier": 1.2,
        "orbGain": 0,
        "interactionType": "none",
        "presentationId": "my_skill_presentation"
      }
    }
  }
}
```

## Root Fields

Supported root-level character fields:

- `title`: displayed character name
- `assets`: asset stem used for sprite, icon, and most voice lookup
- `voiceSpeakerId`: optional override for voice arbitration identity
- `class`: display text only
- `spd`: base speed
- `atk`: base attack
- `hp`: max HP
- `standardAbility`: compatibility field for the standard slot
- `skillAbility`: the character's main regular-turn slot
- `ability`: alias / fallback for `skillAbility`
- `ultimate`: the character's ultimate slot
- `ultimatePoints`: number of orbs required to unlock the ultimate
- `startingOrbs`: starting ultimate charge
- `baseShield`: legacy character-level shield fallback
- `abilityKits`: optional named slot-override sets
- `abilities`: nested ability definitions

Recommended practice:

- Keep `skillAbility` and `ability` identical.
- Always define a real regular ability and a real ultimate.
- Use `assets` as a clean stable key because sprite, icon, and voice lookup depend on it.
- Treat `standardAbility` as compatibility plumbing unless you are intentionally using that slot.

## Ability Schema

Nested character abilities are parsed by `parseAbilityDefinition()` in `src/game/core/battle_loader.cpp`.

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

Supported `inputPromptType` values:

- `none`
- `space`
- `wild`
- `custom`
- `arrows`
- `leftRight`
- `spamSpace`

## What Ability Fields Actually Do

Current runtime behavior:

- `multiplier`
  - For `attack`: scales outgoing damage
  - For `debuff`: can also deal damage if `multiplier > 0`
  - For `heal` / `shield`: affects the final presentation-scaled result
- `amountPercentOfCasterMaxHp`
  - preferred support-authoring field for `heal` and `shield`
  - base amount is resolved from the caster's max HP
- `flatHeal`
  - legacy heal fallback when `amountPercentOfCasterMaxHp` is not set
- `baseShield`
  - legacy shield fallback when `amountPercentOfCasterMaxHp` is not set
- `speedBuff`
  - applied by `buff` abilities
- `atkBuff`
  - applied by `buff` abilities
- `orbGain`
  - how many orbs that ability grants when the action is allowed to grant orb gain
  - defaults to `1` when omitted
  - set it to `0` for actions like follow-ups that should not generate charge
- `actionAdvance`
  - advances allies toward their turns after a `buff`
- `selfHpCostPercentOfMax`
  - percent of caster max HP consumed on use
- `selfHpCostPercentIncreasePerUse`
  - extra self-cost added per use
- `selfHpCostPercentMax`
  - cap for the self-cost scaling
- `reviveDeadAllies`
  - only meaningful on `heal` abilities
- `instructionHint`
  - shown automatically during the presentation by `BattleSessionCore`
- `presentationId`
  - used to construct a custom `AbilityPresentation`
- `inputPromptType` and `inputPromptKeys`
  - configure the presentation prompt shown in battle UI

Support authoring rule:

- New healers and shielders should use `amountPercentOfCasterMaxHp`.
- `flatHeal`, ability-level `baseShield`, and root-level `baseShield` remain legacy fallbacks.

## Multiple Abilities and Ability Kits

The current loader supports more than one nested ability per character.

Important details:

- Nested `abilities` entries are not limited to `skill`, `ultimate`, and `standard`.
- Any nested object entry is accepted as long as it resolves to a real ability id.
- All ability ids are merged into one global map, so ids must be unique across all characters, bosses, and `assets/combat/abilities.json`.

`abilityKits` is the data-driven way to define alternate slot mappings for one character without forcing every character into a more complex schema.

Each kit is a partial override with these optional fields:

- `standardAbility`
- `skillAbility`
- `ability` as an alias for `skillAbility`
- `ultimate`

Use cases:

- normal characters can ignore `abilityKits` entirely
- special characters can define alternate skill / ultimate slots for follow-ups, powered states, or scripted swaps

Practical rule:

- define the base kit in the normal root fields
- define alternate slot maps in `abilityKits`
- define any extra authored ability data inside `abilities`

## Current Turn Logic

The current playable-turn flow in `src/game/core/battle_manager.cpp` behaves like this:

- A normal player turn resolves the character's current regular ability slot.
- That slot comes from the base character definition, optionally modified by any active kit or turn-level kit override.
- When a character reaches full ultimate charge, the system queues an extra turn for the ultimate.
- That ultimate turn is a separate turn actor in the action-value system.

Practical consequence:

- Treat `skillAbility` as the real regular action button.
- Treat `ultimate` as a separate extra-turn action.
- Use `abilityKits` when a character needs alternate versions of those slots.

## Buff Behavior

Buffs are implemented in `BattleManager`.

Current rules:

- Buffs are party buffs on allies, not generic status-effect objects.
- A buff from the same source character and same ability does not stack on the same target.
- Reapplying the same buff refreshes / replaces the existing values.
- Buffs expire when the source character starts a new non-extra turn.
- Buffs are removed if the source dies.
- Buffs are removed if the target dies.

Speed-specific behavior:

- Speed buffs update the turn actor's action value using `oldAV * oldSpd / newSpd`.
- This means a speed buff can immediately move a character closer to their turn if they have not acted yet.

## Damage / Healing Timing

Two patterns are supported:

1. Passive presentation:
   - presentation plays
   - runtime applies the normal effect after the presentation ends

2. Timed presentation:
   - presentation emits hit / heal events during playback
   - runtime applies damage / healing exactly when those events occur

If your ability is multi-hit or needs precise timing, make the presentation emit hit events.

## Voice Authoring Rules

Safest current voice layout for playable characters:

- Put clips under `assets/combat/voices/<assets>/`
- Use the clip names listed in this guide
- Prefer `.wav` unless you already know the asset pipeline for `.opus`

Current runtime clip usage:

- regular skill voice: `ability`, fallback `skill`
- extra presentation cue: `ability2`
- ultimate ability voice: `ultimate`
- splash / activation voice: `ready`, fallback `special`
- damage reaction: `hit`
- defeat: `dead`
- healing received: `healed`
- revive event: `revived`
- shield gained: `shielded`
- idle banter: `idle`

## Authoring Checklist

Before testing a new character, make sure all of these are true:

- Character exists in `assets/combat/characters.json`
- `assets` matches real sprite and icon files
- `skillAbility`, `ability`, and your slotted nested ability ids match
- `ultimate` matches a real ultimate ability id
- Any `abilityKits` point to real ability ids
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

- If `isLineupFixed` is `true`, the fixed lineup must contain at least one valid character.
- If you explicitly set `partySize`, keep it consistent with the final merged fixed lineup.
- If you omit `partySize`, the loader usually resolves it correctly for fixed battles.

## Testing Tips

Fastest current ways to test:

- Run an existing demo executable with an explicit battle key:
  - `./build/bin/demo my_test_battle`
- Or add a dedicated demo target in `CMakeLists.txt`
- If you add a dedicated demo target, also map its compile definition in `src/demo.cpp`

## Known Gotchas

- `ultimatePoints` is effectively treated as at least `1` by runtime clamps.
- A character with no ultimate at all is still not a clean supported case.
- `standardAbility` is mostly compatibility / slot plumbing right now.
- `.png` is still the safest cross-runtime art format for combat sprites and icons.
- Voice lookup for playable characters is centered on `assets`, not `title`.
- Special mechanics can still require engine code in `src/game/core/battle_manager.cpp` even when the character data itself is fully authored in JSON.
