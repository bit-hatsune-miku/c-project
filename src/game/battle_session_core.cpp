#include "battle_session_core.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/ability_system.h"
#include "core/easing.h"
#include "core/battle_flow_controller.h"
#include "core/battle_turn_flow.h"
#include "presentation/presentation_runtime.h"
#include "render/battle_asset_loading.h"
#include "render/battle_party_staging.h"
#include "render/battle_world_renderer.h"

namespace battle {

namespace {

constexpr float kBossCharacterDistanceWorld = 420.0f;
constexpr float kCharacterGapWorld          = 1200.0f;
constexpr float kDuelCharacterSlotX         = -0.5f * kCharacterGapWorld;
constexpr float kDuelBossSlotX              = 0.0f;
constexpr float kDuelCharacterBaseY         = 300.0f;
constexpr float kBossPhaseIntroDurationSeconds = 0.90f;
constexpr float kCharacterVisibilityAnimSeconds = 0.22f;
constexpr float kCharacterVisibilityOffsetPx    = 70.0f;
constexpr float kBossPhaseIntroStartOffsetX = -170.0f;
constexpr float kBossPhaseIntroStartOffsetY = 40.0f;
constexpr float kBossPhaseIntroStartOffsetZ = -125.0f;
constexpr float kBossPhaseIntroEndOffsetX = -180.0f;
constexpr float kBossPhaseIntroEndOffsetY = 80.0f;
constexpr float kBossPhaseIntroEndOffsetZ = -129.0f;
constexpr float kBossPhaseIntroLookOffsetY = -10.0f;
constexpr float kBossPhaseIntroLookOffsetZ = -95.0f;
constexpr float kPi = 3.14159265359f;

/**
 * @brief Convert an angle from radians to degrees.
 *
 * @param radians Angle in radians.
 * @return float Angle in degrees.
 */
float radiansToDegrees(float radians) {
    return radians * (180.0f / kPi);
}

using WorldEntity = render::SceneEntity;

/**
 * @brief Maps numeric key presses to a party member index.
 *
 * Corrupted Miku expands the mapping to `1..0` for slots `1..10` and
 * `Shift + 1..0` for slots `11..20`. Other battles keep the legacy `1..4`
 * mapping.
 *
 * @param key SDL key code to map.
 * @param mod SDL modifier mask active for the key event.
 * @param battleDefinition Active battle definition.
 * @return int Mapped party index, or `-1` if the key does not correspond to a manual-ultimate slot.
 */
int manualUltimatePartyIndexFromKey(SDL_Keycode key,
                                    SDL_Keymod mod,
                                    const BattleDefinition& battleDefinition) {
    const auto digitIndexFromKey = [](SDL_Keycode digitKey) -> int {
        switch (digitKey) {
            case SDLK_1:
            case SDLK_KP_1:
                return 0;
            case SDLK_2:
            case SDLK_KP_2:
                return 1;
            case SDLK_3:
            case SDLK_KP_3:
                return 2;
            case SDLK_4:
            case SDLK_KP_4:
                return 3;
            case SDLK_5:
            case SDLK_KP_5:
                return 4;
            case SDLK_6:
            case SDLK_KP_6:
                return 5;
            case SDLK_7:
            case SDLK_KP_7:
                return 6;
            case SDLK_8:
            case SDLK_KP_8:
                return 7;
            case SDLK_9:
            case SDLK_KP_9:
                return 8;
            case SDLK_0:
            case SDLK_KP_0:
                return 9;
            default:
                return -1;
        }
    };

    const int digitIndex = digitIndexFromKey(key);
    if (digitIndex < 0) {
        return -1;
    }

    if (battleDefinition.key == "miku_plot_twist") {
        return (mod & KMOD_SHIFT) != 0 ? digitIndex + 10 : digitIndex;
    }

    return digitIndex < 4 ? digitIndex : -1;
}

/**
 * @brief Produces a short hint string for entering a boss phase.
 *
 * @param phaseIndex Phase transition index: `1` maps to the message for entering phase two, `2` maps to the message for entering phase three; other values produce a generic phase-entry message.
 * @return std::string The hint text to display: "ENTERING PHASE TWO" for `phaseIndex == 1`, "ENTERING PHASE THREE" for `phaseIndex == 2`, or "ENTERING NEW PHASE" otherwise.
 */
std::string bossPhaseHintText(int phaseIndex) {
    switch (phaseIndex) {
        case 1:
            return "ENTERING PHASE TWO";
        case 2:
            return "ENTERING PHASE THREE";
        default:
            return "ENTERING NEW PHASE";
    }
}

/**
 * @brief Creates a camera positioned for the start of a boss phase intro.
 *
 * @param bossEntity The boss world entity whose position is used as the reference.
 * @return Camera3D Camera placed at an offset from the boss with focal length set for the intro.
 */
Camera3D makeBossPhaseIntroStartCamera(const WorldEntity& bossEntity) {
    Camera3D camera;
    camera.posX = bossEntity.worldX + kBossPhaseIntroStartOffsetX;
    camera.posY = bossEntity.worldY + kBossPhaseIntroStartOffsetY;
    camera.posZ = bossEntity.worldZ + kBossPhaseIntroStartOffsetZ;
    camera.focalLength = 38000.0f;
    return camera;
}

/**
 * @brief Creates the target camera used at the end of a boss phase intro.
 *
 * Constructs a Camera3D positioned relative to the provided boss entity using
 * the predefined end-offset constants and a fixed focal length suitable for
 * the boss phase cinematic.
 *
 * @param bossEntity The boss world entity used as the positional anchor.
 * @return Camera3D Camera positioned at the boss-phase intro end offset with the configured focal length.
 */
Camera3D makeBossPhaseIntroEndCamera(const WorldEntity& bossEntity) {
    Camera3D camera;
    camera.posX = bossEntity.worldX + kBossPhaseIntroEndOffsetX;
    camera.posY = bossEntity.worldY + kBossPhaseIntroEndOffsetY;
    camera.posZ = bossEntity.worldZ + kBossPhaseIntroEndOffsetZ;
    camera.focalLength = 36000.0f;
    return camera;
}

/**
 * @brief Orient the camera to look at a target point near the boss.
 *
 * Sets the camera's yawDegrees and pitchDegrees so the camera is aimed at a point
 * offset from the boss entity by the module's boss-intro look offsets. Angles are
 * assigned in degrees.
 *
 * @param camera Camera object to modify (yawDegrees and pitchDegrees will be set).
 * @param bossEntity Boss entity whose world position defines the look target.
 */
void aimCameraAtBossIntroTarget(Camera3D& camera, const WorldEntity& bossEntity) {
    const float lookX = bossEntity.worldX;
    const float lookY = bossEntity.worldY + kBossPhaseIntroLookOffsetY;
    const float lookZ = bossEntity.worldZ + kBossPhaseIntroLookOffsetZ;
    const float dx = lookX - camera.posX;
    const float dy = lookY - camera.posY;
    const float dz = lookZ - camera.posZ;
    const float horizontalDist = std::sqrt((dx * dx) + (dy * dy));
    camera.yawDegrees = radiansToDegrees(std::atan2(-dx, dy));
    camera.pitchDegrees = radiansToDegrees(std::atan2(dz, std::max(1.0f, horizontalDist)));
}

} // namespace

// ---------------------------------------------------------------------------
// initialize / shutdown
/**
 * @brief Initializes the battle session, prepares assets, camera, entities, and hooks.
 *
 * Initializes the internal battle manager and runtime state for a new battle session, registers the ability presentation runner, loads required sprite and icon textures and stage render data, configures camera staging and combat begin animation, and prepares entity and HUD/feedback state for rendering and updates.
 *
 * @param renderer SDL renderer used to load and render textures and sprites.
 * @param battleDefinition Definition of the battle to initialize (boss, stage, and related data).
 * @param partyKeys Ordered list of party member keys used to construct the player party.
 * @param hooks Callback hooks and platform integrations to use during the session.
 * @return bool `true` if initialization completed successfully and the session is ready, `false` on failure.
 */

bool BattleSessionCore::initialize(SDL_Renderer* renderer,
                                   const BattleDefinition& battleDefinition,
                                   const std::vector<std::string>& partyKeys,
                                   Hooks hooks) {
    shutdown();
    renderer_ = renderer;
    hooks_    = std::move(hooks);

    ability::setPresentationInteractionRunner([this](const PresentationContext& context) {
        return runPresentationInteraction(context);
    });

    if (!manager_.initialize(battleDefinition, partyKeys)) {
        std::cerr << "[BattleSessionCore] Battle initialization failed\n";
        return false;
    }

    const BattleState& battleState = manager_.getBattleState();
    if (battleState.party.empty()) {
        std::cerr << "[BattleSessionCore] Battle has no party members\n";
        return false;
    }

    entities_.reserve(battleState.party.size() + 1);

    for (size_t i = 0; i < battleState.party.size(); ++i) {
        WorldEntity character;
        character.key        = battleState.party[i].key;
        character.assetName  = manager_.resolveCharacterAssetId(static_cast<int>(i));
        character.isBoss     = false;
        character.partyIndex = static_cast<int>(i);
        character.worldX     = kDuelCharacterSlotX;
        character.worldY     = kDuelCharacterBaseY;
        character.worldZ     = 0.0f;
        character.fallbackColor = render::colorFromKey(character.key, false);
        entities_.push_back(character);
    }

    WorldEntity boss;
    boss.key        = battleState.boss.key;
    boss.assetName  = battleState.boss.assets;
    boss.isBoss     = true;
    boss.worldX     = kDuelBossSlotX;
    boss.worldY     = kDuelCharacterBaseY + kBossCharacterDistanceWorld;
    boss.worldZ     = 0.0f;
    boss.fallbackColor = render::colorFromKey(boss.key, true);
    entities_.push_back(boss);

    for (size_t i = 0; i < battleState.party.size(); ++i) {
        const std::string assetId = manager_.resolveCharacterAssetId(static_cast<int>(i));
        if (textureByAsset_.find(assetId) == textureByAsset_.end()) {
            const auto loaded = render::tryLoadCombatSpriteTexture(renderer, assetId);
            textureByAsset_[assetId] = loaded.has_value() ? *loaded : nullptr;
        }
    }
    if (textureByAsset_.find(battleState.boss.assets) == textureByAsset_.end()) {
        const auto loaded = render::tryLoadCombatSpriteTexture(renderer, battleState.boss.assets);
        textureByAsset_[battleState.boss.assets] = loaded.has_value() ? *loaded : nullptr;
    }

    if (textureByAsset_.find("miku") == textureByAsset_.end()) {
        const auto loaded = render::tryLoadCombatSpriteTexture(renderer, "miku");
        textureByAsset_["miku"] = loaded.has_value() ? *loaded : nullptr;
    }

    const TurnState& turnState = manager_.getTurnState();
    for (const TurnActor& actor : turnState.actors) {
        const std::string iconId  = actor.assetId.empty() ? actor.key : actor.assetId;
        const std::string iconKey = (actor.type == ParticipantType::Boss) ? "boss_" + iconId : iconId;
        if (iconByAsset_.find(iconKey) == iconByAsset_.end()) {
            const auto loaded = render::tryLoadCombatIconTexture(renderer, iconId);
            iconByAsset_[iconKey] = loaded.has_value() ? *loaded : nullptr;
        }
    }

    if (!render::loadStageRenderData(renderer, battleDefinition.stageKey, stageRenderData_)) {
        std::cerr << "[BattleSessionCore] Stage initialization failed\n";
        return false;
    }
    Camera3D goalCamera = render::makeDefaultBattleCamera();
    render::applyStageCameraOverride(stageRenderData_.definition.camera, goalCamera);
    cameraStaging_.setGoalCamera(goalCamera);
    cameraStaging_.reset(camera_);

    SDL_Texture* mikuSprite = nullptr;
    if (const auto mikuIt = textureByAsset_.find("miku"); mikuIt != textureByAsset_.end()) {
        mikuSprite = mikuIt->second;
    }
    SDL_Texture* bossSprite = nullptr;
    if (const auto bossIt = textureByAsset_.find(battleState.boss.assets); bossIt != textureByAsset_.end()) {
        bossSprite = bossIt->second;
    }
    combatBeginAnimation_.reset(mikuSprite, bossSprite);

    freeViewEnabled_  = false;
    frameAccumulator_ = 0.0f;
    feedback_.reset(manager_);
    activeUltimateTurnSplash_.reset();
    previewUltimateSplashPartyIndex_ = -1;
    bossPhaseIntro_ = BossPhaseIntroState{};
    processedBattleEventCount_ = manager_.getRecentActionEvents().size();
    discardNextUpdateDelta_ = false;
    finished_    = false;
    initialized_ = true;
    return true;
}

/**
 * @brief Releases all runtime resources and resets the battle session to an uninitialized state.
 *
 * Clears camera debug logs, unregisters the presentation runner, destroys cached textures and stage
 * render data, shuts down feedback and HUD state, clears entities and presentation/splash state,
 * invokes the shutdown hook if present, and resets internal counters and flags so the session may be
 * reinitialized or destroyed.
 */
void BattleSessionCore::shutdown() {
    freeViewCameraDebugLog_.clear();
    ability::setPresentationInteractionRunner(nullptr);
    renderer_ = nullptr;

    for (auto& [_, tex] : textureByAsset_) {
        if (tex != nullptr) SDL_DestroyTexture(tex);
    }
    textureByAsset_.clear();

    for (auto& [_, tex] : iconByAsset_) {
        if (tex != nullptr) SDL_DestroyTexture(tex);
    }
    iconByAsset_.clear();

    render::destroyStageRenderData(stageRenderData_);

    feedback_.shutdown();
    hud_.reset();
    entities_.clear();
    activeUltimateTurnSplash_.reset();
    previewUltimateSplashPartyIndex_ = -1;
    bossPhaseIntro_ = BossPhaseIntroState{};

    if (hooks_.onShutdown) hooks_.onShutdown();
    hooks_ = {};
    processedBattleEventCount_ = 0;
    discardNextUpdateDelta_ = false;

    initialized_ = false;
    finished_    = false;
}

// ---------------------------------------------------------------------------
// handleEvent
/**
 * @brief Process a single SDL input event to control the battle session.
 *
 * Handles keyboard and mouse-wheel input to finish the session, toggle and
 * control free camera view, request or start manual ultimate turns, skip or
 * advance ultimate splash animations, and trigger the default player turn (and
 * subsequent automatic turns when allowed). Ignores key-repeat events and
 * consumes input while the combat-begin animation is active.
 *
 * @param event The SDL event to handle.
 */

void BattleSessionCore::handleEvent(const SDL_Event& event) {
    if (!initialized_ || finished_) return;

    if (event.type == SDL_KEYDOWN && event.key.repeat != 0) return;

    if (combatBeginAnimation_.isActive()) {
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            finished_ = true;
        }
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        if (activeUltimateTurnSplash_) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                finished_ = true;
                return;
            }
            if (event.key.keysym.sym == SDLK_SPACE) {
                activeUltimateTurnSplash_->skip();
            }
            (void)handleManualUltimateHotkey(
                event.key.keysym.sym,
                static_cast<SDL_Keymod>(event.key.keysym.mod));
            return;
        }

        if (event.key.keysym.sym == SDLK_ESCAPE) {
            finished_ = true;
            return;
        }
        if (event.key.keysym.sym == SDLK_f) {
            freeViewEnabled_ = !freeViewEnabled_;
            if (!freeViewEnabled_) {
                cameraStaging_.snapToGoalCamera(camera_);
                freeViewCameraDebugLog_.clear();
            }
            return;
        }
        if (handleManualUltimateHotkey(
                event.key.keysym.sym,
                static_cast<SDL_Keymod>(event.key.keysym.mod))) return;
        if (event.key.keysym.sym != SDLK_SPACE) return;

        const flow::PreviewActorContext preview = flow::inspectPreviewActor(manager_);
        if (preview.valid &&
            preview.type == ParticipantType::Character &&
            preview.isExtraTurn &&
            preview.extraTurnAction == BattleAction::Ultimate &&
            !preview.autoExecute &&
            preview.partyIndex != previewUltimateSplashPartyIndex_) {
            maybeStartUltimateTurnSplash(preview, false);
            if (activeUltimateTurnSplash_) {
                return;
            }
        }

        if (hooks_.onSpacePressed && hooks_.onSpacePressed()) return;

        const bool spaceGated = hooks_.isSpaceEnabledForBattle
                                    ? !hooks_.isSpaceEnabledForBattle()
                                    : false;
        if (spaceGated || manager_.isBattleOver()) return;

        const flow::PlayerTurnExecution turnExec = flow::executeDefaultPlayerTurn(manager_);
        if (!turnExec.executed) return;

        const bool allowAutoTurns = hooks_.onPlayerTurnExecuted
                                        ? hooks_.onPlayerTurnExecuted(turnExec)
                                        : true;
        if (allowAutoTurns) manager_.processAutomaticTurns();
        return;
    }

    if (event.type == SDL_MOUSEWHEEL && freeViewEnabled_ && !cameraStaging_.isIntroActive()) {
        camera_.focalLength += event.wheel.y * 500.0f;
        camera_.focalLength = std::clamp(camera_.focalLength, 1000.0f, 50000.0f);
    }
}

