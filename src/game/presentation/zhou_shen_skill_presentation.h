#pragma once

#include "ability_presentation.h"

#include <vector>

namespace battle {

class ZhouShenSkillPresentation : public AbilityPresentation {
public:
    ZhouShenSkillPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    int consumeAbilityAudioCues() override;
    float getInputMultiplier() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;

    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    void setPresentationValue(int value) override;

    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldUseCenteredPartyLayout() const override;

private:
    void emitSingerFeedback(int singerIndex);
    static float buffMultiplierForSingerCount(int singerCount);
    static std::string rewardTextForSingerCount(int singerCount);

    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    int singerCount_ = 1;
    int emittedFeedbackCount_ = 0;
    int pendingAbilityAudioCues_ = 0;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
};

} // namespace battle
