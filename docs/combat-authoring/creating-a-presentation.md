# Creating a Presentation

This document explains how to add a new combat presentation for either a playable character or a boss.

Primary implementation files:

- `src/game/presentation/ability_presentation.h`
- `src/game/presentation/ability_presentation.cpp`
- `src/game/presentation/presentation_runtime.cpp`
- `src/game/presentation/presentation_registry.cpp`
- `src/game/battle_session_core.cpp`
- `CMakeLists.txt`

## Overview

A presentation is a C++ class derived from `AbilityPresentation`.

You use a presentation when you want any of the following:

- custom animation
- camera choreography
- player input during the move
- exact hit timing
- presentation-specific SFX or BGM control
- splash art customization

The data flow is:

1. Ability JSON sets `presentationId`
2. `PresentationRegistry` maps that id to a factory
3. `BattleSessionCore` asks `presentation_runtime::runAbilityPresentation()` to play it
4. The presentation returns multipliers, hit events, result text, audio commands, and optional camera/layout overrides

## File and Build Steps

To add a new presentation:

1. Create a header and source file in `src/game/presentation/`
2. Add both files to `BATTLE_PRESENTATION_SOURCES` in `CMakeLists.txt`
3. Include the header in `src/game/presentation/presentation_registry.cpp`
4. Register the `presentationId` there
5. Reference that same `presentationId` from ability JSON

If you skip step 2 or 4, the presentation will not exist at runtime.

## Minimal Class Skeleton

```cpp
#pragma once

#include "ability_presentation.h"

namespace battle {

class MyPresentation : public AbilityPresentation {
public:
    MyPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

private:
    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
};

} // namespace battle
```

Minimal registration:

```cpp
registry.registerPresentation("my_presentation", [](
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) -> std::unique_ptr<AbilityPresentation> {
    return std::make_unique<MyPresentation>(
        casterWorldX, casterWorldY, casterWorldZ,
        targetWorldX, targetWorldY, targetWorldZ
    );
});
```

## Constructor Inputs

Every presentation factory receives:

- caster world position
- target world position

Those values are prepared by `presentation_runtime.cpp`.

Default assumptions:

- Character presentation:
  - caster = acting ally
  - target = boss
- Boss presentation:
  - caster = boss
  - target = party target if one is provided, otherwise a default ally position

## Required Methods

Every presentation must implement:

- `start()`
- `update(float deltaTime)`
- `render(SDL_Renderer*, int, int, const Camera3D&)`
- `isComplete() const`

Typical pattern:

- initialize counters in `start()`
- advance animation state in `update()`
- draw the presentation in `render()`
- return `true` in `isComplete()` when the move is done

## Optional Hooks You Can Override

The base class in `ability_presentation.h` exposes a lot of extension points.

### Input

- `onSpacePressed()`
- `onKeyPressed(SDL_Keycode key)`
- `getInputMultiplier() const`
- `consumeHitDamageMultiplier()`
- `getInputResultText() const`

Use these when the player needs to react during the animation.

### Hit / heal timing

- `consumeHitEvents()`
- `getDamageLabelHitCount() const`

If the presentation emits hit events, the runtime applies damage/healing during playback instead of waiting until the presentation ends.

Use hit events when:

- the move is multi-hit
- timing matters
- dodging/parrying changes whether a specific hit lands

If you do not emit hit events, the normal ability effect is applied after the presentation ends.

### Audio

- `consumeAbilityAudioCues()`
- `consumeAudioCommands()`

`consumeAbilityAudioCues()` is for generic "play the caster's voice line now" timing.

`consumeAudioCommands()` is for explicit commands like:

- play one-shot SFX
- play overlapping one-shot SFX
- start loop
- stop loop
- stop all presentation SFX
- pause BGM
- resume BGM

The supported command enum is `PresentationAudioCommandType`.

### Camera and staging

- `overridesCamera() const`
- `applyCameraState(Camera3D& camera) const`
- `getFocusedPartyIndex() const`
- `shouldHideNonCasterCharacters() const`
- `shouldRenderCasterEntity() const`
- `shouldRenderBossEntity() const`
- `shouldRenderAboveHud() const`
- `shouldBlackoutWorld() const`
- `shouldUseCenteredPartyLayout() const`
- `renderBelowWorld(...)`

Use these when you need cinematic framing or special world composition.

### Runtime plumbing / custom data

