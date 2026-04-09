#include "demo_battle_session.h"

#if 0

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/ability_system.h"
#include "core/battle_manager.h"
#include "presentation/ability_presentation.h"
#include "render/battle_ui.h"
#include "render/camera_3d.h"
#include "core/easing.h"
#include "vn/vn_script.h"
#include "vn/vn_system.h"

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
    const std::string backgroundPath = line.background.empty()
        ? std::string{}
        : (vn::isHexColorString(line.background) ? line.background : resolvePath(line.background));
    const std::string voicePath = line.voice.empty() ? std::string{} : resolvePath(line.voice);
    const std::string bgmPath = line.bgm.empty() ? std::string{} : resolvePath(line.bgm);
    const std::string fontPath = line.fontPath.empty() ? std::string{} : resolvePath(line.fontPath);

    if (line.clearBackground) {
        vn::setBackground("");
    }

    vn::showLine(
        line.text,
        speakerName,
        iconPath,
        voicePath,
        fontPath,
        line.autoAdvanceOnVoiceEnd,
        line.iconFrameCount,
        line.iconFps,
        backgroundPath,
        bgmPath,
        line.bgmVolume,
        line.bgmStop,
        line.bgmPause
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
        renderer_ = renderer;

        battle::registerAllPresentations();

        battle::ability::setPresentationInteractionRunner([this](const PresentationContext& context) {
            return runPresentationInteraction(context, manager);
        });

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
            const std::string iconId = actor.assetId.empty() ? actor.key : actor.assetId;
            const std::string iconKey = (actor.type == ParticipantType::Boss) ? "boss_" + iconId : iconId;
            if (iconByAsset.find(iconKey) == iconByAsset.end()) {
                const auto loaded = tryLoadIcon(renderer, iconId);
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
        if (bossKey == "jiafeiBoss") {
            freeViewEnabled = true;
        }
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
        battle::ability::setPresentationInteractionRunner(nullptr);
        renderer_ = nullptr;

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

        vn::stopVoicePlayback();
        vn::showLine("", "", "");
        hud.reset();

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
                bool closedDialogueThisPress = false;
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
                        return;
                    }
                    closedDialogueThisPress = true;
                } else if (!vn::isLineFinished()) {
                    vn::onSpacePressed();
                    return;
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
                    return;
                }

                if (!closedDialogueThisPress) {
                    return;
                }
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
        if (presentationPlaybackActive) {
            if (presentationCasterIsBoss) {
                focusedEntityIndex = static_cast<int>(entities.size() - 1);
            } else if (presentationCasterPartyIndex >= 0 &&
                       presentationCasterPartyIndex < static_cast<int>(battleState.party.size())) {
                const CharacterDefinition& currentCharacter = battleState.party[static_cast<size_t>(presentationCasterPartyIndex)];
                entities[0].key = currentCharacter.key;
                entities[0].assetName = currentCharacter.assets;
                entities[0].fallbackColor = colorFromKey(currentCharacter.key, false);
                focusedEntityIndex = 0;
            }
        } else if (nextActorIndex >= 0 && nextActorIndex < static_cast<int>(liveTurnState.actors.size())) {
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

        hud.syncFromManager(manager);
        hud.draw(renderer, screenWidth, screenHeight, iconByAsset);

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

    float runPresentationInteraction(const PresentationContext& context) {
        if (!initialized || finished || renderer_ == nullptr) {
            return 1.0f;
        }
        if (context.presentationId.empty()) {
            return 1.0f;
        }

        float casterX = kDuelCharacterSlotX;
        float casterY = kDuelCharacterBaseY;
        float casterZ = 0.0f;
        float targetX = kDuelBossSlotX;
        float targetY = kDuelCharacterBaseY + kBossCharacterDistanceWorld;
        float targetZ = 0.0f;

        if (context.isBoss) {
            casterX = kDuelBossSlotX;
            casterY = kDuelCharacterBaseY + kBossCharacterDistanceWorld;
            targetX = kDuelCharacterSlotX;
            targetY = kDuelCharacterBaseY;
        }

        std::unique_ptr<AbilityPresentation> presentation = PresentationRegistry::instance().create(
            context.presentationId,
            casterX, casterY, casterZ,
            targetX, targetY, targetZ
        );
        if (!presentation) {
            std::cerr << "[Presentation] Missing presentation id: " << context.presentationId << "\n";
            return 1.0f;
        }

        std::cout << "[Presentation] Playing: " << context.presentationId << "\n";

        presentationPlaybackActive = true;
        presentationCasterIsBoss = context.isBoss;
        presentationCasterPartyIndex = context.casterIndex;

        presentation->start();
        Uint64 lastCounter = SDL_GetPerformanceCounter();

        while (!finished && !presentation->isComplete()) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    finished = true;
                    break;
                }

                if (event.type == SDL_WINDOWEVENT &&
                    (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                     event.window.event == SDL_WINDOWEVENT_RESIZED)) {
                    vn::setViewportSize(event.window.data1, event.window.data2);
                }

                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        finished = true;
                        break;
                    }
                    if (event.key.keysym.sym == SDLK_SPACE) {
                        presentation->onSpacePressed();
                    }
                    presentation->onKeyPressed(event.key.keysym.sym);
                }
            }

            const Uint64 now = SDL_GetPerformanceCounter();
            const float deltaSeconds = static_cast<float>(now - lastCounter) /
                static_cast<float>(SDL_GetPerformanceFrequency());
            lastCounter = now;

            presentation->update(deltaSeconds);

            Camera3D previousCamera = camera;
            std::vector<WorldEntity> previousEntities = entities;

            if (presentation->overridesCamera()) {
                presentation->applyCameraState(camera);
            }

            float overrideX = 0.0f;
            float overrideY = 0.0f;
            float overrideZ = 0.0f;
            if (presentation->getCasterWorldOverride(overrideX, overrideY, overrideZ)) {
                const size_t casterEntityIndex = context.isBoss ? 1u : 0u;
                if (casterEntityIndex < entities.size()) {
                    entities[casterEntityIndex].worldX = overrideX;
                    entities[casterEntityIndex].worldY = overrideY;
                    entities[casterEntityIndex].worldZ = overrideZ;
                }
            }

            int screenWidth = 1280;
            int screenHeight = 720;
            SDL_GetRendererOutputSize(renderer_, &screenWidth, &screenHeight);
            render(renderer_, screenWidth, screenHeight);
            presentation->render(renderer_, screenWidth, screenHeight, camera);
            SDL_RenderPresent(renderer_);

            entities = std::move(previousEntities);
            camera = previousCamera;
        }

        presentationPlaybackActive = false;
        presentationCasterIsBoss = false;
        presentationCasterPartyIndex = -1;

        return presentation->getInputMultiplier();
    }

    bool initialized = false;
    bool finished = false;
    SDL_Renderer* renderer_ = nullptr;
    BattleManager manager;
    std::vector<WorldEntity> entities;
    std::map<std::string, SDL_Texture*> textureByAsset;
    std::map<std::string, SDL_Texture*> iconByAsset;
    SDL_Texture* floorTileTexture = nullptr;
    ui::BattleHud hud;
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
    bool presentationPlaybackActive = false;
    bool presentationCasterIsBoss = false;
    int presentationCasterPartyIndex = -1;
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

#endif

