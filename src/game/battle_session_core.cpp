#include "battle_session_core.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/ability_system.h"
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

using WorldEntity = render::SceneEntity;

} // namespace

// ---------------------------------------------------------------------------
// initialize / shutdown
// ---------------------------------------------------------------------------

bool BattleSessionCore::initialize(SDL_Renderer* renderer,
                                   const std::string& bossKey,
                                   const std::vector<std::string>& partyKeys,
                                   Hooks hooks) {
    shutdown();
    renderer_ = renderer;
    hooks_    = std::move(hooks);

    ability::setPresentationInteractionRunner([this](const PresentationContext& context) {
        return runPresentationInteraction(context);
    });

    if (!manager_.initialize(bossKey, partyKeys)) {
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
        character.assetName  = battleState.party[i].assets;
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

    for (const CharacterDefinition& ch : battleState.party) {
        if (textureByAsset_.find(ch.assets) == textureByAsset_.end()) {
            const auto loaded = render::tryLoadCombatSpriteTexture(renderer, ch.assets);
            textureByAsset_[ch.assets] = loaded.has_value() ? *loaded : nullptr;
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

    floorTileTexture_ = render::createBattleWorldFloorTileTexture(renderer);
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
    processedBattleEventCount_ = manager_.getRecentActionEvents().size();
    finished_    = false;
    initialized_ = true;
    return true;
}

void BattleSessionCore::shutdown() {
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

    if (floorTileTexture_ != nullptr) {
        SDL_DestroyTexture(floorTileTexture_);
        floorTileTexture_ = nullptr;
    }

    feedback_.shutdown();
    hud_.reset();
    entities_.clear();

    if (hooks_.onShutdown) hooks_.onShutdown();
    hooks_ = {};
    processedBattleEventCount_ = 0;

    initialized_ = false;
    finished_    = false;
}

// ---------------------------------------------------------------------------
// handleEvent
// ---------------------------------------------------------------------------

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
        if (event.key.keysym.sym == SDLK_ESCAPE) {
            finished_ = true;
            return;
        }
        if (event.key.keysym.sym == SDLK_f) {
            freeViewEnabled_ = !freeViewEnabled_;
            if (!freeViewEnabled_) cameraStaging_.snapToGoalCamera(camera_);
            return;
        }
        if (event.key.keysym.sym != SDLK_SPACE) return;

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
// ---------------------------------------------------------------------------

void BattleSessionCore::update(float deltaSeconds) {
    if (!initialized_ || finished_) return;

    if (combatBeginAnimation_.isActive()) {
        combatBeginAnimation_.update(deltaSeconds);
        return;
    }

    if (hooks_.onPreUpdate) hooks_.onPreUpdate(manager_, deltaSeconds);

    const bool dialogueActive = hooks_.isDialogueInProgress && hooks_.isDialogueInProgress();
    const bool spaceEnabled   = !hooks_.isSpaceEnabledForBattle || hooks_.isSpaceEnabledForBattle();

    if (!dialogueActive && !spaceEnabled && manager_.isBattleOver()) {
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
        computeCharacterPositions(bossActing, actingPartyIndex);
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
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    camera_.posY += kCamSpeed;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  camera_.posY -= kCamSpeed;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  camera_.posX -= kCamSpeed;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) camera_.posX += kCamSpeed;
        if (keys[SDL_SCANCODE_Q]) camera_.pitchDegrees -= 2.0f;
        if (keys[SDL_SCANCODE_E]) camera_.pitchDegrees += 2.0f;
    }

    cameraStaging_.update(camera_, deltaSeconds, freeViewEnabled_);

    feedback_.syncFromManager(manager_, presentationPlaybackActive_);
    feedback_.update(deltaSeconds);
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

void BattleSessionCore::render(SDL_Renderer* renderer, int screenWidth, int screenHeight) {
    if (!initialized_ || finished_) return;

    if (combatBeginAnimation_.isActive()) {
        combatBeginAnimation_.render(renderer, screenWidth, screenHeight);
        return;
    }

    if (entities_.empty()) return;

    camera_.screenCenterX = screenWidth  * 0.5f;
    camera_.screenCenterY = screenHeight * 0.5f;

    int focusedEntityIndex = static_cast<int>(entities_.size() - 1);
    const TurnState& liveTurnState = manager_.getTurnState();
    const int nextActorIndex = manager_.getPreviewNextActorIndex();

    if (presentationPlaybackActive_) {
        for (size_t i = 0; i < entities_.size(); ++i) {
            const WorldEntity& e = entities_[i];
            if (presentationCasterIsBoss_ && e.isBoss) {
                focusedEntityIndex = static_cast<int>(i); break;
            }
            if (!presentationCasterIsBoss_ && !e.isBoss && e.partyIndex == presentationCasterPartyIndex_) {
                focusedEntityIndex = static_cast<int>(i); break;
            }
        }
    } else if (nextActorIndex >= 0 && nextActorIndex < static_cast<int>(liveTurnState.actors.size())) {
        const TurnActor& nextActor = liveTurnState.actors[static_cast<size_t>(nextActorIndex)];
        for (size_t i = 0; i < entities_.size(); ++i) {
            const WorldEntity& e = entities_[i];
            if (nextActor.type == ParticipantType::Boss && e.isBoss) {
                focusedEntityIndex = static_cast<int>(i); break;
            }
            if (nextActor.type == ParticipantType::Character && !e.isBoss && e.partyIndex == nextActor.partyIndex) {
                focusedEntityIndex = static_cast<int>(i); break;
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderClear(renderer);

    render::renderBattleWorld(
        renderer,
        screenWidth,
        screenHeight,
        camera_,
        floorTileTexture_,
        entities_,
        focusedEntityIndex,
        textureByAsset_,
        frameAccumulator_,
        [&](const WorldEntity& entity) {
            return feedback_.getShakeOffsetX(entity.isBoss, entity.partyIndex);
        }
    );

    if (activePresentation_ != nullptr && !activePresentation_->shouldRenderAboveHud()) {
        activePresentation_->render(renderer, screenWidth, screenHeight, camera_);
    }

    hud_.syncFromManager(manager_);
    hud_.draw(renderer, screenWidth, screenHeight, iconByAsset_);

    if (activePresentation_ != nullptr && activePresentation_->shouldRenderAboveHud()) {
        activePresentation_->render(renderer, screenWidth, screenHeight, camera_);
    }

    std::vector<render::FeedbackEntityAnchor> anchors;
    anchors.reserve(entities_.size());
    for (const WorldEntity& entity : entities_) {
        render::FeedbackEntityAnchor anchor;
        anchor.isBoss     = entity.isBoss;
        anchor.partyIndex = entity.partyIndex;
        anchor.worldX     = entity.worldX;
        anchor.worldY     = entity.worldY;
        anchor.worldZ     = entity.worldZ;
        anchor.visible    = entity.visible;
        anchors.push_back(anchor);
    }
    feedback_.render(renderer, camera_, anchors);

    if (hooks_.onPostRender) hooks_.onPostRender();
}

// ---------------------------------------------------------------------------
// accessors
// ---------------------------------------------------------------------------

bool BattleSessionCore::isFinished() const { return finished_; }
bool BattleSessionCore::isCombatBeginAnimationActive() const { return combatBeginAnimation_.isActive(); }

BattleManager& BattleSessionCore::getBattleManager() { return manager_; }
const BattleManager& BattleSessionCore::getBattleManager() const { return manager_; }

// ---------------------------------------------------------------------------
// private helpers
// ---------------------------------------------------------------------------

void BattleSessionCore::computeCharacterPositions(bool bossActing, int actingPartyIndex) {
    render::computeDefaultPartyCharacterPositions(
        entities_,
        static_cast<int>(manager_.getBattleState().party.size()),
        bossActing,
        actingPartyIndex
    );
}

float BattleSessionCore::runPresentationInteraction(const PresentationContext& context) {
    if (!initialized_ || finished_ || renderer_ == nullptr) return 1.0f;

    // Resolve the caster's sprite texture for the splash animation.
    SDL_Texture* casterSprite = nullptr;
    for (const auto& entity : entities_) {
        const bool isCaster = context.isBoss
            ? entity.isBoss
            : (!entity.isBoss && entity.partyIndex == context.casterIndex);
        if (isCaster) {
            auto it = textureByAsset_.find(entity.assetName);
            if (it != textureByAsset_.end()) casterSprite = it->second;
            break;
        }
    }

    presentation_runtime::PlaybackStateRefs stateRefs;
    stateRefs.playbackActive      = &presentationPlaybackActive_;
    stateRefs.casterIsBoss        = &presentationCasterIsBoss_;
    stateRefs.casterPartyIndex    = &presentationCasterPartyIndex_;
    stateRefs.activePresentation  = &activePresentation_;

    presentation_runtime::PlaybackCallbacks callbacks;
    callbacks.casterSpriteTexture = casterSprite;
    callbacks.onWindowResized     = hooks_.onWindowResized;
    callbacks.onAbilityAudioCues  = [&](int cueCount) {
        if (cueCount <= 0) {
            return;
        }
        if (hooks_.onPresentationAbilityAudio) {
            hooks_.onPresentationAbilityAudio(context, cueCount, manager_);
        }
    };
    callbacks.onHitEvents = [&](int hitEvents, int damageLabelHitCount) {
        if (hitEvents <= 0) {
            return;
        }

        const AbilityDefinition* abilityDef = manager_.findAbilityDefinition(context.abilityId);
        if (abilityDef == nullptr) {
            feedback_.queuePresentationHitShakes(context.isBoss, hitEvents, manager_);
            return;
        }

        int baseAtk = 0;
        if (context.isBoss) {
            baseAtk = manager_.getBattleState().boss.atk;
        } else {
            const BattleState& state = manager_.getBattleState();
            if (context.casterIndex >= 0 &&
                static_cast<size_t>(context.casterIndex) < state.party.size()) {
                baseAtk = state.party[static_cast<size_t>(context.casterIndex)].atk;
            }
        }

        float hitDamageMultiplier = 1.0f;
        if (activePresentation_ != nullptr) {
            hitDamageMultiplier = std::max(0.0f, activePresentation_->consumeHitDamageMultiplier());
        }

        const int totalDamage = std::max(1, static_cast<int>(
            baseAtk * abilityDef->multiplier * hitDamageMultiplier
        ));
        const int perHitDamage = std::max(1, totalDamage / std::max(1, damageLabelHitCount));

        manager_.applyPresentationHitDamage(context.isBoss, perHitDamage, hitEvents);
        if (hooks_.onPresentationHitAudio) {
            hooks_.onPresentationHitAudio(context.isBoss, hitEvents, manager_);
            manager_.markPresentationHitAudioPlayed();
        }
        feedback_.queuePresentationHitFeedback(context.isBoss, hitEvents, perHitDamage, manager_);
    };
    callbacks.onPostUpdate = [&](float deltaSeconds) {
        feedback_.syncFromManager(manager_, presentationPlaybackActive_);
        feedback_.update(deltaSeconds);
    };
    callbacks.onBossPresentationFrame = [&]() {
        computeCharacterPositions(true, -1);
    };
    callbacks.setRenderOverlay = [this](std::function<void(SDL_Renderer*, int, int)> fn) {
        activeOverlay_ = std::move(fn);
    };
    callbacks.clearRenderOverlay = [this]() {
        activeOverlay_ = nullptr;
    };
    callbacks.renderAndPresentFrame = [&]() {
        int screenWidth  = 1280;
        int screenHeight = 720;
        SDL_GetRendererOutputSize(renderer_, &screenWidth, &screenHeight);
        render(renderer_, screenWidth, screenHeight);
        if (activeOverlay_) activeOverlay_(renderer_, screenWidth, screenHeight);
        SDL_RenderPresent(renderer_);
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

    if (hooks_.onPresentationEnd) {
        hooks_.onPresentationEnd(context, result.resultText);
    }

    return result.multiplier;
}

void BattleSessionCore::setHint(const std::string& text, Uint32 displayMs) { hud_.setHint(text, displayMs); }
void BattleSessionCore::clearHint() { hud_.clearHint(); }

} // namespace battle