// ---------------------------------------------------------------------------
// update
/**
 * @brief Advance the battle session state by a frame.
 *
 * Processes boss-phase transitions and combat-begin animation, updates entity layouts,
 * boss-phase intro camera animation, active ultimate splash animations, camera staging
 * (including optional free-view controls), and feedback synchronization; also evaluates
 * battle completion and updates internal timing accumulators.
 *
 * @param deltaSeconds Time elapsed since the previous update, in seconds.
 */

void BattleSessionCore::update(float deltaSeconds) {
    if (!initialized_ || finished_) return;

    if (combatBeginAnimation_.isActive()) {
        combatBeginAnimation_.update(deltaSeconds);
        return;
    }

    // Ability presentations run their own blocking loop. The next outer-frame delta
    // would otherwise include that whole playback time and instantly consume the
    // short camera intro that starts for the next character turn.
    if (discardNextUpdateDelta_) {
        discardNextUpdateDelta_ = false;
        deltaSeconds = 0.0f;
    }

    while (const std::optional<BossPhaseTransition> transition = manager_.consumeBossPhaseTransition()) {
        startBossPhaseIntro(*transition);
        if (hooks_.onBossPhaseTransition) {
            hooks_.onBossPhaseTransition(*transition, manager_);
        }
    }

    if (bossPhaseIntro_.active) {
        const flow::PreviewActorContext preview = flow::inspectPreviewActor(manager_);
        bool bossActing = true;
        int actingPartyIndex = -1;
        if (preview.valid && preview.type == ParticipantType::Character) {
            bossActing = false;
            actingPartyIndex = preview.partyIndex;
        }
        updateSceneEntities(deltaSeconds, bossActing, actingPartyIndex);
        updateBossPhaseIntro(deltaSeconds);
        feedback_.syncFromManager(manager_, presentationPlaybackActive_);
        feedback_.update(deltaSeconds);
        return;
    }

    if (hooks_.onPreUpdate) hooks_.onPreUpdate(manager_, deltaSeconds);

    const bool dialogueActive = hooks_.isDialogueInProgress && hooks_.isDialogueInProgress();
    const bool bossDefeated = manager_.getBossCurrentHp() <= 0;
    bool bossDeathFadeComplete = true;
    if (bossDefeated) {
        bossDeathFadeComplete = false;
        for (const WorldEntity& entity : entities_) {
            if (!entity.isBoss) {
                continue;
            }
            bossDeathFadeComplete = !entity.visible || entity.spriteAlpha <= 0.001f;
            break;
        }
    }
    const bool finishBlockedByHost =
        hooks_.isBattleFinishBlocked && hooks_.isBattleFinishBlocked(manager_);

    if (manager_.isBattleOver() &&
        !dialogueActive &&
        !presentationPlaybackActive_ &&
        !activeUltimateTurnSplash_ &&
        bossDeathFadeComplete &&
        !finishBlockedByHost) {
        finished_ = true;
    }

    frameAccumulator_ += deltaSeconds;

    {
        bool bossActing      = true;
        int actingPartyIndex = -1;
        const flow::PreviewActorContext preview = flow::inspectPreviewActor(manager_);
        if (preview.valid && preview.type == ParticipantType::Character) {
            bossActing       = false;
            actingPartyIndex = preview.partyIndex;
        }
        updateSceneEntities(deltaSeconds, bossActing, actingPartyIndex);
        maybeStartUltimateTurnSplash(preview, dialogueActive);
    }

    if (activeUltimateTurnSplash_) {
        activeUltimateTurnSplash_->update(deltaSeconds);
        if (activeUltimateTurnSplash_->isComplete()) {
            activeUltimateTurnSplash_.reset();
        }
    }

    const auto& actionEvents = manager_.getRecentActionEvents();
    const std::size_t safeStart = std::min(processedBattleEventCount_, actionEvents.size());
    for (std::size_t i = safeStart; i < actionEvents.size(); ++i) {
        const BattleActionEvent& event = actionEvents[i];
        if (event.actorType == ParticipantType::Boss) {
            cameraStaging_.queueCharacterTurnIntro();
        }
    }
    processedBattleEventCount_ = actionEvents.size();

    cameraStaging_.notifyTurnPreview(
        manager_.getTurnState(),
        manager_.getPreviewNextActorIndex(),
        dialogueActive,
        freeViewEnabled_,
        camera_
    );

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (freeViewEnabled_ && !cameraStaging_.isIntroActive()) {
        constexpr float kCamSpeed = 15.0f;
        constexpr float kRotationSpeed = 2.0f;
        if (keys[SDL_SCANCODE_W]) camera_.posY += kCamSpeed;
        if (keys[SDL_SCANCODE_S]) camera_.posY -= kCamSpeed;
        if (keys[SDL_SCANCODE_A]) camera_.posX -= kCamSpeed;
        if (keys[SDL_SCANCODE_D]) camera_.posX += kCamSpeed;
        if (keys[SDL_SCANCODE_E]) camera_.posZ += kCamSpeed;
        if (keys[SDL_SCANCODE_Q]) camera_.posZ -= kCamSpeed;

        if (keys[SDL_SCANCODE_LEFT])  camera_.yawDegrees -= kRotationSpeed;
        if (keys[SDL_SCANCODE_RIGHT]) camera_.yawDegrees += kRotationSpeed;
        if (keys[SDL_SCANCODE_UP])    camera_.pitchDegrees -= kRotationSpeed;
        if (keys[SDL_SCANCODE_DOWN])  camera_.pitchDegrees += kRotationSpeed;
        camera_.pitchDegrees = clampFreeViewPitchDegrees(camera_.pitchDegrees);
    }

    cameraStaging_.update(camera_, deltaSeconds, freeViewEnabled_);
    freeViewCameraDebugLog_.update(freeViewEnabled_ && !cameraStaging_.isIntroActive(), camera_);

    feedback_.syncFromManager(manager_, presentationPlaybackActive_);
    feedback_.update(deltaSeconds);
}

