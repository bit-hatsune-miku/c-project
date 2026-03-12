#define GL_GLEXT_PROTOTYPES

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "game/battle_manager.h"
#include "game/camera_3d.h"
#include "game/easing.h"
#include "game/turn_system.h"

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kSpriteFrameTime = 0.15f;

constexpr float kBossCharacterDistanceWorld = 420.0f;
constexpr float kCharacterGapWorld = 1200.0f;
constexpr float kDuelCharacterSlotX = -0.5f * kCharacterGapWorld;
constexpr float kDuelBossSlotX = 0.0f;
constexpr float kDuelCharacterBaseY = 300.0f;

constexpr float kGoalCameraPosX = -405.0f;
constexpr float kGoalCameraPosY = 45.0f;
constexpr float kGoalCameraPosZ = -175.0f;
constexpr float kGoalCameraPitch = 5.0f;
constexpr float kGoalCameraYaw = 4.0f;
constexpr float kGoalCameraFocal = 50000.0f;

constexpr float kActionIntroOffsetX = -130.0f;
constexpr float kActionIntroOffsetY = -110.0f;
constexpr float kActionIntroOffsetZ = 28.0f;
constexpr float kActionIntroDurationSeconds = 0.22f;

struct WorldEntity {
    std::string key;
    std::string assetName;
    bool isBoss = false;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    SDL_Color fallbackColor{200, 200, 200, 255};
};

struct CameraIntroAnimation {
    bool active = false;
    float elapsed = 0.0f;
    float duration = kActionIntroDurationSeconds;
    float startX = 0.0f;
    float startY = 0.0f;
    float startZ = 0.0f;
    float startPitch = 0.0f;
    float startYaw = 0.0f;
    float startFocal = 0.0f;
    float goalX = 0.0f;
    float goalY = 0.0f;
    float goalZ = 0.0f;
    float goalPitch = 0.0f;
    float goalYaw = 0.0f;
    float goalFocal = 0.0f;
};

struct SoftwareSceneRenderer {
    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* floorTileTexture = nullptr;
    std::map<std::string, SDL_Texture*> textureByAsset;
    std::vector<std::string> loadedAssets;
    int width = 0;
    int height = 0;

    ~SoftwareSceneRenderer() {
        destroy();
    }

    void destroy() {
        if (floorTileTexture != nullptr) {
            SDL_DestroyTexture(floorTileTexture);
            floorTileTexture = nullptr;
        }
        for (auto& [_, texture] : textureByAsset) {
            if (texture != nullptr) {
                SDL_DestroyTexture(texture);
            }
        }
        textureByAsset.clear();
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (surface != nullptr) {
            SDL_FreeSurface(surface);
            surface = nullptr;
        }
        width = 0;
        height = 0;
    }

    bool initialize(int newWidth, int newHeight, const std::vector<std::string>& assetNames);
};

struct GlScreenBlitter {
    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;
    int textureWidth = 0;
    int textureHeight = 0;

    ~GlScreenBlitter() {
        destroy();
    }

    bool initialize();
    void destroy();
    void ensureTextureSize(int width, int height);
    void uploadSurface(SDL_Surface* surface);
    void draw();
};

std::string resolvePath(const std::string& relativePath) {
    const std::array<std::string, 3> candidates = {
        relativePath,
        "../" + relativePath,
        "../../" + relativePath
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return relativePath;
}

std::string findFontPath() {
    const std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return std::string();
}

bool isWindowResizeEvent(const SDL_Event& event) {
    return event.type == SDL_WINDOWEVENT &&
           (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            event.window.event == SDL_WINDOWEVENT_RESIZED);
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string normalizeCombatKey(std::string key) {
    if (key == "cupcake") {
        return "cupcakke";
    }
    return key;
}

std::string findCombatImagePath(const std::string& folder, const std::string& assetName) {
    const std::array<std::pair<std::string, std::string>, 2> candidates = {
        std::pair<std::string, std::string>{
            resolvePath("assets/combat/" + folder + "/" + assetName + ".png"),
            "../combat/" + folder + "/" + assetName + ".png"
        },
        std::pair<std::string, std::string>{
            resolvePath("assets/combat/" + folder + "/" + assetName + ".webp"),
            "../combat/" + folder + "/" + assetName + ".webp"
        }
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate.first)) {
            return candidate.second;
        }
    }
    return std::string();
}

void setElementText(Rml::ElementDocument* document, const std::string& id, const std::string& text) {
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetInnerRML(text);
    }
}

void setElementDisplay(Rml::ElementDocument* document, const std::string& id, bool visible) {
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetProperty("display", visible ? "block" : "none");
    }
}

void setPortraitDecorator(Rml::ElementDocument* document, const std::string& id, const std::string& assetName) {
    if (Rml::Element* element = document->GetElementById(id)) {
        const std::string path = findCombatImagePath("icons", assetName);
        if (!path.empty()) {
            element->SetProperty("decorator", "image(" + path + " cover center center)");
        }
    }
}