- `setTargetPartyIndex(int index)`
- `setTargetWorldPosition(float x, float y, float z)`
- `setPresentationValue(int value)`
- `setExternalTextures(SDL_Texture* caster, SDL_Texture* target)`
- `getCorrectToneCount() const`

These are extension points.

Important:

- `setPresentationValue()` and `getCorrectToneCount()` only matter if session/battle code consumes them
- They are not a generic gameplay system by themselves

## Character vs Boss Presentations

## Character presentations

Typical behavior:

- Caster is the acting ally
- Target is the boss
- Duel layout is used by default
- Non-caster allies are hidden by default unless you override that behavior
- Non-ultimate character actions can show splash art before the presentation
- Ultimate extra turns also get a preview splash from `BattleSessionCore`

Good use cases:

- projectiles from caster to boss
- ally-focused buffs/heals
- zoom-out ally cutscenes
- lane/rhythm UI for a playable skill

## Boss presentations

Typical behavior:

- Caster is the boss
- Boss splash art is automatically allowed
- Party can be hidden or narrowed to a focused ally
- Many boss presentations use `getFocusedPartyIndex()` to frame one ally
- Boss presentation runtime can keep the boss layout centered via `onBossPresentationFrame`

Good use cases:

- telegraphed dodge/parry attacks
- all-party hit sequences
- target-picking cinematic attacks
- heavy screen-space effects

Important limitation:

- Generic boss logic does not automatically support advanced targeting rules
- If the presentation needs a random chosen ally, custom battle-manager code may still be required

## Splash Art

Splash art is handled through `SplashArtAnimation`.

If you want a custom splash:

- override `getSplashConfig(SDL_Texture* sprite) const`

If you return nothing:

- bosses still get a default splash from the caster sprite

The splash label uses the ability `name` from JSON.

## Hints and Result Text

`instructionHint` from ability JSON is shown automatically during presentation playback by `BattleSessionCore`.

`getInputResultText()` lets the presentation return a short result summary like:

- timing result
- damage increase
- dodge success ratio
- tone score

The SDL demo session currently shows result text mainly for:

- boss abilities
- interactive abilities

So if you want nice feedback, implement both:

- `instructionHint` in JSON
- `getInputResultText()` in the presentation

## Damage Model: When to Emit Hit Events

Use this rule:

- If the move is basically one effect with no exact timing needs, do not emit hit events
- If the move needs exact hit timing, emit hit events

Examples:

- simple projectile skill: often no hit events needed
- dodge boss attack: emit hit events when each successful hit actually lands
- heal burst with exact timing: emit hit events so healing lands on the intended frame
- split-hit boss finisher: emit multiple hit events over time

## Asset Loading in Presentations

Presentations usually load their own textures directly.

Recommended pattern:

- use `platform::path::resolvePath(...)`
- load images with `SDL_image`
- keep load-once flags like `attemptedTextureLoad_`
- destroy owned textures in the destructor

If you need caster/boss textures already loaded by the world:

- use `setExternalTextures()`

## Registering the Presentation

Presentation ids are not discovered automatically.

You must:

- include your header in `src/game/presentation/presentation_registry.cpp`
- call `registry.registerPresentation("your_id", ...)`

If the id is missing at runtime, `presentation_runtime.cpp` logs:

- `Missing presentation id: ...`

and the move falls back to default multiplier behavior without the animation.

## Testing Workflow

Recommended workflow:

1. Add or reuse a battle in `assets/combat/battles.json`
2. Run `./build/bin/demo your_battle_key`
3. If the move is large or boss-specific, add a dedicated demo target in `CMakeLists.txt`
4. If you add a dedicated demo target, wire its compile definition in `src/demo.cpp`

This is how the existing focused boss demos are set up.

## Presentation Checklist

- New `.h` and `.cpp` exist in `src/game/presentation/`
- Both files were added to `BATTLE_PRESENTATION_SOURCES` in `CMakeLists.txt`
- Header is included in `src/game/presentation/presentation_registry.cpp`
- `presentationId` is registered there
- Ability JSON references the exact same `presentationId`
- Any custom textures/audio paths resolve correctly through `platform::path::resolvePath`
- You decided whether the move should emit hit events or let runtime apply one final effect
- You added `instructionHint` if the player needs input guidance

## Known Gotchas

- `getCorrectToneCount()` and `setPresentationValue()` are not generic systems by themselves; they need matching game logic
- If two abilities share the same `presentationId`, they will use the same factory
- Boss target choice is only generic for simple cases
- New files are not auto-included by CMake
- Result-text display behavior is host-specific and slightly different between the demo and app runtimes
