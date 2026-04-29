# Architecture

This document is a practical map of the current codebase.

It focuses on:

- which files own which parts of the game
- how rendering currently works
- what is missing or awkward in the current rendering architecture
- what the intended target architecture should be

It is not a design pitch. It is a description of the code as it exists now, with a clearer target direction.

## Submission Note

For the final assignment, the code is being presented as a **readable systems map**
rather than a full engine textbook. The most important files for grading are:

- `CMakeLists.txt` for build/package structure
- `src/main.cpp` for overall game flow
- `src/game/core/*` for battle data loading and combat logic
- `src/game/app_battle_session.*` for the shipped battle runtime
- `src/game/vn/*` for story presentation
- `src/platform/path_resolution.h` for cross-platform asset lookup

If a reviewer wants a fast understanding of the current game, they should read
those files first and then use this document as a guide to the rest of the tree.

## 1. Big Picture

The project currently has two rendering worlds:

1. The main game and story flow use `SDL_Renderer`.
2. Battle has two paths:
   - an SDL battle path used by the main game and demo executables
   - a newer OpenGL + RmlUi battle path used by `battle_testing`

So the project does not have one unified renderer yet.

Current split:

```text
Main game
  src/main.cpp
  -> SDL window + SDL_Renderer
  -> VN rendering
  -> menu rendering
  -> demo battle session rendering

Battle demo / in-story battle
  src/game/battle_session_core.*
  -> SDL_Renderer world rendering
  -> SDL HUD rendering
  -> SDL presentation overlays

Rml battle app
  src/game/app_battle_session.*
  -> software SDL scene render to offscreen surface
  -> upload to OpenGL texture
  -> draw full-screen quad
  -> render RmlUi HUD on top
```

That split is the main architectural fact to understand before changing graphics code.

## 2. File Structure

### Main app shell

- `src/main.cpp`
  Top-level app loop. Owns screen switching, event routing, story progression, and final per-frame render dispatch.
- `src/window.h`
- `src/window.cpp`
  Window wrapper. Can run in SDL renderer mode or OpenGL mode. `present()` chooses `SDL_RenderPresent` or `SDL_GL_SwapWindow`.

### Story / VN

- `src/game/vn/vn_system.h`
- `src/game/vn/vn_system.cpp`
  Story renderer and dialogue runtime. Draws background, portrait/icon, dialogue box, typewriter text, and voice playback.
- `src/game/vn/vn_script.h`
- `src/game/vn/vn_script.cpp`
  JSON loading for VN scripts.

### Main-game menus and settings

- `src/GameMenu/main_menu.cpp`
  Main menu rendering and input.
- `src/GameMenu/load_menu.cpp`
  Load screen rendering and input.
- `src/GameMenu/pause_menu.cpp`
  Pause menu rendering and input.
- `src/Settings/settings.cpp`
  Settings screen rendering and input.
- `src/GameMenu/menu_shared.h`
  Shared menu/app state, helper declarations, and screen enums.

These files are important because the menu system is not a separate UI framework. It is a set of SDL immediate-mode draw functions plus shared app state.

### Shared battle runtime

- `src/game/battle_session_core.h`
- `src/game/battle_session_core.cpp`
  Shared SDL battle runtime used by the in-story battle and demo executables. Owns battle manager, world entities, battle camera, feedback, HUD, and presentation playback hooks.
- `src/game/demo_battle_session.h`
- `src/game/demo_battle_session.cpp`
  Wrapper around the shared battle core for demo/tutorial/in-story use.

### Battle gameplay logic

- `src/game/core/battle_manager.*`
  Core battle state and action resolution.
- `src/game/core/battle_flow_controller.*`
- `src/game/core/battle_turn_flow.*`
- `src/game/core/turn_system.*`
  Turn preview, turn order, action execution flow, and automatic turns.
- `src/game/core/ability_system.*`
  Ability execution and presentation hook integration.
- `src/game/core/battle_loader.*`
  Loads battle definitions from JSON.

### Battle rendering, SDL path

- `src/game/render/camera_3d.*`
  Perspective math, world-to-screen conversion, depth, and scale.
- `src/game/render/battle_world_renderer.*`
  Draws the floor and battle sprites with SDL.
- `src/game/render/battle_party_staging.*`
  Computes party positions in the duel layout / lineup layout.
- `src/game/render/battle_camera_staging.*`
  Moves the battle camera toward the current turn framing.