#include <cctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "audio/battle_voice_arbiter.h"
#include "audio/bgm_player.h"
#include "audio/battle_bgm_controller.h"
#include "audio/wav_one_shot.h"
#include "battle_session_core.h"
#include "core/battle_loader.h"
#include "demo/battle_party_setup.h"
#include "demo/demo_narrative_flow.h"
#include "presentation/ability_presentation.h"
#include "presentation/miku_rhythm_game.h"
#include "vn/vn_system.h"
#include "../platform/path_resolution.h"

namespace battle::demo {

namespace {

game::audio::WavOneShotPlayer gOneShotAudio;
game::audio::WavOneShotPlayer gPresentationSfxAudio;
game::audio::BgmPlayer gBgmPlayer;
game::audio::BgmPlayer gBattleBgmCrossfadePlayer;
game::audio::BgmPlayer gPresentationLoopAudio;

std::string getPresentationCasterVoiceKey(const PresentationContext& context, const BattleManager& manager) {
    if (context.isBoss) {
        const BattleState& battleState = manager.getBattleState();
        if (!battleState.boss.assets.empty()) {
            return battleState.boss.assets;
        }
        return battleState.boss.key;
    }

    const BattleState& battleState = manager.getBattleState();
    if (context.casterIndex < 0 || static_cast<size_t>(context.casterIndex) >= battleState.party.size()) {
        return std::string();
    }

    return manager.resolveCharacterVoiceAssetId(context.casterIndex);
}

bool playCombatVoiceClip(const std::string& assetName, const std::string& clipName, float volume, int repeatCount = 1) {
    if (assetName.empty() || clipName.empty() || repeatCount <= 0) {
        return false;
    }

    if (const auto clipPath = platform::path::resolveCombatVoicePath(assetName, clipName);
        clipPath.has_value()) {
        bool played = false;
        for (int index = 0; index < repeatCount; ++index) {
            if (gOneShotAudio.playWavOneShot(*clipPath, volume)) {
                played = true;
            }
        }
        return played;
    }
    return false;
}

bool playResolvedVoicePath(const std::string& path, float volume, int repeatCount = 1) {
    if (path.empty() || repeatCount <= 0) {
        return false;
    }

    const std::optional<std::string> resolved = platform::path::resolveAudioPath(path);
    if (!resolved.has_value()) {
        return false;
    }

    bool played = false;
    for (int index = 0; index < repeatCount; ++index) {
        if (gOneShotAudio.playWavOneShot(*resolved, volume)) {
            played = true;
        }
    }
    return played;
}

bool playResolvedOneShot(game::audio::WavOneShotPlayer& player,
                         const std::string& path,
                         float volume,
                         bool replaceExisting = true) {
    if (path.empty()) {
        return false;
    }

    const std::optional<std::string> resolved = platform::path::resolveAudioPath(path);
    if (!resolved.has_value()) {
        return false;
    }

    return player.playWavOneShot(*resolved, volume, replaceExisting);
}

bool playResolvedLoop(game::audio::BgmPlayer& player, const std::string& path, float volume) {
    if (path.empty()) {
        return false;
    }

    const std::optional<std::string> resolved = platform::path::resolveAudioPath(path);
    if (!resolved.has_value()) {
        return false;
    }

    return player.play(*resolved, volume);
}

std::optional<std::string> resolveCombatVoiceClipPath(
    const std::string& assetName,
    std::initializer_list<const char*> clipNames) {
    if (assetName.empty()) {
        return std::nullopt;
    }

    for (const char* clipName : clipNames) {
        if (clipName == nullptr || *clipName == '\0') {
            continue;
        }
        if (const auto clipPath = platform::path::resolveCombatVoicePath(assetName, clipName);
            clipPath.has_value()) {
            return *clipPath;
        }
    }

    return std::nullopt;
}

std::string normalizeVoiceLookupToken(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return normalized;
}

std::string combatVoiceSpeakerKey(const CharacterDefinition& character) {
    if (!character.voiceSpeakerId.empty()) {
        return character.voiceSpeakerId;
    }
    if (!character.voiceAssetId.empty()) {
        return character.voiceAssetId;
    }
    if (!character.assets.empty()) {
        return character.assets;
    }
    return character.key;
}

std::string combatVoiceAssetId(const CharacterDefinition& character) {
    if (!character.voiceAssetId.empty()) {
        return character.voiceAssetId;
    }
    if (!character.assets.empty()) {
        return character.assets;
    }
    return character.key;
}

std::string combatVoiceSpeakerKey(const BossDefinition& boss) {
    if (!boss.voiceSpeakerId.empty()) {
        return boss.voiceSpeakerId;
    }
    if (!boss.assets.empty()) {
        return boss.assets;
    }
    return boss.key;
}

bool shouldUseUltimateVoiceClip(const BattleActionEvent& event) {
    return event.action == BattleAction::Ultimate ||
        (event.actorType == ParticipantType::Character &&
         event.actorKey == "zhouShen" &&
         event.abilityId == "ZhouShenBigFishFollowUp");
}

bool shouldUseUltimateVoiceClip(const PresentationContext& context) {
    return context.isUltimate ||
        (!context.isBoss && context.abilityId == "ZhouShenBigFishFollowUp");
}

std::string inferScriptSpeakerKey(const vn::ScriptEntry& entry, const BattleState& battleState) {
    if (!entry.voiceSpeakerId.empty()) {
        return entry.voiceSpeakerId;
    }

    const auto matchesCharacter = [&](const CharacterDefinition& character, const std::string& normalizedToken) {
        return normalizedToken == normalizeVoiceLookupToken(character.assets) ||
            normalizedToken == normalizeVoiceLookupToken(character.voiceAssetId) ||
            normalizedToken == normalizeVoiceLookupToken(character.key) ||
            normalizedToken == normalizeVoiceLookupToken(character.title);
    };

    if (!entry.icon.empty()) {
        const std::string stem = std::filesystem::path(entry.icon).stem().string();
        const std::string normalizedStem = normalizeVoiceLookupToken(stem);
        for (const CharacterDefinition& character : battleState.party) {
            if (matchesCharacter(character, normalizedStem)) {
                return combatVoiceSpeakerKey(character);
            }
        }
        if (normalizedStem == normalizeVoiceLookupToken(battleState.boss.assets) ||
            normalizedStem == normalizeVoiceLookupToken(battleState.boss.key) ||
            normalizedStem == normalizeVoiceLookupToken(battleState.boss.title)) {
            return combatVoiceSpeakerKey(battleState.boss);
        }
    }

    const std::string normalizedSpeaker = normalizeVoiceLookupToken(entry.speaker);
    for (const CharacterDefinition& character : battleState.party) {
        if (matchesCharacter(character, normalizedSpeaker)) {
            return combatVoiceSpeakerKey(character);
        }
    }

    if (normalizedSpeaker == normalizeVoiceLookupToken(battleState.boss.assets) ||
        normalizedSpeaker == normalizeVoiceLookupToken(battleState.boss.key) ||
        normalizedSpeaker == normalizeVoiceLookupToken(battleState.boss.title)) {
        return combatVoiceSpeakerKey(battleState.boss);
    }

    return std::string();
}

void stopPresentationAudioPlayback(bool resumeBgm = false) {
    gPresentationLoopAudio.stop();
    gPresentationSfxAudio.stopAllPlayback();
    if (resumeBgm) {
        gBgmPlayer.resume();
    }
}

bool playBossHitVoice(const BattleState& battleState, float volume, int repeatCount = 1) {
    if (playResolvedVoicePath(battleState.boss.voiceHit, volume, repeatCount)) {
        return true;
    }

    if (playCombatVoiceClip(battleState.boss.assets, "hit", volume, repeatCount)) {
        return true;
    }

    return playCombatVoiceClip(battleState.boss.key, "hit", volume, repeatCount);
}

std::optional<std::string> resolveBossHitVoicePath(const BattleState& battleState) {
    if (!battleState.boss.voiceHit.empty()) {
        const std::string resolved = platform::path::resolvePath(battleState.boss.voiceHit);
        if (std::filesystem::exists(resolved)) {
            return resolved;
        }
    }

    if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.boss.assets, "hit"); hitVoice.has_value()) {
        return *hitVoice;
    }
    if (const auto hitVoice = platform::path::resolveCombatVoicePath(battleState.boss.key, "hit"); hitVoice.has_value()) {
        return *hitVoice;
    }
    return std::nullopt;
}