// ---------------------------------------------------------------------------
// render
/**
 * @brief Renders the current battle frame to the given SDL renderer.
 *
 * Draws the scene (backdrop, floor, props), all world entities with per-entity shake offsets,
 * HUD and presentation overlays, and feedback popups. Respects combat-begin animation playback,
 * blackout state, and presentation layering when deciding where to render feedback.
 *
 * @param renderer SDL renderer to draw into.
 * @param screenWidth Width of the rendering target in pixels.
 * @param screenHeight Height of the rendering target in pixels.
 */

void BattleSessionCore::render(SDL_Renderer* renderer, int screenWidth, int screenHeight) {
    if (!initialized_ || finished_) return;

    if (combatBeginAnimation_.isActive()) {
        combatBeginAnimation_.render(renderer, screenWidth, screenHeight);
        return;
    }

    if (entities_.empty()) return;
    BattleFrameSnapshot snapshot = buildFrameSnapshot(screenWidth, screenHeight);
    SDL_SetRenderDrawColor(renderer,
                           snapshot.blackoutWorld ? 0 : 20,
                           snapshot.blackoutWorld ? 0 : 20,
                           snapshot.blackoutWorld ? 0 : 25,
                           255);
    SDL_RenderClear(renderer);

    render::renderBattleBackdrop(
        renderer,
        screenWidth,
        screenHeight,
        snapshot.camera,
        stageRenderData_
    );
    render::renderBattleFloor(
        renderer,
        screenWidth,
        screenHeight,
        snapshot.camera,
        stageRenderData_.definition.floor,
        snapshot.renderFloor ? stageRenderData_.floorTexture : nullptr
    );
    renderCompatibilityBelowWorld(renderer, screenWidth, screenHeight);
    render::renderBattleProps(
        renderer,
        screenWidth,
        screenHeight,
        snapshot.camera,
        stageRenderData_.props
    );

    render::renderBattleEntities(
        renderer,
        screenWidth,
        screenHeight,
        snapshot.camera,
        snapshot.entities,
        snapshot.focusedEntityIndex,
        textureByAsset_,
        snapshot.frameAccumulator,
        [&](const WorldEntity& entity) {
            const auto it = std::find_if(snapshot.entities.begin(), snapshot.entities.end(), [&](const WorldEntity& candidate) {
                return candidate.isBoss == entity.isBoss &&
                       candidate.partyIndex == entity.partyIndex &&
                       candidate.assetName == entity.assetName;
            });
            if (it == snapshot.entities.end()) {
                return 0.0f;
            }
            const size_t index = static_cast<size_t>(std::distance(snapshot.entities.begin(), it));
            return index < snapshot.shakeOffsetsX.size() ? snapshot.shakeOffsetsX[index] : 0.0f;
        }
    );

    if (snapshot.blackoutWorld) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        const SDL_Rect full{0, 0, screenWidth, screenHeight};
        SDL_RenderFillRect(renderer, &full);
    }

    const bool renderFeedbackInOverlay =
        hasVisibleFeedbackPopups() &&
        activePresentation_ != nullptr &&
        activePresentation_->shouldRenderAboveHud();

    renderCompatibilityMidWorld(renderer, screenWidth, screenHeight, !renderFeedbackInOverlay);

    hud_.syncFromManager(manager_);
    hud_.draw(renderer, screenWidth, screenHeight, iconByAsset_);

    renderCompatibilityOverlay(renderer, screenWidth, screenHeight, renderFeedbackInOverlay);

    if (hooks_.onPostRender) hooks_.onPostRender();
}

