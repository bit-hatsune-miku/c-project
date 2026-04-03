#ifndef BATTLE_SESSION_CORE_H
#define BATTLE_SESSION_CORE_H

#include <functional>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "core/battle_manager.h"
#include "core/battle_flow_controller.h"
#include "core/battle_turn_flow.h"
#include "presentation/ability_presentation.h"
#include "presentation/splash_art_animation.h"
#include "render/battle_camera_staging.h"
#include "render/battle_combat_begin_animation.h"
#include "render/battle_feedback.h"
#include "render/battle_scene_types.h"
#include "render/battle_stage.h"
#include "render/battle_ui.h"
#include "render/camera_3d.h"
#include "render/free_view_camera_debug_log.h"

namespace battle {

// Generic SDL2-based battle session runtime.
// Owns all shared machinery: scene entities, textures, camera, HUD, feedback system,
// and ability presentation playback. Demo-specific behaviour is injected via Hooks.
class BattleSessionCore {
public:
    struct BattleFrameSnapshot {
        Camera3D camera{};
        const render::StageDefinition* stage = nullptr;
        std::vector<render::SceneEntity> entities;
        std::vector<render::FeedbackEntityAnchor> feedbackAnchors;
        std::vector<float> shakeOffsetsX;
        int focusedEntityIndex = -1;
        float frameAccumulator = 0.0f;
        bool blackoutWorld = false;
        bool renderFloor = true;
        bool presentationPlaybackActive = false;
        bool presentationCasterIsBoss = false;
        int presentationCasterPartyIndex = -1;
        const AbilityPresentation* activePresentation = nullptr;
        const SplashArtAnimation* activeUltimateTurnSplash = nullptr;
    };

    struct Hooks {
        // Return true when narrative dialogue is in progress.
        // Used to gate camera intro animations and the finish condition.
        std::function<bool()> isDialogueInProgress;

        // Called on SPACE key press before battle input is processed.
        // Return true if the hook consumed the event (e.g. dialogue advance).
        std::function<bool()> onSpacePressed;

        // Return true when battle-turn space input is permitted by the host
        // (e.g. tutorial has been cleared).
        std::function<bool()> isSpaceEnabledForBattle;

        // Called after a player turn is executed.
        // Return true to allow automatic (enemy/follow-up) turns to proceed.
        std::function<bool(const flow::PlayerTurnExecution&)> onPlayerTurnExecuted;

        // Called at the start of each update frame with the manager and deltaSeconds.
        // Use for narrative state updates, vn::update, etc.
        std::function<void(BattleManager&, float)> onPreUpdate;

        // Called once per boss phase transition after manager state has been updated.
        std::function<void(const BossPhaseTransition&, BattleManager&)> onBossPhaseTransition;

        // Return true to keep the battle session alive after the manager reports
        // battle over, for host-managed timing such as death voice playback.
        std::function<bool(const BattleManager&)> isBattleFinishBlocked;

        // Called immediately when a presentation reports hit events.
        // Use for hit voice playback that should land exactly on hit timing.
        std::function<void(const PresentationContext&, int hitEvents, BattleManager&)> onPresentationHitAudio;

        // Called immediately when a presentation applies heal events.
        // Use for healing voice playback that should land as soon as the heal resolves.
        std::function<void(const PresentationContext&, int hitEvents, BattleManager&)> onPresentationHealAudio;

        // Called when a presentation emits an explicit ability-audio cue.
        // Use for voice lines that should begin slightly after the animation starts.
        std::function<void(const PresentationContext&, int cueCount, BattleManager&)> onPresentationAbilityAudio;

        // Called when a presentation emits explicit timed audio commands.
        // Use for presentation-specific SFX/BGM control.
        std::function<void(const PresentationContext&, const std::vector<PresentationAudioCommand>&)>
            onPresentationAudioCommands;

        // Called when a splash art intro is about to start rendering.
        // Use for ready/announcement voice lines tied to the caster.
        std::function<void(const PresentationContext&, const std::string& casterAssets)> onPresentationSplashVoice;

        // Called immediately after a presentation finishes playback.
        // Use for cleanup like clearing hint messages.
        std::function<void(const PresentationContext&, const std::string& resultText)> onPresentationEnd;

        // Called when the presentation runtime detects a window resize (e.g. vn::setViewportSize).
        std::function<void(int width, int height)> onWindowResized;

