# Stage Decoration

Stage decoration is data-driven for the shared battle runtime.

The authoring model is:

1. A battle in `assets/combat/battles.json` chooses a `stageKey`.
2. That `stageKey` is looked up in `assets/combat/stages.json`.
3. The stage loader builds the floor, backdrop, props, and optional camera override for the battle session.

Current relevant files:

- `assets/combat/battles.json`
- `assets/combat/stages.json`
- `src/game/core/battle_loader.cpp`
- `src/game/battle_session_core.cpp`
- `src/game/render/battle_stage.h`
- `src/game/render/battle_stage.cpp`
- `src/game/render/battle_backdrop_projection.h`
- `src/game/render/battle_world_renderer.cpp`
- `src/game/render/gl_battle_scene_renderer.cpp`
- `src/game/app_battle_session.cpp`

## Mental Model

Think of it as a two-step lookup:

- `battles.json` says which stage a battle wants
- `stages.json` says what that stage contains

So yes, the normal workflow is:

- create a stage entry in `assets/combat/stages.json`
- give it a unique key such as `city_rooftop_night`
- fill in the visual parameters for that stage
- point one or more battles at it with `"stageKey": "city_rooftop_night"`

Example:

```json
{
  "battles": {
    "my_battle": {
      "bossKey": "myBoss",
      "stageKey": "city_rooftop_night"
    }
  }
}
```

and then:

```json
{
  "stages": {
    "city_rooftop_night": {
      "floor": {
        "centerX": -300.0,
        "centerY": 1180.0,
        "width": 3200.0,
        "depth": 3600.0,
        "tileSize": 220.0,
        "baseColor": "#2E313C",
        "accentColor": "#343845"
      },
      "backdrop": {
        "mode": "parallax",
        "image": "assets/vn/backgrounds/ch4/1.png",
        "parallaxStrengthX": 0.45,
        "parallaxStrengthY": 0.28,
        "gradientTopColor": "#1F2740",
        "gradientBottomColor": "#0A101B"
      },
      "props": [
        {
          "texture": "assets/combat/presentations/wechatalipay/weixin.png",
          "worldX": -800.0,
          "worldY": 1700.0,
          "worldZ": 40.0,
          "pixelWidth": 420,
          "pixelHeight": 420,
          "tint": "#AFFFF4",
          "alpha": 0.35
        }
      ],
      "camera": {
        "posY": 60.0,
        "pitchDegrees": 6.0,
        "focalLength": 52000.0
      }
    }
  }
}
```

## Current Schema

Each stage entry supports these sections:

- `floor`
- `backdrop`
- `props`
- `camera`

### `floor`

Controls the battle plane under the characters.

Fields:

- `centerX`
- `centerY`
- `width`
- `depth`
- `tileSize`
- `texture` optional
- `baseColor`
- `accentColor`

Notes:

- `width` and `depth` are in world units
- `tileSize` is the desired tile size, but the renderer caps floor geometry to a `20 x 20` budget
- if `texture` is missing or fails to load, the engine generates a checker floor from `baseColor` and `accentColor`
- floor size only affects the environment, not combat logic or party/boss slot positions

### `backdrop`

Controls the far background.

Shared fields:

- `mode` optional
- `gradientTopColor`
- `gradientBottomColor`

Mode values:

- `screen`
- `parallax`
- `panorama`
- `skybox`

If `mode` is omitted, it defaults to `screen`.

The gradient always renders first as a fallback mood layer. The selected backdrop mode then draws over it.

Quick comparison:

| Mode | Best For | Main Strength | Main Weakness |
| --- | --- | --- | --- |
| `screen` | dialogue-heavy fights, simple stages, safest baseline | cheapest and most stable | feels flat |
| `parallax` | most normal battles that want a bit more depth | good depth-per-cost ratio | still a 2D plate |
| `panorama` | spaces where the surrounding walls/horizon matter more than ceiling/floor | strong sense of place from one image | limited top/bottom coverage |
| `skybox` | showcase arenas, open spaces, or scenes where looking around in all directions matters | strongest full-environment feel | most expensive and hardest to author cleanly |

#### `screen`

Use when you want a fixed background plate that does not react to the camera.

Fields:

- `image` optional

Behavior:

- the image is stretched to the screen
- camera movement and rotation do not change it
- this is the cheapest and safest readability-first option

Strengths:

- cheapest mode
- easiest to author
- very stable behind fighters

Weaknesses:

- no camera-reactive depth
- can feel flat if the stage wants strong environmental presence

Example:

```json
{
  "backdrop": {
    "mode": "screen",
    "image": "assets/vn/backgrounds/ch1/1.png",
    "gradientTopColor": "#4F6073",
    "gradientBottomColor": "#162029"
  }
}
```

#### `parallax`

Use when you want a screen-space backdrop that drifts a little with camera motion.

Fields:

- `image` required in practice
- `parallaxStrengthX` optional
- `parallaxStrengthY` optional

Behavior:

- the image stays a 2D plate, not a world object
- the renderer gives it overscan and shifts it slightly with camera yaw/pitch
- stronger values create more drift, but too much can look fake quickly

Recommended range:

- `0.15` to `0.75`

Strengths:

- still cheap
- adds motion and depth without needing a full sky system
- good middle ground for most outdoor or scenic fights

Weaknesses:

- still fundamentally a 2D plate
- large values can look artificial or swimmy

Example:

```json
{
  "backdrop": {
    "mode": "parallax",
    "image": "assets/vn/backgrounds/ch0/5.png",
    "parallaxStrengthX": 0.55,
    "parallaxStrengthY": 0.35,
    "gradientTopColor": "#5A2A5B",
    "gradientBottomColor": "#081522"
  }
}
```

