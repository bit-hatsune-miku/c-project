# CLAUDE.md - Project Guide

## What This Is

A C++17 / SDL2 game combining a **visual novel (VN)** system with a **turn-based combat** system. The setting features Vocaloid/internet culture characters (Hatsune Miku, CupcakKe, Lyoo, etc.) attending BIT (Beijing Institute of Technology). The story is in chapter 0: "Dreams and Dorms."

## Build System

- **CMake 3.16+**, C++17
- Dependencies: SDL2, SDL2_ttf (optional), SDL2_image (optional), nlohmann/json (fetched via CMake)
- Compile definitions: `VN_ENABLE_TTF`, `VN_ENABLE_IMAGE`, `BATTLE_ENABLE_TTF`, `BATTLE_ENABLE_IMAGE` (set automatically when libs are found)
- Build commands:
  ```
  cmake -S . -B build
  cmake --build build
  ```
- Run from project root (assets use relative paths that search `./`, `../`, `../../`):
  ```
  ./build/bin/vn_testing       # VN-only (chapter 0 story)
  ./build/bin/battle_testing   # Combat-only sandbox
  ./build/bin/demo             # Combined VN + combat demo (the main showcase)
  ```

## Three Executables

| Target | Entry point | Purpose |
|---|---|---|
| `vn_testing` | `src/main.cpp` | Standalone VN player. Loads `ch0.json`, plays dialogue with typewriter text, voice, backgrounds, icons. Space to advance. |
| `battle_testing` | `src/battle_main.cpp` | Standalone combat sandbox. CLI args: `<bossKey> <char1> [char2...]`. Has 3D camera (F=freeview, WASD/QE/arrows/wheel), turn order, abilities, sprites. |
| `demo` | `src/demo.cpp` | **Main demo / the actual game flow.** Integrates VN dialogue sequences into the combat loop. Scripted tutorial: intro dialogue -> battle with Miku+CupcakKe vs Lyoo boss -> triggered dialogue after key combat moments -> victory dialogue. This is the primary target. |

## What Each Executable Builds (source files + runtime assets)

### `vn_testing` (VN-only)
Source files compiled:
- `src/main.cpp` — entry point
- `src/window.cpp` / `src/window.h` — SDL2 window wrapper
- `src/game/vn_system.cpp` / `src/game/vn_system.h` — VN rendering engine
- `src/game/vn_script.cpp` / `src/game/vn_script.h` — JSON script loader

Assets loaded at runtime:
- `assets/vn/json/ch0.json` — the script it plays
- `assets/vn/backgrounds/ch0/*.jpg` — background images referenced by ch0.json
- `assets/vn/voices/ch0/*.wav` — voice clips referenced by ch0.json

Libraries linked: SDL2, nlohmann_json, (+ SDL2_ttf, SDL2_image if found)

### `battle_testing` (combat-only)
Source files compiled:
- `src/battle_main.cpp` — entry point
- `src/window.cpp` / `src/window.h`
- `src/game/battle_manager.cpp` / `.h` — battle orchestrator
- `src/game/battle_loader.cpp` / `.h` — JSON data loaders
- `src/game/turn_system.cpp` / `.h` — action value turn order
- `src/game/ability_system.cpp` / `.h` — ability effects
- `src/game/battle_ui.cpp` / `.h` — HUD drawing
- `src/game/camera_3d.cpp` / `.h` — 3D perspective camera
- `src/game/easing.cpp` / `.h` — animation math

Assets loaded at runtime:
- `assets/combat/characters.json` — party member stats
- `assets/combat/boss.json` — boss stats
- `assets/combat/abilities.json` — ability definitions
- `assets/combat/sprites/*.png` — character sprites
- `assets/combat/icons/*.png` — turn order icons

Libraries linked: SDL2, nlohmann_json, (+ SDL2_ttf, SDL2_image if found)

### `demo` (the main game — VN + combat combined)
Source files compiled: **everything from both above, but with `src/demo.cpp` as entry point instead**
- `src/demo.cpp` — entry point
- `src/window.cpp` / `src/window.h`
- ALL `src/game/*.cpp` / `src/game/*.h` files (vn_system, vn_script, battle_manager, battle_loader, turn_system, ability_system, battle_ui, camera_3d, easing)

