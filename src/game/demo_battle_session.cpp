#include "demo_battle_session.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "battle_manager.h"
#include "battle_ui.h"
#include "camera_3d.h"
#include "easing.h"
#include "vn_script.h"
#include "vn_system.h"

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle::demo {
namespace {

constexpr float kBossCharacterDistanceWorld = 420.0f;
constexpr float kCharacterGapWorld = 1200.0f;
constexpr float kDuelCharacterSlotX = -0.5f * kCharacterGapWorld;
constexpr float kDuelBossSlotX = 0.0f;
constexpr float kDuelCharacterBaseY = 300.0f;
constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kSpriteFrameTime = 0.15f;
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

using DialogueLine = vn::ScriptEntry;

void applyGoalCamera(Camera3D& camera) {
    camera.posX = kGoalCameraPosX;
    camera.posY = kGoalCameraPosY;
    camera.posZ = kGoalCameraPosZ;
    camera.pitchDegrees = kGoalCameraPitch;
    camera.yawDegrees = kGoalCameraYaw;
    camera.focalLength = kGoalCameraFocal;
}

void startActionIntroCamera(Camera3D& camera, CameraIntroAnimation& anim) {
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

void updateActionIntroCamera(Camera3D& camera, CameraIntroAnimation& anim, float deltaSeconds) {
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

void showDialogueLine(const DialogueLine& line) {
    const std::string speakerName = vn::getDisplaySpeakerName(line);
    const std::string iconPath = line.icon.empty() ? std::string{} : resolvePath(line.icon);
    const std::string backgroundPath = line.background.empty() ? std::string{} : resolvePath(line.background);
    const std::string voicePath = line.voice.empty() ? std::string{} : resolvePath(line.voice);
    const std::string fontPath = line.fontPath.empty() ? std::string{} : resolvePath(line.fontPath);

    vn::showLine(
        line.text,
        speakerName,
        iconPath,
        voicePath,
        fontPath,
        line.autoAdvanceOnVoiceEnd,
        line.iconFrameCount,
        line.iconFps,
        backgroundPath
    );
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

    SDL_Rect rect{0, 0, cell, cell};
    for (int y = 0; y < texSize; y += cell) {
        for (int x = 0; x < texSize; x += cell) {
            rect.x = x;
            rect.y = y;
            const bool alt = ((x / cell) + (y / cell)) % 2 == 0;
            SDL_FillRect(surface, &rect, alt ? c0 : c1);
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void drawFloor(SDL_Renderer* renderer, int screenWidth, int screenHeight, const Camera3D& camera, SDL_Texture* floorTileTexture) {
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
            if (maxX < -200.0f || minX > screenWidth + 200.0f || maxY < -200.0f || minY > screenHeight + 200.0f) {
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
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr) {
        return std::nullopt;
    }
    return texture;
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
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr) {
        return std::nullopt;
    }
    return texture;
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
    outLines.insert(outLines.end(), script.entries.begin(), script.entries.end());

    return !outLines.empty();
}

} // namespace

class SessionImpl {
public:
    bool initialize(SDL_Renderer* renderer) {
        shutdown();

        std::vector<std::string> partyKeys = {"miku", "cupcakke"};
        if (!manager.initialize("lyoo", partyKeys)) {
            std::cerr << "[Demo] Battle initialization failed\n";
            return false;
        }

        const BattleState& battleState = manager.getBattleState();
        if (battleState.party.empty()) {
            std::cerr << "[Demo] Battle has no party members\n";
            return false;
        }

        entities.reserve(battleState.party.size() + 1);

        WorldEntity character;
        character.key = battleState.party[0].key;
        character.assetName = battleState.party[0].assets;
        character.isBoss = false;
        character.worldX = kDuelCharacterSlotX;
        character.worldY = kDuelCharacterBaseY;
        character.worldZ = 0.0f;
        character.fallbackColor = colorFromKey(character.key, false);
        entities.push_back(character);

        WorldEntity boss;
        boss.key = battleState.boss.key;
        boss.assetName = battleState.boss.assets;
        boss.isBoss = true;
        boss.worldX = kDuelBossSlotX;
        boss.worldY = kDuelCharacterBaseY + kBossCharacterDistanceWorld;
        boss.worldZ = 0.0f;
        boss.fallbackColor = colorFromKey(boss.key, true);
        entities.push_back(boss);

        for (const CharacterDefinition& currentCharacter : battleState.party) {
            if (textureByAsset.find(currentCharacter.assets) == textureByAsset.end()) {
                const auto loaded = tryLoadTexture(renderer, currentCharacter.assets);
                textureByAsset[currentCharacter.assets] = loaded.has_value() ? *loaded : nullptr;
            }
        }
        if (textureByAsset.find(battleState.boss.assets) == textureByAsset.end()) {
            const auto loaded = tryLoadTexture(renderer, battleState.boss.assets);
            textureByAsset[battleState.boss.assets] = loaded.has_value() ? *loaded : nullptr;
        }

        const TurnState& turnState = manager.getTurnState();
        for (const TurnActor& actor : turnState.actors) {
            const std::string iconKey = (actor.type == ParticipantType::Boss) ? "boss_" + actor.key : actor.key;
            if (iconByAsset.find(iconKey) == iconByAsset.end()) {
                const auto loaded = tryLoadIcon(renderer, actor.key);
                iconByAsset[iconKey] = loaded.has_value() ? *loaded : nullptr;
            }
        }

        floorTileTexture = createFloorTileTexture(renderer);
        applyGoalCamera(camera);

        if (!loadDialogueLinesFromScript("assets/vn/json/demo.json", introDialogueLines) ||
            !loadDialogueLinesFromScript("assets/vn/json/demo_after_miku_first_skill.json", postMikuSkillDialogueLines) ||
            !loadDialogueLinesFromScript("assets/vn/json/demo_after_miku_first_ultimate.json", postMikuUltimateDialogueLines) ||
            !loadDialogueLinesFromScript("assets/vn/json/demo_after_lyoo_attack_post_miku_ultimate.json", postLyooAttackAfterMikuUltimateDialogueLines) ||
            !loadDialogueLinesFromScript("assets/vn/json/demo_boss_defeated.json", bossDefeatedDialogueLines)) {
            shutdown();
            return false;
        }

        dialogueInProgress = true;
        spaceEnabledForBattle = false;
        dialogueClosing = false;
        currentDialogueLine = 0;
        hasShownMikuFirstSkillTutorial = false;
        hasShownMikuFirstUltimateTutorial = false;
        hasShownPostLyooAttackAfterMikuUltimateTutorial = false;
        pendingPostLyooAttackAfterMikuUltimateTutorial = false;
        hasShownBossDefeatedDialogue = false;
        freeViewEnabled = false;
        cameraOscillationTime = 0.0f;
        frameAccumulator = 0.0f;
        cameraIntro = {};
        lastTurnToken.clear();
        finished = false;
        initialized = true;

        startDialogueSequence(introDialogueLines);
        return true;
    }

    void shutdown() {
        for (auto& [_, texture] : textureByAsset) {
            if (texture != nullptr) {
                SDL_DestroyTexture(texture);
            }
        }
        textureByAsset.clear();

        for (auto& [_, texture] : iconByAsset) {
            if (texture != nullptr) {
                SDL_DestroyTexture(texture);
            }
        }
        iconByAsset.clear();

        if (floorTileTexture != nullptr) {
            SDL_DestroyTexture(floorTileTexture);
            floorTileTexture = nullptr;
        }

        battle::ui::shutdownFonts();
        vn::stopVoicePlayback();
        vn::showLine("", "", "");

        entities.clear();
        introDialogueLines.clear();
        postMikuSkillDialogueLines.clear();
        postMikuUltimateDialogueLines.clear();
        postLyooAttackAfterMikuUltimateDialogueLines.clear();
        bossDefeatedDialogueLines.clear();
        activeDialogueLines = nullptr;
        initialized = false;
        finished = false;
    }

    void handleEvent(const SDL_Event& event) {
        if (!initialized || finished) {
            return;
        }

        if (event.type == SDL_KEYDOWN && event.key.repeat != 0) {
            return;
        }

        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                finished = true;
                return;
            }
            if (event.key.keysym.sym == SDLK_f) {
                freeViewEnabled = !freeViewEnabled;
                if (!freeViewEnabled) {
                    applyGoalCamera(camera);
                }
                return;
            }
            if (event.key.keysym.sym != SDLK_SPACE) {
                return;
            }

            if (dialogueInProgress) {
                if (dialogueClosing) {
                    const bool finalVictoryDialogue = activeDialogueLines == &bossDefeatedDialogueLines;
                    dialogueInProgress = false;
                    dialogueClosing = false;
                    spaceEnabledForBattle = !finalVictoryDialogue;
                    if (activeDialogueLines == &postMikuUltimateDialogueLines &&
                        !hasShownPostLyooAttackAfterMikuUltimateTutorial) {
                        pendingPostLyooAttackAfterMikuUltimateTutorial = true;
                    }
                    if (finalVictoryDialogue) {
                        finished = true;
                    }
                } else if (!vn::isLineFinished()) {
                    vn::onSpacePressed();
                } else {
                    ++currentDialogueLine;
                    if (activeDialogueLines != nullptr &&
                        currentDialogueLine < static_cast<int>(activeDialogueLines->size())) {
                        const DialogueLine& line = (*activeDialogueLines)[static_cast<size_t>(currentDialogueLine)];
                        showDialogueLine(line);
                    } else {
                        vn::showLine("", "", "");
                        dialogueClosing = true;
                    }
                }
                return;
            }

            if (!spaceEnabledForBattle || manager.isBattleOver()) {
                return;
            }

            bool mikuActingNow = false;
            bool mikuUltimateActingNow = false;
            const int actorPreviewIndex = manager.getPreviewNextActorIndex();
            if (actorPreviewIndex >= 0) {
                const TurnState& turnState = manager.getTurnState();
                if (actorPreviewIndex < static_cast<int>(turnState.actors.size())) {
                    const TurnActor& previewActor = turnState.actors[static_cast<size_t>(actorPreviewIndex)];
                    mikuActingNow = previewActor.type == ParticipantType::Character && previewActor.key == "miku";
                    mikuUltimateActingNow = mikuActingNow && previewActor.isExtraTurn;
                }
            }

            if (!manager.executePlayerTurn()) {
                return;
            }

            if (mikuUltimateActingNow && !hasShownMikuFirstUltimateTutorial) {
                hasShownMikuFirstUltimateTutorial = true;
                startDialogueSequence(postMikuUltimateDialogueLines);
            } else if (mikuActingNow && !hasShownMikuFirstSkillTutorial) {
                hasShownMikuFirstSkillTutorial = true;
                startDialogueSequence(postMikuSkillDialogueLines);
            } else if (!pendingPostLyooAttackAfterMikuUltimateTutorial) {
                manager.processAutomaticTurns();
            }
            return;
        }

        if (event.type == SDL_MOUSEWHEEL && freeViewEnabled && !cameraIntro.active) {
            camera.focalLength += event.wheel.y * 500.0f;
            camera.focalLength = std::clamp(camera.focalLength, 1000.0f, 50000.0f);
        }
    }

    void update(float deltaSeconds) {
        if (!initialized || finished) {
            return;
        }

        if (!dialogueInProgress && manager.isBattleOver() && !hasShownBossDefeatedDialogue) {
            hasShownBossDefeatedDialogue = true;
            startDialogueSequence(bossDefeatedDialogueLines);
        }

        if (!dialogueInProgress) {
            if (pendingPostLyooAttackAfterMikuUltimateTutorial && !hasShownPostLyooAttackAfterMikuUltimateTutorial) {
                const int previewActorIndex = manager.getPreviewNextActorIndex();
                bool nextIsBoss = false;
                if (previewActorIndex >= 0) {
                    const TurnState& turnState = manager.getTurnState();
                    if (previewActorIndex < static_cast<int>(turnState.actors.size())) {
                        const TurnActor& previewActor = turnState.actors[static_cast<size_t>(previewActorIndex)];
                        nextIsBoss = previewActor.type == ParticipantType::Boss;
                    }
                }

                if (nextIsBoss && manager.processAutomaticTurns()) {
                    hasShownPostLyooAttackAfterMikuUltimateTutorial = true;
                    pendingPostLyooAttackAfterMikuUltimateTutorial = false;
                    startDialogueSequence(postLyooAttackAfterMikuUltimateDialogueLines);
                }
            } else {
                manager.processAutomaticTurns();
            }
        }

        frameAccumulator += deltaSeconds;
        vn::update(deltaSeconds);

        if (!dialogueInProgress) {
            const int previewActorIndex = manager.getPreviewNextActorIndex();
            std::string turnToken = "none";
            bool nextIsCharacter = false;
            if (previewActorIndex >= 0) {
                const TurnState& turnState = manager.getTurnState();
                if (previewActorIndex < static_cast<int>(turnState.actors.size())) {
                    const TurnActor& actor = turnState.actors[static_cast<size_t>(previewActorIndex)];
                    turnToken = (actor.type == ParticipantType::Boss ? "B:" : "C:") +
                                actor.key + ":" +
                                std::to_string(actor.partyIndex) + ":" +
                                (actor.isExtraTurn ? "E" : "N");
                    nextIsCharacter = actor.type == ParticipantType::Character;
                }
            }
            if (turnToken != lastTurnToken) {
                if (nextIsCharacter && !freeViewEnabled) {
                    startActionIntroCamera(camera, cameraIntro);
                }
                lastTurnToken = std::move(turnToken);
            }
        }

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
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_Q]) {
            camera.pitchDegrees -= 2.0f;
        }
        if (freeViewEnabled && !cameraIntro.active && keys[SDL_SCANCODE_E]) {
            camera.pitchDegrees += 2.0f;
        }

        updateActionIntroCamera(camera, cameraIntro, deltaSeconds);

        if (!freeViewEnabled && !cameraIntro.active) {
            applyGoalCamera(camera);
            cameraOscillationTime += deltaSeconds;
            camera.yawDegrees = kGoalCameraYaw + std::sin(cameraOscillationTime * 0.55f) * 1.8f;
        }
    }

    void render(SDL_Renderer* renderer, int screenWidth, int screenHeight) {
        if (!initialized || finished) {
            return;
        }

        const BattleState& battleState = manager.getBattleState();
        if (entities.empty()) {
            return;
        }

        camera.screenCenterX = screenWidth * 0.5f;
        camera.screenCenterY = screenHeight * 0.5f;

        int focusedEntityIndex = static_cast<int>(entities.size() - 1);
        const TurnState& liveTurnState = manager.getTurnState();
        const int nextActorIndex = manager.getPreviewNextActorIndex();
        if (nextActorIndex >= 0 && nextActorIndex < static_cast<int>(liveTurnState.actors.size())) {
            const TurnActor& nextActor = liveTurnState.actors[static_cast<size_t>(nextActorIndex)];
            if (nextActor.type == ParticipantType::Character &&
                nextActor.partyIndex >= 0 &&
                nextActor.partyIndex < static_cast<int>(battleState.party.size())) {
                const CharacterDefinition& currentCharacter = battleState.party[static_cast<size_t>(nextActor.partyIndex)];
                entities[0].key = currentCharacter.key;
                entities[0].assetName = currentCharacter.assets;
                entities[0].fallbackColor = colorFromKey(currentCharacter.key, false);
                focusedEntityIndex = 0;
            }
        }
        const WorldEntity& focusedEntity = entities[static_cast<size_t>(
            std::clamp(focusedEntityIndex, 0, static_cast<int>(entities.size() - 1))
        )];

        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderClear(renderer);

        drawFloor(renderer, screenWidth, screenHeight, camera, floorTileTexture);

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

        std::sort(drawList.begin(), drawList.end(), [](const DrawCall& lhs, const DrawCall& rhs) {
            return lhs.depth > rhs.depth;
        });

        for (const DrawCall& drawCall : drawList) {
            const WorldEntity& entity = entities[drawCall.index];
            const float scale = camera.getPerspectiveScale(entity.worldX, entity.worldY, entity.worldZ);
            const bool isFocused = entity.key == focusedEntity.key && entity.isBoss == focusedEntity.isBoss;
            const float focusScale = isFocused ? 1.13f : 1.0f;

            const int drawWidth = static_cast<int>(kSuggestedBaseSpriteWidth * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));
            const int drawHeight = static_cast<int>(kSuggestedBaseSpriteHeight * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f));

            SDL_Rect dstRect{
                static_cast<int>(drawCall.screen.x) - drawWidth / 2,
                static_cast<int>(drawCall.screen.y) - drawHeight,
                std::max(8, drawWidth),
                std::max(8, drawHeight)
            };

            SDL_Texture* texture = nullptr;
            auto textureIt = textureByAsset.find(entity.assetName);
            if (textureIt != textureByAsset.end()) {
                texture = textureIt->second;
            }

            if (texture != nullptr) {
                int texWidth = 0;
                int texHeight = 0;
                SDL_QueryTexture(texture, nullptr, nullptr, &texWidth, &texHeight);

                const int frameCount = texWidth / kSuggestedBaseSpriteWidth;
                const bool isAnimated = frameCount > 1 && (texWidth % kSuggestedBaseSpriteWidth == 0);
                if (isAnimated) {
                    const int currentFrame = static_cast<int>(frameAccumulator / kSpriteFrameTime) % frameCount;
                    SDL_Rect srcRect{
                        currentFrame * kSuggestedBaseSpriteWidth,
                        0,
                        kSuggestedBaseSpriteWidth,
                        texHeight
                    };
                    SDL_RenderCopy(renderer, texture, &srcRect, &dstRect);
                } else {
                    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
                }
            } else {
                SDL_SetRenderDrawColor(renderer, entity.fallbackColor.r, entity.fallbackColor.g, entity.fallbackColor.b, 255);
                SDL_RenderFillRect(renderer, &dstRect);
                SDL_SetRenderDrawColor(renderer, 16, 16, 20, 255);
                SDL_RenderDrawRect(renderer, &dstRect);
            }

            if (isFocused) {
                SDL_SetRenderDrawColor(renderer, 250, 230, 96, 255);
                SDL_Rect ringRect{dstRect.x - 6, dstRect.y - 6, dstRect.w + 12, dstRect.h + 12};
                SDL_RenderDrawRect(renderer, &ringRect);
            }
        }

        const int activeActorIndex = manager.getPreviewNextActorIndex();
        battle::ui::drawBossHeaderUI(renderer, screenWidth, battleState, manager.getBossCurrentHp(), manager.getBossMaxHp());
        battle::ui::drawCharacterStatusUI(renderer, screenWidth, screenHeight, manager, battleState, iconByAsset);
        battle::ui::drawTurnOrderUI(renderer, manager.getTurnState(), iconByAsset, activeActorIndex);

        if (dialogueInProgress && !dialogueClosing) {
            vn::render();
        }
    }

    bool isFinished() const {
        return finished;
    }

