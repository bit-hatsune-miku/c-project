#pragma once

#include "ability_presentation.h"

#include <vector>

namespace battle {

class DiscipleBossPresentation : public AbilityPresentation {
public:
    DiscipleBossPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    int consumeHitEvents() override;
    std::vector<int> consumeHitTargetIndices() override;
    void setResolvedTargetIndices(const std::vector<int>& targetIndices) override;
    int getFocusedPartyIndex() const override;
    void setTargetWorldPosition(float x, float y, float z) override;
    bool shouldBlackoutWorld() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldHideNonCasterCharacters() const override;

private:
    enum class Phase {
        Intro,
        Strike,
        Outro,
        Complete
    };

    void beginStrike();

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    Phase phase_ = Phase::Intro;
    float phaseElapsed_ = 0.0f;
    int currentStrikeIndex_ = 0;
    int currentTargetIndex_ = -1;
    bool hitQueued_ = false;
    int pendingHitEvents_ = 0;
    std::vector<int> targetSequence_;
    std::vector<int> pendingHitTargetIndices_;
};

} // namespace battle
