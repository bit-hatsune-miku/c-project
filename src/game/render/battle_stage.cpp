#include "battle_stage.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <nlohmann/json.hpp>

#include "../core/battle_loader.h"
#include "battle_asset_loading.h"
#include "battle_world_renderer.h"

using json = nlohmann::json;

namespace battle::render {
namespace {

constexpr const char* kDefaultStageKey = "default_stage";
constexpr const char* kStageRegistryPath = "assets/combat/stages.json";
constexpr size_t kMaxStageProps = 4;

StageDefinition makeBuiltInDefaultStage() {
    StageDefinition stage;
    stage.key = kDefaultStageKey;
    stage.floor.centerX = -300.0f;
    stage.floor.centerY = 1150.0f;
    stage.floor.width = 2800.0f;
    stage.floor.depth = 3200.0f;
    stage.floor.tileSize = 200.0f;
    stage.floor.baseColor = SDL_Color{46, 49, 60, 255};
    stage.floor.accentColor = SDL_Color{52, 56, 69, 255};
    stage.backdrop.gradientTopColor = SDL_Color{19, 23, 34, 255};
    stage.backdrop.gradientBottomColor = SDL_Color{5, 6, 10, 255};
    return stage;
}

std::optional<StageBackdropMode> parseBackdropMode(const std::string& value) {
    if (value == "screen") {
        return StageBackdropMode::Screen;
    }
    if (value == "parallax") {
        return StageBackdropMode::Parallax;
    }
    if (value == "panorama") {
        return StageBackdropMode::Panorama;
    }
    if (value == "skybox") {
        return StageBackdropMode::Skybox;
    }
    return std::nullopt;
}

bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return 10 + (c - 'A');
}

std::optional<SDL_Color> parseHexColor(const std::string& value) {
    if (value.size() != 7 && value.size() != 9) {
        return std::nullopt;
    }
    if (value.front() != '#') {
        return std::nullopt;
    }
    for (size_t i = 1; i < value.size(); ++i) {
        if (!isHexDigit(value[i])) {
            return std::nullopt;
        }
    }

    auto parseByte = [&](size_t offset) -> Uint8 {
        return static_cast<Uint8>((hexValue(value[offset]) << 4) | hexValue(value[offset + 1]));
    };

    SDL_Color color{};
    color.r = parseByte(1);
    color.g = parseByte(3);
    color.b = parseByte(5);
    color.a = (value.size() == 9) ? parseByte(7) : 255;
    return color;
}

void parseColorField(const json& node, const char* key, SDL_Color& inOutColor) {
    if (!node.is_object() || key == nullptr || !node.contains(key)) {
        return;
    }
    const json& colorJson = node.at(key);
    if (!colorJson.is_string()) {
        return;
    }
    const auto color = parseHexColor(colorJson.get<std::string>());
    if (color.has_value()) {
        inOutColor = *color;
    }
}

void parseOptionalFloatField(const json& node, const char* key, std::optional<float>& outValue) {
    if (!node.is_object() || key == nullptr || !node.contains(key)) {
        return;
    }
    const json& valueJson = node.at(key);
    if (!valueJson.is_number()) {
        return;
    }
    outValue = valueJson.get<float>();
}

void parseFloorDefinition(const json& floorJson, StageFloorDefinition& inOutFloor) {
    if (!floorJson.is_object()) {
        return;
    }

    if (floorJson.contains("centerX") && floorJson.at("centerX").is_number()) {
        inOutFloor.centerX = floorJson.at("centerX").get<float>();
    }
    if (floorJson.contains("centerY") && floorJson.at("centerY").is_number()) {
        inOutFloor.centerY = floorJson.at("centerY").get<float>();
    }
    if (floorJson.contains("width") && floorJson.at("width").is_number()) {
        inOutFloor.width = std::max(64.0f, floorJson.at("width").get<float>());
    }
    if (floorJson.contains("depth") && floorJson.at("depth").is_number()) {
        inOutFloor.depth = std::max(64.0f, floorJson.at("depth").get<float>());
    }
    if (floorJson.contains("tileSize") && floorJson.at("tileSize").is_number()) {
        inOutFloor.tileSize = std::max(16.0f, floorJson.at("tileSize").get<float>());
    }
    if (floorJson.contains("texture") && floorJson.at("texture").is_string()) {
        inOutFloor.texturePath = floorJson.at("texture").get<std::string>();
    }

    parseColorField(floorJson, "baseColor", inOutFloor.baseColor);
    parseColorField(floorJson, "accentColor", inOutFloor.accentColor);
}

void parseBackdropDefinition(const json& backdropJson, StageBackdropDefinition& inOutBackdrop) {
    if (!backdropJson.is_object()) {
        return;
    }

    if (backdropJson.contains("mode") && backdropJson.at("mode").is_string()) {
        const auto parsedMode = parseBackdropMode(backdropJson.at("mode").get<std::string>());
        if (parsedMode.has_value()) {
            inOutBackdrop.mode = *parsedMode;
        }
    }
    if (backdropJson.contains("image") && backdropJson.at("image").is_string()) {
        inOutBackdrop.imagePath = backdropJson.at("image").get<std::string>();
    }
    if (backdropJson.contains("parallaxStrengthX") && backdropJson.at("parallaxStrengthX").is_number()) {
        inOutBackdrop.parallaxStrengthX =
            std::clamp(backdropJson.at("parallaxStrengthX").get<float>(), 0.0f, 1.0f);
    }
    if (backdropJson.contains("parallaxStrengthY") && backdropJson.at("parallaxStrengthY").is_number()) {
        inOutBackdrop.parallaxStrengthY =
            std::clamp(backdropJson.at("parallaxStrengthY").get<float>(), 0.0f, 1.0f);
    }
    if (backdropJson.contains("skybox") && backdropJson.at("skybox").is_object()) {
        const json& skyboxJson = backdropJson.at("skybox");
        if (skyboxJson.contains("front") && skyboxJson.at("front").is_string()) {
            inOutBackdrop.skybox.frontTexturePath = skyboxJson.at("front").get<std::string>();
        }
        if (skyboxJson.contains("back") && skyboxJson.at("back").is_string()) {
            inOutBackdrop.skybox.backTexturePath = skyboxJson.at("back").get<std::string>();
        }
        if (skyboxJson.contains("left") && skyboxJson.at("left").is_string()) {
            inOutBackdrop.skybox.leftTexturePath = skyboxJson.at("left").get<std::string>();
        }
        if (skyboxJson.contains("right") && skyboxJson.at("right").is_string()) {
            inOutBackdrop.skybox.rightTexturePath = skyboxJson.at("right").get<std::string>();
        }
        if (skyboxJson.contains("top") && skyboxJson.at("top").is_string()) {
            inOutBackdrop.skybox.topTexturePath = skyboxJson.at("top").get<std::string>();
        }
        if (skyboxJson.contains("bottom") && skyboxJson.at("bottom").is_string()) {
            inOutBackdrop.skybox.bottomTexturePath = skyboxJson.at("bottom").get<std::string>();
        }
    }
    parseColorField(backdropJson, "gradientTopColor", inOutBackdrop.gradientTopColor);
    parseColorField(backdropJson, "gradientBottomColor", inOutBackdrop.gradientBottomColor);
}

std::optional<StagePropDefinition> parsePropDefinition(const json& propJson) {
    if (!propJson.is_object()) {
        return std::nullopt;
    }

    StagePropDefinition prop;
    if (propJson.contains("texture") && propJson.at("texture").is_string()) {
        prop.texturePath = propJson.at("texture").get<std::string>();
    }
    if (prop.texturePath.empty()) {
        return std::nullopt;
    }
    if (propJson.contains("worldX") && propJson.at("worldX").is_number()) {
        prop.worldX = propJson.at("worldX").get<float>();
    }
    if (propJson.contains("worldY") && propJson.at("worldY").is_number()) {
        prop.worldY = propJson.at("worldY").get<float>();
    }
    if (propJson.contains("worldZ") && propJson.at("worldZ").is_number()) {
        prop.worldZ = propJson.at("worldZ").get<float>();
    }
    if (propJson.contains("pixelWidth") && propJson.at("pixelWidth").is_number_integer()) {
        prop.pixelWidth = std::max(1, propJson.at("pixelWidth").get<int>());
    }
    if (propJson.contains("pixelHeight") && propJson.at("pixelHeight").is_number_integer()) {
        prop.pixelHeight = std::max(1, propJson.at("pixelHeight").get<int>());
    }
    if (propJson.contains("alpha") && propJson.at("alpha").is_number()) {
        prop.alpha = std::clamp(propJson.at("alpha").get<float>(), 0.0f, 1.0f);
    }
    parseColorField(propJson, "tint", prop.tint);
    return prop;
}

void parseCameraOverride(const json& cameraJson, StageCameraOverride& inOutCamera) {
    if (!cameraJson.is_object()) {
        return;
    }

    parseOptionalFloatField(cameraJson, "posX", inOutCamera.posX);
    parseOptionalFloatField(cameraJson, "posY", inOutCamera.posY);
    parseOptionalFloatField(cameraJson, "posZ", inOutCamera.posZ);
    parseOptionalFloatField(cameraJson, "pitchDegrees", inOutCamera.pitchDegrees);
    parseOptionalFloatField(cameraJson, "yawDegrees", inOutCamera.yawDegrees);
    parseOptionalFloatField(cameraJson, "focalLength", inOutCamera.focalLength);
}

bool parseStageDefinition(const json& stageJson, StageDefinition& inOutStage) {
    if (!stageJson.is_object()) {
        return false;
    }

    if (stageJson.contains("floor")) {
        parseFloorDefinition(stageJson.at("floor"), inOutStage.floor);
    }
    if (stageJson.contains("backdrop")) {
        parseBackdropDefinition(stageJson.at("backdrop"), inOutStage.backdrop);
    }
    if (stageJson.contains("props") && stageJson.at("props").is_array()) {
        inOutStage.props.clear();
        for (const json& propJson : stageJson.at("props")) {
            if (inOutStage.props.size() >= kMaxStageProps) {
                break;
            }
            const auto prop = parsePropDefinition(propJson);
            if (prop.has_value()) {
                inOutStage.props.push_back(*prop);
            }
        }
    }
    if (stageJson.contains("camera")) {
        parseCameraOverride(stageJson.at("camera"), inOutStage.camera);
    }
    return true;
}

const json* findStageJsonByKey(const json& root, const std::string& stageKey) {
    if (root.is_object() && root.contains("stages")) {
        const json& stagesJson = root.at("stages");
        if (stagesJson.is_object()) {
            auto it = stagesJson.find(stageKey);
            if (it != stagesJson.end()) {
                return &it.value();
            }
        } else if (stagesJson.is_array()) {
            for (const json& entry : stagesJson) {
                if (entry.is_object() && entry.value("key", "") == stageKey) {
                    return &entry;
                }
            }
        }
    } else if (root.is_object()) {
        auto it = root.find(stageKey);
        if (it != root.end()) {
            return &it.value();
        }
    }

    return nullptr;
}

} // namespace