- `src/game/render/battle_ui.*`
  SDL HUD for turn order, boss HP, party cards, and hint text.
- `src/game/render/battle_feedback.*`
  Damage numbers, shakes, and combat feedback overlays.
- `src/game/render/battle_combat_begin_animation.*`
  Intro animation before battle control begins.
- `src/game/render/battle_asset_loading.*`
  Loads combat sprite/icon textures.
- `src/game/render/battle_scene_types.h`
  Shared entity structs for battle scene rendering.

### Battle rendering, RmlUi/OpenGL path

- `src/game/app_battle_session.h`
- `src/game/app_battle_session.cpp`
  Newer battle app wrapper. Owns OpenGL setup, RmlUi context, software world renderer state, HUD document updates, and battle loop for `battle_testing`.
- `src/game/render/battle_scene_renderer.h`
  Header-only software scene renderer used by the RmlUi/OpenGL path.
- `src/game/render/gl_screen_blitter.h`
  Uploads the software-rendered scene to GL and draws it full screen.
- `src/game/ui/battle_session_document_updates.h`
  Pushes battle state into the RmlUi document.
- `src/game/ui/battle_session_overlay_bindings.h`
  Wires RmlUi controls to battle callbacks.
- `src/game/ui/battle_session_ui_state.h`
  State structs for toast, tutorial, rhythm challenge, and pause/settings overlays.

### Battle presentations

- `src/game/presentation/*.cpp`
- `src/game/presentation/*.h`
  Ability-specific animations, minigames, splash art, and special overlays.

These presentation files are still mostly SDL-rendered overlays even when the base battle path is different.

## 3. How Rendering Currently Works

## 3.0 Runtime Flow Summary

The easiest way to understand the code is to follow the runtime in this order:

```text
main.cpp
  -> initialize window / audio / save systems
  -> load story or menu state
  -> route to VN, battle, selector, post-battle, or credits
  -> persist progression and settings

vn_system.cpp
  -> present dialogue scenes
  -> request story transitions or battle launches

battle_loader.cpp + battle_manager.cpp
  -> load authored combat data
  -> execute turn-based battle rules

app_battle_session.cpp
  -> bind battle rules to rendering, audio, HUD, and scripted presentations
```

## 3.1 Window / backend ownership

The `Window` class can run in two modes:

- SDL renderer mode
- OpenGL mode

`Window::present()` does this:

- if GL context exists: `SDL_GL_SwapWindow`
- else if SDL renderer exists: `SDL_RenderPresent`

That means the backend choice is still a top-level app choice, not a per-subsystem abstraction.

## 3.2 Main game rendering

The main game render loop lives in `src/main.cpp`.

Per frame, it does this:

1. Clear the SDL renderer with `window.clear(...)`.
2. Decide which screen is active from `AppState.screen`.
3. Render one of:
   - VN scene
   - pause overlay on top of VN
   - settings screen
   - load screen
   - battle demo session
   - main menu
4. Call `window.present()`.

Important detail:

- `main.cpp` is the final render router.
- Menus are not pushed through a scene graph.
- Each screen mostly draws itself directly into the same SDL renderer.

### Main game draw order

For story mode:

1. `vn::render()`
2. optional pause/settings overlay on top
3. `window.present()`

For battle mode inside the main app:

1. `battleSession->render(...)`
2. optional pause/settings overlay on top
3. `window.present()`

For menu-only screens:

1. menu screen render
2. `window.present()`

## 3.3 How VN rendering works

`src/game/vn/vn_system.cpp` owns story rendering.

It keeps global/static state such as:

- current background texture
- current icon texture
- current text
- current speaker name
- current font(s)
- voice playback state
- typewriter progress

Current VN rendering is direct SDL drawing:

1. Draw current background texture.
2. Draw icon/portrait if present.
3. Draw dialogue box panels.
4. Draw speaker name.
5. Draw visible portion of the line using the typewriter state.
6. Draw any supporting UI accents.

Text rendering uses `src/platform/text_fallback.h` correctly:

- Latin stays on the normal font.
- CJK runs switch to the CJK fallback font.
- mixed strings are split into runs instead of switching the whole string.
- baseline alignment is preserved between Latin and CJK runs.

That part is one of the better-structured text systems in the project.

## 3.4 How menu rendering works

Main menu, pause, load, and settings screens are all custom SDL-drawn UIs.

They render with helper functions such as:

