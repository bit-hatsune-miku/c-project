#ifndef BATTLE_BACKDROP_PROJECTION_H
#define BATTLE_BACKDROP_PROJECTION_H

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "battle_stage.h"

namespace battle::render {

struct BackdropQuad {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

struct SkyboxBackdropVertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

using SkyboxBackdropBatches = std::array<std::vector<SkyboxBackdropVertex>, kStageSkyboxFaceCount>;

namespace detail {

constexpr float kBackdropPi = 3.14159265f;
constexpr float kBackdropTwoPi = kBackdropPi * 2.0f;
constexpr float kBackdropFocalScale = 0.01f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct SkyboxSample {
    StageSkyboxFace face = StageSkyboxFace::Front;
    float u = 0.5f;
    float v = 0.5f;
    Vec3 direction{};
};

inline float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline float wrap01(float value) {
    float wrapped = std::fmod(value, 1.0f);
    if (wrapped < 0.0f) {
        wrapped += 1.0f;
    }
    return wrapped;
}

inline float effectiveFocal(const Camera3D& camera) {
    return std::max(1.0f, camera.focalLength * kBackdropFocalScale);
}

inline float horizontalFovRadians(const Camera3D& camera, int screenWidth) {
    return 2.0f * std::atan(std::max(1.0f, static_cast<float>(screenWidth)) / (2.0f * effectiveFocal(camera)));
}

inline float verticalFovRadians(const Camera3D& camera, int screenHeight) {
    return 2.0f * std::atan(std::max(1.0f, static_cast<float>(screenHeight)) / (2.0f * effectiveFocal(camera)));
}

inline Vec3 normalize(Vec3 value) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.00001f) {
        return Vec3{0.0f, 1.0f, 0.0f};
    }
    const float inverseLength = 1.0f / length;
    return Vec3{value.x * inverseLength, value.y * inverseLength, value.z * inverseLength};
}

inline Vec3 cameraSpaceToWorldDirection(const Camera3D& camera, Vec3 cameraDirection) {
    cameraDirection = normalize(cameraDirection);

    const float pitch = camera.pitchDegrees * (kBackdropPi / 180.0f);
    const float cosPitch = std::cos(pitch);
    const float sinPitch = std::sin(pitch);

    const float x1 = cameraDirection.x;
    const float y1 = cameraDirection.y * cosPitch + cameraDirection.z * sinPitch;
    const float z1 = cameraDirection.y * sinPitch - cameraDirection.z * cosPitch;

    const float yaw = camera.yawDegrees * (kBackdropPi / 180.0f);
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);

    return normalize(Vec3{
        x1 * cosYaw - y1 * sinYaw,
        x1 * sinYaw + y1 * cosYaw,
        z1
    });
}

inline Vec3 worldDirectionForScreenPoint(const Camera3D& camera, float screenX, float screenY) {
    const float focal = effectiveFocal(camera);
    const float projectedX = (screenX - static_cast<float>(camera.screenCenterX)) / focal;
    const float projectedZ = -(screenY - static_cast<float>(camera.screenCenterY)) / focal;
    return cameraSpaceToWorldDirection(camera, Vec3{projectedX, 1.0f, projectedZ});
}