/**
 * @brief Capture a render-time snapshot of the current battle frame.
 *
 * Constructs and returns a BattleFrameSnapshot populated with the current
 * camera (with screen center set from the provided dimensions), a pointer to
 * the active stage definition, the current entity list and feedback anchors,
 * focused entity index, accumulated frame time, presentation and rendering
 * flags, active presentation/overlay pointers, the active ultimate-splash
 * animation pointer, and per-entity horizontal shake offsets computed from
 * feedback state.
 *
 * @param screenWidth Width of the render target in pixels; used to set the camera screen center.
 * @param screenHeight Height of the render target in pixels; used to set the camera screen center.
 * @return BattleFrameSnapshot Snapshot of all data required by the renderer for the current frame.
 */
BattleSessionCore::BattleFrameSnapshot BattleSessionCore::buildFrameSnapshot(int screenWidth, int screenHeight) const {
    BattleFrameSnapshot snapshot;
    snapshot.camera = camera_;
    snapshot.stage = &stageRenderData_.definition;
    snapshot.camera.screenCenterX = screenWidth * 0.5f;
    snapshot.camera.screenCenterY = screenHeight * 0.5f;
    snapshot.entities = entities_;
    snapshot.feedbackAnchors = buildFeedbackAnchors();
    snapshot.focusedEntityIndex = computeFocusedEntityIndex();
    snapshot.frameAccumulator = frameAccumulator_;
    snapshot.blackoutWorld = activePresentation_ != nullptr && activePresentation_->shouldBlackoutWorld();
    snapshot.renderFloor = activePresentation_ == nullptr || activePresentation_->shouldRenderFloor();
    snapshot.presentationPlaybackActive = presentationPlaybackActive_;
    snapshot.presentationCasterIsBoss = presentationCasterIsBoss_;
    snapshot.presentationCasterPartyIndex = presentationCasterPartyIndex_;
    snapshot.activePresentation = activePresentation_;
    snapshot.activeUltimateTurnSplash = activeUltimateTurnSplash_.get();
    snapshot.shakeOffsetsX.reserve(snapshot.entities.size());
    for (const WorldEntity& entity : snapshot.entities) {
        snapshot.shakeOffsetsX.push_back(feedback_.getShakeOffsetX(entity.isBoss, entity.partyIndex));
    }
    return snapshot;
}

void BattleSessionCore::renderCompatibilityBelowWorld(SDL_Renderer* renderer, int screenWidth, int screenHeight) {
    if (activePresentation_ == nullptr) {
        return;
    }

    Camera3D renderCamera = camera_;
    renderCamera.screenCenterX = screenWidth * 0.5f;
    renderCamera.screenCenterY = screenHeight * 0.5f;
    activePresentation_->renderBelowWorld(renderer, screenWidth, screenHeight, renderCamera);
}

void BattleSessionCore::renderCompatibilityMidWorld(SDL_Renderer* renderer,
                                                    int screenWidth,
                                                    int screenHeight,
                                                    bool includeFeedback) {
    Camera3D renderCamera = camera_;
    renderCamera.screenCenterX = screenWidth * 0.5f;
    renderCamera.screenCenterY = screenHeight * 0.5f;

    if (activePresentation_ != nullptr && !activePresentation_->shouldRenderAboveHud()) {
        activePresentation_->render(renderer, screenWidth, screenHeight, renderCamera);
    }

    if (includeFeedback) {
        feedback_.render(renderer, renderCamera, buildFeedbackAnchors());
    }
}

void BattleSessionCore::renderCompatibilityOverlay(SDL_Renderer* renderer,
                                                   int screenWidth,
                                                   int screenHeight,
                                                   bool includeFeedback) {
    Camera3D renderCamera = camera_;
    renderCamera.screenCenterX = screenWidth * 0.5f;
    renderCamera.screenCenterY = screenHeight * 0.5f;

    if (activePresentation_ != nullptr && activePresentation_->shouldRenderAboveHud()) {
        activePresentation_->render(renderer, screenWidth, screenHeight, renderCamera);
    }
    if (activeUltimateTurnSplash_) {
        activeUltimateTurnSplash_->renderOverlay(renderer, screenWidth, screenHeight);
    }
    if (activeOverlay_) {
        activeOverlay_(renderer, screenWidth, screenHeight);
    }
    if (includeFeedback) {
        feedback_.render(renderer, renderCamera, buildFeedbackAnchors());
    }
}

bool BattleSessionCore::hasCompatibilityOverlay() const {
    return (activePresentation_ != nullptr && activePresentation_->shouldRenderAboveHud()) ||
           activeUltimateTurnSplash_ != nullptr ||
           static_cast<bool>(activeOverlay_);
}

