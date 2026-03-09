# Quick Start

## Fresh clone

```bash
cmake -S . -B build
cmake --build build
./build/bin/vn_testing
```

## If CMake complains about `CMakeCache.txt`

That means the existing `build/` directory was generated on a different machine or in a different path. Remove it and reconfigure:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
```

## Notes

- Do not commit the generated `build/` directory.
- Optional features enable themselves automatically when `SDL2_ttf` and `SDL2_image` are installed.