std::vector<int> getSortedTurnActorIndices(const battle::TurnState& turnState) {
    std::vector<int> sorted(turnState.actors.size());
    std::iota(sorted.begin(), sorted.end(), 0);
    std::sort(sorted.begin(), sorted.end(), [&](int lhs, int rhs) {
        constexpr float kEpsilon = 0.0001f;
        const battle::TurnActor& a = turnState.actors[static_cast<size_t>(lhs)];
        const battle::TurnActor& b = turnState.actors[static_cast<size_t>(rhs)];
        if (std::fabs(a.currentActionValue - b.currentActionValue) > kEpsilon) {
            return a.currentActionValue < b.currentActionValue;
        }
        return battle::turn::turnPriorityLess(a, b);
    });
    return sorted;
}

void setOrbState(Rml::Element* orb, bool visible, bool filled, bool gold = false) {
    if (orb == nullptr) {
        return;
    }
    orb->SetClass("hidden", !visible);
    orb->SetClass("full", filled);
    orb->SetClass("gold", filled && gold);
}

void updateBossOrbRow(Rml::ElementDocument* document, int currentHp, int maxHp) {
    const int safeMaxHp = std::max(1, maxHp);
    const float ratio = static_cast<float>(std::clamp(currentHp, 0, safeMaxHp)) / static_cast<float>(safeMaxHp);
    const int filled = std::clamp(static_cast<int>(std::ceil(ratio * 6.0f)), 0, 6);
    for (int i = 0; i < 6; ++i) {
        if (Rml::Element* orb = document->GetElementById("boss-orb-" + std::to_string(i + 1))) {
            setOrbState(orb, true, i < filled, i == filled - 1 && filled > 0);
        }
    }
}

void updateUnitOrbRow(Rml::ElementDocument* document, int unitIndex, int charge, int required) {
    const int safeRequired = std::clamp(required, 1, 6);
    const int safeCharge = std::clamp(charge, 0, safeRequired);
    for (int i = 0; i < 6; ++i) {
        if (Rml::Element* orb = document->GetElementById(
                "unit-" + std::to_string(unitIndex) + "-orb-" + std::to_string(i + 1))) {
            setOrbState(orb, i < safeRequired, i < safeCharge, safeCharge == safeRequired && i == safeRequired - 1);
        }
    }
}

std::string hpColorForRatio(float ratio) {
    if (ratio > 0.66f) {
        return "#48bf7b";
    }
    if (ratio > 0.33f) {
        return "#e4a943";
    }
    return "#d96b56";
}

void updateBattleHudDocument(Rml::ElementDocument* document, const battle::BattleManager& manager) {
    const battle::BattleState& battleState = manager.getBattleState();
    const battle::TurnState& turnState = manager.getTurnState();
    const int activeActorIndex = manager.getPreviewNextActorIndex();

    setElementText(document, "boss-name", uppercase(battleState.boss.key));
    const int bossCurrentHp = manager.getBossCurrentHp();
    const int bossMaxHp = std::max(1, manager.getBossMaxHp());
    const int bossPercent = static_cast<int>(std::round((100.0f * bossCurrentHp) / bossMaxHp));
    setElementText(document, "boss-percent", "HP " + std::to_string(bossPercent) + "%");
    if (Rml::Element* bossFill = document->GetElementById("boss-fill")) {
        bossFill->SetProperty("width", std::to_string(bossPercent) + "%");
    }
    updateBossOrbRow(document, bossCurrentHp, bossMaxHp);

    const std::vector<int> sortedActorIndices = getSortedTurnActorIndices(turnState);
    for (int slot = 0; slot < 5; ++slot) {
        const std::string slotIndex = std::to_string(slot + 1);
        const bool hasActor = slot < static_cast<int>(sortedActorIndices.size());
        setElementDisplay(document, "turn-card-" + slotIndex, hasActor);
        if (!hasActor) {
            continue;
        }

        const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(sortedActorIndices[static_cast<size_t>(slot)])];
        setPortraitDecorator(document, "turn-portrait-" + slotIndex, actor.key);
        if (Rml::Element* card = document->GetElementById("turn-card-" + slotIndex)) {
            card->SetClass("active", sortedActorIndices[static_cast<size_t>(slot)] == activeActorIndex);
            card->SetClass("boss", actor.type == battle::ParticipantType::Boss);
        }
        if (Rml::Element* accent = document->GetElementById("turn-accent-" + slotIndex)) {
            accent->SetClass("boss-accent", actor.type == battle::ParticipantType::Boss);
        }
    }

    for (int i = 0; i < 4; ++i) {
        const std::string index = std::to_string(i + 1);
        const bool hasCharacter = i < static_cast<int>(battleState.party.size());
        setElementDisplay(document, "unit-card-" + index, hasCharacter);
        if (!hasCharacter) {
            continue;
        }

        const battle::CharacterDefinition& character = battleState.party[static_cast<size_t>(i)];
        const int currentHp = manager.getCharacterCurrentHp(i);
        const int maxHp = std::max(1, manager.getCharacterMaxHp(i));
        const float ratio = static_cast<float>(std::clamp(currentHp, 0, maxHp)) / static_cast<float>(maxHp);

        setElementText(document, "unit-name-" + index, uppercase(character.title));
        setElementText(document, "unit-hp-text-" + index, std::to_string(currentHp));
        setPortraitDecorator(document, "unit-portrait-" + index, character.assets);
        updateUnitOrbRow(document, i + 1, manager.getCharacterUltimateCharge(i), manager.getCharacterUltimateRequired(i));

        if (Rml::Element* hpFill = document->GetElementById("unit-hp-fill-" + index)) {
            const int fillWidth = std::clamp(static_cast<int>(std::round(ratio * 218.0f)), 0, 218);
            hpFill->SetProperty("width", std::to_string(fillWidth) + "px");
            hpFill->SetProperty("background-color", hpColorForRatio(ratio));
        }
        if (Rml::Element* card = document->GetElementById("unit-card-" + index)) {
            const bool isFocused = activeActorIndex >= 0 &&
                activeActorIndex < static_cast<int>(turnState.actors.size()) &&
                turnState.actors[static_cast<size_t>(activeActorIndex)].type == battle::ParticipantType::Character &&
                turnState.actors[static_cast<size_t>(activeActorIndex)].partyIndex == i;
            card->SetClass("focus", isFocused);
            card->SetClass("ghost", currentHp <= 0);
        }
    }
}