bool BattleSessionCore::hasVisibleFeedbackPopups() const {
    return feedback_.hasVisiblePopups();
}

// ---------------------------------------------------------------------------
// accessors
// ---------------------------------------------------------------------------

bool BattleSessionCore::isFinished() const { return finished_; }
bool BattleSessionCore::isCombatBeginAnimationActive() const { return combatBeginAnimation_.isActive(); }

BattleManager& BattleSessionCore::getBattleManager() { return manager_; }
const BattleManager& BattleSessionCore::getBattleManager() const { return manager_; }

int BattleSessionCore::computeFocusedEntityIndex() const {
    if (entities_.empty()) {
        return -1;
    }

    int focusedEntityIndex = static_cast<int>(entities_.size() - 1);
    const TurnState& liveTurnState = manager_.getTurnState();
    const int nextActorIndex = manager_.getPreviewNextActorIndex();

    if (presentationPlaybackActive_) {
        int presentationFocusedPartyIndex = -1;
        if (activePresentation_ != nullptr) {
            presentationFocusedPartyIndex = activePresentation_->getFocusedPartyIndex();
        }

        if (presentationFocusedPartyIndex >= 0) {
            for (size_t i = 0; i < entities_.size(); ++i) {
                const WorldEntity& e = entities_[i];
                if (!e.isBoss && e.partyIndex == presentationFocusedPartyIndex) {
                    return static_cast<int>(i);
                }
            }
        } else {
            for (size_t i = 0; i < entities_.size(); ++i) {
                const WorldEntity& e = entities_[i];
                if (presentationCasterIsBoss_ && e.isBoss) {
                    return static_cast<int>(i);
                }
                if (!presentationCasterIsBoss_ && !e.isBoss && e.partyIndex == presentationCasterPartyIndex_) {
                    return static_cast<int>(i);
                }
            }
        }
        return focusedEntityIndex;
    }

    if (nextActorIndex >= 0 && nextActorIndex < static_cast<int>(liveTurnState.actors.size())) {
        const TurnActor& nextActor = liveTurnState.actors[static_cast<size_t>(nextActorIndex)];
        for (size_t i = 0; i < entities_.size(); ++i) {
            const WorldEntity& e = entities_[i];
            if (nextActor.type == ParticipantType::Boss && e.isBoss) {
                return static_cast<int>(i);
            }
            if (nextActor.type == ParticipantType::Character && !e.isBoss && e.partyIndex == nextActor.partyIndex) {
                return static_cast<int>(i);
            }
        }
    }

    return focusedEntityIndex;
}

std::vector<render::FeedbackEntityAnchor> BattleSessionCore::buildFeedbackAnchors() const {
    std::vector<render::FeedbackEntityAnchor> anchors;
    anchors.reserve(entities_.size());
    for (const WorldEntity& entity : entities_) {
        anchors.push_back(render::FeedbackEntityAnchor{
            entity.isBoss,
            entity.partyIndex,
            entity.worldX,
            entity.worldY,
            entity.worldZ,
            entity.visible
        });
    }
    return anchors;
}

// ---------------------------------------------------------------------------
// private helpers
/**
 * @brief Handles numeric hotkeys to request a manual ultimate turn for a party member.
 *
 * If the pressed key maps to a party index, this requests a manual ultimate turn from the battle
 * manager. Depending on the request result, it may produce a HUD hint describing why the ultimate
 * could not be queued (meter not ready, already queued, or unavailable).
 *
 * @param key SDL key code for the pressed key (numeric keys `1–4` and keypad `KP_1–KP_4` map to party indices).
 * @return bool `true` if the key was handled (it mapped to a party index or battle input was blocked), `false` if the key did not map to any party index.
 */

bool BattleSessionCore::handleManualUltimateHotkey(SDL_Keycode key, SDL_Keymod mod) {
    const int partyIndex = manualUltimatePartyIndexFromKey(key, mod, manager_.getBattleDefinition());
    if (partyIndex < 0) {
        return false;
    }
    if (static_cast<size_t>(partyIndex) >= manager_.getBattleState().party.size()) {
        return false;
    }

    const bool battleInputEnabled = hooks_.isSpaceEnabledForBattle
        ? hooks_.isSpaceEnabledForBattle()
        : true;
    if (!battleInputEnabled || manager_.isBattleOver()) {
        return true;
    }

    const ManualUltimateRequestResult result = manager_.requestManualUltimateTurn(partyIndex);
    switch (result) {
        case ManualUltimateRequestResult::Queued:
            break;
        case ManualUltimateRequestResult::MeterNotReady: {
            const BattleState& battleState = manager_.getBattleState();
            const std::string actorLabel =
                (partyIndex >= 0 && static_cast<size_t>(partyIndex) < battleState.party.size())
                    ? battleState.party[static_cast<size_t>(partyIndex)].title
                    : ("ALLY " + std::to_string(partyIndex + 1));
            const int charge = manager_.getCharacterUltimateCharge(partyIndex);
            const int required = manager_.getCharacterUltimateRequired(partyIndex);
            const int missing = std::max(0, required - charge);
            setHint(
                actorLabel + " NEEDS " + std::to_string(missing) + " MORE " +
                (missing == 1 ? "ABILITY." : "ABILITIES."),
                1800
            );
            break;
        }
        case ManualUltimateRequestResult::AlreadyQueued:
            setHint("ULTIMATE ALREADY QUEUED.", 1400);
            break;
        case ManualUltimateRequestResult::Unavailable:
            setHint("ULTIMATE NOT AVAILABLE.", 1400);
            break;
    }

    return true;
}

/**
 * @brief Starts an ultimate-turn splash animation when the upcoming preview indicates a player character's manual ultimate.
 *
 * Checks the provided preview context and, if it represents a non-auto, extra-turn ultimate for a valid party character and no other presentation or splash is active, creates and starts the splash animation and records which party index triggered it.
 *
 * @param preview Preview actor context describing the next actor and action.
 * @param dialogueActive If true, suppresses starting the splash (dialogue blocks splash playback).
 */
void BattleSessionCore::maybeStartUltimateTurnSplash(const flow::PreviewActorContext& preview, bool dialogueActive) {
    if (dialogueActive || presentationPlaybackActive_ || activeUltimateTurnSplash_) {
        return;
    }

    const bool isCharacterUltimatePreview =
        preview.valid &&
        preview.type == ParticipantType::Character &&
        preview.isExtraTurn &&
        preview.extraTurnAction == BattleAction::Ultimate &&
        !preview.autoExecute &&
        preview.partyIndex >= 0;

    if (!isCharacterUltimatePreview) {
        previewUltimateSplashPartyIndex_ = -1;
        return;
    }

    if (preview.partyIndex == previewUltimateSplashPartyIndex_) {
        return;
    }

    const BattleState& battleState = manager_.getBattleState();
    if (static_cast<size_t>(preview.partyIndex) >= battleState.party.size()) {
        return;
    }

    const std::string abilityId = manager_.resolveCharacterAbilityId(
        preview.partyIndex,
        preview.extraTurnAction,
        preview.abilityKitOverride
    );
    if (abilityId.empty()) {
        return;
    }
    const AbilityDefinition* abilityDef = manager_.findAbilityDefinition(abilityId);

    SplashArtConfig cfg;
    cfg.abilityName = abilityDef != nullptr ? abilityDef->name : abilityId;
    const std::string assetId = manager_.resolveCharacterAssetId(preview.partyIndex, preview.abilityKitOverride);
    if (renderer_ != nullptr && textureByAsset_.find(assetId) == textureByAsset_.end()) {
        const auto loaded = render::tryLoadCombatSpriteTexture(renderer_, assetId);
        textureByAsset_[assetId] = loaded.has_value() ? *loaded : nullptr;
    }
    if (const auto texIt = textureByAsset_.find(assetId); texIt != textureByAsset_.end()) {
        cfg.sprite = texIt->second;
    }

    activeUltimateTurnSplash_ = std::make_unique<SplashArtAnimation>(cfg);
    activeUltimateTurnSplash_->start();
    previewUltimateSplashPartyIndex_ = preview.partyIndex;
}

