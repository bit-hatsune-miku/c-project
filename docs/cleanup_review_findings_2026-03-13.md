# Cleanup Review Findings

Date: 2026-03-13
Scope:
- `src/main.cpp`
- `src/battle_main.cpp`
- `src/rmlui_battle_smoke.cpp`
- `src/rmlui_battle_main.cpp`
- `src/window.*`
- `src/GameMenu/*`
- `src/Settings/*`
- `src/game/core/*`
- `src/game/app_battle_session.*`
- `src/game/demo_battle_session.*`
- `src/game/vn/*`
- `src/game/render/*`
- `src/game/presentation/*`
- `assets/vn/json/*`
- `assets/combat/*`

Constraint: no application code changes made. This is a documentation-only cleanup review.

## Summary

The main readability and context-bloat problems are concentrated in a small set of oversized files:
- `src/game/app_battle_session.cpp`
- `src/main.cpp`
- `src/game/vn/vn_system.cpp`
- `src/game/demo_battle_session.cpp`
- `src/rmlui_battle_smoke.cpp`
- `src/game/render/battle_ui.cpp`
- `src/GameMenu/menu_shared.h`

The most important cleanup direction is to split orchestration from rendering, UI mutation, content loading, and platform helpers. The codebase currently repeats the same camera/path/audio/render helpers in multiple battle entrypoints, which makes fixes drift and inflates the amount of code an agent or human has to load for routine changes.

## Highest-priority findings

### 1. `src/game/app_battle_session.cpp` is the primary god-file
References:
- `src/game/app_battle_session.cpp:252`
- `src/game/app_battle_session.cpp:644`
- `src/game/app_battle_session.cpp:1043`
- `src/game/app_battle_session.cpp:1381`
- `src/game/app_battle_session.cpp:2137`

What is not optimal:
- One translation unit owns path resolution, font lookup, one-shot audio, HUD document mutation, tutorial sequencing, rhythm challenge flow, pause/settings handling, software scene rendering, GL blitting, RmlUi lifecycle, SDL event handling, and overall session orchestration.
- `SessionImpl` is effectively a hidden subsystem container instead of a focused coordinator.

Why it matters:
- Small changes to battle flow, HUD, rendering, or settings require loading a very large file with unrelated concerns.
- The file overlaps heavily with `src/game/demo_battle_session.cpp` and `src/rmlui_battle_smoke.cpp`.

Recommended breakup:
- `src/game/app/session_controller.*`
- `src/game/app/session_ui_bindings.*`
- `src/game/app/session_pause_settings.*`
- `src/game/app/session_tutorials.*`
- `src/game/app/session_rhythm.*`
- `src/game/render/battle_scene_renderer.*`
- `src/game/render/gl_screen_blitter.*`
- `src/game/audio/wav_one_shot.*`
- `src/platform/path_resolver.*`

### 2. Battle presentation logic is duplicated across multiple implementations
References:
- `src/battle_main.cpp:1`
- `src/rmlui_battle_smoke.cpp:1`
- `src/game/demo_battle_session.cpp:1`
- `src/game/app_battle_session.cpp:1`

What is not optimal:
- Camera constants, camera intro animation, path resolution, texture loading, floor rendering, sprite drawing, voice helpers, and world-entity logic exist in multiple near-duplicate forms.

Why it matters:
- Fixes must be rediscovered and repeated.
- It is unclear which battle path is canonical and which ones are legacy.

Recommended cleanup:
- Decide which battle path is authoritative.
- Extract shared scene/camera/asset helpers into reusable modules.
- Move legacy-only paths under `src/legacy/` or delete them once replaced.

### 3. `src/main.cpp` is overloaded and acts as a hidden dependency hub
References:
- `src/main.cpp:32`
- `src/main.cpp:48`
- `src/main.cpp:222`
- `src/main.cpp:562`
- `src/main.cpp:606`
- `src/main.cpp:627`

What is not optimal:
- The main entrypoint contains path helpers, texture helpers, render-target helpers, text rendering, geometry drawing, menu panel drawing, story bootstrapping, battle bootstrapping, process launch, resource destruction, and the top-level app loop.
- `src/GameMenu/menu_shared.h` declares helpers that are implemented here, so menu code depends on the main translation unit for non-entrypoint functionality.