const std::string& stageSkyboxFacePath(const StageSkyboxDefinition& skybox, StageSkyboxFace face) {
    switch (face) {
    case StageSkyboxFace::Front:
        return skybox.frontTexturePath;
    case StageSkyboxFace::Back:
        return skybox.backTexturePath;
    case StageSkyboxFace::Left:
        return skybox.leftTexturePath;
    case StageSkyboxFace::Right:
        return skybox.rightTexturePath;
    case StageSkyboxFace::Top:
        return skybox.topTexturePath;
    case StageSkyboxFace::Bottom:
        return skybox.bottomTexturePath;
    case StageSkyboxFace::Count:
        break;
    }

    return skybox.frontTexturePath;
}

bool loadStageDefinition(const std::string& stageKey, StageDefinition& outStage) {
    StageDefinition defaultStage = makeBuiltInDefaultStage();

    json root;
    if (loader::readJsonRoot(loader::resolveAssetPath(kStageRegistryPath), root, "stage")) {
        if (const json* defaultStageJson = findStageJsonByKey(root, kDefaultStageKey)) {
            StageDefinition parsedDefault = defaultStage;
            if (parseStageDefinition(*defaultStageJson, parsedDefault)) {
                defaultStage = std::move(parsedDefault);
            } else {
                std::cerr << "[Battle] Invalid default stage definition, using built-in fallback\n";
            }
        } else {
            std::cerr << "[Battle] default_stage missing from stage registry, using built-in fallback\n";
        }

        const std::string resolvedStageKey = stageKey.empty() ? std::string{kDefaultStageKey} : stageKey;
        if (const json* stageJson = findStageJsonByKey(root, resolvedStageKey)) {
            StageDefinition parsedStage = defaultStage;
            parsedStage.key = resolvedStageKey;
            if (parseStageDefinition(*stageJson, parsedStage)) {
                outStage = std::move(parsedStage);
                return true;
            }
            std::cerr << "[Battle] Invalid stage '" << resolvedStageKey << "', using default_stage fallback\n";
        } else if (resolvedStageKey != kDefaultStageKey) {
            std::cerr << "[Battle] Stage key not found: " << resolvedStageKey
                      << ", using default_stage fallback\n";
        }
    }

    outStage = std::move(defaultStage);
    return true;
}