/**
 * @brief Begins the boss phase intro sequence and configures its camera and hint.
 *
 * Activates the boss phase intro state for the given transition, sets the phase
 * index and hint text, computes start/goal cameras aimed at the boss, sets the
 * current camera to the intro start pose, and displays the phase hint.
 */
void BattleSessionCore::startBossPhaseIntro(const BossPhaseTransition& transition) {
    int bossEntityIndex = -1;
    for (int index = 0; index < static_cast<int>(entities_.size()); ++index) {
        if (entities_[static_cast<size_t>(index)].isBoss) {
            bossEntityIndex = index;
            break;
        }
    }
    if (bossEntityIndex < 0 || static_cast<size_t>(bossEntityIndex) >= entities_.size()) {
        return;
    }

    bossPhaseIntro_.active = true;
    bossPhaseIntro_.phaseIndex = transition.toPhaseIndex;
    bossPhaseIntro_.hintText = bossPhaseHintText(transition.toPhaseIndex);
    bossPhaseIntro_.elapsed = 0.0f;
    bossPhaseIntro_.duration = kBossPhaseIntroDurationSeconds;
    bossPhaseIntro_.startCamera =
        makeBossPhaseIntroStartCamera(entities_[static_cast<size_t>(bossEntityIndex)]);
    bossPhaseIntro_.goalCamera =
        makeBossPhaseIntroEndCamera(entities_[static_cast<size_t>(bossEntityIndex)]);
    aimCameraAtBossIntroTarget(bossPhaseIntro_.startCamera, entities_[static_cast<size_t>(bossEntityIndex)]);
    aimCameraAtBossIntroTarget(bossPhaseIntro_.goalCamera, entities_[static_cast<size_t>(bossEntityIndex)]);
    camera_ = bossPhaseIntro_.startCamera;
    setHint(bossPhaseIntro_.hintText);
}

/**
 * @brief Advances the boss-phase intro sequence, animating the camera and hint.
 *
 * Progresses the timed boss phase intro by the given delta and interpolates the scene camera
 * from the stored start pose to the goal pose using an ease-out quint easing curve.
 * When the intro completes the intro state is deactivated, the camera is snapped to the goal,
 * and the hint text is cleared.
 *
 * @param deltaSeconds Time elapsed since the last update in seconds.
 */
void BattleSessionCore::updateBossPhaseIntro(float deltaSeconds) {
    if (!bossPhaseIntro_.active) {
        return;
    }

    setHint(bossPhaseIntro_.hintText);
    bossPhaseIntro_.elapsed += deltaSeconds;
    const float t = easing::clamp01(bossPhaseIntro_.elapsed / std::max(0.001f, bossPhaseIntro_.duration));
    const float eased = easing::easeOutQuint(t);

    camera_.posX = easing::lerp(bossPhaseIntro_.startCamera.posX, bossPhaseIntro_.goalCamera.posX, eased);
    camera_.posY = easing::lerp(bossPhaseIntro_.startCamera.posY, bossPhaseIntro_.goalCamera.posY, eased);
    camera_.posZ = easing::lerp(bossPhaseIntro_.startCamera.posZ, bossPhaseIntro_.goalCamera.posZ, eased);
    camera_.pitchDegrees = easing::lerp(
        bossPhaseIntro_.startCamera.pitchDegrees, bossPhaseIntro_.goalCamera.pitchDegrees, eased);
    camera_.yawDegrees = easing::lerp(
        bossPhaseIntro_.startCamera.yawDegrees, bossPhaseIntro_.goalCamera.yawDegrees, eased);
    camera_.focalLength = easing::lerp(
        bossPhaseIntro_.startCamera.focalLength, bossPhaseIntro_.goalCamera.focalLength, eased);

    if (t >= 1.0f) {
        bossPhaseIntro_.active = false;
        camera_ = bossPhaseIntro_.goalCamera;
        clearHint();
    }
}

/**
 * @brief Updates character layout and visibility transitions for the current turn.
 *
 * Updates entity positions according to whether the boss is acting and which party member is acting,
 * then advances per-entity visibility/alpha/vertical-offset animations using the supplied frame delta.
 *
 * @param deltaSeconds Time elapsed since the last update, in seconds.
 * @param bossActing True if the boss is currently acting and the layout should reflect a boss-centric turn.
 * @param actingPartyIndex Index of the party member who is acting, or -1 if none/not applicable.
 */
void BattleSessionCore::updateSceneEntities(float deltaSeconds, bool bossActing, int actingPartyIndex) {
    if (renderer_ != nullptr) {
        const BattleState& battleState = manager_.getBattleState();
        for (WorldEntity& entity : entities_) {
            if (entity.isBoss ||
                entity.partyIndex < 0 ||
                static_cast<size_t>(entity.partyIndex) >= battleState.party.size()) {
                continue;
            }

            const std::string assetId = manager_.resolveCharacterAssetId(entity.partyIndex);
            if (!assetId.empty()) {
                entity.assetName = assetId;
                if (textureByAsset_.find(assetId) == textureByAsset_.end()) {
                    const auto loaded = render::tryLoadCombatSpriteTexture(renderer_, assetId);
                    textureByAsset_[assetId] = loaded.has_value() ? *loaded : nullptr;
                }
            }
        }

        for (const TurnActor& actor : manager_.getTurnState().actors) {
            const std::string iconId = actor.assetId.empty() ? actor.key : actor.assetId;
            const std::string iconKey = (actor.type == ParticipantType::Boss) ? "boss_" + iconId : iconId;
            if (iconByAsset_.find(iconKey) == iconByAsset_.end()) {
                const auto loaded = render::tryLoadCombatIconTexture(renderer_, iconId);
                iconByAsset_[iconKey] = loaded.has_value() ? *loaded : nullptr;
            }
        }
    }

    computeCharacterPositions(bossActing, actingPartyIndex);
    updateCharacterVisibilityTransitions(deltaSeconds);
}

void BattleSessionCore::computeCharacterPositions(bool bossActing, int actingPartyIndex) {
    std::vector<bool> livingPartyMembers(manager_.getBattleState().party.size(), false);
    for (size_t i = 0; i < livingPartyMembers.size(); ++i) {
        livingPartyMembers[i] = manager_.isCharacterAlive(static_cast<int>(i));
    }

    render::computeDefaultPartyCharacterPositions(
        entities_,
        livingPartyMembers,
        bossActing,
        actingPartyIndex
    );

    const bool bossAlive = manager_.getBossCurrentHp() > 0;
    for (WorldEntity& entity : entities_) {
        if (!entity.isBoss) {
            continue;
        }
        entity.lineupVisible = bossAlive;
        break;
    }
}

void BattleSessionCore::updateCharacterVisibilityTransitions(float deltaSeconds) {
    const float step = kCharacterVisibilityAnimSeconds <= 0.0f
        ? 1.0f
        : deltaSeconds / kCharacterVisibilityAnimSeconds;

    for (WorldEntity& entity : entities_) {
        if (entity.lineupVisible) {
            entity.spriteAlpha = std::min(1.0f, entity.spriteAlpha + step);
            const float eased = easing::easeOutCubic(easing::clamp01(entity.spriteAlpha));
            entity.spriteOffsetYPx = -kCharacterVisibilityOffsetPx * (1.0f - eased);
            entity.visible = true;
            continue;
        }

        entity.spriteAlpha = std::max(0.0f, entity.spriteAlpha - step);
        const float fadeT = easing::clamp01(1.0f - entity.spriteAlpha);
        entity.spriteOffsetYPx = -kCharacterVisibilityOffsetPx * easing::easeOutCubic(fadeT);
        entity.visible = entity.spriteAlpha > 0.001f;
        if (!entity.visible) {
            entity.spriteOffsetYPx = 0.0f;
        }
    }
}

