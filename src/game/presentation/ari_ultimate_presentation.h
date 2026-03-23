#pragma once

#include "ability_presentation.h"

#include <string>
#include <vector>

namespace battle {

class AriUltimatePresentation : public AbilityPresentation {
public:
    AriUltimatePresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~AriUltimatePresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    int consumeAbilityAudioCues() override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;
    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldUseCenteredPartyLayout() const override;

private:
    struct FrameSlot {
        SDL_Texture* texture = nullptr;
        bool attemptedLoad = false;
    };

    enum class Phase {
        UiAnimation,
        Reveal,
        Complete
    };

    static std::string resolvePath(const std::string& relativePath);

    void releaseFrames();
    void maintainFrameWindow(SDL_Renderer* renderer, int frameIndex);
    void ensureFrameLoaded(SDL_Renderer* renderer, int frameIndex);

    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    Phase phase_ = Phase::UiAnimation;
    float phaseElapsed_ = 0.0f;
    int pendingAbilityAudioCues_ = 0;
    std::vector<FrameSlot> frames_;
    std::vector<PresentationAudioCommand> pendingAudioCommands_;
    bool bgmPaused_ = false;
    bool bgmResumeQueued_ = false;
};

} // namespace battle
