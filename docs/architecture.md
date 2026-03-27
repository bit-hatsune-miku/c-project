# Architecture

This document describes how the current codebase actually works.

It focuses on:

- which executables exist
- which runtime path the main game uses
- where story, menus, battle logic, and rendering live
- how SDL2, OpenGL, and RmlUi are combined today
- where the architecture is still split or transitional

It is intentionally descriptive first. The improvement notes at the end are based on the current implementation, not an older plan.

## 1. Big Picture

The project currently has one main game shell and two different battle front ends.

### Main game

The main executable (`OurUndergroundBITIdol`, plus `ch0` and `finale`) is driven by [`src/main.cpp`](/c-project/src/main.cpp).

When `ENABLE_RMLUI=ON`, the main game works like this:

- main menu, story UI, story pause, story load, and story settings are rendered through the RmlUi front-end session in [`src/graphics/front_ui_session.cpp`](/c-project/src/graphics/front_ui_session.cpp)
- the visual novel runtime itself still lives in [`src/game/vn/vn_system.cpp`](/c-project/src/game/vn/vn_system.cpp)
- in-story battles still run through the SDL battle runtime in [`src/game/battle_session_core.cpp`](/c-project/src/game/battle_session_core.cpp) wrapped by [`src/game/demo_battle_session.cpp`](/c-project/src/game/demo_battle_session.cpp)

So the main game is not "all RmlUi" and not "all SDL". It switches window backends depending on the active screen.

### Standalone battle app

`battle_testing` uses [`src/rmlui_battle_main.cpp`](/c-project/src/rmlui_battle_main.cpp) plus [`src/game/app_battle_session.cpp`](/c-project/src/game/app_battle_session.cpp).

That runtime is separate from the main game battle wrapper:

- battle logic still uses `BattleManager`
- the world is rendered into an offscreen SDL software surface
- that surface is uploaded to OpenGL each frame
- RmlUi renders the HUD and overlays on top

This is a separate battle app, not the path the main story currently uses.

### Legacy / demo executables

There are also standalone SDL-only programs:

- `demo*` executables use [`src/demo.cpp`](/c-project/src/demo.cpp) plus [`src/game/demo_battle_session.cpp`](/c-project/src/game/demo_battle_session.cpp)
- `battle_testing_legacy` and the non-Rml fallback use [`src/battle_main.cpp`](/c-project/src/battle_main.cpp)

These are useful for testing, but they are not the main story runtime.

## 2. Build and Executable Layout

The build graph in [`CMakeLists.txt`](/c-project/CMakeLists.txt) is one of the easiest ways to understand the architecture split.

### Main story executables

- `OurUndergroundBITIdol`
- `ch0`
- `finale`

These all include:

- `src/main.cpp`
- VN runtime
- save/load system
- battle core
- demo battle wrapper
- battle presentations

When `ENABLE_RMLUI=ON`, they also include:

- `src/graphics/front_ui_*`
- `src/game/app_battle_session.*`
- RmlUi GL3 backend files

Important detail: the main executable links both the front-ui Rml stack and the SDL battle stack, but `src/main.cpp` still chooses which one is active by screen.

### Battle-focused executables

- `battle_testing`: OpenGL window, RmlUi HUD, software-rendered world
- `battle_testing_legacy`: older SDL battle program
- `battle_testing_jiafei`: variant of the battle testing program

### Demo executables

- `demo`
- `demo_jiafei`
- `demo_lyoo_plot_twist`
- `demo_wechatalipay`
- `demo_ari`
- `demo_luotianyi`

These all go through the SDL demo battle wrapper and mostly differ by compile-time default battle key.

## 3. Core App Shell

### Window and backend switching

[`src/window.cpp`](/c-project/src/window.cpp) owns the actual SDL window and can run in two modes:

- SDL renderer mode
- OpenGL mode

Key behavior:

- `enableRenderer()` recreates the window without GL and creates `SDL_Renderer`
- `enableOpenGL()` recreates the window with GL and creates the GL context
- `present()` chooses `SDL_RenderPresent` or `SDL_GL_SwapWindow`

That means backend choice is a top-level runtime concern, not a hidden renderer abstraction.

### Main loop ownership

[`src/main.cpp`](/c-project/src/main.cpp) owns:

- `AppState`
- screen transitions
- switching between front-ui and renderer mode
- save/load requests
- battle session creation and teardown
- story advancement into battles and back out again

The main loop is still the central orchestrator.

## 4. App State and Screen Model

The primary state model is in [`src/GameMenu/menu_shared.h`](/c-project/src/GameMenu/menu_shared.h).

`AppState` owns:

- the active `ScreenState`
- menu selections
- pause/load/settings return paths
- live `GameSettings`
- current `StorySession`
- transient notice/save/load flags
- pending battle key and post-battle script references

Important screen states:

- `MainMenu`
- `Playing`
- `PauseMenu`
- `Settings`
- `LoadMenu`
- `BattleDemo`

Despite the name, `BattleDemo` is also the in-story battle state for the main app. It is not just a menu sandbox.

## 5. Main Game Runtime Flow

The current main-game flow in [`src/main.cpp`](/c-project/src/main.cpp) looks like this:

```text
main loop
  poll SDL events
  route events by ScreenState
  process front-ui commands
  process save/load side effects
  create or destroy battle session as needed
  update active system
  switch backend if screen type changed
  render active screen
  present window
```

### Backend policy in the main app

When `shouldUseFrontUiScreen(state)` is true, the app uses:

- OpenGL window mode
- `graphics::frontui::Session`
- RmlUi documents for menu/story overlays

When it is false, the app restores:

- SDL renderer mode
- VN renderer bindings
- SDL battle or SDL settings/load rendering

So the main app actively swaps the window backend at runtime.

### Story-to-battle transition

Story entries can trigger battle via fields in VN script entries loaded by [`src/game/vn/vn_script.cpp`](/c-project/src/game/vn/vn_script.cpp).

In [`src/main.cpp`](/c-project/src/main.cpp):

- `vn::consumeAdvanceRequest()` advances dialogue
- if the current script entry has `battleKey` or `battleId`, `beginBattle(...)` is called
- `beginBattle(...)` validates the battle through [`src/game/core/battle_loader.cpp`](/c-project/src/game/core/battle_loader.cpp)
- `state.screen` becomes `BattleDemo`
- `battle::demo::Session` is created

After battle:

- victory can load a follow-up story script
- otherwise the app usually returns to the main menu

## 6. Front UI Architecture

The RmlUi front-end stack used by the main game lives under `src/graphics/`.

### Session layer

[`src/graphics/front_ui_session.cpp`](/c-project/src/graphics/front_ui_session.cpp) owns:

- RmlUi initialization and shutdown
- GL render interface setup
- the active document stack
- viewport scaling
- event forwarding into RmlUi
- synchronizing document controllers back into `AppState`

It computes a document stack from `AppState`. For example:

- `MainMenu` -> main menu document
- `Playing` -> story document
- story pause -> story document + pause document
- story pause load -> story + pause + load

This stack behavior is one of the more structured parts of the current UI architecture.

### Document controller layer

[`src/graphics/front_ui_document.h`](/c-project/src/graphics/front_ui_document.h) defines a small retained-controller model:

- one controller per screen document
- controller owns selection, event listeners, and state sync
- controller can emit commands back to `main.cpp`

Implemented controllers:

- [`src/graphics/front_ui_main_menu.cpp`](/c-project/src/graphics/front_ui_main_menu.cpp)
- [`src/graphics/front_ui_story.cpp`](/c-project/src/graphics/front_ui_story.cpp)
- [`src/graphics/front_ui_pause.cpp`](/c-project/src/graphics/front_ui_pause.cpp)
- [`src/graphics/front_ui_load.cpp`](/c-project/src/graphics/front_ui_load.cpp)
- [`src/graphics/front_ui_settings.cpp`](/c-project/src/graphics/front_ui_settings.cpp)

### Important architectural detail

The story screen is not a separate VN implementation. [`src/graphics/front_ui_story.cpp`](/c-project/src/graphics/front_ui_story.cpp) reads `vn::PresentationState` from `vn_system` and mirrors that state into RmlUi elements.

So the front UI layer is a presentation shell around the same VN runtime, not a replacement for it.

## 7. VN System

The VN runtime lives in:

- [`src/game/vn/vn_system.h`](/c-project/src/game/vn/vn_system.h)
- [`src/game/vn/vn_system.cpp`](/c-project/src/game/vn/vn_system.cpp)

### What it owns

`vn_system.cpp` keeps mostly global static state:

- current speaker
- current text
- voice path and playback state
- current icon and background
- typewriter progress
- fonts
- current renderer pointer

### Two presentation outputs

The VN system supports two different consumers:

1. SDL drawing through `vn::render()`
2. data export through `vn::getPresentationState()`

The first is used in SDL story/battle overlays.

The second is used by the Rml story screen to render:

- background art
- portrait
- speaker name
- visible typewriter text

### Text handling

Mixed Latin/CJK font fallback is handled through [`src/platform/text_fallback.h`](/c-project/src/platform/text_fallback.h).

This part is relatively clean:

- strings are split into runs
- Latin stays on the Latin font
- CJK uses fallback font only where needed
- baseline alignment is preserved across runs

## 8. Save / Load and Story State

The save system is in:

- [`src/game/save/save.h`](/c-project/src/game/save/save.h)
- [`src/game/save/save.cpp`](/c-project/src/game/save/save.cpp)

`src/main.cpp` is still the place that turns save/load requests into side effects:

- build save game from current story state
- detect duplicate manual saves
- load scripts back from chapter ids
- restore settings
- re-enter `Playing`

That logic is not fully encapsulated behind a screen/session class yet.

## 9. Battle Data and Core Logic

The battle domain model lives in `src/game/core/`.

### Data loading

[`src/game/core/battle_loader.cpp`](/c-project/src/game/core/battle_loader.cpp) loads and validates authored JSON from:

- `assets/combat/boss.json`
- `assets/combat/characters.json`
- `assets/combat/battles.json`
- `assets/combat/abilities.json`

It resolves:

- boss definitions
- character definitions
- battle definitions
- ability definitions

So authored JSON is the real content layer for battle setup.

### Battle manager

[`src/game/core/battle_manager.h`](/c-project/src/game/core/battle_manager.h) and [`src/game/core/battle_manager.cpp`](/c-project/src/game/core/battle_manager.cpp) own the gameplay state:

- boss stats and HP
- party members and per-character runtime state
- shields, buffs, and orb charge
- turn state
- recent action events for UI/audio/feedback
- action execution
- battle-over checks

This is the core gameplay authority used by both battle front ends.

### Turn system and flow helpers

- [`src/game/core/turn_system.h`](/c-project/src/game/core/turn_system.h)
- [`src/game/core/turn_system.cpp`](/c-project/src/game/core/turn_system.cpp)
- [`src/game/core/battle_turn_flow.h`](/c-project/src/game/core/battle_turn_flow.h)
- [`src/game/core/battle_turn_flow.cpp`](/c-project/src/game/core/battle_turn_flow.cpp)
- [`src/game/core/battle_flow_controller.h`](/c-project/src/game/core/battle_flow_controller.h)
- [`src/game/core/battle_flow_controller.cpp`](/c-project/src/game/core/battle_flow_controller.cpp)

These cover:

- action-value turn ordering
- previewing the next actor
- extra turns
- player turn execution helpers
- automatic boss/follow-up processing

### Ability and presentation integration

- [`src/game/core/ability_system.h`](/c-project/src/game/core/ability_system.h)
- [`src/game/core/ability_system.cpp`](/c-project/src/game/core/ability_system.cpp)

The important architectural pattern here is:

- battle logic computes an `AbilityExecutionContext`
- ability logic can trigger a `PresentationContext`
- the active session front end injects a presentation interaction runner

That is how the same gameplay layer can drive different battle wrappers.

## 10. Main-App Battle Runtime

The battle path the main story currently uses is:

```text
src/main.cpp
  -> battle::demo::Session
     -> optional party setup
     -> BattleSessionCore
     -> optional demo/tutorial narrative overlay
```

### Demo/session wrapper

[`src/game/demo_battle_session.cpp`](/c-project/src/game/demo_battle_session.cpp) is the wrapper used by the main app and demo executables.

It is responsible for:

- loading the selected `BattleDefinition`
- showing the party setup screen when the lineup is not fixed
- creating `BattleSessionCore`
- wiring narrative hooks for tutorial battles
- routing outcome back to the caller