/**
 * @brief Plays an ability presentation for the given context and applies its in-game effects.
 *
 * Runs the full presentation playback (rendering frames, routing audio callbacks, and handling
 * unhandled input) while keeping the battle scene synchronized for correct camera and projectile
 * framing. During playback this function may apply healing, damage, feedback events, and other
 * presentation-driven effects to the battle manager; it also updates scene layout, HUD hints,
 * overlay rendering, and presentation-related state used by the session.
 *
 * @param context PresentationContext describing the caster, target, ability id, and presentation id.
 * @return float Damage/heal multiplier produced by the presentation (or 1.0 if playback was not run).
 */
float BattleSessionCore::runPresentationInteraction(const PresentationContext& context) {
    if (!initialized_ || finished_ || renderer_ == nullptr) return 1.0f;

    // Automatic follow-up turns can fire before the outer update loop restages the party.
    // Force the scene into the current actor's duel layout so projectile paths and camera
    // framing match a normal turn for that same character.
    updateSceneEntities(0.0f, context.isBoss, context.isBoss ? -1 : context.casterIndex);
    cameraStaging_.snapToGoalCamera(camera_);

    const std::string casterAssets = resolvePresentationCasterAsset(context);
    const AbilityDefinition* abilityDef = manager_.findAbilityDefinition(context.abilityId);

    if (abilityDef != nullptr && !abilityDef->instructionHint.empty()) {
        setHint(abilityDef->instructionHint);
    } else {
        clearHint();
    }

    // Resolve the caster's and target's sprite textures for the splash animation.
    auto resolveTexture = [this](const render::SceneEntity& entity) -> SDL_Texture* {
        auto it = textureByAsset_.find(entity.assetName);
        return (it != textureByAsset_.end()) ? it->second : nullptr;
    };

    auto resolveTextureForEntity = [&](auto predicate) -> SDL_Texture* {
        for (const auto& entity : entities_) {
            if (!predicate(entity)) {
                continue;
            }
            if (SDL_Texture* texture = resolveTexture(entity)) {
                return texture;
            }
        }
        return nullptr;
    };

    SDL_Texture* casterSprite = resolveTextureForEntity([&context](const render::SceneEntity& entity) {
        return context.isBoss ? entity.isBoss
                              : (!entity.isBoss && entity.partyIndex == context.casterIndex);
    });

    SDL_Texture* targetSprite = resolveTextureForEntity([&context](const render::SceneEntity& entity) {
        if (context.isBoss) {
            if (context.targetIndex >= 0) {
                return !entity.isBoss && entity.partyIndex == context.targetIndex;
            }
            return !entity.isBoss;
        }
        return entity.isBoss;
    });

    presentation_runtime::PlaybackStateRefs stateRefs;
    stateRefs.playbackActive      = &presentationPlaybackActive_;
    stateRefs.casterIsBoss        = &presentationCasterIsBoss_;
    stateRefs.casterPartyIndex    = &presentationCasterPartyIndex_;
    stateRefs.activePresentation  = &activePresentation_;

    presentation_runtime::PlaybackCallbacks callbacks;
    callbacks.splashSpriteTexture = casterSprite;
    callbacks.casterSpriteTexture = casterSprite;
    callbacks.targetSpriteTexture = targetSprite;
    callbacks.overlayCasterSpriteTexture = casterSprite;
    callbacks.overlayTargetSpriteTexture = targetSprite;
    {
        const BattleState& state = manager_.getBattleState();
        callbacks.partyTargetableStates.reserve(state.party.size());
        for (std::size_t i = 0; i < state.party.size(); ++i) {
            callbacks.partyTargetableStates.push_back(
                manager_.isCharacterAlive(static_cast<int>(i))
            );
        }
    }
    callbacks.onWindowResized     = hooks_.onWindowResized;
    callbacks.onUnhandledKeyDown  = [this](SDL_Keycode key) {
        (void)handleManualUltimateHotkey(key, static_cast<SDL_Keymod>(SDL_GetModState()));
    };
    callbacks.onAudioCommands     = [&](const std::vector<PresentationAudioCommand>& commands) {
        if (commands.empty()) {
            return;
        }
        const bool handlesAbilityVoice = std::any_of(
            commands.begin(),
            commands.end(),
            [](const PresentationAudioCommand& command) {
                return command.type == PresentationAudioCommandType::PlayVoiceOneShot;
            }
        );
        if (handlesAbilityVoice) {
            manager_.markPresentationAbilityAudioPlayed();
        }
        if (hooks_.onPresentationAudioCommands) {
            hooks_.onPresentationAudioCommands(context, commands);
        }
    };
    callbacks.onAbilityAudioCues  = [&](int cueCount) {
        if (cueCount <= 0) {
            return;
        }
        if (hooks_.onPresentationAbilityAudio) {
            hooks_.onPresentationAbilityAudio(context, cueCount, manager_);
            manager_.markPresentationAbilityAudioPlayed();
        }
    };
    callbacks.onHitEvents = [&](int hitEvents, int damageLabelHitCount) {
        if (hitEvents <= 0) {
            return;
        }

        if (abilityDef == nullptr) {
            feedback_.queuePresentationHitShakes(context.isBoss, hitEvents, manager_);
            return;
        }

        if (abilityDef->type == AbilityType::Heal) {
            const float healMultiplier = activePresentation_ != nullptr
                ? std::max(0.0f, activePresentation_->getInputMultiplier())
                : 1.0f;
            const int casterMaxHp = context.isBoss
                ? manager_.getBossMaxHp()
                : manager_.getCharacterMaxHp(context.casterIndex);
            const int totalHeal = ability::resolveSupportAmount(
                *abilityDef,
                casterMaxHp,
                healMultiplier,
                abilityDef->flatHeal
            );
            const int perHitHeal = totalHeal / std::max(1, hitEvents);
            const int targetPartyIndex =
                (abilityDef->targetRule == TargetRule::SingleAlly && activePresentation_ != nullptr)
                ? activePresentation_->getFocusedPartyIndex()
                : -1;
            const int totalRequestedHealing = manager_.applyPresentationHealing(
                context.isBoss,
                perHitHeal,
                hitEvents,
                abilityDef->targetRule,
                targetPartyIndex,
                abilityDef->reviveDeadAllies,
                abilityDef->specialDamageSource != SpecialDamageSource::StoredHealingTally
            );
            feedback_.queuePresentationHealFeedback(
                context.isBoss,
                hitEvents,
                perHitHeal,
                manager_,
                targetPartyIndex
            );
            if (hooks_.onPresentationHealAudio) {
                hooks_.onPresentationHealAudio(context, hitEvents, manager_);
            }

            int specialDamage = 0;
            if (!context.isBoss && abilityDef->specialDamageSource == SpecialDamageSource::AppliedHeal) {
                specialDamage = manager_.applyConvertedPlayerSpecialDamageToBoss(
                    totalRequestedHealing,
                    abilityDef->multiplier,
                    true,
                    context.casterIndex
                );
            } else if (!context.isBoss &&
                       abilityDef->specialDamageSource == SpecialDamageSource::StoredHealingTally) {
                specialDamage = manager_.applyConvertedPlayerSpecialDamageToBoss(
                    manager_.consumeTetoHealingTally(),
                    abilityDef->multiplier,
                    true,
                    context.casterIndex
                );
            }

            if (specialDamage > 0) {
                if (hooks_.onPresentationHitAudio) {
                    hooks_.onPresentationHitAudio(context, 1, manager_);
                    manager_.markPresentationHitAudioPlayed();
                }
                (void)feedback_.queuePresentationHitFeedback(
                    context.isBoss,
                    1,
                    specialDamage,
                    manager_,
                    -1
                );
            }
            return;
        }

        if (ability::isTeamShieldBurstUltimate(*abilityDef)) {
            const int totalDamage = manager_.applyCurrentTeamShieldDamageToBoss(
                true,
                abilityDef->multiplier,
                context.casterIndex
            );
            if (totalDamage > 0) {
                if (hooks_.onPresentationHitAudio) {
                    hooks_.onPresentationHitAudio(context, hitEvents, manager_);
                    manager_.markPresentationHitAudioPlayed();
                }
                (void)feedback_.queuePresentationHitFeedback(
                    context.isBoss,
                    hitEvents,
                    totalDamage,
                    manager_,
                    -1
                );
            }
            return;
        }

        const int baseAtk = context.isBoss
            ? manager_.getBossEffectiveAtk()
            : manager_.getCharacterEffectiveAtk(context.casterIndex);

        float hitDamageMultiplier = 1.0f;
        if (activePresentation_ != nullptr) {
            hitDamageMultiplier = std::max(0.0f, activePresentation_->consumeHitDamageMultiplier());
        }

        const float damageBuffMultiplier = context.isBoss
            ? 1.0f
            : manager_.getCharacterDamageBuffMultiplier(context.casterIndex);
        const float comboMultiplier = context.isBoss
            ? 1.0f
            : comboDamageMultiplier(manager_.getComboState().comboCount);
        float abilityMult = abilityDef->multiplier;
        float totalDamageRaw =
            static_cast<float>(baseAtk) *
            abilityMult *
            hitDamageMultiplier *
            damageBuffMultiplier *
            comboMultiplier;
        const int totalDamage = std::max(1, static_cast<int>(totalDamageRaw));
        const int perHitDamage = std::max(1, totalDamage / std::max(1, damageLabelHitCount));
        const int presentationTargetPartyIndex = context.isBoss ? context.targetIndex : -1;
        const std::vector<int> targetedHitIndices =
            activePresentation_ != nullptr
                ? activePresentation_->consumeHitTargetIndices()
                : std::vector<int>{};

        if (context.isBoss && !targetedHitIndices.empty()) {
            for (int targetPartyIndex : targetedHitIndices) {
                if (targetPartyIndex < 0) {
                    continue;
                }

                manager_.applyPresentationHitDamage(
                    true,
                    perHitDamage,
                    1,
                    targetPartyIndex,
                    context.casterIndex
                );
                if (hooks_.onPresentationHitAudio) {
                    hooks_.onPresentationHitAudio(context, 1, manager_);
                    manager_.markPresentationHitAudioPlayed();
                }
                (void)feedback_.queuePresentationHitFeedback(
                    true,
                    1,
                    perHitDamage,
                    manager_,
                    targetPartyIndex
                );
            }
            return;
        }

        manager_.applyPresentationHitDamage(
            context.isBoss,
            perHitDamage,
            hitEvents,
            presentationTargetPartyIndex,
            context.casterIndex
        );
        if (hooks_.onPresentationHitAudio) {
            hooks_.onPresentationHitAudio(context, hitEvents, manager_);
            manager_.markPresentationHitAudioPlayed();
        }
        (void)feedback_.queuePresentationHitFeedback(
            context.isBoss,
            hitEvents,
            perHitDamage,
            manager_,
            presentationTargetPartyIndex
        );
    };
    bool consumedPresentationFeedback = false;
    callbacks.onPostUpdate = [&](float deltaSeconds) {
        const bool useCenteredPartyLayout =
            activePresentation_ != nullptr &&
            activePresentation_->shouldUseCenteredPartyLayout();
        const bool bossActingLayout = context.isBoss || useCenteredPartyLayout;
        const int actingPartyIndex = bossActingLayout ? -1 : context.casterIndex;

        if (hooks_.onPresentationFrameUpdate) {
            hooks_.onPresentationFrameUpdate(deltaSeconds);
        }

        if (activePresentation_ != nullptr) {
            const std::vector<PresentationFeedbackEvent> feedbackEvents =
                activePresentation_->consumeFeedbackEvents();
            for (const PresentationFeedbackEvent& feedbackEvent : feedbackEvents) {
                manager_.applyPresentationFeedback(context.isBoss, feedbackEvent);
                consumedPresentationFeedback = true;
            }
        }

        updateSceneEntities(deltaSeconds, bossActingLayout, actingPartyIndex);
        feedback_.syncFromManager(manager_, presentationPlaybackActive_);
        feedback_.update(deltaSeconds);
    };
    callbacks.onBossPresentationFrame = [&]() {
        computeCharacterPositions(true, -1);

        if (activePresentation_ == nullptr) {
            return;
        }

        const int focusedPartyIndex = activePresentation_->getFocusedPartyIndex();
        if (focusedPartyIndex < 0) {
            return;
        }

        for (const WorldEntity& entity : entities_) {
            if (entity.isBoss || entity.partyIndex != focusedPartyIndex) {
                continue;
            }

            activePresentation_->setTargetWorldPosition(entity.worldX, entity.worldY, entity.worldZ);
            break;
        }
    };
    callbacks.setRenderOverlay = [this](std::function<void(SDL_Renderer*, int, int)> fn) {
        activeOverlay_ = std::move(fn);
    };
    callbacks.clearRenderOverlay = [this]() {
        activeOverlay_ = nullptr;
    };
    callbacks.renderAndPresentFrame = [&]() {
        if (hooks_.onRenderAndPresentFrame) {
            hooks_.onRenderAndPresentFrame();
            return;
        }

        int screenWidth  = 1280;
        int screenHeight = 720;
        SDL_GetRendererOutputSize(renderer_, &screenWidth, &screenHeight);
        render(renderer_, screenWidth, screenHeight);
        SDL_RenderPresent(renderer_);
    };
    callbacks.onSplashArtStart = [this, casterAssets, &context]() {
        if (hooks_.onPresentationSplashVoice) {
            hooks_.onPresentationSplashVoice(context, casterAssets);
        }
    };

    const presentation_runtime::PlaybackResult result = presentation_runtime::runAbilityPresentation(
        renderer_,
        finished_,
        camera_,
        entities_,
        context,
        stateRefs,
        callbacks
    );

    if (!context.isBoss &&
        context.presentationId == "rang_wo_men_shuo_zhong_wen" &&
        result.correctToneCount > 0) {
        manager_.addLuotianyiCorrectTones(result.correctToneCount);
    }
    if (!context.isBoss && context.abilityId == "LoveAndBeautyShock") {
        manager_.addSailorVenusSpaceTally(context.casterIndex, result.scoreValue);
    }

    if (!consumedPresentationFeedback && result.feedbackSignal.valid()) {
        PresentationFeedbackEvent finalFeedback;
        finalFeedback.signal = result.feedbackSignal;
        finalFeedback.multiplier = result.multiplier;
        manager_.applyPresentationFeedback(context.isBoss, finalFeedback);
    }

    if (hooks_.onPresentationEnd) {
        hooks_.onPresentationEnd(context, result.resultText);
    }

    discardNextUpdateDelta_ = true;

    return result.multiplier;
}

std::string BattleSessionCore::resolvePresentationCasterAsset(const PresentationContext& context) const {
    const BattleState& battleState = manager_.getBattleState();
    if (context.isBoss) {
        return battleState.boss.assets.empty() ? battleState.boss.key : battleState.boss.assets;
    }
    if (context.casterIndex >= 0 && static_cast<size_t>(context.casterIndex) < battleState.party.size()) {
        return manager_.resolveCharacterVoiceAssetId(context.casterIndex);
    }
    return std::string();
}

void BattleSessionCore::setHint(const std::string& text, Uint32 displayMs) { hud_.setHint(text, displayMs); }
void BattleSessionCore::clearHint() { hud_.clearHint(); }

} // namespace battle
