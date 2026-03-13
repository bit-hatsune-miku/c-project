#ifndef CAMERA_3D_H
#define CAMERA_3D_H

#include <SDL2/SDL.h>
#include <cmath>

namespace battle {

class Camera3D {
public:
    Camera3D();

    // World position
    float posX = 0.0f;
    float posY = -150.0f;
    float posZ = 200.0f;

    // Rotation (pitch and yaw in degrees)
    float pitchDegrees = 28.0f;
    float yawDegrees = 0.0f;

    // Optics
    float focalLength = 300.0f;

    // Screen reference
    int screenCenterX = 640;
    int screenCenterY = 360;

    // Transform world point to screen
    SDL_FPoint worldToScreen(float worldX, float worldY, float worldZ) const;

    // Get depth (for sorting and scale)
    float getDepth(float worldX, float worldY, float worldZ) const;

    // Get perspective scale
    float getPerspectiveScale(float worldX, float worldY, float worldZ) const;

private:
    float pitchRadians() const;
};

} // namespace battle

#endif // CAMERA_3D_H
