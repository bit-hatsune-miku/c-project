#pragma once

#include "ability_presentation.h"

#include <string>

namespace battle {

class WechatalipayUltimatePresentation : public AbilityPresentation {
public:
    WechatalipayUltimatePresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~WechatalipayUltimatePresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldRenderCasterEntity() const override;
    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;

private:
    enum class Phase {
        Search,
        Lock,
        Zoom,
        Hold,
        Complete
    };

    struct LoadedTexture {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

    static std::string resolvePath(const std::string& relativePath);

    void ensureTextureLoaded(SDL_Renderer* renderer);
    void destroyTexture();
    void advancePhase(Phase nextPhase);
    SDL_FPoint bossScreenAnchor(const Camera3D& camera) const;
    SDL_FPoint searchPhoneOffset(float progress) const;
    SDL_FRect phoneRect(const SDL_FPoint& center, float scale) const;
    void drawPhone(SDL_Renderer* renderer, const SDL_FRect& rect, Uint8 alpha) const;
    void drawLockReticle(SDL_Renderer* renderer, const SDL_FPoint& center, float radius, Uint8 alpha) const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    LoadedTexture phoneTexture_{};
    bool attemptedTextureLoad_ = false;

    Phase phase_ = Phase::Search;
    float phaseElapsed_ = 0.0f;
    bool queuedUltimateAudio_ = false;
    bool hitTriggered_ = false;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    int screenW_ = 1280;
    int screenH_ = 720;
};

} // namespace battle