- `drawCyberPanel(...)`
- `drawJaggedButtonPanel(...)`
- `drawSlantedPanel(...)`
- `drawNeonLine(...)`

The main menu has an extra offscreen step:

- `beginMenuCanvas(...)` sets an SDL render target texture
- menu UI is drawn into a 1280x720 reference layout
- `endMenuCanvas(...)` draws that texture back to the window

Other screens usually use `beginReferenceLayout(...)` instead:

- set a viewport matching the letterboxed reference area
- set SDL scale
- draw directly into the window renderer

So even inside the main game, UI rendering is not fully uniform:

- main menu often uses an offscreen canvas
- other screens mostly render straight to the window renderer

## 3.5 Battle rendering, SDL path

The main app and demo executables currently use the SDL battle path through `BattleSessionCore`.

### Who owns what

`BattleSessionCore` owns:

- `BattleManager`
- scene entities
- sprite and icon textures
- battle floor texture
- battle HUD
- battle camera
- camera staging
- feedback system
- ability presentation playback state

### Current SDL battle frame order

`BattleSessionCore::render(...)` currently does this:

1. If combat-begin intro is active, render only that intro and return.
2. Set camera screen center from current window size.
3. Choose the focused entity based on turn preview or current presentation.
4. Clear the renderer.
5. Draw the floor with `renderBattleFloor(...)`.
6. If a presentation wants to draw below the world, draw it.
7. Draw battle entities with `renderBattleEntities(...)`.
8. If a presentation should render below the HUD, draw it now.
9. Sync and draw the SDL HUD with `battle_ui`.
10. If a presentation should render above the HUD, draw it now.
11. Draw combat feedback overlays.
12. Draw ultimate splash overlay if active.
13. Run `hooks_.onPostRender()` for wrapper-specific overlays like VN dialogue.

That is the actual current battle draw stack.

### How the world is drawn

`src/game/render/battle_world_renderer.cpp` handles the SDL world.

It currently renders:

- a tiled floor plane using `SDL_RenderGeometry`
- billboard-like character sprites
- fallback rectangles when textures are missing
- a focus ring around the active/focused entity

Important details:

- There is no depth buffer.
- Entities are sorted manually by projected depth.
- The floor is clipped manually against a near plane.
- Characters are still fundamentally flat sprites in a projected 3D layout.

So this is a faux-3D sprite battle renderer, not a full 3D renderer.

### How battle UI is drawn on the SDL path

`src/game/render/battle_ui.cpp` draws:

- turn order
- boss HP
- party status cards
- hint text

It is also direct SDL immediate-mode drawing.

One architectural smell here:

- `battle_ui.cpp` currently keeps a global `g_lastBattleHudManager` pointer for shield rendering access.

That should eventually be removed and replaced with explicit HUD model data.

## 3.6 Battle rendering, RmlUi/OpenGL path

The newer `battle_testing` executable uses `src/game/app_battle_session.cpp`.

This path is different from the SDL battle path.

### Current frame order

`SessionImpl::render()` in `app_battle_session.cpp` currently does this:

1. Render the battle world into `sceneRenderer_` with `renderBattleScene(...)`.
2. Upload the resulting software surface to GL with `screenBlitter_.uploadSurface(...)`.
3. Clear the GL backbuffer.
4. Draw the uploaded battle scene as a full-screen textured quad.
5. Run `RmlUi`:
   - `context_->Update()`
   - `context_->Render()`
6. Let the outer loop call `window.present()`, which swaps the GL window.

So the newer battle renderer is not a native GL world renderer yet.

It is:

- software-render battle scene
- copy to GL texture
- draw that texture
- render RmlUi on top

### What `battle_scene_renderer.h` actually is

Despite the name, `src/game/render/battle_scene_renderer.h` is currently a header-only software battle renderer utility.

It owns:

- offscreen SDL surface/renderer
- floor texture
- world sprite textures

It is mainly there so the RmlUi/OpenGL battle app can keep using SDL-style scene drawing and then blit the result to the OpenGL output.

### How the Rml battle HUD works

RmlUi owns the on-screen HUD and overlays in this path.

Files involved:

- `battle_session_document_updates.h`
- `battle_session_overlay_bindings.h`
- `battle_session_ui_state.h`

The game logic updates Rml element text/classes/properties every frame.

That means this path has a cleaner HUD layer than the SDL HUD path, but the world renderer underneath is still transitional.

## 3.7 Ability presentation rendering

Ability presentations are still very important to understanding the graphics architecture.

