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

For Opus audio playback support, also install the opusfile development package:

sudo apt install libopusfile-dev

2. Configure + build:

cmake -S . -B build
cmake --build build

3. Run:

./build/bin/OurUndergroundBITIdol

## Windows setup (recommended: vcpkg)

1. Install Visual Studio (Desktop development with C++) and CMake.
2. Install vcpkg and then SDL2:

vcpkg install sdl2:x64-windows opusfile:x64-windows

3. Configure + build from project folder:

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

4. Run:

Run `OurUndergroundBITIdol.exe` from the generated build output directory.

## macOS setup

1. Install tools and SDL2:

brew install cmake sdl2

For Opus audio playback support, also install:

brew install opusfile

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

- Front-end shell for main menu, story, pause, load, and settings flows
- Scripted story playback with typewriter text plus voice-volume and text-speed settings
- Mouse and keyboard driven navigation across both the modern front-end UI and legacy SDL flows
- Manual save slots plus autosave support for story progress and player settings
- Mixed rendering stack: RmlUi on SDL2/OpenGL GL3 for the front-end shell, with some gameplay and legacy screens still using SDL2 renderer graphics
- Separate executable targets for the main app, the default RmlUi battle test, the legacy SDL battle test, and the scripted demo wrapper
- Modular folder layout that keeps platform glue, UI shell code, runtime systems, and content data separate

## Project Structure / Architecture

The codebase is split by responsibility so platform glue, UI shell code, runtime systems, and content can evolve separately without exposing all game-specific details in one place.

High-level layout:

- `src/` contains the application entry points, rendering integration, front-end UI controllers, gameplay runtime, and persistence code
- `assets/` stores data-driven content, UI markup/styles, art, audio, and other runtime resources
- `docs/` holds internal design notes and architecture references
- `build/` is the generated output directory created by CMake

### Rendering model

The project currently uses a mixed UI/rendering stack:

- The modern front-end shell uses **RmlUi** rendered through **SDL2 + OpenGL GL3**
- That integration lives in `src/graphics/`, where the front-end session manages document stacks and `rmlui_sdl_gl_renderer.*` bridges RmlUi to the SDL/OpenGL window
- Some gameplay paths and older screens still render through the **SDL2 renderer** directly
- In practice, the app switches between the RmlUi/OpenGL front-end path and SDL-rendered gameplay/legacy paths depending on screen state

### Source layout

- `src/main.cpp` wires together the main app flow, including front-end UI, story progression, save/load transitions, and battle handoff
- `src/window.*` wraps SDL window creation plus renderer/OpenGL mode switching
- `src/graphics/` contains the RmlUi front-end layer:
  - document/controller interfaces
  - front-end screen controllers for menu, story, pause, load, and settings
  - the front-end session stack manager
  - the SDL2/OpenGL GL3 render bridge used by RmlUi
- `src/GameMenu/` holds SDL-rendered menu and overlay flows still used by legacy or non-Rml paths
- `src/Settings/` keeps the older standalone settings controller used by SDL-rendered flows
- `src/platform/` contains platform-facing helpers such as asset-path resolution and mixed-font text fallback rules
- `src/game/` contains the runtime systems:
  - `vn/` for script parsing and visual novel presentation state
  - `save/` for save-file serialization and slot discovery
  - `core/` for battle rules, flow, loading, and shared combat state
  - `render/` for battle scene rendering, HUD, camera, feedback, and asset-loading helpers
  - `presentation/` for specialized battle presentation sequences and supporting runtime logic
  - `demo/` plus shared session files for scripted demo/tutorial flows
  - `audio/` for lightweight playback helpers
  - `ui/` for battle-session UI state and document binding helpers
- Top-level battle/demo entry files such as `battle_main.cpp`, `rmlui_battle_main.cpp`, `rmlui_battle_smoke.cpp`, and `demo.cpp` package the shared systems into different executables

### Public-facing file map

This is an intentionally broad map of the repository, meant to show ownership and integration points without documenting every game-specific implementation detail.

```text
├── CMakeLists.txt                - Cross-platform build configuration and executable target setup
├── src/
│   ├── main.cpp                  - Main application loop and screen-to-screen coordination
│   ├── demo.cpp                  - Standalone wrapper for the shared demo flow
│   ├── battle_main.cpp           - Legacy SDL battle test entry point
│   ├── rmlui_battle_main.cpp     - Default RmlUi battle entry point
│   ├── rmlui_battle_smoke.cpp    - Lightweight RmlUi smoke-test entry point
│   ├── window.cpp
│   ├── window.h                  - SDL window plus renderer/OpenGL mode management
│   ├── GameMenu/                 - SDL-rendered menu, pause, load, and confirmation flows
│   ├── Settings/                 - Legacy settings controller and related UI plumbing
│   ├── graphics/                 - RmlUi front-end controllers, session stack, and SDL2_GL3 integration
│   ├── platform/                 - Asset path and text/font fallback helpers
│   └── game/
│       ├── app_battle_session.*  - App-facing battle session for the RmlUi/main-app path
│       ├── battle_session_core.* - Shared battle runtime shell
│       ├── demo_battle_session.* - Shared demo battle session used by multiple entry points
│       ├── audio/                - Lightweight audio playback helpers
│       ├── core/                 - Combat rules, flow control, loading, and shared state
│       ├── demo/                 - Scripted demo/tutorial wrappers around combat
│       ├── presentation/         - Battle presentation sequences and supporting runtime pieces
│       ├── render/               - Battle rendering, HUD, camera, feedback, and asset helpers
│       ├── save/                 - Save data models, serialization, and slot/file management
│       ├── ui/                   - Battle-session UI state and document-binding helpers
│       └── vn/                   - Story script loading and VN presentation runtime
├── assets/
│   ├── combat/                   - Combat-facing data, sprites, icons, and audio resources
│   ├── rmlui/                    - RmlUi markup, stylesheets, fonts, and shared UI assets
│   │   └── front_ui/             - Front-end menu/story/pause/load/settings documents and shared styles
│   └── vn/                       - Story JSON, backgrounds, portraits, UI art, and voice assets
├── docs/                         - Internal design notes and architecture writeups
└── build/                        - Build output directory (generated)
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