### Party setup

- [`src/game/demo/battle_party_setup.cpp`](/c-project/src/game/demo/battle_party_setup.cpp)
- [`src/game/demo/battle_party_setup_ui.cpp`](/c-project/src/game/demo/battle_party_setup_ui.cpp)

This is an SDL screen that:

- loads all available character definitions
- applies locked/fixed lineup rules from the battle definition
- lets the player choose a party
- returns a `PartySetupResult`

### Demo narrative flow

- [`src/game/demo/demo_narrative_flow.cpp`](/c-project/src/game/demo/demo_narrative_flow.cpp)

This is tutorial-specific glue:

- loads tutorial dialogue scripts from `assets/vn/json/`
- gates battle input until tutorial dialogue finishes
- starts post-action tutorials after specific player turns
- shows boss-defeated dialogue

This is content-specific logic, not generic battle flow.

## 11. BattleSessionCore

[`src/game/battle_session_core.cpp`](/c-project/src/game/battle_session_core.cpp) is the shared SDL battle runtime.

### Responsibilities

It owns:

- `BattleManager`
- scene entities
- sprite/icon textures
- floor texture
- battle HUD
- camera and camera staging
- combat-begin intro animation
- combat feedback overlays
- active ability presentation playback

### Hook model

`BattleSessionCore::Hooks` lets wrapper code inject behavior for:

- dialogue gating
- space-key consumption before battle input
- per-frame narrative updates
- presentation audio
- post-render VN overlay
- shutdown cleanup

This is the main extension point that allows tutorial/demo logic to wrap the same core runtime.

### SDL battle frame order

The render order is currently:

1. combat-begin intro if active
2. choose focused entity from turn preview or presentation state
3. clear renderer
4. draw battle floor
5. draw presentation content below world if needed
6. draw battle entities
7. draw presentation content below HUD if needed
8. sync and draw SDL HUD
9. draw presentation content above HUD if needed
10. draw feedback overlays
11. draw ultimate splash if active
12. call `hooks_.onPostRender()` for wrapper overlays such as VN dialogue

So VN overlay-in-battle is currently an outer hook layered on top of the SDL battle renderer.

## 12. SDL Battle Rendering

The SDL battle renderer is split across `src/game/render/`.

Important files:

- [`src/game/render/battle_world_renderer.cpp`](/c-project/src/game/render/battle_world_renderer.cpp)
- [`src/game/render/battle_party_staging.cpp`](/c-project/src/game/render/battle_party_staging.cpp)
- [`src/game/render/battle_camera_staging.cpp`](/c-project/src/game/render/battle_camera_staging.cpp)
- [`src/game/render/battle_ui.cpp`](/c-project/src/game/render/battle_ui.cpp)
- [`src/game/render/battle_feedback.cpp`](/c-project/src/game/render/battle_feedback.cpp)
- [`src/game/render/battle_combat_begin_animation.cpp`](/c-project/src/game/render/battle_combat_begin_animation.cpp)

### World rendering model

The world renderer is a faux-3D sprite scene:

- characters are still flat billboards
- floor is projected with SDL geometry
- entity ordering is manually depth-sorted
- there is no depth buffer

So this is staged 3D-looking sprite combat, not a full 3D scene renderer.

## 13. Standalone Rml/OpenGL Battle Runtime

The separate battle app is:

- [`src/rmlui_battle_main.cpp`](/c-project/src/rmlui_battle_main.cpp)
- [`src/game/app_battle_session.cpp`](/c-project/src/game/app_battle_session.cpp)

### What it does

This runtime owns:

- OpenGL context usage
- RmlUi context and HUD document
- a `BattleManager`
- HUD overlay state such as pause/settings/tutorial/rhythm
- a software scene renderer
- a GL blitter that uploads the software-rendered frame

### Current render pipeline

Per frame it does:

1. render the battle scene into `SoftwareSceneRenderer`
2. upload the SDL surface to a GL texture with `GlScreenBlitter`
3. clear the GL backbuffer
4. draw the uploaded scene full-screen
5. update and render the RmlUi HUD

So this is not a native GL world renderer yet. It is an SDL software scene rendered into a GL presentation layer.

### Important limitation

This battle app does not mirror the full SDL session architecture.

At initialization it currently builds only:

