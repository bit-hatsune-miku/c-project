# Windows Build Handoff

This note is for the final Windows submission build.

The goal is simple:

1. build a real Windows `Release` executable
2. stage the portable submission folder
3. smoke-test the shipped game on Windows

Do not treat this as a general development session. The only priority is a
Windows-runnable submission package.

## Start Clean

Use a **fresh terminal clone** on Windows.

Do **not** reuse the old Windows worktree that created replica branches and
stash confusion.

Do **not** restore old stashes.

Do **not** create a side branch unless the build fails and isolation becomes
necessary.

Do **not** rely on the old GitHub Desktop checkout for the final submission
build.

Current required commit:

```text
branch: main
commit: cc3ff48
message: docs: add windows build handoff
```

Recommended clone flow:

```powershell
git clone git@github.com:bit-hatsune-miku/c-project.git
cd c-project
git checkout main
git lfs install
git lfs pull
git lfs ls-files
git log -1
git status
```

Expected:

- `git log -1` shows `cc3ff48`
- `git status` is clean
- `git lfs ls-files` lists the tracked large audio files
- missing `.wav` assets are now present as real media files instead of small
  pointer text files

## Toolchain

Install:

- Visual Studio 2022
- workload: `Desktop development with C++`
- CMake
- vcpkg
- Git LFS

Install the static dependencies:

```powershell
vcpkg install sdl2:x64-windows-static sdl2-image:x64-windows-static sdl2-ttf:x64-windows-static opusfile:x64-windows-static
```

## Configure

From the repo root:

```powershell
cmake -S . -B build-win ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

## Build

Build the final submission staging target:

```powershell
cmake --build build-win --config Release --target stage_submission
```

This should produce:

```text
build-win/submission/
├── OurUndergroundBITIdol.exe
├── README.md
└── assets/
```

## Run

Launch:

```powershell
build-win/submission/OurUndergroundBITIdol.exe
```

## Smoke Test

Test the Windows `Release` build only.

Minimum required checks:

1. Boot to main menu
2. Start story
3. Clear one ordinary boss
4. Clear the finale route through credits
5. Confirm:
   - Chinese text renders correctly
   - battle voice and BGM play
   - credits song and subtitles play
   - fullscreen/windowed behavior is acceptable
   - no obvious frame pacing disaster appears

If Windows still looks worse than Linux, investigate only:

- vsync / frame pacing behavior
- driver/backend differences
- first-load hitches

Do **not** reopen feature work.

## Packaging Notes

The staged package currently keeps full-fidelity assets.

That means the package is expected to be much larger than `250 MB`.

So the practical delivery path is:

1. keep the full Windows submission package
2. upload it to **Baidu Netdisk**
3. include the permanent link in the final submission

Do not cut major assets unless the teacher explicitly refuses the Netdisk path.

## What Not To Do

- Do not reuse old stashes
- Do not let the agent create a replica branch of `main`
- Do not use the old GitHub Desktop checkout as the submission worktree
- Do not switch to gameplay/content work
- Do not waste time on new features
- Do not ship preview/demo executables with the final package

## Suggested Prompt For Windows Codex

Use this directly if needed:

```text
Work from a fresh terminal clone of org/main. Do not reuse stashes, do not use the old GitHub Desktop checkout, and do not create a replica branch unless the build fails. Run git lfs install and git lfs pull before building so the large audio assets are actually present. Then build a Windows Release package with MSVC + vcpkg static using the stage_submission target. Verify the packaged exe runs from build-win/submission with assets beside it. If build issues appear, fix only Windows build/package blockers, not gameplay features.
```
