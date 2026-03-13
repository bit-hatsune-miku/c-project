# RmlUi HUD Migration

## Current state

The battle HUD is now isolated behind `battle::ui::BattleHud` and fed from `BattleManager` through `syncFromManager()`. Combat logic no longer mutates HUD state directly.

This is the prerequisite for moving the HUD to `RmlUi`, but `RmlUi` is not a drop-in renderer for the current setup.

`ENABLE_RMLUI` is now available in CMake to fetch and build the upstream library without changing the active HUD renderer yet.

## Why it is not wired in yet

The project currently renders through `SDL_Renderer` in [src/window.cpp](../src/window.cpp).

`RmlUi` expects either:

- a custom `RenderInterface`, or
- one of its provided backend paths such as `SDL + OpenGL 3`

That means a real `RmlUi` migration requires a renderer decision first.

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
