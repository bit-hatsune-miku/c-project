# Hatsune Miku: Our Underground BIT Idol

This repository contains the final course-project code and assets for a story-heavy
rhythm/RPG built in C++ with SDL2, OpenGL, and RmlUi.

The project is meant to demonstrate three things clearly:

1. What the game is.
2. How the runtime systems are organized.
3. How to build and run the final Windows deliverable.

## Game Summary

`Hatsune Miku: Our Underground BIT Idol` is a campus-themed boss-rush game with a
visual-novel story layer. The player follows Miku through an underground idol
competition, recruits allies, fights themed boss encounters, and reaches a finale
that turns the comedy-heavy early game into a more explicit story about ambition,
identity, and following one's dreams.

Core deliverable features:

- main menu, settings, save/load, and pause flows
- visual-novel story chapters with battle handoff
- turn-based boss battles with interactive attack prompts
- finale route with `lyoo_plot_twist`, `miku_plot_twist`, and credits

## Runtime Structure

High-level gameplay flow:

```text
Main menu
  -> story chapter
  -> pre-battle VN scene
  -> battle / boss presentation
  -> post-battle VN scene
  -> next story chapter
  -> finale route
  -> credits
```

High-level system flow:

```text
src/main.cpp
  -> owns app state, screen routing, saves, story progression
  -> launches VN scenes or battle sessions

src/game/vn/*
  -> loads JSON script entries
  -> renders dialogue / portraits / backgrounds
  -> plays voice + BGM

src/game/core/*
  -> loads battle data from JSON
  -> resolves turn order, actions, buffs, boss phases, and battle outcomes

src/game/app_battle_session.*
  -> runs the main RmlUi/OpenGL battle experience used by the shipped app

assets/*
  -> stores story scripts, battle data, art, UI, music, and voice assets
```

## Important Source Files

- `CMakeLists.txt`
  Main build configuration, dependency wiring, and submission install layout.
- `src/main.cpp`
  Top-level application loop, story progression, screen switching, and battle
  handoff.
- `src/window.*`
  SDL window ownership and backend switching between SDL renderer mode and
  OpenGL mode.
- `src/game/core/battle_loader.*`
  JSON loading for battles, bosses, characters, and abilities.
- `src/game/core/battle_manager.*`
  Main combat state and action-resolution logic.
- `src/game/app_battle_session.*`
  Main battle session wrapper used by the shipped game.
- `src/game/vn/*`
  Visual-novel presentation system and story-script parsing.
- `src/platform/path_resolution.h`
  Cross-platform runtime asset lookup and font/audio path resolution.

For a deeper explanation of the rendering split and file ownership, see
[docs/architecture.md](docs/architecture.md).

## Controls

- Mouse hover/click works across menu, pause, settings, load, and credits flows
- `W` / `S` or arrow keys move through menu items
- `Enter` / `Space` confirms the current selection
- `Escape` opens pause during story/battle and backs out of confirmation flows
- `F11` toggles fullscreen

## Build Requirements

Common requirements:

- CMake `3.16+`
- C++17 compiler
- SDL2
- SDL2_image
- SDL2_ttf
- opusfile

If CMake reports that `build/CMakeCache.txt` was created in a different source
directory, delete the generated build folder and configure again.

## Linux Build

Debian/Ubuntu example:

```bash
sudo apt update
sudo apt install build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libopusfile-dev
cmake -S . -B build
cmake --build build
./build/bin/OurUndergroundBITIdol
```

## Windows Build (Final Submission Path)

The intended submission path is a **Release** build with **static** vcpkg
dependencies so the packaged game does not rely on copied third-party DLLs.

### 1. Install tools

- Visual Studio 2022 with `Desktop development with C++`
- CMake
- vcpkg

### 2. Install dependencies with the static triplet

```powershell
vcpkg install sdl2:x64-windows-static sdl2-image:x64-windows-static sdl2-ttf:x64-windows-static opusfile:x64-windows-static
```

### 3. Configure a Release build

```powershell
cmake -S . -B build-win ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

### 4. Build the shipped executable

```powershell
cmake --build build-win --config Release --target OurUndergroundBITIdol
```

### 5. Stage the portable package

```powershell
cmake --build build-win --config Release --target stage_submission
```

The staged folder should contain:

```text
build-win/submission/
├── OurUndergroundBITIdol.exe
├── README.md
└── assets/
```

### 6. Run

Launch:

```powershell
build-win/submission/OurUndergroundBITIdol.exe
```

## Packaging Notes For Submission

- The portable Windows package should contain only:
  - `OurUndergroundBITIdol.exe`
  - `assets/`
  - short run instructions / README
- Preview, test, and demo executables are development tools and are not part of
  the submission package.
- If the full-fidelity package exceeds `250 MB`, upload it to **Baidu Netdisk**
  and submit the permanent link together with the source zip.

## What To Smoke-Test Before Submission

Release build only:

1. Main menu and front-end UI
2. One ordinary battle
3. Finale route through credits
4. Chinese font fallback and mixed-language rendering
5. Battle voice, BGM, and credits song playback
6. Fullscreen/windowed behavior and general frame pacing on Windows

## Additional Documentation

- [docs/architecture.md](docs/architecture.md)
- [docs/combat-input-design.md](docs/combat-input-design.md)
- [docs/windows-build-handoff.md](docs/windows-build-handoff.md)
