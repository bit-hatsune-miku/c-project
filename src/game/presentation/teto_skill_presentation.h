#pragma once

#include "ability_presentation.h"

#include <deque>

namespace battle {

class TetoSkillPresentation : public AbilityPresentation {
public:
    TetoSkillPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    bool onKeyPressed(SDL_Keycode key) override;
    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    float getInputMultiplier() const override;
    std::string getInputResultText() const override;
    PresentationFeedbackSignal getFeedbackSignal() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    int getFocusedPartyIndex() const override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldUseCenteredPartyLayout() const override;
    void setPartyTargetableStates(const std::vector<bool>& targetableStates) override;

private:
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    float elapsedInputWindow_ = 0.0f;
    float inputWindowDuration_ = 2.0f;
    int selectedPartyIndex_ = -1;
    int activeHitPartyIndex_ = -1;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    bool forcedPerfect_ = false;
    std::vector<bool> partyTargetableStates_;
    std::deque<int> pendingHitPartyIndices_;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
};

} // namespace battle