bool loadStageRenderData(SDL_Renderer* renderer, const std::string& stageKey, StageRenderData& outStage) {
    destroyStageRenderData(outStage);
    if (renderer == nullptr) {
        return false;
    }

    if (!loadStageDefinition(stageKey, outStage.definition)) {
        return false;
    }

    if (!outStage.definition.floor.texturePath.empty()) {
        const auto loadedFloor = tryLoadTextureFromPath(renderer, outStage.definition.floor.texturePath);
        outStage.floorTexture = loadedFloor.has_value() ? *loadedFloor : nullptr;
    }
    if (outStage.floorTexture == nullptr) {
        outStage.floorTexture = createBattleWorldFloorTileTexture(renderer,
                                                                  outStage.definition.floor.baseColor,
                                                                  outStage.definition.floor.accentColor);
    }

    if (!outStage.definition.backdrop.imagePath.empty()) {
        const auto loadedBackdrop = tryLoadTextureFromPath(renderer, outStage.definition.backdrop.imagePath);
        outStage.backdropTexture = loadedBackdrop.has_value() ? *loadedBackdrop : nullptr;
    }
    for (size_t faceIndex = 0; faceIndex < kStageSkyboxFaceCount; ++faceIndex) {
        const StageSkyboxFace face = static_cast<StageSkyboxFace>(faceIndex);
        const std::string& facePath = stageSkyboxFacePath(outStage.definition.backdrop.skybox, face);
        if (facePath.empty()) {
            continue;
        }

        const auto loadedFaceTexture = tryLoadTextureFromPath(renderer, facePath);
        outStage.skyboxTextures[faceIndex] = loadedFaceTexture.has_value() ? *loadedFaceTexture : nullptr;
    }

    outStage.props.reserve(outStage.definition.props.size());
    for (const StagePropDefinition& prop : outStage.definition.props) {
        const auto loadedTexture = tryLoadTextureFromPath(renderer, prop.texturePath);
        if (!loadedTexture.has_value() || *loadedTexture == nullptr) {
            continue;
        }
        outStage.props.push_back(StagePropRenderItem{prop, *loadedTexture});
    }

    return true;
}