std::optional<std::string> resolveBossDeadVoicePath(const BattleState& battleState) {
    if (const auto deadVoice = platform::path::resolveCombatVoicePath(battleState.boss.assets, "dead"); deadVoice.has_value()) {
        return *deadVoice;
    }
    if (const auto deadVoice = platform::path::resolveCombatVoicePath(battleState.boss.key, "dead"); deadVoice.has_value()) {
        return *deadVoice;
    }
    return std::nullopt;
}

std::optional<std::string> resolveBossHealedVoicePath(const BattleState& battleState) {
    if (const auto healedVoice = platform::path::resolveCombatVoicePath(battleState.boss.assets, "healed");
        healedVoice.has_value()) {
        return *healedVoice;
    }
    if (const auto healedVoice = platform::path::resolveCombatVoicePath(battleState.boss.key, "healed");
        healedVoice.has_value()) {
        return *healedVoice;
    }
    return std::nullopt;
}

void consumeBattleActionEvents(BattleManager& manager, float voiceVolume) {
    const BattleState& battleState = manager.getBattleState();
    for (const BattleActionEvent& event : manager.getRecentActionEvents()) {
        const std::string actorVoiceKey = event.actorType == ParticipantType::Boss
            ? battleState.boss.key
            : ((event.actorPartyIndex >= 0 && event.actorPartyIndex < static_cast<int>(battleState.party.size()))
                ? combatVoiceAssetId(battleState.party[static_cast<size_t>(event.actorPartyIndex)])
                : std::string());

        if (!event.abilityVoicesHandledDuringPresentation &&
            event.action == BattleAction::Skill &&
            shouldUseUltimateVoiceClip(event)) {
            if (const auto ultimateVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "ultimate");
                ultimateVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*ultimateVoice, voiceVolume);
            } else if (const auto abilityVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "ability");
                       abilityVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*abilityVoice, voiceVolume);
            }
        } else if (!event.abilityVoicesHandledDuringPresentation && event.action == BattleAction::Skill) {
            if (const auto abilityVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "ability");
                abilityVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*abilityVoice, voiceVolume);
            } else if (const auto skillVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "skill");
                       skillVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*skillVoice, voiceVolume);
            }
        } else if (!event.abilityVoicesHandledDuringPresentation && event.action == BattleAction::Ultimate) {
            if (const auto ultimateVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "ultimate");
                ultimateVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*ultimateVoice, voiceVolume);
            } else if (const auto abilityVoice = platform::path::resolveCombatVoicePath(actorVoiceKey, "ability");
                       abilityVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*abilityVoice, voiceVolume);
            }
        }

        if (!event.hitVoicesHandledDuringPresentation && event.bossHpAfter < event.bossHpBefore) {
            if (event.bossHpBefore > 0 && event.bossHpAfter <= 0) {
                if (const auto bossDeadVoicePath = resolveBossDeadVoicePath(battleState); bossDeadVoicePath.has_value()) {
                    if (const auto bossHitVoicePath = resolveBossHitVoicePath(battleState); bossHitVoicePath.has_value()) {
                        gOneShotAudio.stopPlayback(*bossHitVoicePath);
                    }
                    (void)gOneShotAudio.playWavOneShot(*bossDeadVoicePath, voiceVolume);
                } else {
                    (void)playBossHitVoice(battleState, voiceVolume);
                }
            } else {
                (void)playBossHitVoice(battleState, voiceVolume);
            }
        }

        if (event.hitVoicesHandledDuringPresentation) {
            continue;
        }

        for (size_t i = 0; i < event.targetPartyIndices.size() && i < event.targetHpBefore.size() && i < event.targetHpAfter.size(); ++i) {
            if (event.targetHpAfter[i] >= event.targetHpBefore[i]) {
                continue;
            }
            const int partyIndex = event.targetPartyIndices[i];
            if (partyIndex < 0 || partyIndex >= static_cast<int>(battleState.party.size())) {
                continue;
            }
            const std::string assetKey = combatVoiceAssetId(battleState.party[static_cast<size_t>(partyIndex)]);
            if (event.targetHpBefore[i] > 0 && event.targetHpAfter[i] <= 0) {
                if (const auto deadVoice = platform::path::resolveCombatVoicePath(assetKey, "dead"); deadVoice.has_value()) {
                    if (const auto hitVoice = platform::path::resolveCombatVoicePath(assetKey, "hit"); hitVoice.has_value()) {
                        gOneShotAudio.stopPlayback(*hitVoice);
                    }
                    (void)gOneShotAudio.playWavOneShot(*deadVoice, voiceVolume);
                } else if (const auto hitVoice = platform::path::resolveCombatVoicePath(assetKey, "hit"); hitVoice.has_value()) {
                    (void)gOneShotAudio.playWavOneShot(*hitVoice, voiceVolume);
                }
            } else if (const auto hitVoice = platform::path::resolveCombatVoicePath(assetKey, "hit"); hitVoice.has_value()) {
                (void)gOneShotAudio.playWavOneShot(*hitVoice, voiceVolume);
            }
        }
    }

    manager.clearRecentActionEvents();
}

} // namespace

class SessionImpl {
public:
    bool initialize(SDL_Renderer* renderer,
                    const std::string& battleKey,
                    const PlayerProgression& progression,
                    std::optional<std::vector<std::string>> initialPartyLineup) {
        shutdown();
        renderer_ = renderer;

        registerAllPresentations();
        narrativeInitialized_ = false;
        activePartyLineup_.clear();

        const std::string resolvedBattleKey = battleKey.empty() ? "tutorial_vs_lyoo" : battleKey;
        if (!loader::loadBattleDefinition(resolvedBattleKey, battleDefinition_)) {
            return false;
        }

        PlayerProgression effectiveProgression = progression;
        battle::normalizePlayerProgression(
            effectiveProgression,
            progression.unlockedCharacterKeys.empty() && progression.currentPartyLineup.empty()
                ? battle::ProgressionFallbackPolicy::FullRoster
                : battle::ProgressionFallbackPolicy::StarterRoster
        );

        if (initialPartyLineup.has_value()) {
            if (!startBattleWithParty(renderer, *initialPartyLineup)) {
                shutdown();
                return false;
            }
        } else {
            if (!partySetup_.initialize(renderer, battleDefinition_, effectiveProgression)) {
                return false;
            }

            if (!partySetup_.shouldSkipSetup()) {
                initialized_ = true;
                return true;
            }

            PartySetupResult startupSelection;
            if (!partySetup_.consumeStartRequest(startupSelection)) {
                shutdown();
                return false;
            }
            if (!startBattleWithParty(renderer, startupSelection.partyKeys)) {
                shutdown();
                return false;
            }
            partySetup_.complete();
        }

        initialized_ = true;
        return true;
    }