Presentations in `src/game/presentation/` can:

- render below the world
- render over the world but under the HUD
- render over the HUD
- override the camera
- override caster/target positions
- run custom minigame input

On the SDL battle path, presentations are integrated by `BattleSessionCore`.

On the Rml/OpenGL path, presentations are run inside `app_battle_session.cpp`, but many of them still render with `SDL_Renderer` style code during their playback loop.

This is one reason the graphics architecture still feels split.

## 4. Current Rendering Weaknesses

These are the main problems in the current architecture.

### 4.1 Two battle rendering stacks

There is not one battle renderer.

There is:

- the shared SDL battle runtime used by the main game
- the newer RmlUi/OpenGL battle app

They do not share the same full rendering path.

### 4.2 Main game rendering is centralized but not modular

`src/main.cpp` still owns too much top-level rendering flow.

It decides:

- active screen
- backdrop source
- whether pause/settings overlay sits on VN or battle
- when battles begin/end

That makes it harder to evolve rendering without growing `main.cpp`.

### 4.3 Menu rendering is immediate-mode and manually duplicated

Menus are visually coherent, but architecturally they are still hand-drawn screen modules.

There is no shared retained UI system for the main app.

### 4.4 The Rml battle path is still transitional

The OpenGL battle app is not yet a real GL-native scene renderer.

It still depends on:

- software scene rendering through SDL surfaces/renderers
- upload to a GL texture every frame

That is good enough for iteration, but not the final architecture.

### 4.5 Battle presentation playback is still blocking

Presentations run their own loops during playback.

That works, but it means the battle runtime is not yet a fully unified single-frame update/render pipeline.

### 4.6 The Rml battle world is still simplified

`app_battle_session.cpp` currently creates only:

- one front party entity
- one boss entity

even though the battle manager can hold a larger party.

So the newer battle app does not yet mirror the full party staging that the shared SDL battle runtime supports.

### 4.7 Some rendering state still leaks across layers

Examples:

- global/static VN renderer state
- HUD needing manager access through a global pointer hack
- presentation code reaching directly into render assumptions

These are workable, but they are not the clean target structure.

## 5. Intended / Target Architecture

The target should be simpler than the current split.

## 5.1 Target goals

The graphics layer should eventually have:

1. One battle scene renderer.
2. One clear separation between:
   - game state
   - scene staging
   - world rendering
   - HUD rendering
   - presentation overlays
3. A smaller `main.cpp`.
4. File responsibilities that are obvious from the folder structure.

## 5.2 Recommended target split

```text
App shell
  screen switching
  window/backend setup
  top-level frame loop

Story renderer
  VN scene
  story overlays

Battle runtime
  battle state
  turn flow
  presentation triggers

Battle scene staging
  choose visible entities
  choose camera target
  compute world positions
  build HUD model

Battle world renderer
  draw floor
  draw sprites/models
  draw environment

Battle HUD renderer
  SDL HUD or RmlUi HUD, but behind one interface

Presentation layer
  splash art
  special attacks
  minigame overlays
```

The key idea is that staging should be separate from drawing.

## 5.3 Practical target for this codebase

The best realistic direction for this project is:

- keep the battle logic in `src/game/core/`
- keep ability-specific visuals in `src/game/presentation/`
- move toward one shared battle scene staging model
- move toward one world renderer implementation
- keep RmlUi only as the HUD/overlay layer if that is the preferred future UI stack

That would reduce duplication and make the battle experience consistent between:

- story battles
- demos
- `battle_testing`

## 5.4 Main-app target

For the main game, a better structure would be:

- `main.cpp` only runs the app loop and screen transitions
- each screen owns its own update/render pair cleanly
- the VN renderer stays self-contained
- pause/settings/load become composable overlay screens instead of branching logic inside `main.cpp`

That does not require rewriting the art style. It just means better ownership boundaries.

## 6. Short Summary

Current reality:

- Main game rendering is SDL renderer based.
- Story rendering is owned by `vn_system.cpp`.
- Menus are hand-drawn SDL UI screens.
- In-story battles use the SDL battle runtime in `BattleSessionCore`.
- `battle_testing` uses a different path: software-render world, upload to GL, then draw RmlUi on top.

Most important architectural gap:

- the project still has two battle rendering architectures instead of one.

Most useful target:

- one shared battle scene pipeline
- one staged battle scene model
- one world renderer
- one HUD layer abstraction
- less render routing logic in `main.cpp`
