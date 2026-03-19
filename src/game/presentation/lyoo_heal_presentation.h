#ifndef LYOO_HEAL_PRESENTATION_H
#define LYOO_HEAL_PRESENTATION_H

#include "ability_presentation.h"

#include <array>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace battle {

class LyooHealPresentation : public AbilityPresentation {
public:
    enum class Variant {
        Skill,
        Ultimate
    };

    LyooHealPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ,
        Variant variant = Variant::Skill
    );
    ~LyooHealPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void onKeyPressed(SDL_Keycode key) override;
    float getInputMultiplier() const override;
    std::string getInputResultText() const override;
    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;

    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderBossEntity() const override;
    bool shouldRenderAboveHud() const override;

private:
    enum class Phase {
        Input,
        Outro,
        Complete
    };

    struct HeartParticle {
        float xNorm = 0.5f;
        float startYNorm = 1.06f;
        float targetYNorm = 0.5f;
        float elapsed = 0.0f;
        float riseDuration = 0.45f;
        float holdDuration = 0.08f;
        float fadeDuration = 0.18f;
        float sizePx = 28.0f;
        bool active = true;
    };

    static std::string resolveAssetPath(const std::string& relativePath);

    void ensureTexturesLoaded(SDL_Renderer* renderer);
    void releaseTextures();
    void updateHearts(float deltaTime);
    void spawnHeartsForValidPress();
    void renderHearts(SDL_Renderer* renderer, int screenW, int screenH) const;
    void renderUiSprite(SDL_Renderer* renderer, int screenW, int screenH) const;
    Uint8 currentOverlayAlpha() const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;
    Variant variant_ = Variant::Skill;

    std::array<SDL_Texture*, 3> frameTextures_{{nullptr, nullptr, nullptr}};
    SDL_Texture* heartTexture_ = nullptr;
    bool attemptedTextureLoad_ = false;

    Phase phase_ = Phase::Input;
    float inputElapsed_ = 0.0f;
    float outroElapsed_ = 0.0f;
    float ambientTime_ = 0.0f;
    int currentFrameIndex_ = 0;
    int nextPressFrameIndex_ = 1;
    float pressFrameHoldRemaining_ = 0.0f;
    int validPressCount_ = 0;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    bool healEventTriggered_ = false;
    std::deque<SDL_Keycode> recentKeys_;
    std::vector<HeartParticle> hearts_;
    mutable Camera3D inputCamera_{};
    mutable Camera3D outroCamera_{};
    std::mt19937 rng_;
};

} // namespace battle

#endif // LYOO_HEAL_PRESENTATION_H