void applyGoalCamera(battle::Camera3D& camera) {
    camera.posX = kGoalCameraPosX;
    camera.posY = kGoalCameraPosY;
    camera.posZ = kGoalCameraPosZ;
    camera.pitchDegrees = kGoalCameraPitch;
    camera.yawDegrees = kGoalCameraYaw;
    camera.focalLength = kGoalCameraFocal;
}

void startActionIntroCamera(battle::Camera3D& camera, CameraIntroAnimation& anim) {
    anim.active = true;
    anim.elapsed = 0.0f;
    anim.duration = kActionIntroDurationSeconds;

    anim.startX = kGoalCameraPosX + kActionIntroOffsetX;
    anim.startY = kGoalCameraPosY + kActionIntroOffsetY;
    anim.startZ = kGoalCameraPosZ + kActionIntroOffsetZ;
    anim.startPitch = kGoalCameraPitch;
    anim.startYaw = kGoalCameraYaw;
    anim.startFocal = kGoalCameraFocal;

    anim.goalX = kGoalCameraPosX;
    anim.goalY = kGoalCameraPosY;
    anim.goalZ = kGoalCameraPosZ;
    anim.goalPitch = kGoalCameraPitch;
    anim.goalYaw = kGoalCameraYaw;
    anim.goalFocal = kGoalCameraFocal;

    camera.posX = anim.startX;
    camera.posY = anim.startY;
    camera.posZ = anim.startZ;
    camera.pitchDegrees = anim.startPitch;
    camera.yawDegrees = anim.startYaw;
    camera.focalLength = anim.startFocal;
}

void updateActionIntroCamera(battle::Camera3D& camera, CameraIntroAnimation& anim, float deltaSeconds) {
    if (!anim.active) {
        return;
    }

    anim.elapsed += deltaSeconds;
    const float t = battle::easing::clamp01(anim.elapsed / std::max(0.001f, anim.duration));
    const float eased = battle::easing::easeOutCubic(t);

    camera.posX = battle::easing::lerp(anim.startX, anim.goalX, eased);
    camera.posY = battle::easing::lerp(anim.startY, anim.goalY, eased);
    camera.posZ = battle::easing::lerp(anim.startZ, anim.goalZ, eased);
    camera.pitchDegrees = battle::easing::lerp(anim.startPitch, anim.goalPitch, eased);
    camera.yawDegrees = battle::easing::lerp(anim.startYaw, anim.goalYaw, eased);
    camera.focalLength = battle::easing::lerp(anim.startFocal, anim.goalFocal, eased);

    if (t >= 1.0f) {
        anim.active = false;
        applyGoalCamera(camera);
    }
}

SDL_Color colorFromKey(const std::string& key, bool boss) {
    unsigned hash = 2166136261u;
    for (char c : key) {
        hash ^= static_cast<unsigned>(static_cast<unsigned char>(c));
        hash *= 16777619u;
    }
    const Uint8 r = static_cast<Uint8>(80 + (hash & 0x7F));
    const Uint8 g = static_cast<Uint8>(80 + ((hash >> 8) & 0x7F));
    const Uint8 b = static_cast<Uint8>(80 + ((hash >> 16) & 0x7F));
    return boss ? SDL_Color{static_cast<Uint8>(std::min(255, r + 30)), 90, 90, 255} : SDL_Color{r, g, b, 255};
}

