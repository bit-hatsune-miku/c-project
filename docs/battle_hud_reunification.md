# Battle HUD Reunification Handoff

## Purpose

This document is the battle-side handoff for the approved HUD revamp work.

It exists so the next implementation pass can continue without re-discovering:

- what the current rendering architecture actually is
- why battle is still the main window-refresh problem
- what the correct hybrid target should be
- which files currently own the battle HUD, the battle app shell, and the backend switching

This is not a design pitch. It is the concrete migration note for taking the approved HTML battle HUD into the main game safely.

## Current architecture truth

### Story and menu flow are already on the front RmlUi path

The main app already treats story/VN and the main front-end UI family as OpenGL-backed front UI screens.

Important current truth:

- `ScreenState::Playing` is treated as a front UI screen in [main.cpp](../src/main.cpp).
- The story document/controller lives in [front_ui_story.cpp](../src/graphics/front_ui_story.cpp).
- Main menu, story, story pause, story settings, load, boss selector, and party loader now already behave like one continuous RmlUi-side flow.

That means story/VN is not the remaining blocker.

### Battle is the remaining major refresh point

The visible refresh happens when the app enters battle because the main game still routes combat through the old renderer-backed session path.

In the current app loop:

- battle is entered via `battle::demo::Session`
- that session is initialized with `window.getRenderer()`
- the app restores the renderer UI/backend before entering battle

Relevant files:

- [main.cpp](../src/main.cpp)
- [demo_battle_session.h](../src/game/demo_battle_session.h)
- [window.cpp](../src/window.cpp)

### Why the refresh happens

The refresh is not caused by the idea of “SDL scene + RmlUi HUD” by itself.

The refresh comes from backend switching inside `Window`.

`Window` can only be in one mode at a time:

- SDL renderer mode
- OpenGL mode

When the app switches modes, [window.cpp](../src/window.cpp) recreates the underlying SDL window via `recreateWindow(...)`.

That destroy/recreate behavior is the actual reason the user sees the battle refresh.

## Single-window target

The correct reunification target is:

- keep story/front UI in the existing OpenGL-backed front path
- keep battle in that same OpenGL-hosted window
- stop switching back to the renderer-only battle session for the main app battle flow

The main goal is not “make battle pure RmlUi.”

The main goal is:

1. keep one OpenGL-hosted window alive across story -> selector -> party loader -> battle
2. render the custom 2.5D battle scene under that same host
3. render the new battle HUD via RmlUi above it
4. avoid `enableRenderer()` / `enableOpenGL()` window recreation during battle entry and battle exit

If this rule is respected, the story-to-battle handoff can stay single-window.

## Hybrid rendering model

### What should stay custom-rendered

Do not try to recreate the battle stage or duel presentation in HTML/CSS.

The custom scene should remain custom-rendered:

- stage floor / duel world
- sprite positioning
- camera motion
- battle world framing

Relevant existing files:

- [app_battle_session.cpp](../src/game/app_battle_session.cpp)
- [battle_scene_renderer.h](../src/game/render/battle_scene_renderer.h)
- [gl_screen_blitter.h](../src/game/render/gl_screen_blitter.h)

### What should move to the new HUD

The approved HTML work should map to the RmlUi HUD layer:

- boss HP shell and readout
- turn-order rail
- party status cards
- action prompt shell
- later battle overlays, only as separate passes

This is the correct split:

- scene stays custom
- HUD becomes RmlUi

## What can remain SDL-style for now

The existing ability presentations do not all need to be rewritten in the first reunification pass.

Current truth:

- many battle presentations still expose `render(SDL_Renderer* renderer, ...)`
- presentation overlays and splash effects are still heavily SDL-renderer-oriented
- this is separate from the core HUD replacement problem

Relevant files:

- [ability_presentation.h](../src/game/presentation/ability_presentation.h)
- [app_battle_session.cpp](../src/game/app_battle_session.cpp)

The first real reunification goal should therefore be:

1. replace the old HUD presentation
2. keep the existing battle scene path underneath
3. keep existing presentation logic working
4. only later decide which presentation overlays deserve their own RmlUi treatment

Do not block the HUD migration on a full presentation rewrite.

## Existing battle RmlUi path to reuse carefully

There is already an older OpenGL + RmlUi battle app path in:

- [app_battle_session.cpp](../src/game/app_battle_session.cpp)

It already proves the basic hybrid model is feasible:

- render software battle scene
- upload/blit it to GL
- render RmlUi HUD on top

That existing path is valuable, but it should not be copied blindly.

It currently contains old HUD visuals and several expensive update patterns.

## Performance guardrails

The likely reason previous attempts felt slow or unstable was not simply “too many images.”

The real risk areas are:

- too much per-frame DOM/property churn
- broad document updates every frame
- repeated decorator/path churn
- unnecessary markup rebuilding
- backend complexity during resize and free-view flows

### Rules for the next implementation pass

1. Cache element handles once.
   Do not keep calling `GetElementById` across large surfaces without reason.