    /**
     * @brief Stops the demo session and resets all session state.
     *
     * Shuts down the party setup, battle core, and narrative systems; stops and detaches the battle BGM controller; clears active party lineup,
     * HP snapshots, presentation audio state, renderer reference, and initialization flags so the session can be safely reinitialized or destroyed.
     */
    void shutdown() {
        partySetup_.shutdown();
        battleDefinition_ = BattleDefinition{};
        core_.shutdown();
        narrative_.shutdown();
        narrative_.setDialogueLinePresenter({});
        voiceArbiter_.stopAll([this](const game::audio::BattleVoicePlayback& playback) {
            stopBattleVoicePlayback(playback);
        });
        voiceArbiter_.clear();
        activeVnVoiceSpeakerKey_.clear();
        battleBgmController_.stop();
        battleBgmController_.detach();
        initialized_ = false;
        narrativeEnabled_ = true;
        narrativeInitialized_ = false;
        presentationAudioSequenceId_.clear();
        presentationAudioCueIndex_ = 0;
        lastPartyHp_.clear();
        lastBossHp_ = 0;
        activePartyLineup_.clear();
        renderer_ = nullptr;
    }

    void handleEvent(const SDL_Event& event) {
        if (!initialized_) {
            return;
        }

        if (partySetup_.isActive()) {
            int windowWidth = lastRenderWidth_;
            int windowHeight = lastRenderHeight_;
            getCurrentWindowSize(windowWidth, windowHeight);
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_ESCAPE) {
                partySetup_.handleEvent(event, windowWidth, windowHeight);
                return;
            }
            partySetup_.handleEvent(event, windowWidth, windowHeight);
            return;
        }

        core_.handleEvent(event);
    }

    void update(float deltaSeconds) {
        if (!initialized_) {
            return;
        }

        if (partySetup_.consumeCancelRequest()) {
            shutdown();
            return;
        }

        PartySetupResult selection;
        if (partySetup_.consumeStartRequest(selection)) {
            if (!startBattleWithParty(renderer_, selection.partyKeys)) {
                shutdown();
                return;
            }
            partySetup_.complete();
        }

        if (partySetup_.isActive()) {
            return;
        }

        if (!narrativeInitialized_ && !core_.isCombatBeginAnimationActive()) {
            narrative_.setDialogueLinePresenter([this](const vn::ScriptEntry& line) {
                presentNarrativeLine(line);
            });
            if (!narrative_.initialize()) {
                shutdown();
                return;
            }
            narrativeInitialized_ = true;
        }

        core_.update(deltaSeconds);
    }

    void render(SDL_Renderer* renderer, int screenWidth, int screenHeight) {
        if (!initialized_) {
            return;
        }

        renderer_ = renderer;
        lastRenderWidth_ = screenWidth;
        lastRenderHeight_ = screenHeight;

        if (partySetup_.isActive()) {
            partySetup_.render(renderer, screenWidth, screenHeight);
            return;
        }

        core_.render(renderer, screenWidth, screenHeight);
    }

    bool isFinished() const {
        return !initialized_ || core_.isFinished();
    }

    BattleOutcome outcome() const {
        if (!initialized_) {
            return BattleOutcome::None;
        }

        const BattleManager& manager = core_.getBattleManager();
        switch (manager.outcome()) {
            case battle::BattleResolvedOutcome::Victory:
                return BattleOutcome::Victory;
            case battle::BattleResolvedOutcome::Defeat:
                return BattleOutcome::Defeat;
            case battle::BattleResolvedOutcome::None:
            default:
                return BattleOutcome::None;
        }
    }

    const std::vector<std::string>& currentPartyLineup() const {
        return activePartyLineup_;
    }

