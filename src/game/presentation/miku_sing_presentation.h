#ifndef MIKU_SING_PRESENTATION_H
#define MIKU_SING_PRESENTATION_H

#include "ability_presentation.h"
#include "miku_rhythm_game.h"
#include <string>
#include <vector>

namespace battle {

// Two-phase presentation:
//   Phase::RhythmGame — 4-lane falling-note mini-game (D/F/J/K).
//                        Player accuracy determines the ability damage bonus.
//   Phase::Animation  — original 6-note projectile fly-toward-target visual.
class MikuSingPresentation : public AbilityPresentation {
public:
    MikuSingPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool getCasterWorldOverride(float& outX, float& outY, float& outZ) const override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;
    void onKeyPressed(SDL_Keycode key) override;
    int consumeAbilityAudioCues() override;
    float getInputMultiplier() const override;
    PresentationFeedbackSignal getFeedbackSignal() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    std::string getInputResultText() const override;
    bool shouldRenderAboveHud() const override;

private:
    PresentationFeedbackEvent buildImmediateFeedbackEvent(const MikuRhythmGame::ResolvedNoteFeedback& feedback) const;
    void spawnNote();

    enum class Phase { RhythmGame, Animation };

    float casterWorldX_, casterWorldY_, casterWorldZ_;
    float targetWorldX_, targetWorldY_, targetWorldZ_;

    Phase          phase_                   = Phase::RhythmGame;
    MikuRhythmGame rhythmGame_;
    bool           pendingAbilityAudioCue_  = false;

    // Animation-phase state
    std::vector<ProjectileParticle> notes_;
    float nextSpawnTime_;
    float spawnInterval_;
    int   notesToSpawn_;
    int   notesSpawned_;
    int   pendingHitEvents_ = 0;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
};

} // namespace battle

#endif // MIKU_SING_PRESENTATION_H