Recommended breakup:
- Keep `src/main.cpp` limited to startup and the app loop.
- Move menu render helpers into `src/ui/render_helpers.*`.
- Move menu resource loading into `src/ui/menu_resources.*`.
- Move mode transitions into `src/app/app_controller.*`.
- Move external process launch into `src/app/process_launch.*`.

### 4. `src/GameMenu/menu_shared.h` is carrying too many unrelated concepts
References:
- `src/GameMenu/menu_shared.h:17`
- `src/GameMenu/menu_shared.h:58`
- `src/GameMenu/menu_shared.h:70`
- `src/GameMenu/menu_shared.h:83`
- `src/GameMenu/menu_shared.h:102`
- `src/GameMenu/menu_shared.h:123`

What is not optimal:
- One header combines app state, story state, menu resources, geometry helper declarations, path helpers, navigation helpers, and screen entrypoints.

Why it matters:
- Files that need one menu type pull in a much wider surface area than necessary.
- It increases rebuild fan-out and context bloat.

Recommended breakup:
- `src/app/app_state.h`
- `src/ui/menu_resources.h`
- `src/ui/layout_helpers.h`
- `src/ui/render_helpers.h`
- `src/GameMenu/main_menu.h`
- `src/GameMenu/pause_menu.h`
- `src/GameMenu/exit_to_main_menu.h`

### 5. `src/game/vn/vn_system.cpp` is both a context hotspot and an architectural singleton
References:
- `src/game/vn/vn_system.cpp:19`
- `src/game/vn/vn_system.cpp:283`
- `src/game/vn/vn_system.cpp:640`

What is not optimal:
- File-scope globals hold renderer state, viewport size, current line state, icon textures, audio device state, and playback flags.
- The same file owns audio lifecycle, font lookup/scaling, rich-text parsing, image loading, typewriter timing, and rendering.

Why it matters:
- The subsystem is hard to test, hard to reuse, and tightly coupled to one mutable global instance.
- Any change to VN rendering or playback requires loading the entire subsystem.

Recommended breakup:
- `src/game/vn/vn_state.*`
- `src/game/vn/vn_audio.*`
- `src/game/vn/vn_assets.*`
- `src/game/vn/vn_text_layout.*`
- `src/game/vn/vn_renderer.*`

## Concrete bug-risk and correctness findings

### 6. VN line state is sticky when optional fields are omitted
References:
- `src/game/vn/vn_system.cpp:650`
- `src/game/vn/vn_system.cpp:653`

What looks broken:
- `showLine()` only updates font/background when the new path is non-empty.
- A line cannot explicitly clear a previous background or revert to a default font.

Impact:
- Content depends on implicit carry-over from prior lines, which is brittle and easy to break.

Recommended cleanup:
- Apply full line state on every `showLine()` call, including explicit clearing of optional assets.

### 7. VN rich-text rendering is doing expensive per-character work every frame
References:
- `src/game/vn/vn_system.cpp:283`

What is not optimal:
- The current path creates SDL surfaces/textures glyph-by-glyph while the line reveals.

Impact:
- Avoidable hot path on long lines and lower-end hardware.

Recommended cleanup:
- Parse and lay out a line once.
- Cache rendered segments and only vary reveal count during typewriter playback.

### 8. Presentation registry coverage looks incomplete, and registration may never happen
References:
- `src/game/presentation/presentation_registry.cpp:8`
- `src/game/presentation/ability_presentation.h:102`
- `assets/combat/abilities.json:2`

What still looks broken:
- Only three presentation IDs are registered in code, while more abilities declare `presentationId` in content.
- Verified missing IDs include at least `basic_attack`, `boss_standard`, `heal_hearts`, `hearts_everywhere_ultimate`, `niagara_falls_ultimate`, `boss_attack_lyoo`, and `boss_attack_lyoo_ultimate`.

Impact:
- Some presentation hooks remain unresolved.

Recommended cleanup:
- Add validation that every `presentationId` referenced by content is registered.

### 9. `src/game/core/ability_system.cpp` only partially implements the declared targeting/effect model
References:
- `src/game/core/ability_system.cpp:14`
- `src/game/core/ability_system.cpp:23`
- `src/game/core/battle_manager.h:53`