inline SkyboxSample projectDirectionToFace(Vec3 direction, StageSkyboxFace forcedFace) {
    const float absX = std::fabs(direction.x);
    const float absY = std::fabs(direction.y);
    const float absZ = std::fabs(direction.z);

    float majorAxis = 1.0f;
    float u = 0.5f;
    float v = 0.5f;

    switch (forcedFace) {
    case StageSkyboxFace::Front:
        majorAxis = std::max(0.0001f, absY);
        u = 0.5f + direction.x / (2.0f * majorAxis);
        v = 0.5f - direction.z / (2.0f * majorAxis);
        break;
    case StageSkyboxFace::Back:
        majorAxis = std::max(0.0001f, absY);
        u = 0.5f - direction.x / (2.0f * majorAxis);
        v = 0.5f - direction.z / (2.0f * majorAxis);
        break;
    case StageSkyboxFace::Left:
        majorAxis = std::max(0.0001f, absX);
        u = 0.5f + direction.y / (2.0f * majorAxis);
        v = 0.5f - direction.z / (2.0f * majorAxis);
        break;
    case StageSkyboxFace::Right:
        majorAxis = std::max(0.0001f, absX);
        u = 0.5f - direction.y / (2.0f * majorAxis);
        v = 0.5f - direction.z / (2.0f * majorAxis);
        break;
    case StageSkyboxFace::Top:
        majorAxis = std::max(0.0001f, absZ);
        u = 0.5f + direction.x / (2.0f * majorAxis);
        v = 0.5f + direction.y / (2.0f * majorAxis);
        break;
    case StageSkyboxFace::Bottom:
        majorAxis = std::max(0.0001f, absZ);
        u = 0.5f + direction.x / (2.0f * majorAxis);
        v = 0.5f - direction.y / (2.0f * majorAxis);
        break;
    case StageSkyboxFace::Count:
        break;
    }

    return SkyboxSample{forcedFace, clamp01(u), clamp01(1.0f - v), direction};
}

inline SkyboxSample projectDirectionToSkybox(Vec3 direction) {
    const float absX = std::fabs(direction.x);
    const float absY = std::fabs(direction.y);
    const float absZ = std::fabs(direction.z);

    if (absY >= absX && absY >= absZ) {
        return projectDirectionToFace(direction, direction.y >= 0.0f ? StageSkyboxFace::Front : StageSkyboxFace::Back);
    }
    if (absX >= absY && absX >= absZ) {
        return projectDirectionToFace(direction, direction.x >= 0.0f ? StageSkyboxFace::Right : StageSkyboxFace::Left);
    }
    return projectDirectionToFace(direction, direction.z >= 0.0f ? StageSkyboxFace::Top : StageSkyboxFace::Bottom);
}

inline void appendSkyboxTriangle(std::vector<SkyboxBackdropVertex>& out,
                                 const SkyboxSample& a,
                                 const SkyboxSample& b,
                                 const SkyboxSample& c,
                                 float ax,
                                 float ay,
                                 float bx,
                                 float by,
                                 float cx,
                                 float cy) {
    out.push_back(SkyboxBackdropVertex{ax, ay, a.u, a.v});
    out.push_back(SkyboxBackdropVertex{bx, by, b.u, b.v});
    out.push_back(SkyboxBackdropVertex{cx, cy, c.u, c.v});
}

