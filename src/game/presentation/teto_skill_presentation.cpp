#include "teto_skill_presentation.h"

#include "../core/easing.h"
#include <iostream>

namespace battle {
namespace {

constexpr float kOutroDurationSeconds = 1.0f;
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

} // namespace

TetoSkillPresentation::TetoSkillPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) : targetX_(targetWorldX)
  , targetY_(targetWorldY)
  , targetZ_(targetWorldZ) {
    (void)casterWorldX;
    (void)casterWorldY;
    (void)casterWorldZ;
    totalDuration_ = kOutroDurationSeconds;
}

void TetoSkillPresentation::start() {
    elapsedTime_ = 0.0f;
    elapsedInputWindow_ = 0.0f;
    selectedPartyIndex_ = -1;
    activeHitPartyIndex_ = -1;
    forcedPerfect_ = false;
    pendingAbilityAudioCues_ = 0;
    pendingHitEvents_ = 0;
    pendingHitPartyIndices_.clear();
    pendingFeedbackEvents_.clear();
}

void TetoSkillPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
    elapsedInputWindow_ += deltaTime;
}

void TetoSkillPresentation::render(SDL_Renderer* renderer,
                                    int screenW,
                                    int screenH,
                                    const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool TetoSkillPresentation::isComplete() const {
    return elapsedTime_ >= kOutroDurationSeconds;
}

bool TetoSkillPresentation::onKeyPressed(SDL_Keycode key) {
    // Map keys 1-4 to party indices 0-3 first so we can consume them
    // even after selection/input close (prevents ultimate-hotkey fallback hints).
    int partyIndex = -1;
    switch (key) {
        case SDLK_1: partyIndex = 0; break;
        case SDLK_2: partyIndex = 1; break;
        case SDLK_3: partyIndex = 2; break;
        case SDLK_4: partyIndex = 3; break;
        default: return false;
    }

    // If input window has closed, ignore but consume numeric keys
    if (elapsedInputWindow_ >= inputWindowDuration_) {
        std::cout << "[TetoSkill] Key " << key << " ignored: inputWindow="
                  << (elapsedInputWindow_ >= inputWindowDuration_) 
                  << " selected=" << (selectedPartyIndex_ >= 0) << "\n";
        return true;
    }

    std::cout << "[TetoSkill] Key pressed: " << key 
              << " (partyIndex=" << partyIndex 
              << ", targetableStates=" << partyTargetableStates_.size() << ")\n";

    // Must have party targetable states configured
    if (partyTargetableStates_.empty()) {
        std::cout << "[TetoSkill] ERROR: partyTargetableStates is empty!\n";
        return true;
    }

    // Check if target is valid and alive
    if (partyIndex < 0 || partyIndex >= static_cast<int>(partyTargetableStates_.size())) {
        std::cout << "[TetoSkill] ERROR: partyIndex " << partyIndex 
                  << " out of bounds (size=" << partyTargetableStates_.size() << ")\n";
        return true;
    }

    if (!partyTargetableStates_[partyIndex]) {
        std::cout << "[TetoSkill] Target " << partyIndex << " is not alive, consuming key\n";
        // Dead or invalid target - consume key but don't select
        return true;
    }

    // Valid selection
    std::cout << "[TetoSkill] Selected party index " << partyIndex << " - PERFECT!\n";
    selectedPartyIndex_ = partyIndex;
    activeHitPartyIndex_ = partyIndex;
    forcedPerfect_ = true;
    pendingAbilityAudioCues_ = 1;
    pendingHitEvents_ += 1;
    pendingHitPartyIndices_.push_back(partyIndex);

    PresentationFeedbackEvent feedbackEvent;
    feedbackEvent.signal = PresentationFeedbackSignal::forcedPerfect();
    feedbackEvent.multiplier = 1.0f;
    feedbackEvent.rewardText.clear();
    feedbackEvent.comboEligible = true;
    pendingFeedbackEvents_.push_back(feedbackEvent);

    return true;
}

int TetoSkillPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int TetoSkillPresentation::consumeHitEvents() {
    if (pendingHitEvents_ <= 0 || pendingHitPartyIndices_.empty()) {
        return 0;
    }

    activeHitPartyIndex_ = pendingHitPartyIndices_.front();
    pendingHitPartyIndices_.pop_front();
    pendingHitEvents_ -= 1;
    return 1;
}

float TetoSkillPresentation::getInputMultiplier() const {
    return forcedPerfect_ ? 1.0f : 0.0f;
}

std::string TetoSkillPresentation::getInputResultText() const {
    if (selectedPartyIndex_ >= 0) {
        return "PERFECT";
    }
    return "";
}

PresentationFeedbackSignal TetoSkillPresentation::getFeedbackSignal() const {
    if (selectedPartyIndex_ >= 0 && forcedPerfect_) {
        std::cout << "[TetoSkill] Returning ForcedPerfect feedback signal\n";
        return PresentationFeedbackSignal::forcedPerfect();
    }
    std::cout << "[TetoSkill] No feedback signal (selected=" << (selectedPartyIndex_ >= 0) 
              << ", forcedPerfect=" << forcedPerfect_ << ")\n";
    return {};
}

std::vector<PresentationFeedbackEvent> TetoSkillPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

int TetoSkillPresentation::getFocusedPartyIndex() const {
    if (activeHitPartyIndex_ >= 0) {
        return activeHitPartyIndex_;
    }
    return selectedPartyIndex_;
}

bool TetoSkillPresentation::overridesCamera() const {
    return true;
}

void TetoSkillPresentation::applyCameraState(Camera3D& camera) const {
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

bool TetoSkillPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool TetoSkillPresentation::shouldRenderCasterEntity() const {
    return false;
}

bool TetoSkillPresentation::shouldRenderBossEntity() const {
    return true;
}

bool TetoSkillPresentation::shouldRenderAboveHud() const {
    return false;
}

bool TetoSkillPresentation::shouldUseCenteredPartyLayout() const {
    return true;
}

void TetoSkillPresentation::setPartyTargetableStates(const std::vector<bool>& targetableStates) {
    partyTargetableStates_ = targetableStates;
}

} // namespace battle
