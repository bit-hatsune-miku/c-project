# SDL2 Beginner Project (C++)

A simple C++ SDL2 application that opens a graphical window with an interactive interface.

If CMake reports that `build/CMakeCache.txt` was created in a different directory, remove the generated build folder and configure again:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
```

## Linux setup (Debian/Ubuntu)

1. Install tools and SDL2:

sudo apt update
sudo apt install build-essential cmake libsdl2-dev

2. Configure + build:

cmake -S . -B build
cmake --build build

3. Run:

./build/bin/OurUndergroundBITIdol

## Windows setup (recommended: vcpkg)

1. Install Visual Studio (Desktop development with C++) and CMake.
2. Install vcpkg and then SDL2:

vcpkg install sdl2:x64-windows

3. Configure + build from project folder:

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

4. Run:

Run `OurUndergroundBITIdol.exe` from the generated build output directory.

## macOS setup

1. Install tools and SDL2:

brew install cmake sdl2

2. Configure + build:

cmake -S . -B build
cmake --build build

3. Run:

./build/bin/OurUndergroundBITIdol

## Controls

- Mouse hover/click works across the main menu, pause menu, settings, and exit confirmation
- `W` / `S` or arrow keys move through menu items
- `Enter` / `Space` confirms the current selection
- `Escape` opens the pause menu during story playback and backs out of the exit confirmation
- `F11` toggles fullscreen
- Close window button quits the app

## Features

- Visual novel style main menu, pause menu, settings screen, and exit confirmation flow
- Scripted story playback with typewriter text and voice-volume/text-speed settings
- Mouse and keyboard driven menu navigation
- Manual save slots plus autosave support for story progress, chapter position, and player settings
- In-game load menu for resuming saves and deleting manual save files from the UI
- Hardware-accelerated rendering
- Separate executable targets for the main VN app, the default RmlUi battle test, the legacy SDL battle test, and the scripted demo
- Modular folder layout for shared menu code, settings UI, and gameplay systems

## Project Structure / Architecture

The codebase is split by responsibility so UI shell code, gameplay systems, and data assets can evolve separately:

- `src/` contains the runtime code, with top-level entry points and feature folders for menus, settings, saves, VN flow, battle logic, rendering, and battle presentations
- `assets/` stores JSON-driven content plus art, sprites, UI files, backgrounds, voice clips, and battle audio
- `docs/` holds design notes and architecture references for larger systems such as battle flow and state management
- `build/` is the generated output directory created by CMake

Inside `src/`, the main layers are:

- `GameMenu/` for shell navigation such as the main menu, pause flow, load menu, and confirmation overlays
- `Settings/` for the standalone settings screen and related state
- `game/vn/` for visual novel script parsing and runtime presentation
- `game/save/` for save-file serialization, autosave/manual save handling, and save slot discovery
- `game/core/` for combat rules, turn order, battle loading, and shared gameplay state
- `game/render/` for battle scene rendering, HUD drawing, camera staging, and combat feedback
- `game/presentation/` for interactive ability sequences and special battle presentation logic
- `game/demo/` plus `demo_battle_session.*` for scripted tutorial/demo battle flow
- `game/app_battle_session.*` and the top-level `*_main.cpp` files for wiring the reusable systems into executable entry points

```
├── CMakeLists.txt         - Cross-platform build configuration and executable target setup
├── src/
│   ├── main.cpp                   - Main application loop for menu, story, save/load, and battle transitions
│   ├── demo.cpp                   - Standalone wrapper around the shared scripted battle demo
│   ├── battle_main.cpp            - Legacy SDL battle test entry point
│   ├── rmlui_battle_main.cpp      - Default RmlUi battle entry point
│   ├── rmlui_battle_smoke.cpp     - Lightweight RmlUi smoke-test entry point
│   ├── window.cpp                 - SDL window/renderer wrapper implementation
│   ├── window.h                   - SDL window/renderer wrapper interface
│   ├── GameMenu/
│   │   ├── main_menu.cpp          - Main menu rendering and input handling
│   │   ├── load_menu.cpp          - Save-slot browser, load flow, and delete confirmation UI
│   │   ├── pause_menu.cpp         - Shared pause menu rendering and input for story and battle
│   │   ├── exit_to_main_menu.cpp  - Exit/overwrite confirmation overlays and shared prompt logic
│   │   └── menu_shared.h          - Shared app state, screen enums, and UI helper declarations
│   ├── Settings/
│   │   ├── settings.cpp           - Settings screen controller, rendering, and input
│   │   └── settings.h             - Settings screen controller interface
│   ├── platform/
│   │   └── path_resolution.h      - Cross-platform asset path resolution helpers
│   └── game/
│       ├── app_battle_session.*   - Battle session used by the main app / RmlUi path
│       ├── battle_session_core.*  - Shared battle runtime shell used by sessions
│       ├── demo_battle_session.*  - Shared demo battle session used by the app and demo target
│       ├── core/                  - Battle manager, loader, turn flow, abilities, and easing helpers
│       ├── demo/                  - Scripted narrative/tutorial flow wrapped around combat
│       ├── presentation/          - Ability presentations, minigames, splash art, and presentation registry
│       ├── render/                - Battle renderer, HUD, camera, feedback, and asset loading
│       ├── save/                  - Save data models, JSON serialization, slot listing, and file management
│       ├── audio/                 - One-shot SFX and BGM playback helpers
│       └── vn/                    - VN runtime state and script parsing/loading
├── assets/
│   ├── combat/                    - Character stats, abilities, battle definitions, sprites, icons, and battle audio
│   ├── rmlui/                     - RmlUi HUD markup, styles, fonts, and icons
│   └── vn/                        - Chapter JSON, backgrounds, portraits/icons, UI images, and voice lines
├── docs/                          - Design notes, migration docs, and deeper architecture writeups
└── build/                         - Build output directory (generated)
```

## What This Project Demonstrates

- SDL2 initialization and window creation
- Event handling (quit events, keyboard input)
- Rendering with hardware acceleration
- Proper resource cleanup
- CMake for cross-platform builds

## Battle Entry Points

- `./build/bin/battle_testing` launches the default RmlUi battle experience.
- `./build/bin/battle_testing_legacy` launches the old SDL battle test directly.
- `./build/bin/OurUndergroundBITIdol battle mode` forwards into the default battle executable.

## Additional Documentation

- [docs/combat-input-design.md](docs/combat-input-design.md) documents the planned Cupcakke and Lyoo input timing, scoring formulas, and balancing knobs.