inline void appendSkyboxCell(SkyboxBackdropBatches& out,
                             const Camera3D& camera,
                             float x0,
                             float y0,
                             float x1,
                             float y1,
                             int remainingDepth) {
    const SkyboxSample sample00 = projectDirectionToSkybox(worldDirectionForScreenPoint(camera, x0, y0));
    const SkyboxSample sample10 = projectDirectionToSkybox(worldDirectionForScreenPoint(camera, x1, y0));
    const SkyboxSample sample11 = projectDirectionToSkybox(worldDirectionForScreenPoint(camera, x1, y1));
    const SkyboxSample sample01 = projectDirectionToSkybox(worldDirectionForScreenPoint(camera, x0, y1));

    const bool sameFace = sample00.face == sample10.face &&
                          sample00.face == sample11.face &&
                          sample00.face == sample01.face;
    if (!sameFace && remainingDepth > 0) {
        const float midX = (x0 + x1) * 0.5f;
        const float midY = (y0 + y1) * 0.5f;
        appendSkyboxCell(out, camera, x0, y0, midX, midY, remainingDepth - 1);
        appendSkyboxCell(out, camera, midX, y0, x1, midY, remainingDepth - 1);
        appendSkyboxCell(out, camera, x0, midY, midX, y1, remainingDepth - 1);
        appendSkyboxCell(out, camera, midX, midY, x1, y1, remainingDepth - 1);
        return;
    }

    if (sameFace) {
        auto& faceVertices = out[static_cast<size_t>(sample00.face)];
        appendSkyboxTriangle(faceVertices, sample00, sample10, sample11, x0, y0, x1, y0, x1, y1);
        appendSkyboxTriangle(faceVertices, sample00, sample11, sample01, x0, y0, x1, y1, x0, y1);
        return;
    }

    const Vec3 triangle0Center = normalize(Vec3{
        (sample00.direction.x + sample10.direction.x + sample11.direction.x) / 3.0f,
        (sample00.direction.y + sample10.direction.y + sample11.direction.y) / 3.0f,
        (sample00.direction.z + sample10.direction.z + sample11.direction.z) / 3.0f
    });
    const SkyboxSample triangle0Face = projectDirectionToSkybox(triangle0Center);
    auto& triangle0Vertices = out[static_cast<size_t>(triangle0Face.face)];
    appendSkyboxTriangle(triangle0Vertices,
                         projectDirectionToFace(sample00.direction, triangle0Face.face),
                         projectDirectionToFace(sample10.direction, triangle0Face.face),
                         projectDirectionToFace(sample11.direction, triangle0Face.face),
                         x0,
                         y0,
                         x1,
                         y0,
                         x1,
                         y1);

    const Vec3 triangle1Center = normalize(Vec3{
        (sample00.direction.x + sample11.direction.x + sample01.direction.x) / 3.0f,
        (sample00.direction.y + sample11.direction.y + sample01.direction.y) / 3.0f,
        (sample00.direction.z + sample11.direction.z + sample01.direction.z) / 3.0f
    });
    const SkyboxSample triangle1Face = projectDirectionToSkybox(triangle1Center);
    auto& triangle1Vertices = out[static_cast<size_t>(triangle1Face.face)];
    appendSkyboxTriangle(triangle1Vertices,
                         projectDirectionToFace(sample00.direction, triangle1Face.face),
                         projectDirectionToFace(sample11.direction, triangle1Face.face),
                         projectDirectionToFace(sample01.direction, triangle1Face.face),
                         x0,
                         y0,
                         x1,
                         y1,
                         x0,
                         y1);
}

} // namespace detail

inline BackdropQuad makeFullscreenBackdropQuad(int screenWidth, int screenHeight) {
    return BackdropQuad{0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight), 0.0f, 0.0f, 1.0f, 1.0f};
}

inline BackdropQuad computeParallaxBackdropQuad(const Camera3D& camera,
                                                int screenWidth,
                                                int screenHeight,
                                                float strengthX,
                                                float strengthY) {
    const float safeStrengthX = std::clamp(strengthX, 0.0f, 1.0f);
    const float safeStrengthY = std::clamp(strengthY, 0.0f, 1.0f);

    const float drawWidth = static_cast<float>(screenWidth) * (1.18f + safeStrengthX * 0.35f);
    const float drawHeight = static_cast<float>(screenHeight) * (1.14f + safeStrengthY * 0.28f);
    const float maxShiftX = std::max(0.0f, (drawWidth - static_cast<float>(screenWidth)) * 0.5f);
    const float maxShiftY = std::max(0.0f, (drawHeight - static_cast<float>(screenHeight)) * 0.5f);

    const float yawNorm = std::clamp(camera.yawDegrees / 12.0f, -1.0f, 1.0f);
    const float pitchNorm = std::clamp(camera.pitchDegrees / 18.0f, -1.0f, 1.0f);
    const float shiftX = yawNorm * maxShiftX * safeStrengthX;
    const float shiftY = -pitchNorm * maxShiftY * safeStrengthY;

    return BackdropQuad{
        (static_cast<float>(screenWidth) - drawWidth) * 0.5f + shiftX,
        (static_cast<float>(screenHeight) - drawHeight) * 0.5f + shiftY,
        (static_cast<float>(screenWidth) + drawWidth) * 0.5f + shiftX,
        (static_cast<float>(screenHeight) + drawHeight) * 0.5f + shiftY,
        0.0f,
        0.0f,
        1.0f,
        1.0f
    };
}