SDL_Texture* createFloorTileTexture(SDL_Renderer* renderer) {
    constexpr int texSize = 64;
    constexpr int cell = 16;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, texSize, texSize, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return nullptr;
    }

    const Uint32 c0 = SDL_MapRGBA(surface->format, 46, 49, 60, 255);
    const Uint32 c1 = SDL_MapRGBA(surface->format, 52, 56, 69, 255);

    SDL_Rect r{0, 0, cell, cell};
    for (int y = 0; y < texSize; y += cell) {
        for (int x = 0; x < texSize; x += cell) {
            r.x = x;
            r.y = y;
            const bool alt = ((x / cell) + (y / cell)) % 2 == 0;
            SDL_FillRect(surface, &r, alt ? c0 : c1);
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

std::optional<SDL_Texture*> tryLoadTexture(SDL_Renderer* renderer, const std::string& assetName) {
#ifdef BATTLE_ENABLE_IMAGE
    const std::array<std::string, 2> candidates = {
        resolvePath("assets/combat/sprites/" + assetName + ".png"),
        resolvePath("assets/combat/sprites/" + assetName + ".webp")
    };

    for (const std::string& path : candidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface == nullptr) {
            continue;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (texture != nullptr) {
            return texture;
        }
    }
#else
    (void)renderer;
    (void)assetName;
#endif
    return std::nullopt;
}

void drawFloorGridLines(SDL_Renderer* renderer, int screenW, int screenH, const battle::Camera3D& camera) {
    const float floorZ = 0.0f;
    const float centerX = -300.0f;
    const float centerY = 510.0f;
    const float tileSize = 200.0f;
    const int tilesX = 14;
    const int tilesY = 16;

    const float startX = centerX - (tilesX * tileSize * 0.5f);
    const float startY = centerY - (tilesY * tileSize * 0.30f);

    SDL_SetRenderDrawColor(renderer, 62, 70, 90, 160);
    for (int tx = 0; tx <= tilesX; ++tx) {
        const float x = startX + tx * tileSize;
        const SDL_FPoint p0 = camera.worldToScreen(x, startY, floorZ);
        const SDL_FPoint p1 = camera.worldToScreen(x, startY + tilesY * tileSize, floorZ);
        if ((p0.x > -200.0f || p1.x > -200.0f) && (p0.x < screenW + 200.0f || p1.x < screenW + 200.0f) &&
            (p0.y > -200.0f || p1.y > -200.0f) && (p0.y < screenH + 200.0f || p1.y < screenH + 200.0f)) {
            SDL_RenderDrawLineF(renderer, p0.x, p0.y, p1.x, p1.y);
        }
    }
    for (int ty = 0; ty <= tilesY; ++ty) {
        const float y = startY + ty * tileSize;
        const SDL_FPoint p0 = camera.worldToScreen(startX, y, floorZ);
        const SDL_FPoint p1 = camera.worldToScreen(startX + tilesX * tileSize, y, floorZ);
        if ((p0.x > -200.0f || p1.x > -200.0f) && (p0.x < screenW + 200.0f || p1.x < screenW + 200.0f) &&
            (p0.y > -200.0f || p1.y > -200.0f) && (p0.y < screenH + 200.0f || p1.y < screenH + 200.0f)) {
            SDL_RenderDrawLineF(renderer, p0.x, p0.y, p1.x, p1.y);
        }
    }
}

void drawFloor(SDL_Renderer* renderer, int screenW, int screenH, const battle::Camera3D& camera, SDL_Texture* floorTileTexture) {
    if (floorTileTexture != nullptr) {
        const float floorZ = 0.0f;
        const float centerX = -300.0f;
        const float centerY = 510.0f;
        const float tileSize = 200.0f;
        const int tilesX = 14;
        const int tilesY = 16;

        const float startX = centerX - (tilesX * tileSize * 0.5f);
        const float startY = centerY - (tilesY * tileSize * 0.30f);

        const int indices[6] = {0, 1, 2, 0, 2, 3};
        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                const float x0 = startX + tx * tileSize;
                const float y0 = startY + ty * tileSize;
                const float x1 = x0 + tileSize;
                const float y1 = y0 + tileSize;

                const float d00 = camera.getDepth(x0, y0, floorZ);
                const float d10 = camera.getDepth(x1, y0, floorZ);
                const float d11 = camera.getDepth(x1, y1, floorZ);
                const float d01 = camera.getDepth(x0, y1, floorZ);
                if (d00 <= 1.0f && d10 <= 1.0f && d11 <= 1.0f && d01 <= 1.0f) {
                    continue;
                }

                const SDL_FPoint p00 = camera.worldToScreen(x0, y0, floorZ);
                const SDL_FPoint p10 = camera.worldToScreen(x1, y0, floorZ);
                const SDL_FPoint p11 = camera.worldToScreen(x1, y1, floorZ);
                const SDL_FPoint p01 = camera.worldToScreen(x0, y1, floorZ);

                const float minX = std::min(std::min(p00.x, p10.x), std::min(p11.x, p01.x));
                const float maxX = std::max(std::max(p00.x, p10.x), std::max(p11.x, p01.x));
                const float minY = std::min(std::min(p00.y, p10.y), std::min(p11.y, p01.y));
                const float maxY = std::max(std::max(p00.y, p10.y), std::max(p11.y, p01.y));
                if (maxX < -200.0f || minX > screenW + 200.0f || maxY < -200.0f || minY > screenH + 200.0f) {
                    continue;
                }

                SDL_Vertex verts[4];
                verts[0].position = p00;
                verts[1].position = p10;
                verts[2].position = p11;
                verts[3].position = p01;
                verts[0].color = SDL_Color{255, 255, 255, 255};
                verts[1].color = SDL_Color{255, 255, 255, 255};
                verts[2].color = SDL_Color{255, 255, 255, 255};
                verts[3].color = SDL_Color{255, 255, 255, 255};
                verts[0].tex_coord = SDL_FPoint{0.0f, 0.0f};
                verts[1].tex_coord = SDL_FPoint{1.0f, 0.0f};
                verts[2].tex_coord = SDL_FPoint{1.0f, 1.0f};
                verts[3].tex_coord = SDL_FPoint{0.0f, 1.0f};

                SDL_RenderGeometry(renderer, floorTileTexture, verts, 4, indices, 6);
            }
        }
    }

    drawFloorGridLines(renderer, screenW, screenH, camera);
}

