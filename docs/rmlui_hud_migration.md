# RmlUi HUD Migration

## Current state

The battle HUD is now isolated behind `battle::ui::BattleHud` and fed from `BattleManager` through `syncFromManager()`. Combat logic no longer mutates HUD state directly.

This is the prerequisite for moving the HUD to `RmlUi`, but `RmlUi` is not a drop-in renderer for the current setup.

`ENABLE_RMLUI` now fetches the upstream library and builds the default `battle_testing` target from `src/rmlui_battle_main.cpp`, which runs the OpenGL-backed RmlUi battle path.

## Why the main SDL renderer is not a drop-in RmlUi path

The project currently renders through `SDL_Renderer` in [src/window.cpp](../src/window.cpp).

`RmlUi` expects either:

- a custom `RenderInterface`, or
- one of its provided backend paths such as `SDL + OpenGL 3`

That means the standalone RmlUi battle path is wired in today, but migrating the existing SDL-rendered HUD path still requires a renderer decision first.

## Recommended migration path

1. Keep world rendering custom.
   Move only the HUD, menus, and overlays to `RmlUi`.

2. Replace `SDL_Renderer` window creation with an OpenGL-backed window.
   This is the shortest path to using the upstream `RmlUi` SDL/GL backend.

   The `Window` wrapper now exposes `SDL_Window*` through `getNativeWindow()` so a future `RmlUi` backend can bind to the same window object.

3. Add a `BattleHudViewModel`.
   `BattleHud` already acts like this boundary; the next step is to expose the same data to `RmlUi` documents instead of SDL draw calls.

4. Port HUD sections in order:
   - boss header
   - turn order
   - party status strip

5. Delete the remaining SDL HUD drawing code from `battle_ui.cpp`.

## What to avoid

- Do not let `BattleManager` call into `RmlUi` directly.
- Do not mix layout logic back into battle scene code.
- Do not port the 3D-ish duel world to HTML/CSS. Keep that renderer custom.