- one front character entity
- one boss entity

even though `BattleManager` may hold a larger party.

That makes `battle_testing` useful for HUD and input experiments, but not a full replacement for the main SDL battle session yet.

## 14. Rml Battle HUD Layer

The Rml battle HUD logic is split into small headers under `src/game/ui/`.

Important files:

- [`src/game/ui/battle_session_document_updates.h`](/c-project/src/game/ui/battle_session_document_updates.h)
- [`src/game/ui/battle_session_overlay_bindings.h`](/c-project/src/game/ui/battle_session_overlay_bindings.h)
- [`src/game/ui/battle_session_ui_state.h`](/c-project/src/game/ui/battle_session_ui_state.h)

This layer is responsible for:

- pushing battle state into the Rml document
- binding pause/settings/rhythm click handlers
- managing tutorial overlay state
- managing pause/settings selection state
- showing HUD feedback like toasts and orb blinking

This part is structurally cleaner than the older SDL HUD path because the document update code is already separated from raw event handling.

## 15. Ability Presentations

Ability-specific visuals live under `src/game/presentation/`.

Examples:

- splash art
- minigames
- boss attack sequences
- ability-specific animations

Architecturally, presentations are a shared layer used by both battle front ends.

They can:

- render below the world
- render above the world
- render above the HUD
- emit hit/heal/audio cues
- request interaction results back from the host runtime

This is one of the core reasons battle rendering is still session-driven rather than a pure update/render ECS style pipeline.

## 16. Current Architectural Strengths

The codebase already has some good boundaries:

- authored combat data is externalized in JSON
- `BattleManager` is shared across both battle front ends
- `BattleSessionCore` cleanly separates reusable SDL battle runtime from demo/tutorial wrapper logic
- front-ui documents use controller objects instead of putting all Rml state in `main.cpp`
- VN text fallback for mixed Latin/CJK is handled deliberately instead of by broad non-ASCII switching

## 17. Current Architectural Gaps

These are the main places where the architecture is still split or awkward.

### 17.1 Two battle front ends

There is one gameplay core, but there are still two real battle presentation stacks:

- main app / demos: SDL battle session
- `battle_testing`: software scene + GL blit + Rml HUD

They do not currently represent the same battle scene in the same way.

### 17.2 Runtime backend switching in the main app

`src/main.cpp` has to actively switch the `Window` between:

- OpenGL mode for front-ui screens
- SDL renderer mode for battle and some non-front-ui paths

That works, but it keeps rendering policy in the app shell.

### 17.3 `main.cpp` still owns too much orchestration

It still directly handles:

- screen routing
- save/load side effects
- story progression
- battle creation and cleanup
- backend restoration

The code works, but the app shell remains large and stateful.

### 17.4 VN runtime is globally stateful

`vn_system.cpp` still relies on static globals for nearly all state.

That makes integration straightforward, but it is not a session object and it is easy for multiple systems to depend on it implicitly.

### 17.5 Some battle wrappers are content-specific

`DemoNarrativeFlow` is tutorial-specific, and `app_battle_session.cpp` still has hardcoded battle setup and tutorial assumptions.

That is acceptable for current iteration, but it means some battle wrappers are not generic application layers yet.

### 17.6 Standalone Rml battle is still transitional

`app_battle_session.cpp` has a better HUD architecture than the SDL HUD path, but:

- it still renders the world through SDL software
- it still uploads that result every frame
- it still simplifies party staging compared with the main SDL session

So it is not yet the unified future battle runtime.

## 18. Practical Direction From Here

Based on the current branch, the most realistic direction is:

1. Keep `BattleManager`, loader, turn logic, and presentations as the shared gameplay layer.
2. Keep the Rml front-ui stack for menu/story/pause/load/settings.
3. Decide whether battles should standardize on:
   - the SDL battle session with a future Rml HUD layer, or
   - the Rml/OpenGL battle app after it reaches feature parity.
4. Reduce what `src/main.cpp` directly owns by moving story/battle app flow into higher-level session objects.

The most important architectural fact right now is simple:

- the main shipped game path already uses RmlUi for front-end/story UI
- the main shipped battle path is still the SDL battle session
- the standalone Rml battle app is newer, cleaner in some UI areas, but not yet the same runtime
