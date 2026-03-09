# Camera Tuning Guide

The battle_testing application now supports interactive camera parameter tuning via keyboard and mouse input.

## Controls

- **W/S**: Move camera forward/backward (posY)
- **A/D**: Move camera left/right (posX)
- **Space**: Move camera up (increase posZ)
- **Shift**: Move camera down (decrease posZ)
- **Left/Right Arrows**: Rotate camera horizontally (yaw)
- **Up/Down Arrows**: Rotate camera vertically (pitch)
- **ESC** or close window: Exit and print final camera parameters

## Current Default Parameters

These are the starting camera settings:

```cpp
// src/battle_main.cpp, main()
camera.posX = 0.0f;           // Horizontal position (world units)
camera.posY = -150.0f;        // Depth position (world units, away from viewer)
camera.posZ = 200.0f;         // Vertical height (world units)
camera.pitchDegrees = 28.0f;  // Vertical rotation angle
camera.yawDegrees = 0.0f;     // Horizontal rotation angle
camera.focalLength = 300.0f;  // Perspective focal length
```

## Entity World Positions

Entities are placed at:
- **Characters**: Y=300 (world), spaced horizontally by **400 units** (increased for better visibility)
- **Boss**: Y=720 (world), centered horizontally
- **Floor**: Z=0 (ground plane) - **now camera-aware and moves with you!**

All entities currently spawn at Z=0 (ground level, no height variation).

## Visual Feedback

The debug HUD displays live camera values in the top-left corner:
```
Cam: (posX, posY, posZ) | Pitch: angle° Yaw: angle° | Focal: focal_length
```

Focused entity (next turn actor) has a yellow highlight ring.
Entities are sorted by depth and rendered back-to-front (painter's algorithm).
Floor grid now transforms with camera movement and rotation!

## Aesthetic Goals

When tuning, look for:
1. **Boss noticeably larger than characters** (should be ~1.28x zoom + perspective scale difference)
2. **Characters scale gradually from left to right** (trapezoid arrangement visible)
3. **Floor trapezoid visible** (perspective lines converge toward vanishing point)
4. **No clipping** (all entities on screen without culling)
5. **Character silhouettes identifiable** (not too small, not too large)

## Finding Good Parameters

1. **Adjust depth (posY)**: Move camera backward/forward to fit lineup in frame
   - Too close (small posY): entities huge, floor tiny
   - Too far (large posY): entities tiny, floor dominates
   
2. **Adjust height (posZ)**: Set camera height relative to character baseline
   - Current default (200): looks down at characters
   - Increase: look more downward
   - Decrease: look more level
   
3. **Adjust pitch (mouse)**: Angle of view affects perspective
   - Lower pitch (5-15°): shallower perspective, less vertical stretch
   - Higher pitch (40-85°): steeper perspective, more "looking down"
   
4. **Adjust horizontal (posX)**: Shift view left/right to center the lineup

## When Satisfied

1. Close the window or press ESC
2. Copy the final camera parameters from the console output:
   ```
   [Battle] Final camera settings:
     posX: X.X
     posY: Y.Y
     posZ: Z.Z
     pitchDegrees: D.D
     focalLength: F.F
   ```
3. Update these values in `src/battle_main.cpp` in the `main()` function
4. Rebuild with `make -C build battle_testing`

## Advanced Tuning

For more precise control, edit `src/battle_main.cpp` directly:

```cpp
// Line ~250 in main()
const float camMovementSpeed = 15.0f;      // WASD movement distance per frame
const float pitchSensitivity = 0.3f;       // Mouse pitch sensitivity
```

Increase movement speed for coarse adjustments, decrease for fine-tuning.

## Camera Math Reference

The camera uses a 3D coordinate system:
- **X-axis**: Left/Right (world units)
- **Y-axis**: Forward/Back (viewing depth)
- **Z-axis**: Up/Down (vertical height)

Transformation pipeline:
1. World point → Camera space (translate by camera position)
2. Rotate by pitch around X-axis
3. Perspective projection (focal length divides by depth)
4. Screen space (add screen center offset, invert Z for vertical positioning)

See `src/game/camera_3d.cpp` for implementation details.
