#include "disciple_debuff_presentation.h"

#include "../core/easing.h"

#include <algorithm>
#include <cmath>

namespace battle {
namespace {

constexpr float kSkillDurationSeconds = 0.76f;
constexpr float kUltimateDurationSeconds = 0.98f;
constexpr float kSkillStartOffsetX = 155.0f;
constexpr float kSkillStartOffsetY = -540.0f;
constexpr float kSkillStartOffsetZ = -180.0f;
constexpr float kSkillEndOffsetX = 125.0f;
constexpr float kSkillEndOffsetY = -455.0f;
constexpr float kSkillEndOffsetZ = -150.0f;

float lerpF(float a, float b, float t) {
    return a + ((b - a) * t);
}

} // namespace

DiscipleDebuffPresentation::DiscipleDebuffPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ,
    Variant variant
)
    : variant_(variant)
    , casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = variant_ == Variant::Ultimate ? kUltimateDurationSeconds : kSkillDurationSeconds;
}

void DiscipleDebuffPresentation::start() {
    elapsedTime_ = 0.0f;
    pendingAbilityAudioCues_ = 1;
}

void DiscipleDebuffPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
}

void DiscipleDebuffPresentation::render(SDL_Renderer* renderer,
                                        int screenW,
                                        int screenH,
                                        const Camera3D& camera) {
    if (renderer == nullptr) {
        return;
    }

    const float t = easing::clamp01(elapsedTime_ / std::max(0.001f, totalDuration_));
    const float pulse = std::sin(t * 12.56637f);
    const float vignetteAlpha = variant_ == Variant::Ultimate ? 165.0f : 120.0f;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 15, 0, 18, static_cast<Uint8>(vignetteAlpha * (0.35f + (0.65f * t))));
    SDL_FRect fullRect{0.0f, 0.0f, static_cast<float>(screenW), static_cast<float>(screenH)};
    SDL_RenderFillRectF(renderer, &fullRect);

    const SDL_FPoint bossScreen = camera.worldToScreen(targetX_, targetY_, targetZ_ - 120.0f);
    if (bossScreen.x <= -500000.0f || bossScreen.y <= -500000.0f) {
        return;
    }

    const float ringBase = variant_ == Variant::Ultimate ? 92.0f : 74.0f;
    const float ringPulse = (variant_ == Variant::Ultimate ? 30.0f : 20.0f) * (0.5f + (0.5f * pulse));
    SDL_SetRenderDrawColor(renderer, 255, 52, 86, static_cast<Uint8>(210.0f * (1.0f - (0.25f * t))));
    SDL_FRect innerRect{
        bossScreen.x - ringBase,
        bossScreen.y - ringBase,
        ringBase * 2.0f,
        ringBase * 2.0f
    };
    SDL_RenderDrawRectF(renderer, &innerRect);

    SDL_SetRenderDrawColor(renderer, 255, 170, 190, static_cast<Uint8>(190.0f * (1.0f - (0.20f * t))));
    SDL_FRect outerRect{
        bossScreen.x - (ringBase + ringPulse),
        bossScreen.y - (ringBase + ringPulse),
        (ringBase + ringPulse) * 2.0f,
        (ringBase + ringPulse) * 2.0f
    };
    SDL_RenderDrawRectF(renderer, &outerRect);

    SDL_SetRenderDrawColor(renderer, 255, 64, 64, static_cast<Uint8>(225.0f * (0.65f + (0.35f * (1.0f - t)))));
    SDL_RenderDrawLineF(renderer,
                        bossScreen.x - 110.0f,
                        bossScreen.y - 110.0f,
                        bossScreen.x + 110.0f,
                        bossScreen.y + 110.0f);
    SDL_RenderDrawLineF(renderer,
                        bossScreen.x - 110.0f,
                        bossScreen.y + 110.0f,
                        bossScreen.x + 110.0f,
                        bossScreen.y - 110.0f);
}

bool DiscipleDebuffPresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

int DiscipleDebuffPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

bool DiscipleDebuffPresentation::overridesCamera() const {
    return true;
}

void DiscipleDebuffPresentation::applyCameraState(Camera3D& camera) const {
    const float t = easing::easeOutQuint(
        easing::clamp01(elapsedTime_ / std::max(0.001f, totalDuration_))
    );
    camera.posX = lerpF(targetX_ + kSkillStartOffsetX, targetX_ + kSkillEndOffsetX, t);
    camera.posY = lerpF(targetY_ + kSkillStartOffsetY, targetY_ + kSkillEndOffsetY, t);
    camera.posZ = lerpF(kSkillStartOffsetZ, kSkillEndOffsetZ, t);
    camera.pitchDegrees = lerpF(-4.8f, -3.4f, t);
    camera.yawDegrees = lerpF(-5.0f, -1.5f, t);
    camera.focalLength = lerpF(
        variant_ == Variant::Ultimate ? 25500.0f : 28000.0f,
        variant_ == Variant::Ultimate ? 33500.0f : 31500.0f,
        t
    );
}

bool DiscipleDebuffPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool DiscipleDebuffPresentation::shouldRenderAboveHud() const {
    return false;
}

} // namespace battle
