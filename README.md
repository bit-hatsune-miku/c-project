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

./build/bin/vn_testing

## Windows setup (recommended: vcpkg)

1. Install Visual Studio (Desktop development with C++) and CMake.
2. Install vcpkg and then SDL2:

vcpkg install sdl2:x64-windows

3. Configure + build from project folder:

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

4. Run:

Run `vn_testing.exe` from the generated build output directory.

## macOS setup

1. Install tools and SDL2:

brew install cmake sdl2

2. Configure + build:

cmake -S . -B build
cmake --build build

3. Run:

./build/bin/vn_testing

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
- Hardware-accelerated rendering
- Separate executable targets for the main VN app, battle test, and demo
- Modular folder layout for shared menu code, settings UI, and gameplay systems

## Project Structure

```
├── CMakeLists.txt         - Cross-platform build configuration
├── src/
│   ├── main.cpp           - App entry point, main loop, and shared UI helpers
│   ├── window.cpp         - SDL window/renderer wrapper implementation
│   ├── window.h           - SDL window/renderer wrapper interface
│   ├── battle_main.cpp    - Battle test entry point
│   ├── demo.cpp           - Demo entry point
│   ├── GameMenu/
│   │   ├── main_menu.cpp          - Main menu rendering and input handling
│   │   ├── pause_menu.cpp         - Pause menu rendering and input handling
│   │   ├── exit_to_main_menu.cpp  - Pause-menu exit confirmation overlay and input
│   │   └── menu_shared.h          - Shared app/menu types and cross-file declarations
│   ├── Settings/
│   │   ├── settings.cpp           - Settings screen controller, rendering, and input
│   │   └── settings.h             - Settings screen controller interface
│   └── game/
│       ├── vn_system.*    - Visual novel runtime and rendering
│       ├── vn_script.*    - VN script loading/parsing
│       ├── battle_*.*     - Battle loader, manager, and UI systems
│       ├── ability_system.* - Combat ability definitions and logic
│       ├── turn_system.*  - Turn order and turn flow logic
│       ├── camera_3d.*    - 3D camera helpers for battle scenes
│       └── easing.*       - Shared easing/animation helpers
├── assets/                - VN art, audio, combat data, and sprites
├── docs/                  - Design notes and rough planning docs
└── build/                 - Build output directory (generated)
```

## What This Project Demonstrates

- SDL2 initialization and window creation
- Event handling (quit events, keyboard input)
- Rendering with hardware acceleration
- Proper resource cleanup
- CMake for cross-platform builds
