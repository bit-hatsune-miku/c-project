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

- Close window button to quit
- Press Escape to quit

## Features

- 800x600 graphical window with blue background
- Event handling (window close, keyboard input)
- Hardware-accelerated rendering
- Simple and clean C++ code for beginners

## Project Structure

```
├── CMakeLists.txt         - Cross-platform build configuration
├── src/
│   ├── main.cpp           - App entry point, main loop, and shared UI helpers
│   ├── main_menu.cpp      - Main menu rendering and input handling
│   ├── pause_menu.cpp     - Pause menu and exit confirmation rendering/input
│   ├── menu_shared.h      - Shared app/menu types and cross-file declarations
│   ├── settings.cpp       - Settings screen controller, rendering, and input
│   ├── settings.h         - Settings screen controller interface
│   ├── window.cpp         - SDL window/renderer wrapper implementation
│   ├── window.h           - SDL window/renderer wrapper interface
│   ├── battle_main.cpp    - Battle test entry point
│   ├── demo.cpp           - Demo entry point
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