void destroyStageRenderData(StageRenderData& stage) {
    if (stage.floorTexture != nullptr) {
        SDL_DestroyTexture(stage.floorTexture);
        stage.floorTexture = nullptr;
    }
    if (stage.backdropTexture != nullptr) {
        SDL_DestroyTexture(stage.backdropTexture);
        stage.backdropTexture = nullptr;
    }
    for (SDL_Texture*& skyboxTexture : stage.skyboxTextures) {
        if (skyboxTexture != nullptr) {
            SDL_DestroyTexture(skyboxTexture);
            skyboxTexture = nullptr;
        }
    }
    for (StagePropRenderItem& prop : stage.props) {
        if (prop.texture != nullptr) {
            SDL_DestroyTexture(prop.texture);
            prop.texture = nullptr;
        }
    }
    stage.props.clear();
    stage.definition = StageDefinition{};
}

void applyStageCameraOverride(const StageCameraOverride& overrideValues, Camera3D& camera) {
    if (overrideValues.posX.has_value()) {
        camera.posX = *overrideValues.posX;
    }
    if (overrideValues.posY.has_value()) {
        camera.posY = *overrideValues.posY;
    }
    if (overrideValues.posZ.has_value()) {
        camera.posZ = *overrideValues.posZ;
    }
    if (overrideValues.pitchDegrees.has_value()) {
        camera.pitchDegrees = *overrideValues.pitchDegrees;
    }
    if (overrideValues.yawDegrees.has_value()) {
        camera.yawDegrees = *overrideValues.yawDegrees;
    }
    if (overrideValues.focalLength.has_value()) {
        camera.focalLength = *overrideValues.focalLength;
    }
}

} // namespace battle::render