        // Called at the very end of each render frame (e.g. VN dialogue overlay).
        std::function<void()> onPostRender;

        // Optional custom render/present path for blocking presentation playback.
        // When unset, the core uses its default SDL renderer world path.
        std::function<void()> onRenderAndPresentFrame;

        // Called at the very end of shutdown (e.g. vn::stopVoicePlayback).
        std::function<void()> onShutdown;
    };

    bool initialize(SDL_Renderer* renderer,
                    const BattleDefinition& battleDefinition,
                    const std::vector<std::string>& partyKeys,
                    Hooks hooks = {});

    void shutdown();

    void handleEvent(const SDL_Event& event);
    void update(float deltaSeconds);
    void render(SDL_Renderer* renderer, int screenWidth, int screenHeight);
    BattleFrameSnapshot buildFrameSnapshot(int screenWidth, int screenHeight) const;
    void renderCompatibilityBelowWorld(SDL_Renderer* renderer, int screenWidth, int screenHeight);
    void renderCompatibilityMidWorld(SDL_Renderer* renderer,
                                     int screenWidth,
                                     int screenHeight,
                                     bool includeFeedback = true);
    void renderCompatibilityOverlay(SDL_Renderer* renderer,
                                    int screenWidth,
                                    int screenHeight,
                                    bool includeFeedback = false);
    bool hasCompatibilityOverlay() const;
    bool hasVisibleFeedbackPopups() const;

    bool isFinished() const;
    bool isCombatBeginAnimationActive() const;

    void setHint(const std::string& text, Uint32 displayMs = 0);
    void clearHint();

    BattleManager& getBattleManager();
    const BattleManager& getBattleManager() const;

private:
    int computeFocusedEntityIndex() const;
    std::vector<render::FeedbackEntityAnchor> buildFeedbackAnchors() const;
    bool handleManualUltimateHotkey(SDL_Keycode key);
    void maybeStartUltimateTurnSplash(const flow::PreviewActorContext& preview, bool dialogueActive);
    void startBossPhaseIntro(const BossPhaseTransition& transition);
    void updateBossPhaseIntro(float deltaSeconds);
    void updateSceneEntities(float deltaSeconds, bool bossActing, int actingPartyIndex);
    void computeCharacterPositions(bool bossActing, int actingPartyIndex);
    void updateCharacterVisibilityTransitions(float deltaSeconds);
    float runPresentationInteraction(const PresentationContext& context);

    std::string resolvePresentationCasterAsset(const PresentationContext& context) const;

    bool initialized_ = false;
    bool finished_ = false;
    SDL_Renderer* renderer_ = nullptr;

    BattleManager manager_;
    std::vector<render::SceneEntity> entities_;
    std::map<std::string, SDL_Texture*> textureByAsset_;
    std::map<std::string, SDL_Texture*> iconByAsset_;
    render::StageRenderData stageRenderData_{};

    ui::BattleHud hud_;
    Camera3D camera_;
    render::BattleCameraStaging cameraStaging_;
    render::BattleCombatBeginAnimation combatBeginAnimation_;
    render::FreeViewCameraDebugLog freeViewCameraDebugLog_;
    bool freeViewEnabled_ = false;
    float frameAccumulator_ = 0.0f;

    bool presentationPlaybackActive_ = false;
    bool presentationCasterIsBoss_ = false;
    int presentationCasterPartyIndex_ = -1;
    AbilityPresentation* activePresentation_ = nullptr;
    std::unique_ptr<SplashArtAnimation> activeUltimateTurnSplash_;
    int previewUltimateSplashPartyIndex_ = -1;

    struct BossPhaseIntroState {
        bool active = false;
        int phaseIndex = -1;
        std::string hintText;
        float elapsed = 0.0f;
        float duration = 0.78f;
        Camera3D startCamera{};
        Camera3D goalCamera{};
    } bossPhaseIntro_;

    // Set by runPresentationInteraction during the splash pre-loop;
    // called between world render and SDL_RenderPresent each frame.
    std::function<void(SDL_Renderer*, int, int)> activeOverlay_;

    render::BattleFeedbackSystem feedback_;
    std::size_t processedBattleEventCount_ = 0;
    bool discardNextUpdateDelta_ = false;
    Hooks hooks_;
};

} // namespace battle

#endif