inline std::vector<BackdropQuad> buildPanoramaBackdropQuads(const Camera3D& camera,
                                                            int screenWidth,
                                                            int screenHeight) {
    std::vector<BackdropQuad> quads;
    if (screenWidth <= 0 || screenHeight <= 0) {
        return quads;
    }

    const float yawRadians = camera.yawDegrees * (detail::kBackdropPi / 180.0f);
    const float pitchRadians = camera.pitchDegrees * (detail::kBackdropPi / 180.0f);
    const float horizontalSpan = std::clamp(detail::horizontalFovRadians(camera, screenWidth) / detail::kBackdropTwoPi,
                                            0.08f,
                                            1.0f);
    const float verticalSpan = std::clamp(detail::verticalFovRadians(camera, screenHeight) / detail::kBackdropPi,
                                          0.08f,
                                          1.0f);

    const float centerU = detail::wrap01(0.5f - yawRadians / detail::kBackdropTwoPi);
    const float clampedCenterV = std::clamp(0.5f + pitchRadians / detail::kBackdropPi,
                                            verticalSpan * 0.5f,
                                            1.0f - verticalSpan * 0.5f);
    const float v0 = clampedCenterV - verticalSpan * 0.5f;
    const float v1 = clampedCenterV + verticalSpan * 0.5f;
    const float u0 = centerU - horizontalSpan * 0.5f;
    const float u1 = centerU + horizontalSpan * 0.5f;

    if (u0 >= 0.0f && u1 <= 1.0f) {
        quads.push_back(BackdropQuad{
            0.0f,
            0.0f,
            static_cast<float>(screenWidth),
            static_cast<float>(screenHeight),
            u0,
            v0,
            u1,
            v1
        });
        return quads;
    }

    if (u0 < 0.0f) {
        const float leftSpan = -u0;
        const float rightSpan = horizontalSpan - leftSpan;
        const float splitX = static_cast<float>(screenWidth) * (leftSpan / horizontalSpan);
        quads.push_back(BackdropQuad{0.0f, 0.0f, splitX, static_cast<float>(screenHeight), 1.0f - leftSpan, v0, 1.0f, v1});
        quads.push_back(BackdropQuad{splitX, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight), 0.0f, v0, rightSpan, v1});
        return quads;
    }

    const float firstSpan = 1.0f - u0;
    const float secondSpan = horizontalSpan - firstSpan;
    const float splitX = static_cast<float>(screenWidth) * (firstSpan / horizontalSpan);
    quads.push_back(BackdropQuad{0.0f, 0.0f, splitX, static_cast<float>(screenHeight), u0, v0, 1.0f, v1});
    quads.push_back(BackdropQuad{splitX, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight), 0.0f, v0, secondSpan, v1});
    return quads;
}

inline SkyboxBackdropBatches buildSkyboxBackdropBatches(const Camera3D& camera,
                                                        int screenWidth,
                                                        int screenHeight) {
    SkyboxBackdropBatches batches;
    if (screenWidth <= 0 || screenHeight <= 0) {
        return batches;
    }

    constexpr int kBaseGridX = 12;
    constexpr int kBaseGridY = 8;
    constexpr int kSubdivisionDepth = 2;
    const float cellWidth = static_cast<float>(screenWidth) / static_cast<float>(kBaseGridX);
    const float cellHeight = static_cast<float>(screenHeight) / static_cast<float>(kBaseGridY);

    for (int gridY = 0; gridY < kBaseGridY; ++gridY) {
        for (int gridX = 0; gridX < kBaseGridX; ++gridX) {
            const float x0 = static_cast<float>(gridX) * cellWidth;
            const float y0 = static_cast<float>(gridY) * cellHeight;
            const float x1 = (gridX == kBaseGridX - 1) ? static_cast<float>(screenWidth) : x0 + cellWidth;
            const float y1 = (gridY == kBaseGridY - 1) ? static_cast<float>(screenHeight) : y0 + cellHeight;
            detail::appendSkyboxCell(batches, camera, x0, y0, x1, y1, kSubdivisionDepth);
        }
    }

    return batches;
}

} // namespace battle::render

#endif
