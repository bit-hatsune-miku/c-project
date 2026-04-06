#include "zhou_shen_skill_presentation.h"

#include "../core/easing.h"

#include <algorithm>
#include <utility>

namespace battle {
namespace {

constexpr float kOutroDurationSeconds = 0.90f;
constexpr float kFeedbackIntervalSeconds = 0.20f;
constexpr float kStartOffsetX = -170.0f;
constexpr float kStartOffsetY = 40.0f;
constexpr float kStartOffsetZ = -125.0f;
constexpr float kEndOffsetX = -180.0f;
constexpr float kEndOffsetY = 80.0f;
constexpr float kEndOffsetZ = -129.0f;

Camera3D makeSupportZoomStartCamera(float anchorX, float anchorY, float anchorZ) {
    Camera3D camera;
    camera.posX = anchorX + kStartOffsetX;
    camera.posY = anchorY + kStartOffsetY;
    camera.posZ = anchorZ + kStartOffsetZ;
    camera.pitchDegrees = -1.3f;
    camera.yawDegrees = 180.0f;
    camera.focalLength = 38000.0f;
    return camera;
}

Camera3D makeSupportZoomEndCamera(float anchorX, float anchorY, float anchorZ) {
    Camera3D camera;
    camera.posX = anchorX + kEndOffsetX;
    camera.posY = anchorY + kEndOffsetY;
    camera.posZ = anchorZ + kEndOffsetZ;
    camera.pitchDegrees = -1.0f;
    camera.yawDegrees = 180.0f;
    camera.focalLength = 36000.0f;
    return camera;
}

float lerpF(float a, float b, float t) {
    return a + ((b - a) * t);
}

int clampSingerCount(int singerCount) {
    return std::clamp(singerCount, 1, 4);
}

} // namespace

ZhouShenSkillPresentation::ZhouShenSkillPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    (void)casterWorldX;
    (void)casterWorldY;
    (void)casterWorldZ;
    totalDuration_ = kOutroDurationSeconds;
}

void ZhouShenSkillPresentation::start() {
    elapsedTime_ = 0.0f;
    emittedFeedbackCount_ = 0;
    pendingAbilityAudioCues_ = 1;
    pendingFeedbackEvents_.clear();
}

void ZhouShenSkillPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    while (emittedFeedbackCount_ < singerCount_ &&
           elapsedTime_ >= (static_cast<float>(emittedFeedbackCount_) * kFeedbackIntervalSeconds)) {
        emitSingerFeedback(emittedFeedbackCount_);
        ++emittedFeedbackCount_;
    }
}

void ZhouShenSkillPresentation::render(SDL_Renderer* renderer,
                                       int screenW,
                                       int screenH,
                                       const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool ZhouShenSkillPresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_ && emittedFeedbackCount_ >= singerCount_;
}

int ZhouShenSkillPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

float ZhouShenSkillPresentation::getInputMultiplier() const {
    return buffMultiplierForSingerCount(singerCount_);
}

std::vector<PresentationFeedbackEvent> ZhouShenSkillPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

bool ZhouShenSkillPresentation::overridesCamera() const {
    return true;
}

void ZhouShenSkillPresentation::applyCameraState(Camera3D& camera) const {
    const Camera3D startCamera = makeSupportZoomStartCamera(targetX_, targetY_, targetZ_);
    const Camera3D endCamera = makeSupportZoomEndCamera(targetX_, targetY_, targetZ_);
    const float t = easing::easeOutQuint(
        easing::clamp01(elapsedTime_ / kOutroDurationSeconds)
    );

    camera.posX = lerpF(startCamera.posX, endCamera.posX, t);
    camera.posY = lerpF(startCamera.posY, endCamera.posY, t);
    camera.posZ = lerpF(startCamera.posZ, endCamera.posZ, t);
    camera.pitchDegrees = lerpF(startCamera.pitchDegrees, endCamera.pitchDegrees, t);
    camera.yawDegrees = lerpF(startCamera.yawDegrees, endCamera.yawDegrees, t);
    camera.focalLength = lerpF(startCamera.focalLength, endCamera.focalLength, t);
}

void ZhouShenSkillPresentation::setPresentationValue(int value) {
    singerCount_ = clampSingerCount(value);
}

bool ZhouShenSkillPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool ZhouShenSkillPresentation::shouldRenderCasterEntity() const {
    return true;
}

bool ZhouShenSkillPresentation::shouldRenderBossEntity() const {
    return false;
}

bool ZhouShenSkillPresentation::shouldRenderAboveHud() const {
    return false;
}

bool ZhouShenSkillPresentation::shouldUseCenteredPartyLayout() const {
    return true;
}

void ZhouShenSkillPresentation::emitSingerFeedback(int singerIndex) {
    PresentationFeedbackEvent event;
    event.signal = PresentationFeedbackSignal::forcedPerfect();
    event.multiplier = getInputMultiplier();
    event.comboEligible = false;
    if (singerIndex + 1 == singerCount_) {
        event.rewardText = rewardTextForSingerCount(singerCount_);
    }
    pendingFeedbackEvents_.push_back(std::move(event));
}

float ZhouShenSkillPresentation::buffMultiplierForSingerCount(int singerCount) {
    switch (clampSingerCount(singerCount)) {
        case 1: return 0.125f;
        case 2: return 0.25f;
        case 3: return 0.375f;
        case 4:
        default:
            return 1.0f;
    }
}

std::string ZhouShenSkillPresentation::rewardTextForSingerCount(int singerCount) {
    switch (clampSingerCount(singerCount)) {
        case 1: return "+20% DMG";
        case 2: return "+40% DMG";
        case 3: return "+60% DMG";
        case 4:
        default:
            return "+160% DMG";
    }
}

} // namespace battle
