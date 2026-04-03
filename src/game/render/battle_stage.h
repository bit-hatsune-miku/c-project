#ifndef BATTLE_STAGE_H
#define BATTLE_STAGE_H

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "camera_3d.h"

namespace battle::render {

enum class StageBackdropMode {
    Screen,
    Parallax,
    Panorama,
    Skybox
};

enum class StageSkyboxFace : std::size_t {
    Front = 0,
    Back,
    Left,
    Right,
    Top,
    Bottom,
    Count
};

inline constexpr std::size_t kStageSkyboxFaceCount = static_cast<std::size_t>(StageSkyboxFace::Count);

struct StageSkyboxDefinition {
    std::string frontTexturePath;
    std::string backTexturePath;
    std::string leftTexturePath;
    std::string rightTexturePath;
    std::string topTexturePath;
    std::string bottomTexturePath;
};

struct StageFloorDefinition {
    float centerX = -300.0f;
    float centerY = 1150.0f;
    float width = 2800.0f;
    float depth = 3200.0f;
    float tileSize = 200.0f;
    std::string texturePath;
    SDL_Color baseColor{46, 49, 60, 255};
    SDL_Color accentColor{52, 56, 69, 255};
};

struct StageBackdropDefinition {
    StageBackdropMode mode = StageBackdropMode::Screen;
    std::string imagePath;
    float parallaxStrengthX = 0.18f;
    float parallaxStrengthY = 0.12f;
    StageSkyboxDefinition skybox;
    SDL_Color gradientTopColor{17, 20, 30, 255};
    SDL_Color gradientBottomColor{5, 7, 11, 255};
};

struct StagePropDefinition {
    std::string texturePath;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    int pixelWidth = 320;
    int pixelHeight = 320;
    SDL_Color tint{255, 255, 255, 255};
    float alpha = 1.0f;
};

struct StageCameraOverride {
    std::optional<float> posX;
    std::optional<float> posY;
    std::optional<float> posZ;
    std::optional<float> pitchDegrees;
    std::optional<float> yawDegrees;
    std::optional<float> focalLength;
};

struct StageDefinition {
    std::string key = "default_stage";
    StageFloorDefinition floor;
    StageBackdropDefinition backdrop;
    std::vector<StagePropDefinition> props;
    StageCameraOverride camera;
};

struct StagePropRenderItem {
    StagePropDefinition definition;
    SDL_Texture* texture = nullptr;
};

struct StageRenderData {
    StageDefinition definition;
    SDL_Texture* floorTexture = nullptr;
    SDL_Texture* backdropTexture = nullptr;
    std::array<SDL_Texture*, kStageSkyboxFaceCount> skyboxTextures{};
    std::vector<StagePropRenderItem> props;
};

const std::string& stageSkyboxFacePath(const StageSkyboxDefinition& skybox, StageSkyboxFace face);
bool loadStageDefinition(const std::string& stageKey, StageDefinition& outStage);
bool loadStageRenderData(SDL_Renderer* renderer, const std::string& stageKey, StageRenderData& outStage);
void destroyStageRenderData(StageRenderData& stage);
void applyStageCameraOverride(const StageCameraOverride& overrideValues, Camera3D& camera);

} // namespace battle::render

#endif