2. Update only dirty state.
   Boss HP, unit HP, ult charge, AV values, and active classes should update when values actually change.

3. Prefer classes and stable structure over constant restyling.
   Avoid repeatedly rebuilding markup or reassigning lots of style strings when a class toggle will do.

4. Bind portraits once where possible.
   If a combatant has not changed, do not constantly churn decorators/asset properties.

5. Keep document structure fixed.
   The approved HUD should be rendered through a stable skeleton, not repeated inner markup rebuilds.

6. Treat motion as view behavior, not layout regeneration.
   HP easing, hit flash, active emphasis, and disabled state should mostly be driven by classes, transforms, and small targeted property updates.

7. Keep the hybrid layers separated.
   The scene renderer, the GL blitter, and the RmlUi HUD should stay clearly separated in ownership and timing.

### Current file where churn risk already exists

The main document-push hotspot today is:

- [battle_session_document_updates.h](../src/game/ui/battle_session_document_updates.h)

That file should be treated as the main place to simplify and modernize during the real port.

## Recommended migration order

### 1. Approve the HTML HUD

This phase is now represented by:

- [battle_hud.html](../prototypes/battle_hud.html)
- [battle_hud_motion_lab.html](../prototypes/battle_hud_motion_lab.html)

Do not touch battle runtime logic until the look and motion language are approved.

### 2. Port the approved HUD structure into the battle RmlUi document

Use the approved prototype as the visual/source-of-truth for:

- boss shell
- turn order
- unit cards
- action prompt

The likely runtime document/file to evolve is the existing battle HUD document path:

- [battle_hud.rml](../assets/rmlui/battle_hud.rml)
- [battle_hud.rcss](../assets/rmlui/battle_hud.rcss)

### 3. Move main app battle entry away from the old renderer-only path

The main app currently enters battle through the renderer-backed demo session in [main.cpp](../src/main.cpp).

The next real reunification step should change the main app battle route so it no longer restores the old renderer backend before battle.

That means:

- do not keep the current `battle::demo::Session` as the primary in-app battle route
- instead, route into an OpenGL-hosted battle session compatible with the front UI window lifecycle

### 4. Preserve single-window transitions

Battle entry and exit should follow the same no-refresh expectations already established by:

- main menu
- story
- boss selector
- party loader

The transition shell should remain a UI overlay inside the same window, not a backend swap boundary.

### 5. Tackle remaining battle presentation unification only where necessary

After the HUD is stable and single-window battle entry works:

- audit presentation overlays one by one
- keep SDL-side presentation rendering where it is acceptable
- only rewrite the pieces that truly benefit from tighter RmlUi integration

This should be a selective cleanup, not a mandatory full rewrite.

## Known risk areas

### 1. Free-view and resize interactions

The current battle app path handles camera movement, resize events, scene renderer reinitialization, and GL viewport updates together inside [app_battle_session.cpp](../src/game/app_battle_session.cpp).

That area is sensitive and can regress performance or stability if HUD work gets tangled into it.

### 2. Battle entry and exit transitions

The single-window requirement will fail if battle entry still restores the renderer backend before combat begins.

Any migration PR should explicitly check:

- story -> battle
- boss selector -> party loader -> battle
- battle -> story
- battle -> boss selector
- battle -> main menu

### 3. Presentation overlays that still assume SDL_Renderer

Some battle presentations still assume direct `SDL_Renderer` access.

Those are not a reason to delay the HUD migration, but they are a reason to keep the hybrid scope disciplined.

## Key file map

These are the files a future implementer should read first:

- [src/game/app_battle_session.cpp](../src/game/app_battle_session.cpp)
  Existing OpenGL + RmlUi battle shell and the closest current hybrid proof.

- [src/game/ui/battle_session_document_updates.h](../src/game/ui/battle_session_document_updates.h)
  Current battle document update path and the main place where DOM churn risk lives.

- [src/game/render/battle_ui.cpp](../src/game/render/battle_ui.cpp)
  Old SDL HUD implementation and the best reference for battle HUD behavior/state.

- [src/main.cpp](../src/main.cpp)
  Actual battle entry/exit flow and the place where backend switching behavior currently matters.

- [src/window.cpp](../src/window.cpp)
  The source of the actual window recreation/backend switching behavior.

Supplementary references:

- [docs/rmlui_hud_migration.md](./rmlui_hud_migration.md)
- [docs/architecture.md](./architecture.md)

## Practical implementation defaults

If work resumes from this document, assume the following unless the user explicitly changes direction:

- battle reunification should stay hybrid
- story/VN does not need another rendering migration first
- the first real goal is HUD replacement with no battle-entry window refresh
- the 2.5D scene remains custom-rendered
- existing presentations can remain mixed until individually worth rewriting

## Resume point

If work stops abruptly, the safest next step is:

1. open the approved HTML prototypes
2. restyle the battle RmlUi document to match them
3. reduce document update churn in `battle_session_document_updates.h`
4. replace the main app battle entry path so it stays on the OpenGL-hosted front/battle pipeline

That sequence preserves both the visual goal and the single-window requirement.