void renderBattleScene(SoftwareSceneRenderer& sceneRenderer,
                       const battle::Camera3D& camera,
                       const std::vector<WorldEntity>& entities,
                       int focusedEntityIndex,
                       float frameAccumulator) {
    SDL_SetRenderDrawBlendMode(sceneRenderer.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sceneRenderer.renderer, 16, 18, 26, 255);
    SDL_RenderClear(sceneRenderer.renderer);

    drawFloor(sceneRenderer.renderer, sceneRenderer.width, sceneRenderer.height, camera, sceneRenderer.floorTileTexture);

    struct DrawCall {
        size_t index;
        float depth;
        SDL_FPoint screen;
    };

    std::vector<DrawCall> drawList;
    drawList.reserve(entities.size());
    for (size_t i = 0; i < entities.size(); ++i) {
        const WorldEntity& entity = entities[i];
        drawList.push_back(DrawCall{
            i,
            camera.getDepth(entity.worldX, entity.worldY, entity.worldZ),
            camera.worldToScreen(entity.worldX, entity.worldY, entity.worldZ)
        });
    }

    std::sort(drawList.begin(), drawList.end(), [](const DrawCall& a, const DrawCall& b) {
        return a.depth > b.depth;
    });

    const WorldEntity* focusedEntity = &entities[std::clamp(focusedEntityIndex, 0, static_cast<int>(entities.size() - 1))];

    for (const DrawCall& call : drawList) {
        const WorldEntity& entity = entities[call.index];
        const float scale = camera.getPerspectiveScale(entity.worldX, entity.worldY, entity.worldZ);
        const bool isFocused = (&entity == focusedEntity);
        const float focusScale = isFocused ? 1.13f : 1.0f;

        const int drawW = static_cast<int>(kSuggestedBaseSpriteWidth * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
        const int drawH = static_cast<int>(kSuggestedBaseSpriteHeight * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
        SDL_Rect dst{
            static_cast<int>(call.screen.x) - drawW / 2,
            static_cast<int>(call.screen.y) - drawH,
            std::max(8, drawW),
            std::max(8, drawH)
        };

        SDL_Texture* texture = nullptr;
        const auto texIt = sceneRenderer.textureByAsset.find(entity.assetName);
        if (texIt != sceneRenderer.textureByAsset.end()) {
            texture = texIt->second;
        }

        if (texture != nullptr) {
            int texW = 0;
            int texH = 0;
            SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);
            const int frameCount = texW / kSuggestedBaseSpriteWidth;
            const bool isAnimated = (frameCount > 1) && (texW % kSuggestedBaseSpriteWidth == 0);
            if (isAnimated) {
                const int currentFrame = static_cast<int>(frameAccumulator / kSpriteFrameTime) % frameCount;
                SDL_Rect srcRect{currentFrame * kSuggestedBaseSpriteWidth, 0, kSuggestedBaseSpriteWidth, texH};
                SDL_RenderCopy(sceneRenderer.renderer, texture, &srcRect, &dst);
            } else {
                SDL_RenderCopy(sceneRenderer.renderer, texture, nullptr, &dst);
            }
        } else {
            SDL_SetRenderDrawColor(sceneRenderer.renderer, entity.fallbackColor.r, entity.fallbackColor.g, entity.fallbackColor.b, 255);
            SDL_RenderFillRect(sceneRenderer.renderer, &dst);
            SDL_SetRenderDrawColor(sceneRenderer.renderer, 16, 16, 20, 255);
            SDL_RenderDrawRect(sceneRenderer.renderer, &dst);
        }

        if (isFocused) {
            SDL_SetRenderDrawColor(sceneRenderer.renderer, 250, 230, 96, 255);
            SDL_Rect ring{dst.x - 6, dst.y - 6, dst.w + 12, dst.h + 12};
            SDL_RenderDrawRect(sceneRenderer.renderer, &ring);
        }
    }

    SDL_RenderPresent(sceneRenderer.renderer);
}

bool SoftwareSceneRenderer::initialize(int newWidth, int newHeight, const std::vector<std::string>& assetNames) {
    destroy();

    width = newWidth;
    height = newHeight;
    loadedAssets = assetNames;

    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        std::cerr << "Failed to create software surface: " << SDL_GetError() << "\n";
        return false;
    }

    renderer = SDL_CreateSoftwareRenderer(surface);
    if (renderer == nullptr) {
        std::cerr << "Failed to create software renderer: " << SDL_GetError() << "\n";
        destroy();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    floorTileTexture = createFloorTileTexture(renderer);

    for (const std::string& assetName : loadedAssets) {
        const auto loaded = tryLoadTexture(renderer, assetName);
        textureByAsset[assetName] = loaded.has_value() ? *loaded : nullptr;
    }

    return true;
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<size_t>(std::max(1, logLength)), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    std::cerr << "Shader compilation failed: " << log << "\n";
    glDeleteShader(shader);
    return 0;
}

bool GlScreenBlitter::initialize() {
    static const char* kVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 in_position;
        layout (location = 1) in vec2 in_uv;
        out vec2 frag_uv;
        void main() {
            frag_uv = in_uv;
            gl_Position = vec4(in_position, 0.0, 1.0);
        }
    )";

    static const char* kFragmentShader = R"(
        #version 330 core
        in vec2 frag_uv;
        uniform sampler2D scene_texture;
        out vec4 out_color;
        void main() {
            out_color = texture(scene_texture, frag_uv);
        }
    )";

    vertexShader = compileShader(GL_VERTEX_SHADER, kVertexShader);
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertexShader == 0 || fragmentShader == 0) {
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<size_t>(std::max(1, logLength)), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::cerr << "Program link failed: " << log << "\n";
        destroy();
        return false;
    }

    const float quadVertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void GlScreenBlitter::destroy() {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (program != 0) {
        glDeleteProgram(program);
        program = 0;
    }
    if (vertexShader != 0) {
        glDeleteShader(vertexShader);
        vertexShader = 0;
    }
    if (fragmentShader != 0) {
        glDeleteShader(fragmentShader);
        fragmentShader = 0;
    }
    textureWidth = 0;
    textureHeight = 0;
}

