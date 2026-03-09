#include "camera_3d.h"

namespace battle {

namespace {
constexpr float kFocalUiScale = 0.01f;
}

Camera3D::Camera3D() = default;

float Camera3D::pitchRadians() const {
    return pitchDegrees * 3.14159265f / 180.0f;
}

SDL_FPoint Camera3D::worldToScreen(float worldX, float worldY, float worldZ) const {
    // Translate to camera space
    const float px = worldX - posX;
    const float py = worldY - posY;
    const float pz = worldZ - posZ;

    // Rotate by yaw around Z-axis (horizontal rotation)
    const float yaw = yawDegrees * 3.14159265f / 180.0f;
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);
    
    const float x1 = px * cosYaw + py * sinYaw;
    const float y1 = -px * sinYaw + py * cosYaw;
    const float z1 = pz;

    // Rotate by pitch around X-axis (vertical rotation)
    const float theta = pitchRadians();
    const float cosTheta = std::cos(theta);
    const float sinTheta = std::sin(theta);

    const float xc = x1;
    const float yc = y1 * cosTheta + z1 * sinTheta;
    const float zc = y1 * sinTheta - z1 * cosTheta;

    // Perspective projection
    if (yc <= 0.01f) {
        // Explicitly return a far off-screen point for behind-camera values.
        // Callers should still cull by depth using getDepth().
        return SDL_FPoint{-1000000.0f, -1000000.0f};
    }

    const float effectiveFocal = focalLength * kFocalUiScale;
    const float xProj = effectiveFocal * (xc / yc);
    const float zProj = effectiveFocal * (zc / yc);

    return SDL_FPoint{
        static_cast<float>(screenCenterX) + xProj,
        static_cast<float>(screenCenterY) - zProj
    };
}

float Camera3D::getDepth(float worldX, float worldY, float worldZ) const {
    const float px = worldX - posX;
    const float py = worldY - posY;
    const float pz = worldZ - posZ;

    // Apply yaw rotation
    const float yaw = yawDegrees * 3.14159265f / 180.0f;
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);
    
    const float y1 = -px * sinYaw + py * cosYaw;
    const float z1 = pz;

    // Apply pitch rotation
    const float theta = pitchRadians();
    const float cosTheta = std::cos(theta);
    const float sinTheta = std::sin(theta);

    return y1 * cosTheta + z1 * sinTheta;
}

float Camera3D::getPerspectiveScale(float worldX, float worldY, float worldZ) const {
    const float depth = getDepth(worldX, worldY, worldZ);
    if (depth <= 0.01f) {
        return 0.0f;
    }
    const float effectiveFocal = focalLength * kFocalUiScale;
    return effectiveFocal / depth;
}

} // namespace battle
