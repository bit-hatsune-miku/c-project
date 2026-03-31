#pragma once

#include "ability_presentation.h"

#include <string>
#include <vector>

namespace battle {

class MikuSelfCorruptionPresentation : public AbilityPresentation {
public:
    enum class NativePhase {
        CheerLine,
        Whiteout,
        BirdView,
        IntroFrames,
        GlitchFrames,
        CorruptionLoop,
        ReturnToWorld,
        FinalHits,
        Complete
    };

    struct NativeState {
        NativePhase phase = NativePhase::Complete;
        float elapsedTime = 0.0f;
        float phaseProgress = 1.0f;
        int emittedHitBursts = 0;
    };

    MikuSelfCorruptionPresentation(
        float casterWorldX,
        float casterWorldY,
        float casterWorldZ,
        float targetWorldX,
        float targetWorldY,
        float targetWorldZ);
    ~MikuSelfCorruptionPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    void preload(SDL_Renderer* renderer) override;
    bool isComplete() const override;

    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;

    void setPartyAssetNames(const std::vector<std::string>& assetNames) override;

    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderAboveHud() const override;
    bool shouldRenderFloor() const override;

    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;

    NativeState buildNativeState() const;

private:
    void ensureFallbackAssets(SDL_Renderer* renderer);
    void releaseFallbackAssets();
    void queueAudioCommand(PresentationAudioCommandType type, const std::string& id, float volume = 1.0f);
    NativePhase currentPhase() const;
    float phaseElapsed() const;
    float phaseProgress() const;
    void applyLookAt(Camera3D& camera, float lookX, float lookY, float lookZ) const;
    void applyShake(Camera3D& camera, float timeSeconds, float magnitude) const;
    SDL_Texture* loadedFrameAtIndex(std::size_t index) const;
    void renderFallbackFrame(SDL_Renderer* renderer, SDL_Texture* texture, int screenW, int screenH) const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    std::vector<std::string> partyAssetNames_;
    std::vector<PresentationAudioCommand> pendingAudioCommands_;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    int emittedHitBursts_ = 0;
    int nextCheerVoiceIndex_ = 0;

    std::vector<SDL_Texture*> fallbackFrames_;
    SDL_Renderer* fallbackRenderer_ = nullptr;
    bool attemptedFallbackLoad_ = false;
};

} // namespace battle