void GlScreenBlitter::ensureTextureSize(int width, int height) {
    if (textureWidth == width && textureHeight == height) {
        return;
    }

    textureWidth = width;
    textureHeight = height;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GlScreenBlitter::uploadSurface(SDL_Surface* surface) {
    if (surface == nullptr) {
        return;
    }

    ensureTextureSize(surface->w, surface->h);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface->w, surface->h, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GlScreenBlitter::draw() {
    glDisable(GL_BLEND);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "scene_texture"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

class RenderInterfaceGL3SDL final : public RenderInterface_GL3 {
public:
    Rml::TextureHandle LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) override {
#ifdef BATTLE_ENABLE_IMAGE
        Rml::FileInterface* fileInterface = Rml::GetFileInterface();
        Rml::FileHandle fileHandle = fileInterface->Open(source);
        if (!fileHandle) {
            return {};
        }

        fileInterface->Seek(fileHandle, 0, SEEK_END);
        const size_t bufferSize = fileInterface->Tell(fileHandle);
        fileInterface->Seek(fileHandle, 0, SEEK_SET);

        using Rml::byte;
        Rml::UniquePtr<byte[]> buffer(new byte[bufferSize]);
        fileInterface->Read(buffer.get(), bufferSize, fileHandle);
        fileInterface->Close(fileHandle);

        const size_t extIndex = source.rfind('.');
        const Rml::String extension = (extIndex == Rml::String::npos ? Rml::String() : source.substr(extIndex + 1));

        SDL_Surface* surface = IMG_LoadTyped_RW(SDL_RWFromMem(buffer.get(), static_cast<int>(bufferSize)), 1, extension.c_str());
        if (surface == nullptr) {
            Rml::Log::Message(Rml::Log::LT_ERROR, "Could not load texture: %s", source.c_str());
            return {};
        }

        if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
            SDL_Surface* convertedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(surface);
            if (convertedSurface == nullptr) {
                return {};
            }
            surface = convertedSurface;
        }

        textureDimensions = {surface->w, surface->h};

        byte* pixels = static_cast<byte*>(surface->pixels);
        const size_t pixelBytes = static_cast<size_t>(surface->w) * static_cast<size_t>(surface->h) * 4;
        for (size_t i = 0; i < pixelBytes; i += 4) {
            const byte alpha = pixels[i + 3];
            pixels[i + 0] = byte((int(pixels[i + 0]) * int(alpha)) / 255);
            pixels[i + 1] = byte((int(pixels[i + 1]) * int(alpha)) / 255);
            pixels[i + 2] = byte((int(pixels[i + 2]) * int(alpha)) / 255);
        }

        const Rml::TextureHandle textureHandle = GenerateTexture({pixels, pixelBytes}, textureDimensions);
        SDL_FreeSurface(surface);
        return textureHandle;
#else
        return RenderInterface_GL3::LoadTexture(textureDimensions, source);
#endif
    }
};

} // namespace

