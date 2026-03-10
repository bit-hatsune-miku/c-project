#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif
#ifdef BATTLE_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include "window.h"
#include "game/battle_manager.h"
#include "game/camera_3d.h"
#include "game/battle_ui.h"
#include "game/easing.h"
#include "game/vn_script.h"
#include "game/vn_system.h"

namespace {

// World entity positions
const float kBossCharacterDistanceWorld = 420.0f;
const float kCharacterGapWorld = 1200.0f;
const float kDuelCharacterSlotX = -0.5f * kCharacterGapWorld;
const float kDuelBossSlotX = 0.0f;
const float kDuelCharacterBaseY = 300.0f;

// Sprite baseline
constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kSpriteFrameTime = 0.15f;

// Track elapsed time for frame animation
Uint32 lastFrameTime = SDL_GetTicks();
float frameAccumulator = 0.0f;

// Floor visualization (simple trapezoid for reference)
constexpr float kFloorTopYRatio = 0.35f;
constexpr float kFloorBottomYRatio = 0.85f;
constexpr float kFloorHalfWidthTopRatio = 0.15f;
constexpr float kFloorHalfWidthBottomRatio = 0.45f;
constexpr int kFloorDepthLines = 8;
constexpr int kFloorGridLines = 6;

// Goal camera tuning values
constexpr float kGoalCameraPosX = -405.0f;
constexpr float kGoalCameraPosY = 45.0f;
constexpr float kGoalCameraPosZ = -175.0f;
constexpr float kGoalCameraPitch = 5.0f;
constexpr float kGoalCameraYaw = 4.0f;
constexpr float kGoalCameraFocal = 50000.0f;

// Short action-start intro camera offset (further back-left)
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

struct DialogueLine {
    std::string speaker;
    std::string text;
    std::string iconPath;
};

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

std::string resolvePath(const std::string& relativePath) {
    const std::vector<std::string> candidates = {
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

void drawFloor(SDL_Renderer* renderer, int screenW, int screenH, const battle::Camera3D& camera, SDL_Texture* floorTileTexture) {
    if (floorTileTexture == nullptr) {
        return;
    }

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

std::optional<SDL_Texture*> tryLoadTexture(SDL_Renderer* renderer, const std::string& assetName) {
#ifdef BATTLE_ENABLE_IMAGE
    const std::string pngPath = resolvePath("assets/combat/sprites/" + assetName + ".png");
    if (!std::filesystem::exists(pngPath)) {
        return std::nullopt;
    }
    SDL_Surface* surface = IMG_Load(pngPath.c_str());
    if (surface == nullptr) {
        return std::nullopt;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (tex == nullptr) {
        return std::nullopt;
    }
    return tex;
#else
    (void)renderer;
    (void)assetName;
    return std::nullopt;
#endif
}

std::optional<SDL_Texture*> tryLoadIcon(SDL_Renderer* renderer, const std::string& assetName) {
#ifdef BATTLE_ENABLE_IMAGE
    const std::string pngPath = resolvePath("assets/combat/icons/" + assetName + ".png");
    if (!std::filesystem::exists(pngPath)) {
        return std::nullopt;
    }
    SDL_Surface* surface = IMG_Load(pngPath.c_str());
    if (surface == nullptr) {
        return std::nullopt;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (tex == nullptr) {
        return std::nullopt;
    }
    return tex;
#else
    (void)renderer;
    (void)assetName;
    return std::nullopt;
#endif
}

bool loadDialogueLinesFromScript(const std::string& jsonRelativePath, std::vector<DialogueLine>& outLines) {
    vn::Script script;
    if (!vn::loadScript(resolvePath(jsonRelativePath), script)) {
        std::cerr << "[Demo] Failed to load dialogue script: " << jsonRelativePath << "\n";
        return false;
    }

    outLines.clear();
    outLines.reserve(script.entries.size());
    for (const vn::ScriptEntry& entry : script.entries) {
        DialogueLine line;
        line.speaker = vn::getDisplaySpeakerName(entry);
        line.text = entry.text;
        line.iconPath = entry.icon.empty() ? std::string{} : resolvePath(entry.icon);
        outLines.push_back(std::move(line));
    }

    return !outLines.empty();
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Window window("Demo - Tutorial Battle (Space=dialogue/act, F=toggle freeview)", 1280, 720);
    if (!window.isOpen()) {
        std::cerr << "Failed to initialize window\n";
        return 1;
    }

#ifdef BATTLE_ENABLE_IMAGE
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        std::cerr << "[Demo] SDL_image PNG init failed\n";
    }
#endif

#ifdef BATTLE_ENABLE_TTF
    if (TTF_Init() != 0) {
        std::cerr << "[Demo] SDL_ttf init failed: " << TTF_GetError() << "\n";
    }
#endif

    // Initialize VN system
    if (!vn::initialize(window.getRenderer(), window.getWidth(), window.getHeight())) {
        std::cerr << "[Demo] VN system initialization failed\n";
        return 1;
    }

    // Initialize battle with Cupcakke + Miku vs first Lyoo boss
    battle::BattleManager manager;
    std::vector<std::string> partyKeys = {"miku", "cupcakke"};
    if (!manager.initialize("lyoo", partyKeys)) {
        std::cerr << "[Demo] Battle initialization failed\n";
        return 1;
    }

    const battle::BattleState& state = manager.getBattleState();

    std::vector<WorldEntity> entities;
    entities.reserve(state.party.size() + 1);

    // Use first party member in duel view
    {
        WorldEntity character;
        character.key = state.party[0].key;
        character.assetName = state.party[0].assets;
        character.isBoss = false;
        character.worldX = kDuelCharacterSlotX;
        character.worldY = kDuelCharacterBaseY;
        character.worldZ = 0.0f;
        character.fallbackColor = colorFromKey(character.key, false);
        entities.push_back(character);
    }

    {
        WorldEntity boss;
        boss.key = state.boss.key;
        boss.assetName = state.boss.assets;
        boss.isBoss = true;
        boss.worldX = kDuelBossSlotX;
        boss.worldY = kDuelCharacterBaseY + kBossCharacterDistanceWorld;
        boss.worldZ = 0.0f;
        boss.fallbackColor = colorFromKey(boss.key, true);
        entities.push_back(boss);
    }

    std::map<std::string, SDL_Texture*> textureByAsset;
    for (const battle::CharacterDefinition& c : state.party) {
        if (textureByAsset.find(c.assets) != textureByAsset.end()) {
            continue;
        }
        const auto loaded = tryLoadTexture(window.getRenderer(), c.assets);
        textureByAsset[c.assets] = loaded.has_value() ? *loaded : nullptr;
    }
    if (textureByAsset.find(state.boss.assets) == textureByAsset.end()) {
        const auto loaded = tryLoadTexture(window.getRenderer(), state.boss.assets);
        textureByAsset[state.boss.assets] = loaded.has_value() ? *loaded : nullptr;
    }

    // Load icons for turn order UI
    std::map<std::string, SDL_Texture*> iconByAsset;
    const battle::TurnState& turnState = manager.getTurnState();
    for (const battle::TurnActor& actor : turnState.actors) {
        const std::string iconKey = (actor.type == battle::ParticipantType::Boss) ? "boss_" + actor.key : actor.key;
        if (iconByAsset.find(iconKey) != iconByAsset.end()) {
            continue;
        }
        const auto loaded = tryLoadIcon(window.getRenderer(), actor.key);
        iconByAsset[iconKey] = loaded.has_value() ? *loaded : nullptr;
    }

    SDL_Texture* floorTileTexture = createFloorTileTexture(window.getRenderer());

    // Create Camera3D with default tuning values
    battle::Camera3D camera;
    camera.screenCenterX = window.getWidth() * 0.5f;
    camera.screenCenterY = window.getHeight() * 0.5f;
    applyGoalCamera(camera);

    CameraIntroAnimation cameraIntro;
    std::string lastTurnToken;
    bool freeViewEnabled = false;
    float cameraOscillationTime = 0.0f;

    std::vector<DialogueLine> introDialogueLines;
    std::vector<DialogueLine> postMikuSkillDialogueLines;
    std::vector<DialogueLine> postMikuUltimateDialogueLines;
    std::vector<DialogueLine> postLyooAttackAfterMikuUltimateDialogueLines;
    std::vector<DialogueLine> bossDefeatedDialogueLines;
    if (!loadDialogueLinesFromScript("assets/vn/json/demo.json", introDialogueLines)) {
        return 1;
    }
    if (!loadDialogueLinesFromScript("assets/vn/json/demo_after_miku_first_skill.json", postMikuSkillDialogueLines)) {
        return 1;
    }
    if (!loadDialogueLinesFromScript("assets/vn/json/demo_after_miku_first_ultimate.json", postMikuUltimateDialogueLines)) {
        return 1;
    }
    if (!loadDialogueLinesFromScript("assets/vn/json/demo_after_lyoo_attack_post_miku_ultimate.json", postLyooAttackAfterMikuUltimateDialogueLines)) {
        return 1;
    }
    if (!loadDialogueLinesFromScript("assets/vn/json/demo_boss_defeated.json", bossDefeatedDialogueLines)) {
        return 1;
    }

    // Initialize dialogue state
    bool dialogueInProgress = true;
    bool spaceEnabledForBattle = false;
    bool dialogueClosing = false;  // Intermediate state: dialogue finished but needs one more space to close
    int currentDialogueLine = 0;
    bool hasShownMikuFirstSkillTutorial = false;
    bool hasShownMikuFirstUltimateTutorial = false;
    bool hasShownPostLyooAttackAfterMikuUltimateTutorial = false;
    bool pendingPostLyooAttackAfterMikuUltimateTutorial = false;
    bool hasShownBossDefeatedDialogue = false;
    const std::vector<DialogueLine>* activeDialogueLines = &introDialogueLines;

    auto startDialogueSequence = [&](const std::vector<DialogueLine>& lines) {
        activeDialogueLines = &lines;
        dialogueInProgress = true;
        dialogueClosing = false;
        spaceEnabledForBattle = false;
        currentDialogueLine = 0;

        if (!activeDialogueLines->empty()) {
            const auto& line = (*activeDialogueLines)[0];
            vn::showLine(line.text, line.speaker, line.iconPath);
        }
    };

    startDialogueSequence(*activeDialogueLines);

    bool running = true;
    while (running && window.isOpen()) {
        // Check if battle is over and show victory dialogue
        if (!dialogueInProgress && manager.isBattleOver() && !hasShownBossDefeatedDialogue) {
            hasShownBossDefeatedDialogue = true;
            startDialogueSequence(bossDefeatedDialogueLines);
        }

        // Boss turns are automatic, but only if dialogue is done
        if (!dialogueInProgress) {
            if (pendingPostLyooAttackAfterMikuUltimateTutorial && !hasShownPostLyooAttackAfterMikuUltimateTutorial) {
                const int previewActorIndex = manager.getPreviewNextActorIndex();
                bool nextIsBoss = false;
                if (previewActorIndex >= 0) {
                    const battle::TurnState& ts = manager.getTurnState();
                    if (previewActorIndex < static_cast<int>(ts.actors.size())) {
                        const battle::TurnActor& previewActor = ts.actors[static_cast<size_t>(previewActorIndex)];
                        nextIsBoss = (previewActor.type == battle::ParticipantType::Boss);
                    }
                }

                if (nextIsBoss) {
                    const bool progressed = manager.processAutomaticTurns();
                    if (progressed) {
                        hasShownPostLyooAttackAfterMikuUltimateTutorial = true;
                        pendingPostLyooAttackAfterMikuUltimateTutorial = false;
                        startDialogueSequence(postLyooAttackAfterMikuUltimateDialogueLines);
                    }
                }
            } else {
                manager.processAutomaticTurns();
            }
        }

        // Update frame timing
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;
        frameAccumulator += deltaTime;

        // Update VN system
        vn::update(deltaTime);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_f) {
                    freeViewEnabled = !freeViewEnabled;
                    if (!freeViewEnabled) {
                        applyGoalCamera(camera);
                    }
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    // Space advances dialogue if in progress
                    if (dialogueInProgress) {
                        if (dialogueClosing) {
                            // Dialogue finished and closing - this space press enables battle
                            if (activeDialogueLines == &postMikuUltimateDialogueLines &&
                                !hasShownPostLyooAttackAfterMikuUltimateTutorial) {
                                pendingPostLyooAttackAfterMikuUltimateTutorial = true;
                            }
                            dialogueInProgress = false;
                            dialogueClosing = false;
                            spaceEnabledForBattle = true;
                        } else if (!vn::isLineFinished()) {
                            // First press: finish current line if not done
                            vn::onSpacePressed();
                        } else {
                            // Line is finished, try to advance to next
                            currentDialogueLine++;
                            if (currentDialogueLine < static_cast<int>(activeDialogueLines->size())) {
                                // Display next line
                                const auto& line = (*activeDialogueLines)[static_cast<size_t>(currentDialogueLine)];
                                vn::showLine(line.text, line.speaker, line.iconPath);
                            } else {
                                // All dialogue finished - clear the display and wait for one more space
                                vn::showLine("", "", "");  // Clear the dialogue box
                                dialogueClosing = true;
                            }
                        }
                    } else if (spaceEnabledForBattle) {
                        // Player confirms current character action
                        bool mikuActingNow = false;
                        bool mikuUltimateActingNow = false;
                        const int actorPreviewIndex = manager.getPreviewNextActorIndex();
                        if (actorPreviewIndex >= 0) {
                            const battle::TurnState& ts = manager.getTurnState();
                            if (actorPreviewIndex < static_cast<int>(ts.actors.size())) {
                                const battle::TurnActor& previewActor = ts.actors[static_cast<size_t>(actorPreviewIndex)];
                                mikuActingNow = (previewActor.type == battle::ParticipantType::Character && previewActor.key == "miku");
                                mikuUltimateActingNow = (mikuActingNow && previewActor.isExtraTurn);
                            }
                        }

                        if (manager.executePlayerTurn()) {
                            if (mikuUltimateActingNow && !hasShownMikuFirstUltimateTutorial) {
                                hasShownMikuFirstUltimateTutorial = true;
                                startDialogueSequence(postMikuUltimateDialogueLines);
                            } else if (mikuActingNow && !hasShownMikuFirstSkillTutorial) {
                                hasShownMikuFirstSkillTutorial = true;
                                startDialogueSequence(postMikuSkillDialogueLines);
                            } else {
                                // If we're waiting to show dialogue right after Lyoo's next attack,
                                // don't consume boss turns here. Let the top-of-loop pending trigger
                                // process exactly that boss attack and then start dialogue.
                                if (!pendingPostLyooAttackAfterMikuUltimateTutorial) {
                                    manager.processAutomaticTurns();
                                }
                            }
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                // Mouse wheel adjusts focal length
                if (freeViewEnabled && !cameraIntro.active) {
                    camera.focalLength += event.wheel.y * 500.0f;
                    camera.focalLength = std::clamp(camera.focalLength, 1000.0f, 50000.0f);
                }
            }
        }

        // Trigger a short camera intro whenever a new character action starts (if dialogue done)
        if (!dialogueInProgress) {
            int previewActorIndex = manager.getPreviewNextActorIndex();
            std::string turnToken = "none";
            bool nextIsCharacter = false;
            if (previewActorIndex >= 0) {
                const battle::TurnState& ts = manager.getTurnState();
                if (previewActorIndex < static_cast<int>(ts.actors.size())) {
                    const battle::TurnActor& actor = ts.actors[static_cast<size_t>(previewActorIndex)];
                    turnToken = (actor.type == battle::ParticipantType::Boss ? "B:" : "C:") +
                                actor.key + ":" +
                                std::to_string(actor.partyIndex) + ":" +
                                (actor.isExtraTurn ? "E" : "N");
                    nextIsCharacter = actor.type == battle::ParticipantType::Character;
                }
            }
            if (turnToken != lastTurnToken) {
                if (nextIsCharacter && !freeViewEnabled) {
                    startActionIntroCamera(camera, cameraIntro);
                }
                lastTurnToken = turnToken;
            }
        }

        // WASD Camera Movement (horizontal plane)
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const float camMovementSpeed = 15.0f;
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_W]) {
            camera.posY += camMovementSpeed;
        }
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_S]) {
            camera.posY -= camMovementSpeed;
        }
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_A]) {
            camera.posX -= camMovementSpeed;
        }
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_D]) {
            camera.posX += camMovementSpeed;
        }

        // Arrow keys
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_UP]) {
            camera.posY += camMovementSpeed;
        }
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_DOWN]) {
            camera.posY -= camMovementSpeed;
        }
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_LEFT]) {
            camera.posX -= camMovementSpeed;
        }
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_RIGHT]) {
            camera.posX += camMovementSpeed;
        }

        // Q/E for pitch
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_Q]) {
            camera.pitchDegrees -= 2.0f;
        }
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_E]) {
            camera.pitchDegrees += 2.0f;
        }

        // Update camera animation intro
        updateActionIntroCamera(camera, cameraIntro, deltaTime);

        // Apply oscillation only when character is focused and not in free view
        if (!freeViewEnabled && !cameraIntro.active) {
            applyGoalCamera(camera);
            cameraOscillationTime += deltaTime;
            camera.yawDegrees = kGoalCameraYaw + std::sin(cameraOscillationTime * 0.55f) * 1.8f;
        }

        // Update duel character sprite + focus from current turn actor.
        int focusedEntityIndex = static_cast<int>(entities.size() - 1); // default boss
        const battle::TurnState& liveTurnState = manager.getTurnState();
        const int nextActorIndex = manager.getPreviewNextActorIndex();
        if (nextActorIndex >= 0 && nextActorIndex < static_cast<int>(liveTurnState.actors.size())) {
            const battle::TurnActor& nextActor = liveTurnState.actors[static_cast<size_t>(nextActorIndex)];
            if (nextActor.type == battle::ParticipantType::Character &&
                nextActor.partyIndex >= 0 &&
                nextActor.partyIndex < static_cast<int>(state.party.size())) {
                const battle::CharacterDefinition& currentChar = state.party[static_cast<size_t>(nextActor.partyIndex)];
                entities[0].key = currentChar.key;
                entities[0].assetName = currentChar.assets;
                entities[0].fallbackColor = colorFromKey(currentChar.key, false);
                focusedEntityIndex = 0;
            }
        }
        const WorldEntity* focusedEntity = &entities[std::clamp(focusedEntityIndex, 0, static_cast<int>(entities.size() - 1))];

        // Rendering
        SDL_Renderer* renderer = window.getRenderer();
        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderClear(renderer);

        // Draw floor
        drawFloor(renderer, window.getWidth(), window.getHeight(), camera, floorTileTexture);

        // Draw sprites with depth sorting
        struct DrawCall {
            size_t index;
            float depth;
            SDL_FPoint screen;
        };
        
        std::vector<DrawCall> drawList;
        drawList.reserve(entities.size());
        
        for (size_t i = 0; i < entities.size(); ++i) {
            const WorldEntity& e = entities[i];
            const float depth = camera.getDepth(e.worldX, e.worldY, e.worldZ);
            const SDL_FPoint screen = camera.worldToScreen(e.worldX, e.worldY, e.worldZ);
            drawList.push_back(DrawCall{i, depth, screen});
        }

        // Sort by depth (painter's algorithm)
        std::sort(drawList.begin(), drawList.end(), [](const DrawCall& a, const DrawCall& b) {
            return a.depth > b.depth;
        });

        // Render entities with proper perspective scaling
        for (const DrawCall& call : drawList) {
            const WorldEntity& e = entities[call.index];
            const float scale = camera.getPerspectiveScale(e.worldX, e.worldY, e.worldZ);
            const bool isFocused = (&e == focusedEntity);
            const float focusScale = isFocused ? 1.13f : 1.0f;

            const int drawW = static_cast<int>(kSuggestedBaseSpriteWidth * scale * focusScale * (e.isBoss ? 1.28f : 1.0f));
            const int drawH = static_cast<int>(kSuggestedBaseSpriteHeight * scale * focusScale * (e.isBoss ? 1.28f : 1.0f));

            SDL_Rect dst{
                static_cast<int>(call.screen.x) - drawW / 2,
                static_cast<int>(call.screen.y) - drawH,
                std::max(8, drawW),
                std::max(8, drawH)
            };

            SDL_Texture* tex = textureByAsset[e.assetName];
            if (tex != nullptr) {
                int texW = 0, texH = 0;
                SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH);
                
                // Check if this is a multi-frame sprite
                const int frameCount = texW / kSuggestedBaseSpriteWidth;
                const bool isAnimated = (frameCount > 1) && (texW % kSuggestedBaseSpriteWidth == 0);
                
                if (isAnimated) {
                    // Multi-frame sprite: cycle through frames
                    const int currentFrame = static_cast<int>(frameAccumulator / kSpriteFrameTime) % frameCount;
                    SDL_Rect srcRect{
                        currentFrame * kSuggestedBaseSpriteWidth,
                        0,
                        kSuggestedBaseSpriteWidth,
                        texH
                    };
                    SDL_RenderCopy(renderer, tex, &srcRect, &dst);
                } else {
                    // Single-frame sprite or texture
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                }
            } else {
                // Fallback: solid rectangle
                SDL_SetRenderDrawColor(renderer, e.fallbackColor.r, e.fallbackColor.g, e.fallbackColor.b, 255);
                SDL_RenderFillRect(renderer, &dst);
                SDL_SetRenderDrawColor(renderer, 16, 16, 20, 255);
                SDL_RenderDrawRect(renderer, &dst);
            }

            if (isFocused) {
                SDL_SetRenderDrawColor(renderer, 250, 230, 96, 255);
                SDL_Rect ring{dst.x - 6, dst.y - 6, dst.w + 12, dst.h + 12};
                SDL_RenderDrawRect(renderer, &ring);
            }
        }

        // Draw UI
        int activeActorIndex = manager.getPreviewNextActorIndex();
        battle::ui::drawBossHeaderUI(renderer, window.getWidth(), state, manager.getBossCurrentHp(), manager.getBossMaxHp());
        battle::ui::drawCharacterStatusUI(renderer, window.getWidth(), window.getHeight(), manager, state, iconByAsset);
        battle::ui::drawTurnOrderUI(renderer, manager.getTurnState(), iconByAsset, activeActorIndex);

        // Draw VN dialogue only while visible
        if (dialogueInProgress && !dialogueClosing) {
            vn::render();
        }

        SDL_RenderPresent(renderer);
    }

    vn::shutdown();
    battle::ui::shutdownFonts();
    if (floorTileTexture != nullptr) {
        SDL_DestroyTexture(floorTileTexture);
    }
    for (auto& [key, texture] : textureByAsset) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    for (auto& [key, texture] : iconByAsset) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }

#ifdef BATTLE_ENABLE_TTF
    TTF_Quit();
#endif
#ifdef BATTLE_ENABLE_IMAGE
    IMG_Quit();
#endif

    return 0;
}