What looks broken:
- `AbilityDefinition` supports `SingleEnemy`, `AllEnemies`, `SingleAlly`, `AllAllies`, and `Self`.
- The execution path only covers a narrow subset of those semantics.
- Buff/debuff handling is effectively absent.

Impact:
- New content can declare valid-looking ability data that does not behave as specified.

Recommended cleanup:
- Split target resolution from effect resolution.
- Add a dedicated `battle_effects.*` or `ability_resolution.*` module.

### 11. Extra-turn handling is inconsistent, and stale preview slots can block player actions
References:
- `src/game/core/battle_manager.cpp:250`
- `src/game/core/battle_manager.cpp:306`
- `src/game/core/battle_manager.cpp:370`
- `src/game/core/battle_manager.cpp:553`
- `src/game/core/turn_system.cpp:117`
- `src/game/core/battle_manager.h:208`

What still looks risky:
- `queueExtraTurnForCharacter()` still looks incomplete as a long-term scheduling mechanism because it mutates actor state without a clearly bounded lifecycle.

Impact:
- If extra-turn support grows later, actor vectors may still drift or accumulate unexpectedly.

Recommended cleanup:
- If extra turns remain planned, redesign them as bounded scheduled inserts with explicit cleanup.

### 12. `launchDefaultBattleMode()` uses `std::system()` with incomplete shell escaping
References:
- `src/main.cpp:594`
- `src/main.cpp:606`

What looks risky:
- The custom quoting only escapes `"` and `\\`.
- That is not a safe general-purpose shell escaping strategy on POSIX.

Impact:
- Fragile argument handling and avoidable shell-invocation risk.

Recommended cleanup:
- Replace with direct process spawning such as `execv` or `posix_spawn`.

### 14. Some content/assets appear broken or incomplete today
References:
- `assets/vn/json/demo.json:58`
- `assets/combat/characters.json:72`

Warnings:
- `assets/combat/characters.json` defines `luotianyi`, but the corresponding combat assets and abilities do not appear to exist in the repository.

Recommended cleanup:
- Add content validation during load for asset paths, ability IDs, and presentation IDs.
- Warn clearly when content is incomplete; do not try to auto-heal missing assets.

## Medium-priority maintainability findings

### 15. `src/game/render/battle_ui.cpp` mixes model sync, animation state, and drawing
References:
- `src/game/render/battle_ui.cpp:182`
- `src/game/render/battle_ui.cpp:474`

What is not optimal:
- The file combines `BattleManager` -> HUD model syncing, HP transition state, damage flash state, and all draw routines in one place.
- Layout math is heavily hardcoded.

Recommended breakup:
- `battle_hud_model_builder.*`
- `battle_hud_animations.*`
- `battle_hud_layout.*`
- `battle_hud_draw_turn_order.*`
- `battle_hud_draw_party.*`
- `battle_hud_draw_boss.*`

### 16. `BattleManager` and `battle_manager.h` carry too many layers at once
References:
- `src/game/core/battle_manager.h:10`
- `src/game/core/battle_manager.h:129`
- `src/game/core/battle_manager.cpp:83`
- `src/game/core/battle_manager.cpp:409`

What is not optimal:
- Combat data types, state types, event types, runtime behavior, loading, turn progression, and effect execution all sit behind one type/header.

Recommended breakup:
- `battle_types.h`
- `battle_state.h`
- `battle_events.h`
- `battle_character.h/.cpp`
- `battle_action_resolver.h/.cpp`
- `battle_manager.h/.cpp`

### 17. The app session battle model and the rendered world model do not line up cleanly
References:
- `src/game/app_battle_session.cpp:1451`
- `src/game/app_battle_session.cpp:1479`

What is not optimal:
- The battle manager is initialized with four party members, but the scene entity list is effectively a focused duel presentation.

Impact:
- The rendering model is conceptually disconnected from the combat model and can surprise future maintainers.

Recommended cleanup:
- Either formalize this as a named “focused duel presentation” layer or render all party members.

### 18. Menu/settings rendering is visually intentional but structurally hardcoded
References:
- `src/GameMenu/main_menu.cpp:11`
- `src/GameMenu/pause_menu.cpp:18`
- `src/Settings/settings.cpp:13`

What is not optimal:
- These files combine layout constants, decorative drawing primitives, and controller logic into large files.

