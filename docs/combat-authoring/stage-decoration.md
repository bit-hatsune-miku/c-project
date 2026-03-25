# Stage Decoration

This document is intentionally a placeholder.

`stageKey` already exists in battle data, but stage decoration is not a fully authored system yet.

Current relevant files:

- `assets/combat/battles.json`
- `src/game/core/battle_loader.cpp`
- `src/game/battle_session_core.cpp`
- `src/game/render/battle_world_renderer.cpp`

## Current State

Right now:

- `BattleDefinition` stores `stageKey`
- `battle_loader.cpp` parses `stageKey`
- battles already set values like `default_stage`

But the renderer does not yet load per-stage content from that key.

The world currently uses:

- a generic procedural floor tile texture from `createBattleWorldFloorTileTexture()`
- no stage-specific background
- no stage-specific props
- no stage-specific lighting setup
- no stage-specific camera presets

So, at the moment, `stageKey` is mostly a placeholder and future hook.

## What You Should Do Today

For now, keep using:

```json
"stageKey": "default_stage"
```

in `assets/combat/battles.json`.

If you want a battle to be future-proof, still fill `stageKey` with a sensible identifier, even if nothing visual happens yet.

Example:

```json
{
  "battles": {
    "my_battle": {
      "id": 400,
      "name": "My Battle",
      "bossKey": "myBoss",
      "isLineupFixed": true,
      "lineup": ["miku", "lyoo", "jiafei"],
      "stageKey": "city_rooftop_night"
    }
  }
}
```

That way the content key already exists when we wire a real stage system later.

## What a Real Stage System Will Probably Need

This is a proposed future direction, not current behavior.

Likely future stage data:

- floor texture or floor material
- background art or skybox
- prop sprites / layered scenery
- boss anchor and party anchor offsets
- default camera preset overrides
- lighting / tint / atmosphere values
- optional stage-specific SFX or ambient loop

Likely future asset layout:

- `assets/combat/stages.json`
- `assets/combat/stages/<stageKey>/...`

Possible future JSON shape:

```json
{
  "city_rooftop_night": {
    "floorTexture": "assets/combat/stages/city_rooftop_night/floor.png",
    "background": "assets/combat/stages/city_rooftop_night/background.png",
    "props": [
      {
        "texture": "assets/combat/stages/city_rooftop_night/sign.png",
        "worldX": -300.0,
        "worldY": 900.0,
        "worldZ": 0.0
      }
    ],
    "camera": {
      "posX": -405.0,
      "posY": 45.0,
      "posZ": -175.0,
      "pitchDegrees": 5.0,
      "yawDegrees": 4.0,
      "focalLength": 50000.0
    }
  }
}
```

Again, this schema does not exist yet. It is only a placeholder proposal.

## Current Technical Limitation

If you want real stage decoration right now, you would need to extend at least:

- `BattleDefinition` consumption in runtime code
- stage asset loading
- `BattleSessionCore::initialize()`
- `renderBattleFloor()` and/or `renderBattleWorld()`
- likely new stage scene entities or background layers

So for now, treat stage decoration as an engine task, not as a data-only task.

## Practical Recommendation

Until the stage system is formalized:

- keep `stageKey` populated in every battle
- assume the battle will still render on the generic floor
- do not prepare large stage asset pipelines yet unless you are also implementing the renderer support
