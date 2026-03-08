# 2.5D Combat System - Quick Start Guide

You now have a working SDL2 + OpenGL boilerplate for your 2.5D combat system!

## Project Structure

```
src/
├── main.cpp           # Main loop, camera updates
├── window.h/cpp       # SDL2 + OpenGL window setup
├── renderer.h/cpp     # Scene rendering (ground, effects)
├── camera.h/cpp       # 3D camera with smooth panning
├── sprite.h/cpp       # Billboard sprites (2D facing camera)
├── shader.h/cpp       # Shader management
├── glm_setup.h        # GLM math library setup
└── stb_image_impl.cpp # Image loading (for pixel art textures)
```

## Key Components Explained

### 1. **Camera System** (`camera.h/cpp`)
- Controls the 3D view
- `pan()` - smooth camera movement between turns
- `lookAt()` - focus on target
- `setPosition()` - absolute camera positioning

```cpp
camera.pan(glm::vec3(2.0f, 0.0f, 0.0f)); // Smooth pan to the side
```

### 2. **Billboard Sprites** (`sprite.h/cpp`)
- 2D sprites that always face the camera
- Perfect for pixel art characters
- `setPosition()` - place in 3D space
- `loadTexture()` - load PNG/JPG

```cpp
Sprite enemy;
enemy.loadTexture("assets/enemy.png");
enemy.setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
```

### 3. **Shaders** (`shader.h/cpp`)
- Currently using basic passthrough
- Ready for pixel art effects, outlines, etc.
- Modify the shader strings in `renderer.cpp` to add effects

### 4. **Main Loop** (`main.cpp`)
- Handles timing (60 FPS cap)
- Input polling (ESC to quit)
- Smooth camera movement demo

## Next Steps (In Order)

### 1. Load a Sprite (5 min)
- Create/find a 64x64 PNG pixel art sprite
- Put it in `assets/` folder
- Add to main.cpp:
```cpp
Sprite player;
player.loadTexture("assets/character.png");
player.setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
```

### 2. Add Attack Animation (10 min)
- Store multiple sprite frames
- Cycle through them on attack
- Add frame index and timer

### 3. Camera Shake Effect (5 min)
- Add small random offsets to camera pos on attack
- Decay over 200ms

### 4. Basic UI (10 min)
- Print HP bars (overlay 2D text on 3D scene)
- Or use a separate 2D overlay layer

### 5. Multiple Enemies (15 min)
- Create `std::vector<Sprite>` for enemies
- Position them around the scene
- Render each with different positions

## Building & Running

```bash
cd /home/lyes/GitHub/c-project
mkdir -p build && cd build
cmake ..
cmake --build .
./bin/combat_2d5
```

## Tips for Fast Progress

1. **Start simple**: Get one sprite rendering before adding complexity
2. **Use placeholder art**: Don't spend time on art, use simple colored rectangles initially
3. **Test frequently**: Rebuild often to catch issues early
4. **Focus on camera movement**: This is your visual "wow" factor
5. **Delegate effects**: Save advanced shaders for later

## Common Tweaks

### Adjust Camera Distance
In `main.cpp`, change:
```cpp
Camera camera(glm::vec3(0.0f, 2.0f, 5.0f));
                                        ^ distance from scene
```

### Adjust Field of View
In `camera.cpp`, modify:
```cpp
glm::radians(45.0f) // increase for wider view
```

### Change Background Color
In `window.cpp`:
```cpp
glClearColor(0.1f, 0.1f, 0.15f, 1.0f); // R, G, B, Alpha
```

## Copilot Tips

- Ask Copilot: "Add a sprite animation system with frame timing"
- Ask: "Create a camera shake effect"
- Ask: "How do I render text in OpenGL?"
- Ask: "Add a simple particle system for attacks"

You're ready to code! Good luck! 🚀