#### `panorama`

Use when you have one wide panorama image and want camera rotation to reveal different parts of it.

Fields:

- `image` required in practice

Behavior:

- the image is treated like a panoramic strip
- yaw and pitch select a moving window into that image
- translation is ignored, so it behaves like a distant environment
- best results come from a true wide panorama, ideally around `2:1`

Strengths:

- excellent when the surrounding environment matters more than the ceiling or floor
- strong sense of horizontal place with only one image
- good fit for interiors where the walls matter but the roof/floor do not need full skybox coverage

Weaknesses:

- top and bottom coverage are limited compared with a true skybox
- source art needs to be wide enough or the effect feels cramped
- not ideal when players should feel enclosed in all directions

Example fit:

- an indoor environment with a fairly close roof where the side walls and room wrap matter much more than looking straight up or down

Example:

```json
{
  "backdrop": {
    "mode": "panorama",
    "image": "assets/combat/panoramas/rooftop_sunset.png",
    "gradientTopColor": "#694055",
    "gradientBottomColor": "#110A13"
  }
}
```

#### `skybox`

Use when you want a cubemap-style distant world.

Fields:

- `skybox.front`
- `skybox.back`
- `skybox.left`
- `skybox.right`
- `skybox.top`
- `skybox.bottom`

Behavior:

- the renderer treats the camera as if it is inside a cube
- camera rotation changes which faces are visible
- translation is ignored, so the skybox feels infinitely far away
- missing faces simply fall back to the gradient in those regions

Strengths:

- best full-surround option
- strongest sense of being inside a larger space
- works well when up/down views matter visually

Weaknesses:

- most asset-heavy option
- hardest to author cleanly because seams and face orientation matter
- usually overkill for small or dialogue-heavy fights

Example:

```json
{
  "backdrop": {
    "mode": "skybox",
    "gradientTopColor": "#171924",
    "gradientBottomColor": "#05060B",
    "skybox": {
      "front": "assets/combat/skyboxes/night/front.png",
      "back": "assets/combat/skyboxes/night/back.png",
      "left": "assets/combat/skyboxes/night/left.png",
      "right": "assets/combat/skyboxes/night/right.png",
      "top": "assets/combat/skyboxes/night/top.png",
      "bottom": "assets/combat/skyboxes/night/bottom.png"
    }
  }
}
```

### `props`

Controls large stage decorations behind the fighters.

Each prop supports:

- `texture`
- `worldX`
- `worldY`
- `worldZ`
- `pixelWidth`
- `pixelHeight`
- `tint` optional
- `alpha` optional

Notes:

- props are world-projected, so they participate in the 2.5D perspective
- props are currently intended for back-stage decoration, not gameplay objects
- the loader only keeps up to `4` props per stage in v1
- props fully off-screen are culled before draw

### `camera`

Optional overrides for the default battle camera.

Supported fields:

- `posX`
- `posY`
- `posZ`
- `pitchDegrees`
- `yawDegrees`
- `focalLength`

Notes:

- all fields are optional
- unspecified fields keep the default battle camera values
- this is for stage flavor only; the camera is not auto-fit to floor size

## Runtime Behavior

At battle startup:

- `BattleDefinition.stageKey` is read from `assets/combat/battles.json`
- the active battle session asks the stage loader for that stage
- the stage loader reads `assets/combat/stages.json`
- the selected stage is merged on top of `default_stage`
- textures are loaded and cached for the session

If something is missing:

- unknown `stageKey` falls back to `default_stage`
- missing `default_stage` falls back to built-in hardcoded defaults
- missing floor texture falls back to procedural floor colors
- missing screen/parallax/panorama image means gradient-only background
- missing skybox faces are skipped instead of failing battle startup
- missing prop textures are skipped instead of failing battle startup

## Render Order

The SDL and GL battle runtimes currently draw in this order:

1. backdrop gradient and backdrop mode
2. floor
3. compatibility below-world presentation pass
4. stage props
5. characters and boss
6. mid-world presentation pass / feedback
7. HUD and overlays

This keeps the stage decorative while preserving combat readability.

## Practical Authoring Advice

When making a new stage:

- start by copying `default_stage`
- tune `floor.width`, `floor.depth`, and `floor.centerY` first
- choose the cheapest backdrop mode that sells the stage
- use `screen` for stability, `parallax` for light depth, `panorama` for scenic rotation, and `skybox` for showcase spaces
- add at most `1` to `2` hero props first and check silhouette clarity
- only add camera overrides if the default framing feels clearly wrong for that stage

For the current system, prefer:

- broad color/gradient atmosphere
- a readable floor with moderate contrast
- a few large props instead of many small props
- low visual noise behind the combat lane
- `screen` or `parallax` for most fights
- `panorama` or `skybox` only when the art really benefits from camera-reactive background motion

## Useful Test Stages

`assets/combat/stages.json` now includes a few quick test entries:

- `default_stage` for fixed `screen` backdrop behavior
- `tutorial_test_stage` for `parallax`
- `debug_panorama_stage` for `panorama`
- `debug_skybox_stage` for `skybox`

You can point any battle at one of those keys to test the mode quickly.

## Future Options

If skybox source art needs per-face correction later, likely useful authoring additions would be:

- `flipX`
- `flipY`
- `rotate90`

Those are not implemented right now because the current skybox path is working well enough without adding more authoring complexity.

## Current Limitation

The stage system is shared by the SDL runtime and the main GL battle scene path, but presentation-specific native GL effects still have their own custom rendering layers on top.
