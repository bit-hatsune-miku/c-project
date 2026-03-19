#ifndef FREE_VIEW_CAMERA_DEBUG_LOG_H
#define FREE_VIEW_CAMERA_DEBUG_LOG_H

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#include "camera_3d.h"

namespace battle::render {

class FreeViewCameraDebugLog {
public:
    void update(bool enabled, const Camera3D& camera) {
        if (!enabled) {
            clear();
            return;
        }

        if (visible_ && cameraMatches(camera)) {
            return;
        }

        char line[160];
        std::snprintf(
            line,
            sizeof(line),
            "[FreeView] Cam(%.0f, %.0f, %.0f) Pitch %.1f Yaw %.1f Focal %.0f",
            camera.posX,
            camera.posY,
            camera.posZ,
            camera.pitchDegrees,
            camera.yawDegrees,
            camera.focalLength
        );

        const std::string output(line);
        std::cout << "\r" << output;
        if (output.size() < previousWidth_) {
            std::cout << std::string(previousWidth_ - output.size(), ' ');
        }
        std::cout << std::flush;

        lastCamera_ = camera;
        previousWidth_ = output.size();
        visible_ = true;
    }

    void clear() {
        if (!visible_) {
            return;
        }

        std::cout << "\r" << std::string(previousWidth_, ' ') << "\r" << std::flush;
        visible_ = false;
        previousWidth_ = 0;
    }

private:
    bool cameraMatches(const Camera3D& camera) const {
        return nearlyEqual(lastCamera_.posX, camera.posX) &&
               nearlyEqual(lastCamera_.posY, camera.posY) &&
               nearlyEqual(lastCamera_.posZ, camera.posZ) &&
               nearlyEqual(lastCamera_.pitchDegrees, camera.pitchDegrees) &&
               nearlyEqual(lastCamera_.yawDegrees, camera.yawDegrees) &&
               nearlyEqual(lastCamera_.focalLength, camera.focalLength);
    }

    static bool nearlyEqual(float lhs, float rhs) {
        return std::fabs(lhs - rhs) <= 0.01f;
    }

    Camera3D lastCamera_{};
    std::size_t previousWidth_ = 0;
    bool visible_ = false;
};

} // namespace battle::render

#endif