Recommended breakup:
- `*_layout.h` for geometry tokens
- `*_skin.cpp` for reusable drawing primitives
- `*_view.cpp` for render composition
- `*_controller.cpp` for event handling and transitions

### 19. `Window` is coupling global SDL lifetime and window/backend ownership
References:
- `src/window.cpp:8`
- `src/window.cpp:43`
- `src/window.cpp:112`

What is not optimal:
- `Window` initializes and shuts down SDL globally while also managing renderer/OpenGL backend behavior.

Impact:
- Harder to support test harnesses, multiple windows, or future runtime ownership cleanup.

Recommended cleanup:
- Introduce an `SdlRuntime` or `AppRuntime` owner for global initialization.
- Keep `Window` focused on the native window and backend attachment.

### 29. Several smaller cleanup issues are verified and should be tracked, but are lower priority
References:
- `src/game/app_battle_session.cpp:369`
- `src/game/app_battle_session.cpp:558`
- `src/game/app_battle_session.h:16`
- `src/game/core/battle_manager.h:17`
- `src/game/core/battle_manager.h:22`
- `src/game/vn/vn_system.cpp:155`

Verified issues:
- `playWavOneShot()` opens a new `SDL_AudioDeviceID` per call without any cap or pre-cleanup thresholding.
- `loadTutorialScriptLibrary()` collapses "file failed to load" and "insufficient entries" into the same `false` result.
- `battle::app::Session` deletes copy operations but does not explicitly declare move support.
- `BossDefinition` and `CharacterDefinition` still expose both legacy `ability` and explicit `standardAbility` / `skillAbility` / `ultimate` fields.
- `startingOrbs` defaults to `1`, which is fail-open during migration because omitted values can seed immediate skill usage.
- The VN font lookup prefers system fonts before repo-shipped fonts.

Recommended cleanup:
- Treat these as follow-up cleanup items after the higher-risk issues above.

## Recommended cleanup order

1. Split `src/game/app_battle_session.cpp` into orchestration, UI, tutorials, pause/settings, rendering, and audio modules.
2. Decide which battle path is canonical and extract shared battle-scene helpers used by app/demo/legacy flows.
3. Split `src/main.cpp` and `src/GameMenu/menu_shared.h` so the app root stops acting as a utility library.
4. Break up `src/game/vn/vn_system.cpp`, and fix sticky line-state semantics.
5. Add validation for content assets, ability IDs, and presentation IDs at load time.
6. Separate battle target/effect resolution from `BattleManager`.
7. Replace `std::system()` launch and remove path/helper duplication.

## Suggested split map

- `src/app/`
  - `app_controller.h/.cpp`
  - `app_runtime.h/.cpp`
  - `process_launch.h/.cpp`

- `src/ui/`
  - `menu_resources.h/.cpp`
  - `layout_helpers.h/.cpp`
  - `render_helpers.h/.cpp`

- `src/game/app/`
  - `session_controller.h/.cpp`
  - `session_ui_bindings.h/.cpp`
  - `session_pause_settings.h/.cpp`
  - `session_tutorials.h/.cpp`
  - `session_rhythm.h/.cpp`

- `src/game/render/`
  - `battle_scene_renderer.h/.cpp`
  - `battle_hud_model_builder.h/.cpp`
  - `battle_hud_animations.h/.cpp`
  - `battle_hud_layout.h/.cpp`
  - `gl_screen_blitter.h/.cpp`

- `src/game/vn/`
  - `vn_state.h/.cpp`
  - `vn_audio.h/.cpp`
  - `vn_assets.h/.cpp`
  - `vn_text_layout.h/.cpp`
  - `vn_renderer.h/.cpp`

- `src/game/core/`
  - `battle_types.h`
  - `battle_state.h`
  - `battle_events.h`
  - `battle_character.h/.cpp`
  - `ability_resolution.h/.cpp`
  - `battle_content_loader.h/.cpp`

## Short version

The biggest cleanup win is not micro-refactoring. It is establishing module boundaries so that:
- entrypoints stop being utility libraries
- battle sessions stop owning rendering/UI/audio/content logic directly
- VN stops being a global singleton blob
- duplicated battle helpers stop drifting across four implementations
- content mistakes are caught at load time instead of during play