private:
    void startDialogueSequence(const std::vector<DialogueLine>& lines) {
        activeDialogueLines = &lines;
        dialogueInProgress = true;
        dialogueClosing = false;
        spaceEnabledForBattle = false;
        currentDialogueLine = 0;

        if (!activeDialogueLines->empty()) {
            const DialogueLine& line = (*activeDialogueLines)[0];
            showDialogueLine(line);
        }
    }

    bool initialized = false;
    bool finished = false;
    BattleManager manager;
    std::vector<WorldEntity> entities;
    std::map<std::string, SDL_Texture*> textureByAsset;
    std::map<std::string, SDL_Texture*> iconByAsset;
    SDL_Texture* floorTileTexture = nullptr;
    Camera3D camera;
    CameraIntroAnimation cameraIntro;
    std::string lastTurnToken;
    bool freeViewEnabled = false;
    float cameraOscillationTime = 0.0f;
    float frameAccumulator = 0.0f;
    std::vector<DialogueLine> introDialogueLines;
    std::vector<DialogueLine> postMikuSkillDialogueLines;
    std::vector<DialogueLine> postMikuUltimateDialogueLines;
    std::vector<DialogueLine> postLyooAttackAfterMikuUltimateDialogueLines;
    std::vector<DialogueLine> bossDefeatedDialogueLines;
    const std::vector<DialogueLine>* activeDialogueLines = nullptr;
    bool dialogueInProgress = true;
    bool spaceEnabledForBattle = false;
    bool dialogueClosing = false;
    int currentDialogueLine = 0;
    bool hasShownMikuFirstSkillTutorial = false;
    bool hasShownMikuFirstUltimateTutorial = false;
    bool hasShownPostLyooAttackAfterMikuUltimateTutorial = false;
    bool pendingPostLyooAttackAfterMikuUltimateTutorial = false;
    bool hasShownBossDefeatedDialogue = false;
};

Session::Session() : impl_(std::make_unique<SessionImpl>()) {}

Session::~Session() = default;

bool Session::initialize(SDL_Renderer* renderer) {
    return impl_->initialize(renderer);
}

void Session::shutdown() {
    impl_->shutdown();
}

void Session::handleEvent(const SDL_Event& event) {
    impl_->handleEvent(event);
}

void Session::update(float deltaSeconds) {
    impl_->update(deltaSeconds);
}

void Session::render(SDL_Renderer* renderer, int screenWidth, int screenHeight) {
    impl_->render(renderer, screenWidth, screenHeight);
}

bool Session::isFinished() const {
    return impl_->isFinished();
}

} // namespace battle::demo