Assets loaded at runtime: **all combat assets + all VN assets**
- Everything from `assets/combat/` (characters, boss, abilities, sprites, icons)
- `assets/vn/json/demo.json` + `demo_after_miku_first_skill.json` + `demo_after_miku_first_ultimate.json` + `demo_after_lyoo_attack_post_miku_ultimate.json` + `demo_boss_defeated.json`
- (does NOT load ch0.json — that's only for vn_testing)

Libraries linked: SDL2, nlohmann_json, (+ SDL2_ttf, SDL2_image if found)

**Note:** `src/main.cpp`, `src/battle_main.cpp`, and `src/demo.cpp` are never compiled together — each is the entry point for its own executable. All other source files are shared.

## Source Layout

```
src/
  main.cpp              # vn_testing entry point
  battle_main.cpp       # battle_testing entry point
  demo.cpp              # demo entry point (THE main game executable)
  window.h / .cpp       # SDL2 window wrapper (shared by all targets)
  game/
    vn_system.h / .cpp      # VN rendering engine (typewriter text, speaker box, icon, background, voice)
    vn_script.h / .cpp      # VN script loader (JSON -> ScriptEntry structs, entry types: Dialogue/Thought/Narration)
    battle_manager.h / .cpp # Core battle orchestrator (init, turn execution, HP tracking, win condition)
    battle_loader.h / .cpp  # JSON loaders for characters.json, boss.json, abilities.json
    turn_system.h / .cpp    # Speed-based action value turn order (like Honkai: Star Rail)
    ability_system.h / .cpp # Ability effect execution + presentation interaction stubs
    battle_ui.h / .cpp      # HUD rendering (turn order bar, boss HP header, character status cards)
    camera_3d.h / .cpp      # Perspective camera (world->screen projection, depth sorting, scale)
    easing.h / .cpp         # Animation math (lerp, clamp01, easeOutCubic, easeOutBounce, easeOutBack)
```

## Asset Layout

```
assets/
  combat/
    characters.json     # Party member definitions (key, title, class, spd/atk/hp, ability, ultimate)
    boss.json           # Boss definitions (lyoo, lyooPlotTwist, miku variants)
    abilities.json      # All ability definitions (type, targetRule, multiplier, interactionType)
    sprites/            # Character sprite PNGs (multi-frame horizontal strips, 140px per frame width)
      miku.png, lyoo.png, cupcakke.png
    icons/              # Small character icon PNGs for UI
      miku.png, lyoo.png, cupcakke.png
  vn/
    json/
      ch0.json                                    # Chapter 0 full VN script (19 entries)
      demo.json                                   # Demo intro dialogue
      demo_after_miku_first_skill.json            # Triggered after Miku's first skill use
      demo_after_miku_first_ultimate.json         # Triggered after Miku's first ultimate
      demo_after_lyoo_attack_post_miku_ultimate.json  # Triggered after Lyoo retaliates
      demo_boss_defeated.json                     # Victory dialogue
    backgrounds/ch0/    # VN background images (0.jpg, 1.jpg)
    voices/ch0/         # Voice clips (0.wav through 18.wav)
```

## Key Architecture Concepts

### Combat System (Honkai: Star Rail-inspired)
- **Action Value turn order**: each actor has `baseActionValue = 10000 / spd`. The actor with lowest `currentActionValue` goes next. After acting, their AV resets.
- **Characters** have: ability (basic skill), ultimate (costs ultimatePoints charges), HP, ATK, SPD
- **Bosses** have: one ability, HP, ATK, SPD. Boss turns execute automatically via `processAutomaticTurns()`
- **Player turns** advance via `executePlayerTurn()` (Space key). The game auto-selects the ability (basic vs ultimate when charged)
- **Abilities** have types (Attack/Heal/Buff/Debuff), target rules, multipliers, and interaction types (Rhythm/Parry/None)
- **Ultimate charge**: characters gain 1 point per action. When `ultimateCharge >= ultimatePoints`, ultimate fires and grants an extra turn

### Visual Novel System
- Typewriter text rendering with configurable speed
- Speaker name box, character icon (supports animated sprite strips), background images
- Voice playback with optional auto-advance on voice end
- Script entries have types: Dialogue (normal), Thought (italic/inner voice), Narration (no speaker box)
- `vn::showLine()` is the main API to display a line

### Demo Integration (demo.cpp)
- State machine alternating between VN dialogue sequences and combat
- Dialogue triggers at specific combat events (Miku's first skill, Miku's first ultimate, Lyoo's retaliation, boss defeat)
- `dialogueInProgress` flag pauses combat while dialogue plays
- `startDialogueSequence()` switches to a dialogue set; Space advances lines; extra Space after last line closes dialogue and resumes combat

### 3D Camera
- Pseudo-3D perspective projection (Camera3D with pitch/yaw/focal length)
- Painter's algorithm depth sorting for sprites
- Camera intro animation on turn transitions (easeOutCubic)
- Subtle yaw oscillation when idle on a character's turn
- Freeview mode (F key) for debugging

### Rendering
- SDL2 hardware-accelerated renderer (VSYNC enabled)
- Textured floor tiles via `SDL_RenderGeometry` (quad rendering)
- Multi-frame sprite animation (horizontal strip, 140px frame width, 0.15s per frame)
- Fallback colored rectangles when sprites aren't available
- Focused character gets 1.13x scale + yellow highlight ring

## Characters

| Key | Title | Class | Role |
|---|---|---|---|
| lyoo | Lyoo | healer | Party healer (also a boss variant) |
| miku | Miku | dps | Main DPS, protagonist |
| cupcakke | CupcakKe | dps | Heavy hitter, slow |
| luotianyi | 洛天依 | buffer | Buffer (defined but not yet in demo) |

## Conventions

- Namespaces: `vn::` (visual novel), `battle::` (combat core), `battle::turn::`, `battle::ability::`, `battle::ui::`, `battle::easing::`, `battle::loader::`
- JSON data drives all game content (characters, bosses, abilities, VN scripts)
- Asset paths are relative to project root; `resolvePath()` tries `./`, `../`, `../../` for flexibility when running from build/
- Window is always 1280x720
- `Window` class handles SDL init/cleanup; all three executables share it
- No test framework currently in use
- Platform: primarily Linux (Arch), also supports macOS and Windows (vcpkg)