private:
    void syncHpSnapshots(BattleManager& manager) {
        lastBossHp_ = manager.getBossCurrentHp();
        const BattleState& state = manager.getBattleState();
        lastPartyHp_.assign(state.party.size(), 0);
        for (size_t i = 0; i < state.party.size(); ++i) {
            lastPartyHp_[i] = manager.getCharacterCurrentHp(static_cast<int>(i));
        }
    }

    std::string characterVoiceSpeakerKey(const BattleManager& manager, int partyIndex) const {
        const BattleState& battleState = manager.getBattleState();
        if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= battleState.party.size()) {
            return std::string();
        }
        return combatVoiceSpeakerKey(battleState.party[static_cast<size_t>(partyIndex)]);
    }

    std::string bossVoiceSpeakerKey(const BattleManager& manager) const {
        return combatVoiceSpeakerKey(manager.getBattleState().boss);
    }

    std::string presentationCasterVoiceSpeakerKey(const PresentationContext& context,
                                                  const BattleManager& manager) const {
        return context.isBoss ? bossVoiceSpeakerKey(manager) : characterVoiceSpeakerKey(manager, context.casterIndex);
    }

    void syncFinishedBattleVoiceState() {
        if (!activeVnVoiceSpeakerKey_.empty() && !vn::isVoicePlaying()) {
            activeVnVoiceSpeakerKey_.clear();
        }
    }

    bool isBattleVoicePlaybackActive(const game::audio::BattleVoicePlayback& playback) const {
        switch (playback.channel) {
            case game::audio::BattleVoiceChannel::OneShot:
                return gOneShotAudio.isPlaying(game::audio::WavOneShotPlayer::PlaybackHandle{playback.handleId});
            case game::audio::BattleVoiceChannel::Vn:
                return !activeVnVoiceSpeakerKey_.empty() &&
                    activeVnVoiceSpeakerKey_ == playback.speakerKey &&
                    vn::isVoicePlaying();
            default:
                return false;
        }
    }

    void stopBattleVoicePlayback(const game::audio::BattleVoicePlayback& playback) {
        switch (playback.channel) {
            case game::audio::BattleVoiceChannel::OneShot:
                gOneShotAudio.stopPlayback(game::audio::WavOneShotPlayer::PlaybackHandle{playback.handleId});
                break;
            case game::audio::BattleVoiceChannel::Vn:
                vn::stopVoicePlayback();
                if (activeVnVoiceSpeakerKey_ == playback.speakerKey) {
                    activeVnVoiceSpeakerKey_.clear();
                }
                break;
            default:
                break;
        }
    }

    bool requestBattleVoice(const std::string& speakerKey,
                            game::audio::BattleVoiceKind kind,
                            game::audio::BattleVoiceChannel channel,
                            const std::optional<std::string>& resolvedPath,
                            float volume) {
        syncFinishedBattleVoiceState();
        return voiceArbiter_.request(
            {speakerKey, kind, channel},
            {
                [this](const game::audio::BattleVoicePlayback& playback) {
                    return isBattleVoicePlaybackActive(playback);
                },
                [this](const game::audio::BattleVoicePlayback& playback) {
                    stopBattleVoicePlayback(playback);
                },
                [&]() -> std::optional<std::uint64_t> {
                    if (channel != game::audio::BattleVoiceChannel::OneShot ||
                        !resolvedPath.has_value() ||
                        resolvedPath->empty() ||
                        !std::filesystem::exists(*resolvedPath)) {
                        return std::nullopt;
                    }
                    const auto handle = gOneShotAudio.playTrackedWavOneShot(*resolvedPath, volume, false);
                    if (!handle.has_value()) {
                        return std::nullopt;
                    }
                    return handle->id;
                }
            });
    }

    bool requestBattleVoicePath(const std::string& speakerKey,
                                game::audio::BattleVoiceKind kind,
                                const std::string& path,
                                float volume) {
        if (path.empty()) {
            return false;
        }
        const std::string resolved = platform::path::resolvePath(path);
        if (!std::filesystem::exists(resolved)) {
            return false;
        }
        return requestBattleVoice(
            speakerKey,
            kind,
            game::audio::BattleVoiceChannel::OneShot,
            resolved,
            volume);
    }

    bool requestCombatVoiceClip(const std::string& speakerKey,
                                const std::string& assetName,
                                game::audio::BattleVoiceKind kind,
                                float volume,
                                std::initializer_list<const char*> clipNames) {
        const std::optional<std::string> resolvedPath = resolveCombatVoiceClipPath(assetName, clipNames);
        if (!resolvedPath.has_value()) {
            return false;
        }
        return requestBattleVoice(
            speakerKey,
            kind,
            game::audio::BattleVoiceChannel::OneShot,
            *resolvedPath,
            volume);
    }

    bool playBossHitVoice(const BattleManager& manager, float volume) {
        const BattleState& battleState = manager.getBattleState();
        const std::string speakerKey = bossVoiceSpeakerKey(manager);
        if (requestBattleVoicePath(speakerKey, game::audio::BattleVoiceKind::Hit, battleState.boss.voiceHit, volume)) {
            return true;
        }
        if (requestCombatVoiceClip(speakerKey, battleState.boss.assets, game::audio::BattleVoiceKind::Hit, volume, {"hit"})) {
            return true;
        }
        return requestCombatVoiceClip(speakerKey, battleState.boss.key, game::audio::BattleVoiceKind::Hit, volume, {"hit"});
    }

    bool playBossPhaseTransitionVoice(const BattleManager& manager, const std::string& voicePath, float voiceVolume) {
        if (voicePath.empty()) {
            return false;
        }
        return requestBattleVoicePath(
            bossVoiceSpeakerKey(manager),
            game::audio::BattleVoiceKind::PhaseTransition,
            voicePath,
            voiceVolume);
    }

    bool playBossHealedVoice(const BattleManager& manager, float volume) {
        const BattleState& battleState = manager.getBattleState();
        if (const auto healedVoice = resolveBossHealedVoicePath(battleState); healedVoice.has_value()) {
            return requestBattleVoice(
                bossVoiceSpeakerKey(manager),
                game::audio::BattleVoiceKind::Healed,
                game::audio::BattleVoiceChannel::OneShot,
                *healedVoice,
                volume);
        }
        return false;
    }

    bool playPartyVoiceClip(const BattleManager& manager,
                            int partyIndex,
                            game::audio::BattleVoiceKind kind,
                            float volume,
                            std::initializer_list<const char*> clipNames) {
        const BattleState& battleState = manager.getBattleState();
        if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= battleState.party.size()) {
            return false;
        }
        const CharacterDefinition& character = battleState.party[static_cast<size_t>(partyIndex)];
        return requestCombatVoiceClip(combatVoiceSpeakerKey(character),
                                      combatVoiceAssetId(character),
                                      kind,
                                      volume,
                                      clipNames);
    }

    bool lockPartySpeakerState(const BattleManager& manager, int partyIndex, game::audio::BattleVoiceKind kind) {
        return requestBattleVoice(
            characterVoiceSpeakerKey(manager, partyIndex),
            kind,
            game::audio::BattleVoiceChannel::OneShot,
            std::nullopt,
            1.0f);
    }

    bool lockBossSpeakerState(const BattleManager& manager, game::audio::BattleVoiceKind kind) {
        return requestBattleVoice(
            bossVoiceSpeakerKey(manager),
            kind,
            game::audio::BattleVoiceChannel::OneShot,
            std::nullopt,
            1.0f);
    }

    void presentNarrativeLine(const vn::ScriptEntry& line) {
        const BattleState& battleState = core_.getBattleManager().getBattleState();
        const std::string speakerName = vn::getDisplaySpeakerName(line);
        const std::string iconPath = line.icon.empty() ? std::string{} : platform::path::resolvePath(line.icon);
        const std::string backgroundPath = line.background.empty()
            ? std::string{}
            : (vn::isHexColorString(line.background) ? line.background : platform::path::resolvePath(line.background));
        const std::string voicePath = line.voice.empty() ? std::string{} : platform::path::resolvePath(line.voice);
        const std::string bgmPath = line.bgm.empty() ? std::string{} : platform::path::resolvePath(line.bgm);
        const std::string fontPath = line.fontPath.empty() ? std::string{} : platform::path::resolvePath(line.fontPath);
        const std::string speakerKey = inferScriptSpeakerKey(line, battleState);

        if (line.clearBackground) {
            vn::setBackground("");
        }

        bool allowVoice = false;
        if (!voicePath.empty() && std::filesystem::exists(voicePath)) {
            syncFinishedBattleVoiceState();
            allowVoice = voiceArbiter_.request(
                {speakerKey, game::audio::BattleVoiceKind::Dialogue, game::audio::BattleVoiceChannel::Vn},
                {
                    [this](const game::audio::BattleVoicePlayback& playback) {
                        return isBattleVoicePlaybackActive(playback);
                    },
                    [this](const game::audio::BattleVoicePlayback& playback) {
                        stopBattleVoicePlayback(playback);
                    },
                    []() -> std::optional<std::uint64_t> {
                        return 1;
                    }
                });
        }

        vn::showLine(
            line.text,
            speakerName,
            iconPath,
            allowVoice ? voicePath : std::string{},
            fontPath,
            line.autoAdvanceOnVoiceEnd,
            line.iconFrameCount,
            line.iconFps,
            backgroundPath,
            bgmPath,
            line.bgmVolume,
            line.bgmStop,
            line.bgmPause
        );

        activeVnVoiceSpeakerKey_ = allowVoice ? speakerKey : std::string();
    }

    void consumeBattleActionEvents(BattleManager& manager, float voiceVolume) {
        const BattleState& battleState = manager.getBattleState();
        for (const BattleActionEvent& event : manager.getRecentActionEvents()) {
            const bool actorIsBoss = event.actorType == ParticipantType::Boss;
            const std::string actorSpeakerKey = actorIsBoss
                ? bossVoiceSpeakerKey(manager)
                : characterVoiceSpeakerKey(manager, event.actorPartyIndex);
            const std::string actorAssetName = actorIsBoss
                ? battleState.boss.assets
                : ((event.actorPartyIndex >= 0 && static_cast<size_t>(event.actorPartyIndex) < battleState.party.size())
                    ? combatVoiceAssetId(battleState.party[static_cast<size_t>(event.actorPartyIndex)])
                    : std::string());

            if (!event.abilityVoicesHandledDuringPresentation &&
                event.action == BattleAction::Skill &&
                shouldUseUltimateVoiceClip(event)) {
                if (!requestCombatVoiceClip(actorSpeakerKey,
                                            actorAssetName,
                                            game::audio::BattleVoiceKind::Ultimate,
                                            voiceVolume,
                                            {"ultimate", "ability"}) &&
                    actorIsBoss) {
                    (void)requestCombatVoiceClip(actorSpeakerKey,
                                                 battleState.boss.key,
                                                 game::audio::BattleVoiceKind::Ultimate,
                                                 voiceVolume,
                                                 {"ultimate", "ability"});
                }
            } else if (!event.abilityVoicesHandledDuringPresentation && event.action == BattleAction::Skill) {
                if (!requestCombatVoiceClip(actorSpeakerKey,
                                            actorAssetName,
                                            game::audio::BattleVoiceKind::Ability,
                                            voiceVolume,
                                            {"ability", "skill"}) &&
                    actorIsBoss) {
                    (void)requestCombatVoiceClip(actorSpeakerKey,
                                                 battleState.boss.key,
                                                 game::audio::BattleVoiceKind::Ability,
                                                 voiceVolume,
                                                 {"ability", "skill"});
                }
            } else if (!event.abilityVoicesHandledDuringPresentation && event.action == BattleAction::Ultimate) {
                if (!requestCombatVoiceClip(actorSpeakerKey,
                                            actorAssetName,
                                            game::audio::BattleVoiceKind::Ultimate,
                                            voiceVolume,
                                            {"ultimate", "ability"}) &&
                    actorIsBoss) {
                    (void)requestCombatVoiceClip(actorSpeakerKey,
                                                 battleState.boss.key,
                                                 game::audio::BattleVoiceKind::Ultimate,
                                                 voiceVolume,
                                                 {"ultimate", "ability"});
                }
            }

            if (event.bossHpAfter > event.bossHpBefore) {
                (void)playBossHealedVoice(manager, voiceVolume);
            }

            if (!event.hitVoicesHandledDuringPresentation && event.bossHpAfter < event.bossHpBefore) {
                if (event.bossHpBefore > 0 && event.bossHpAfter <= 0) {
                    if (const auto bossDeadVoicePath = resolveBossDeadVoicePath(battleState); bossDeadVoicePath.has_value()) {
                        (void)requestBattleVoice(
                            bossVoiceSpeakerKey(manager),
                            game::audio::BattleVoiceKind::Dead,
                            game::audio::BattleVoiceChannel::OneShot,
                            *bossDeadVoicePath,
                            voiceVolume);
                    } else {
                        (void)lockBossSpeakerState(manager, game::audio::BattleVoiceKind::Dead);
                    }
                } else {
                    (void)playBossHitVoice(manager, voiceVolume);
                }
            }

            const std::size_t targetCount = std::min(
                {event.targetPartyIndices.size(),
                 event.targetHpBefore.size(),
                 event.targetHpAfter.size(),
                 event.targetShieldBefore.size(),
                 event.targetShieldAfter.size()});
            for (std::size_t i = 0; i < targetCount; ++i) {
                const int partyIndex = event.targetPartyIndices[i];
                if (partyIndex < 0 || static_cast<size_t>(partyIndex) >= battleState.party.size()) {
                    continue;
                }

                const int hpBefore = event.targetHpBefore[i];
                const int hpAfter = event.targetHpAfter[i];
                const int shieldBefore = event.targetShieldBefore[i];
                const int shieldAfter = event.targetShieldAfter[i];

                if (hpAfter < hpBefore) {
                    if (event.hitVoicesHandledDuringPresentation) {
                        continue;
                    }

                    if (hpBefore > 0 && hpAfter <= 0) {
                        if (!playPartyVoiceClip(manager, partyIndex, game::audio::BattleVoiceKind::Dead, voiceVolume, {"dead"})) {
                            (void)lockPartySpeakerState(manager, partyIndex, game::audio::BattleVoiceKind::Dead);
                        }
                    } else if (hpAfter > 0) {
                        (void)playPartyVoiceClip(manager, partyIndex, game::audio::BattleVoiceKind::Hit, voiceVolume, {"hit"});
                    }
                    continue;
                }

                if (hpBefore <= 0 && hpAfter > 0) {
                    if (!playPartyVoiceClip(manager, partyIndex, game::audio::BattleVoiceKind::Revived, voiceVolume, {"revived"})) {
                        (void)lockPartySpeakerState(manager, partyIndex, game::audio::BattleVoiceKind::Revived);
                    }
                    continue;
                }

                if (hpAfter > hpBefore) {
                    (void)playPartyVoiceClip(manager, partyIndex, game::audio::BattleVoiceKind::Healed, voiceVolume, {"healed"});
                    continue;
                }

                if (shieldAfter > shieldBefore) {
                    (void)playPartyVoiceClip(manager, partyIndex, game::audio::BattleVoiceKind::Shielded, voiceVolume, {"shielded"});
                }
            }
        }

        manager.clearRecentActionEvents();
    }

    void getCurrentWindowSize(int& outWidth, int& outHeight) const {
        outWidth = lastRenderWidth_;
        outHeight = lastRenderHeight_;
        if (renderer_ == nullptr) {
            return;
        }

#if SDL_VERSION_ATLEAST(2, 0, 22)
        SDL_Window* window = SDL_RenderGetWindow(renderer_);
        if (window != nullptr) {
            SDL_GetWindowSize(window, &outWidth, &outHeight);
        }
#else
        SDL_GetRendererOutputSize(renderer_, &outWidth, &outHeight);
#endif
    }

    /**
     * @brief Initialize and start a battle session using the given party lineup.
     *
     * Sets up battle core hooks (narrative/dialogue gating, audio and presentation handling,
     * update/render/shutdown callbacks), configures BGM/audio controllers, initializes
     * narrative state, and starts the battle with the provided party lineup. If the
     * battle definition specifies an initial boss BGM, that track is started.
     *
     * @param renderer SDL renderer used for battle rendering and presentation.
     * @param lineup Ordered list of party member keys to use for the battle; must be non-empty.
     * @return true if the battle was successfully initialized and started, false otherwise.
     */
    bool startBattleWithParty(SDL_Renderer* renderer, const std::vector<std::string>& lineup) {
        renderer_ = renderer;

        BattleSessionCore::Hooks hooks;
        hooks.isDialogueInProgress = [this]() {
            if (!narrativeEnabled_) {
                return false;
            }
            if (!narrativeInitialized_) {
                return false;
            }
            return narrative_.isDialogueInProgress();
        };
        hooks.onSpacePressed = [this]() {
            if (!narrativeEnabled_ || !narrativeInitialized_) {
                return false;
            }
            if (!narrative_.isDialogueInProgress()) {
                return false;
            }
            narrative_.onDialogueSpacePressed();
            syncFinishedBattleVoiceState();
            return true;
        };
        hooks.isSpaceEnabledForBattle = [this]() {
            if (!narrativeEnabled_) {
                return true;
            }
            if (!narrativeInitialized_) {
                return false;
            }
            return narrative_.isSpaceEnabledForBattle();
        };
        hooks.onPlayerTurnExecuted = [this](const flow::PlayerTurnExecution& turnExecution) {
            if (!narrativeEnabled_ || !narrativeInitialized_) {
                return true;
            }
            return narrative_.onPlayerTurnExecuted(turnExecution);
        };
        hooks.onPreUpdate = [this](BattleManager& manager, float deltaSeconds) {
            battleBgmController_.update(deltaSeconds);
            gOneShotAudio.cleanupFinishedPlayback();
            gPresentationSfxAudio.cleanupFinishedPlayback();
            syncFinishedBattleVoiceState();
            if (!narrativeEnabled_) {
                manager.processAutomaticTurns();
                consumeBattleActionEvents(manager, 1.0f);
                syncHpSnapshots(manager);
                return;
            }
            if (!narrativeInitialized_) {
                consumeBattleActionEvents(manager, 1.0f);
                syncHpSnapshots(manager);
                return;
            }
            vn::update(deltaSeconds);
            syncFinishedBattleVoiceState();
            narrative_.maybeStartBossDefeatedDialogue(manager);
            narrative_.handleAutomaticProgression(manager);
            consumeBattleActionEvents(manager, 1.0f);
            syncHpSnapshots(manager);
        };
        hooks.onPresentationFrameUpdate = [this](float deltaSeconds) {
            battleBgmController_.update(deltaSeconds);
            gOneShotAudio.cleanupFinishedPlayback();
            gPresentationSfxAudio.cleanupFinishedPlayback();
            syncFinishedBattleVoiceState();
        };
        hooks.onBossPhaseTransition = [this](const BossPhaseTransition& transition, BattleManager& manager) {
            (void)transition;
            const std::string bgmName = manager.getCurrentBossBgm();
            if (bgmName.empty()) {
                return;
            }
            const auto bgmPath = platform::path::resolveCombatBgmPath(bgmName);
            if (!bgmPath.has_value()) {
                return;
            }
            battleBgmController_.requestTrack(*bgmPath, manager.getCurrentBossBgmVolume());
            const BattleState& battleState = manager.getBattleState();
            int phaseIndex = transition.toPhaseIndex;
            if (phaseIndex >= 0 && phaseIndex < static_cast<int>(battleState.boss.phases.size())) {
                const std::string& phaseVoice = battleState.boss.phases[phaseIndex].phaseChangeVoice;
                if (!phaseVoice.empty()) {
                    (void)playBossPhaseTransitionVoice(manager, phaseVoice, 1.0f);
                }
            }
        };
        hooks.isBattleFinishBlocked = [](const BattleManager& manager) {
            if (manager.getBossCurrentHp() > 0) {
                return false;
            }

            const BattleState& battleState = manager.getBattleState();
            const auto bossDeadVoicePath = resolveBossDeadVoicePath(battleState);
            return bossDeadVoicePath.has_value() && gOneShotAudio.isPlaying(*bossDeadVoicePath);
        };
        hooks.onPresentationSplashVoice = [this](const PresentationContext& context, const std::string& casterAssets) {
            const std::string speakerKey = presentationCasterVoiceSpeakerKey(context, core_.getBattleManager());
            if (!requestCombatVoiceClip(speakerKey,
                                        casterAssets,
                                        game::audio::BattleVoiceKind::UltimateActivation,
                                        1.0f,
                                        {"ready", "special"}) &&
                context.isBoss) {
                (void)requestCombatVoiceClip(speakerKey,
                                             core_.getBattleManager().getBattleState().boss.key,
                                             game::audio::BattleVoiceKind::UltimateActivation,
                                             1.0f,
                                             {"ready", "special"});
            }
        };
        hooks.onPresentationAudioCommands = [this](const PresentationContext& context,
                                                   const std::vector<PresentationAudioCommand>& commands) {
            (void)context;

            for (const PresentationAudioCommand& command : commands) {
                switch (command.type) {
                    case PresentationAudioCommandType::PlayOneShot:
                        (void)playResolvedOneShot(
                            gPresentationSfxAudio,
                            command.id,
                            command.volume,
                            true
                        );
                        break;
                    case PresentationAudioCommandType::PlayOneShotAllowOverlap:
                        (void)playResolvedOneShot(
                            gPresentationSfxAudio,
                            command.id,
                            command.volume,
                            false
                        );
                        break;
                    case PresentationAudioCommandType::PlayVoiceOneShot:
                        if (!requestBattleVoicePath(
                                presentationCasterVoiceSpeakerKey(context, core_.getBattleManager()),
                                game::audio::BattleVoiceKind::Ability,
                                command.id,
                                std::clamp(command.volume, 0.0f, 1.0f))) {
                            (void)playResolvedOneShot(
                                gPresentationSfxAudio,
                                command.id,
                                command.volume,
                                false
                            );
                        }
                        break;
                    case PresentationAudioCommandType::StartLoop:
                        (void)playResolvedLoop(gPresentationLoopAudio, command.id, command.volume);
                        break;
                    case PresentationAudioCommandType::StopLoop:
                        gPresentationLoopAudio.stop();
                        break;
                    case PresentationAudioCommandType::StopAllSfx:
                        stopPresentationAudioPlayback(false);
                        break;
                    case PresentationAudioCommandType::PauseBgm:
                        battleBgmController_.pauseWithFade();
                        break;
                    case PresentationAudioCommandType::ResumeBgm:
                        battleBgmController_.resumeWithFade();
                        break;
                }
            }
        };
        hooks.onPresentationAbilityAudio = [this](const PresentationContext& context, int cueCount, BattleManager& manager) {
            if (cueCount <= 0) {
                return;
            }
            const BattleState& battleState = manager.getBattleState();
            const std::string speakerKey = presentationCasterVoiceSpeakerKey(context, manager);
            if (context.isBoss && context.presentationId == "qr_code_attack") {
                for (int i = 0; i < cueCount; ++i) {
                    (void)requestCombatVoiceClip(speakerKey,
                                                 battleState.boss.key,
                                                 game::audio::BattleVoiceKind::Ability,
                                                 1.0f,
                                                 {"ability"});
                }
                return;
            }
            if (context.isBoss) {
                const std::string audioSequenceId = speakerKey + "|" + context.presentationId;
                if (presentationAudioSequenceId_ != audioSequenceId) {
                    presentationAudioSequenceId_ = audioSequenceId;
                    presentationAudioCueIndex_ = 0;
                }

                for (int i = 0; i < cueCount; ++i) {
                    ++presentationAudioCueIndex_;
                    if (requestCombatVoiceClip(speakerKey,
                                               battleState.boss.key,
                                               game::audio::BattleVoiceKind::Ability,
                                               1.0f,
                                               {"ability"})) {
                        continue;
                    }
                    (void)requestCombatVoiceClip(speakerKey,
                                                 battleState.boss.assets,
                                                 game::audio::BattleVoiceKind::Ability,
                                                 1.0f,
                                                 {"ability"});
                }
                return;
            }

            const std::string casterAssetName = getPresentationCasterVoiceKey(context, manager);
            const std::string audioSequenceId = speakerKey + "|" + context.presentationId;
            if (presentationAudioSequenceId_ != audioSequenceId) {
                presentationAudioSequenceId_ = audioSequenceId;
                presentationAudioCueIndex_ = 0;
            }

            for (int i = 0; i < cueCount; ++i) {
                ++presentationAudioCueIndex_;

                std::string clipName = "ability";
                if (casterAssetName == "cupcakke" && context.presentationId == "drum_attack") {
                    clipName = (presentationAudioCueIndex_ == 1) ? "ability" : "ability2";
                } else if (casterAssetName == "cupcakke" && context.presentationId == "niagara_falls_ultimate") {
                    clipName = (presentationAudioCueIndex_ == 1) ? "ultimate" : "ability2";
                } else if (shouldUseUltimateVoiceClip(context)) {
                    clipName = "ultimate";
                }

                const game::audio::BattleVoiceKind kind =
                    clipName == "ultimate"
                    ? game::audio::BattleVoiceKind::Ultimate
                    : game::audio::BattleVoiceKind::Ability;
                if (requestCombatVoiceClip(speakerKey, casterAssetName, kind, 1.0f, {clipName.c_str()})) {
                    continue;
                }

                if ((clipName == "ultimate" || clipName == "ability2") &&
                    requestCombatVoiceClip(speakerKey,
                                           casterAssetName,
                                           game::audio::BattleVoiceKind::Ability,
                                           1.0f,
                                           {"ability"})) {
                    continue;
                }
            }
        };
        hooks.onPresentationHealAudio = [](const PresentationContext& context, int hitEvents, BattleManager& manager) {
            (void)context;
            (void)hitEvents;
            (void)manager;
        };
        hooks.onPresentationEnd = [this](const PresentationContext& context, const std::string& resultText) {
            presentationAudioSequenceId_.clear();
            presentationAudioCueIndex_ = 0;
            if (context.presentationId == "boss_attack_lyoo_plot_twist") {
                stopPresentationAudioPlayback(true);
                battleBgmController_.resume();
            }
            if (context.presentationId == "aesthetic_warning") {
                core_.clearHint();
                return;
            }
            if (!resultText.empty() &&
                    (context.isBoss || context.interactionType != InteractionType::None)) {
                core_.setHint(resultText, 2200);
                return;
            }

            if (context.isBoss || context.interactionType != InteractionType::None) {
                core_.clearHint();
            }
        };
        hooks.onPresentationHitAudio = [this](const PresentationContext& context, int hitEvents, BattleManager& manager) {
            const BattleState& battleState = manager.getBattleState();
            if (context.isBoss) {
                auto handlePartyHitAudio = [&](int partyIndex, int repeatCount) {
                    (void)repeatCount;
                    if (partyIndex < 0 || partyIndex >= static_cast<int>(battleState.party.size())) {
                        return;
                    }

                    const int nowHp = manager.getCharacterCurrentHp(partyIndex);
                    const int prevHp =
                        (partyIndex >= 0 && static_cast<size_t>(partyIndex) < lastPartyHp_.size())
                            ? lastPartyHp_[static_cast<size_t>(partyIndex)]
                            : nowHp;
                    if (partyIndex >= 0 && static_cast<size_t>(partyIndex) < lastPartyHp_.size()) {
                        lastPartyHp_[static_cast<size_t>(partyIndex)] = nowHp;
                    }

                    const bool diedNow = prevHp > 0 && nowHp <= 0;
                    if (diedNow) {
                        if (!playPartyVoiceClip(manager, partyIndex, game::audio::BattleVoiceKind::Dead, 1.0f, {"dead"})) {
                            (void)lockPartySpeakerState(manager, partyIndex, game::audio::BattleVoiceKind::Dead);
                        }
                        return;
                    }

                    if (nowHp <= 0 && !diedNow) {
                        return;
                    }

                    (void)playPartyVoiceClip(manager, partyIndex, game::audio::BattleVoiceKind::Hit, 1.0f, {"hit"});
                };

                if (context.targetIndex >= 0 &&
                    context.targetIndex < static_cast<int>(battleState.party.size())) {
                    handlePartyHitAudio(context.targetIndex, hitEvents);
                } else {
                    for (size_t i = 0; i < battleState.party.size(); ++i) {
                        handlePartyHitAudio(static_cast<int>(i), hitEvents);
                    }
                }
                return;
            }

            const int nowBossHp = manager.getBossCurrentHp();
            const int prevBossHp = lastBossHp_;
            lastBossHp_ = nowBossHp;

            const bool bossDiedNow = prevBossHp > 0 && nowBossHp <= 0;
            if (bossDiedNow) {
                if (const auto bossDeadVoice = resolveBossDeadVoicePath(battleState); bossDeadVoice.has_value()) {
                    (void)requestBattleVoice(
                        bossVoiceSpeakerKey(manager),
                        game::audio::BattleVoiceKind::Dead,
                        game::audio::BattleVoiceChannel::OneShot,
                        *bossDeadVoice,
                        1.0f);
                } else {
                    (void)lockBossSpeakerState(manager, game::audio::BattleVoiceKind::Dead);
                }
                return;
            }

            (void)playBossHitVoice(manager, 1.0f);
        };
        hooks.onWindowResized = [](int width, int height) {
            vn::setViewportSize(width, height);
        };
        hooks.onPostRender = [this]() {
            if (!narrativeEnabled_ || !narrativeInitialized_) {
                return;
            }
            if (narrative_.isDialogueInProgress()) {
                vn::render();
            }
        };
        hooks.onShutdown = [this]() {
            voiceArbiter_.stopAll([this](const game::audio::BattleVoicePlayback& playback) {
                stopBattleVoicePlayback(playback);
            });
            voiceArbiter_.clear();
            activeVnVoiceSpeakerKey_.clear();
            vn::stopVoicePlayback();
            gPresentationLoopAudio.stop();
            gPresentationSfxAudio.shutdown();
            battleBgmController_.stop();
            gOneShotAudio.shutdown();
        };
        if (lineup.empty()) {
            return false;
        }

        activePartyLineup_ = lineup;
        battleBgmController_.attach(gBgmPlayer, gBattleBgmCrossfadePlayer);
        battleBgmController_.setMasterVolume(1.0f);

        core_.shutdown();
        narrative_.shutdown();
        presentationAudioSequenceId_.clear();
        presentationAudioCueIndex_ = 0;
        narrativeEnabled_ = (battleDefinition_.type == "tutorial");
        narrativeInitialized_ = false;
        if (!narrativeEnabled_) {
            narrativeInitialized_ = true;
            narrative_.setDialogueLinePresenter({});
        }

        if (!core_.initialize(renderer, battleDefinition_, lineup, std::move(hooks))) {
            narrative_.shutdown();
            return false;
        }

        syncHpSnapshots(core_.getBattleManager());

        // Start boss BGM if defined in boss.json.
        BattleManager& manager = core_.getBattleManager();
        const std::string initialBgmName = manager.getCurrentBossBgm();
        if (!initialBgmName.empty()) {
            if (const auto bgmPath = platform::path::resolveCombatBgmPath(initialBgmName);
                bgmPath.has_value()) {
                battleBgmController_.playImmediate(*bgmPath, manager.getCurrentBossBgmVolume());
            }
        }

        return true;
    }

    bool initialized_ = false;
    bool narrativeEnabled_ = true;
    bool narrativeInitialized_ = false;
    SDL_Renderer* renderer_ = nullptr;
    int lastRenderWidth_ = 1280;
    int lastRenderHeight_ = 720;
    BattleSessionCore core_;
    DemoNarrativeFlow narrative_;
    std::string presentationAudioSequenceId_;
    int presentationAudioCueIndex_ = 0;
    BattleDefinition battleDefinition_;
    BattlePartySetupScreen partySetup_;
    std::vector<std::string> activePartyLineup_;
    std::vector<int> lastPartyHp_;
    int lastBossHp_ = 0;
    game::audio::BattleBgmController battleBgmController_;
    game::audio::BattleVoiceArbiter voiceArbiter_;
    std::string activeVnVoiceSpeakerKey_;
};

/**
 * @brief Construct a new Session and allocate its internal implementation.
 *
 * Creates the Session object and initializes its private implementation pointer
 * using a newly constructed SessionImpl instance.
 */
Session::Session() : impl_(std::make_unique<SessionImpl>()) {}

Session::~Session() = default;

bool Session::initialize(SDL_Renderer* renderer,
                         const std::string& battleKey,
                         const PlayerProgression& progression,
                         std::optional<std::vector<std::string>> initialPartyLineup) {
    return impl_->initialize(renderer, battleKey, progression, std::move(initialPartyLineup));
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

BattleOutcome Session::outcome() const {
    return impl_->outcome();
}

const std::vector<std::string>& Session::currentPartyLineup() const {
    return impl_->currentPartyLineup();
}

} // namespace battle::demo
