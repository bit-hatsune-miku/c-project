#pragma once

#include "ability_presentation.h"

#include <vector>

namespace battle {

class PomPomGachaPresentation : public AbilityPresentation {
public:
    enum class Variant {
        Skill,
        Ultimate
    };

    PomPomGachaPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ,
        Variant variant
    );
    ~PomPomGachaPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void preload(SDL_Renderer* renderer) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void setResolvedRolls(const std::vector<int>& rolls) override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;
    PresentationFeedbackSignal getFeedbackSignal() const override;

    bool shouldRenderAboveHud() const override;

private:
    struct FrameSlot {
        SDL_Texture* texture = nullptr;
        bool attemptedLoad = false;
    };

    void recalculateTotalDuration();
    void releaseFrames();
    void maintainFrameWindow(SDL_Renderer* renderer, int frameIndex);
    void ensureFrameLoaded(SDL_Renderer* renderer, int frameIndex);
    void ensureStarLoaded(SDL_Renderer* renderer);
    void revealNextPull(SDL_Renderer* renderer);
    void drawStarRow(SDL_Renderer* renderer,
                     int screenW,
                     int screenH,
                     int rowIndex,
                     int rarity,
                     float revealProgress) const;

    Variant variant_ = Variant::Skill;
    std::vector<int> resolvedRolls_;
    std::vector<FrameSlot> frames_;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
    std::vector<PresentationAudioCommand> pendingAudioCommands_;
    int revealedPullCount_ = 0;
    SDL_Texture* starTexture_ = nullptr;
};

} // namespace battle