int main(int argc, char** argv) {
    std::string bossKey = "lyoo";
    std::vector<std::string> partyKeys = {"iroha", "kaguya", "miku", "cupcakke"};

    if (argc >= 2) {
        bossKey = normalizeCombatKey(argv[1]);
    }
    if (argc >= 3) {
        partyKeys.clear();
        for (int i = 2; i < argc; ++i) {
            partyKeys.emplace_back(normalizeCombatKey(argv[i]));
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

#ifdef BATTLE_ENABLE_IMAGE
    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_WEBP) & (IMG_INIT_PNG | IMG_INIT_WEBP)) == 0) {
        std::cerr << "SDL_image init failed: " << IMG_GetError() << "\n";
    }
#endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "Battle Testing - RmlUi HUD Smoke",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kWindowWidth,
        kWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    if (window == nullptr) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (glContext == nullptr) {
        std::cerr << "GL context creation failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);
    SDL_StopTextInput();

    Rml::String glInitMessage;
    if (!RmlGL3::Initialize(&glInitMessage)) {
        std::cerr << "RmlGL3 initialization failed: " << glInitMessage << "\n";
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GlScreenBlitter screenBlitter;
    if (!screenBlitter.initialize()) {
        std::cerr << "Failed to initialize screen blitter\n";
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SystemInterface_SDL systemInterface;
    systemInterface.SetWindow(window);
    RenderInterfaceGL3SDL renderInterface;
    if (!renderInterface) {
        std::cerr << "RmlUi GL3 render interface construction failed\n";
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Rml::SetSystemInterface(&systemInterface);
    Rml::SetRenderInterface(&renderInterface);
    if (!Rml::Initialise()) {
        std::cerr << "RmlUi core initialization failed\n";
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const std::string fontPath = findFontPath();
    if (!fontPath.empty()) {
        Rml::LoadFontFace(fontPath);
    }

    battle::BattleManager manager;
    if (!manager.initialize(bossKey, partyKeys)) {
        std::cerr << "[Battle] Initialization failed.\n";
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const battle::BattleState& state = manager.getBattleState();
    std::vector<std::string> worldAssets;
    for (const battle::CharacterDefinition& character : state.party) {
        worldAssets.push_back(character.assets);
    }
    worldAssets.push_back(state.boss.assets);

    SoftwareSceneRenderer sceneRenderer;
    if (!sceneRenderer.initialize(kWindowWidth, kWindowHeight, worldAssets)) {
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::vector<WorldEntity> entities;
    entities.reserve(2);
    entities.push_back(WorldEntity{
        state.party.front().key,
        state.party.front().assets,
        false,
        kDuelCharacterSlotX,
        kDuelCharacterBaseY,
        0.0f,
        colorFromKey(state.party.front().key, false)
    });
    entities.push_back(WorldEntity{
        state.boss.key,
        state.boss.assets,
        true,
        kDuelBossSlotX,
        kDuelCharacterBaseY + kBossCharacterDistanceWorld,
        0.0f,
        colorFromKey(state.boss.key, true)
    });

    int windowWidth = kWindowWidth;
    int windowHeight = kWindowHeight;
    renderInterface.SetViewport(windowWidth, windowHeight);
    Rml::Context* context = Rml::CreateContext("battle-smoke", Rml::Vector2i(windowWidth, windowHeight));
    if (context == nullptr) {
        std::cerr << "Failed to create RmlUi context\n";
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const std::string documentPath = resolvePath("assets/rmlui/battle_hud.rml");
    Rml::ElementDocument* document = context->LoadDocument(documentPath);
    if (document == nullptr) {
        std::cerr << "Failed to load RmlUi document: " << documentPath << "\n";
        Rml::Shutdown();
        RmlGL3::Shutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    document->Show();
    updateBattleHudDocument(document, manager);

    battle::Camera3D camera;
    camera.screenCenterX = windowWidth * 0.5f;
    camera.screenCenterY = windowHeight * 0.5f;
    applyGoalCamera(camera);

    CameraIntroAnimation cameraIntro;
    bool freeViewEnabled = false;
    float cameraOscillationTime = 0.0f;
    float frameAccumulator = 0.0f;
    Uint32 lastFrameTime = SDL_GetTicks();
    std::string lastTurnToken;

    bool running = true;
    while (running) {
        manager.processAutomaticTurns();

        const Uint32 currentTime = SDL_GetTicks();
        const float deltaTime = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;
        frameAccumulator += deltaTime;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
                continue;
            }

            RmlSDL::InputEventHandler(context, window, event);

            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_f) {
                    freeViewEnabled = !freeViewEnabled;
                    if (!freeViewEnabled) {
                        applyGoalCamera(camera);
                    }
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    if (manager.executePlayerTurn()) {
                        manager.processAutomaticTurns();
                    }
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                if (freeViewEnabled && !cameraIntro.active) {
                    camera.focalLength += event.wheel.y * 500.0f;
                    camera.focalLength = std::clamp(camera.focalLength, 1000.0f, 50000.0f);
                }
            }

            if (isWindowResizeEvent(event)) {
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                renderInterface.SetViewport(windowWidth, windowHeight);
                context->SetDimensions(Rml::Vector2i(windowWidth, windowHeight));
                camera.screenCenterX = windowWidth * 0.5f;
                camera.screenCenterY = windowHeight * 0.5f;
                if (!sceneRenderer.initialize(windowWidth, windowHeight, worldAssets)) {
                    running = false;
                }
            }
        }

        int previewActorIndex = manager.getPreviewNextActorIndex();
        std::string turnToken = "none";
        bool nextIsCharacter = false;
        if (previewActorIndex >= 0) {
            const battle::TurnState& turnState = manager.getTurnState();
            if (previewActorIndex < static_cast<int>(turnState.actors.size())) {
                const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(previewActorIndex)];
                turnToken = (actor.type == battle::ParticipantType::Boss ? "B:" : "C:") +
                            actor.key + ":" + std::to_string(actor.partyIndex) + ":" +
                            (actor.isExtraTurn ? "E" : "N");
                nextIsCharacter = actor.type == battle::ParticipantType::Character;

                if (nextIsCharacter && actor.partyIndex >= 0 &&
                    actor.partyIndex < static_cast<int>(state.party.size())) {
                    const battle::CharacterDefinition& currentChar = state.party[static_cast<size_t>(actor.partyIndex)];
                    entities[0].key = currentChar.key;
                    entities[0].assetName = currentChar.assets;
                    entities[0].fallbackColor = colorFromKey(currentChar.key, false);
                }
            }
        }

        if (turnToken != lastTurnToken) {
            if (nextIsCharacter && !freeViewEnabled) {
                startActionIntroCamera(camera, cameraIntro);
            }
            lastTurnToken = turnToken;
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float camMovementSpeed = 15.0f;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_W]) camera.posY += camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_S]) camera.posY -= camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_A]) camera.posX -= camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_D]) camera.posX += camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_E]) camera.posZ += camMovementSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_Q]) camera.posZ -= camMovementSpeed;

        const float rotationSpeed = 2.0f;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_LEFT]) camera.yawDegrees -= rotationSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_RIGHT]) camera.yawDegrees += rotationSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_UP]) camera.pitchDegrees -= rotationSpeed;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_DOWN]) camera.pitchDegrees += rotationSpeed;
        camera.pitchDegrees = std::clamp(camera.pitchDegrees, 5.0f, 85.0f);

        updateActionIntroCamera(camera, cameraIntro, deltaTime);

        cameraOscillationTime += deltaTime;
        if (!freeViewEnabled && !cameraIntro.active) {
            applyGoalCamera(camera);
            if (nextIsCharacter) {
                constexpr float kOscillationAmplitudeDegrees = 1.8f;
                constexpr float kOscillationSpeed = 0.55f;
                camera.yawDegrees = kGoalCameraYaw + std::sin(cameraOscillationTime * kOscillationSpeed) * kOscillationAmplitudeDegrees;
            }
        }

        renderBattleScene(sceneRenderer, camera, entities, nextIsCharacter ? 0 : 1, frameAccumulator);
        screenBlitter.uploadSurface(sceneRenderer.surface);

        updateBattleHudDocument(document, manager);

        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.035f, 0.043f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        screenBlitter.draw();

        renderInterface.BeginFrame();
        context->Update();
        context->Render();
        renderInterface.EndFrame();

        SDL_GL_SwapWindow(window);
    }

    document->Close();
    Rml::Shutdown();
    RmlGL3::Shutdown();
#ifdef BATTLE_ENABLE_IMAGE
    IMG_Quit();
#endif
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
