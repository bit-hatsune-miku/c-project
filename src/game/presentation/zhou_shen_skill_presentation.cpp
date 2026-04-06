#include "zhou_shen_skill_presentation.h"
#include "../core/easing.h"

namespace battle {

namespace {
constexpr float kDuration = 1.5f;
constexpr float kStartOffsetX = -170.0f;
constexpr float kStartOffsetY = 40.0f;
constexpr float kStartOffsetZ = -125.0f;
constexpr float kEndOffsetX = -180.0f;
constexpr float kEndOffsetY = 80.0f;
constexpr float kEndOffsetZ = -129.0f;

Camera3D makeZoomStartCamera(float anchorX, float anchorY, float anchorZ) {
    Camera3D camera;
    camera.posX = anchorX + kStartOffsetX;
    camera.posY = anchorY + kStartOffsetY;
    camera.posZ = anchorZ + kStartOffsetZ;
    camera.pitchDegrees = -1.3f;
    camera.yawDegrees = 180.0f;
    camera.focalLength = 38000.0f;
    return camera;
}

Camera3D makeZoomEndCamera(float anchorX, float anchorY, float anchorZ) {
    Camera3D camera;
    camera.posX = anchorX + kEndOffsetX;
    camera.posY = anchorY + kEndOffsetY;
    camera.posZ = anchorZ + kEndOffsetZ;
    camera.pitchDegrees = -1.0f;
    camera.yawDegrees = 180.0f;
    camera.focalLength = 36000.0f;
    return camera;
}

float lerpF(float a, float b, float t) { return a + ((b - a) * t); }
}

ZhouShenSkillPresentation::ZhouShenSkillPresentation(int numSingers, float tx, float ty, float tz)
    : numSingers_(numSingers), targetX_(tx), targetY_(ty), targetZ_(tz) {}

void ZhouShenSkillPresentation::start() {
    elapsed_ = 0.0f;
    hitsEmitted_ = 0;
}

void ZhouShenSkillPresentation::update(float dt) {
    elapsed_ += dt;
    float interval = 0.2f;
    int targetHits = static_cast<int>(elapsed_ / interval);
    if (targetHits > numSingers_) targetHits = numSingers_;

    while (hitsEmitted_ < targetHits) {
        hitsEmitted_++;
        PresentationFeedbackEvent ev;
        ev.worldX = targetX_; ev.worldY = targetY_ + 100.0f; ev.worldZ = targetZ_;
        ev.grade = 3; // PERFECT
        ev.isCombo = false;
        pendingFeedbackEvents_.push_back(ev);
    }
}

void ZhouShenSkillPresentation::render(SDL_Renderer*, int, int, const Camera3D&) {}

bool ZhouShenSkillPresentation::isComplete() const {
    return elapsed_ >= kDuration && hitsEmitted_ >= numSingers_;
}

bool ZhouShenSkillPresentation::overridesCamera() const { return true; }

void ZhouShenSkillPresentation::applyCameraState(Camera3D& camera) const {
    const Camera3D startC = makeZoomStartCamera(targetX_, targetY_, targetZ_);
    const Camera3D endC = makeZoomEndCamera(targetX_, targetY_, targetZ_);
    float t = easing::easeOutQuint(easing::clamp01(elapsed_ / kDuration));
    camera.posX = lerpF(startC.posX, endC.posX, t);
    camera.posY = lerpF(startC.posY, endC.posY, t);
    camera.posZ = lerpF(startC.posZ, endC.posZ, t);
    camera.pitchDegrees = lerpF(startC.pitchDegrees, endC.pitchDegrees, t);
    camera.yawDegrees = lerpF(startC.yawDegrees, endC.yawDegrees, t);
    camera.focalLength = lerpF(startC.focalLength, endC.focalLength, t);
}

int ZhouShenSkillPresentation::consumeHitEvents() {
    return 0; // The actual buff applying can just be driven entirely by the battle logic. Or we return hits.
}

std::vector<PresentationFeedbackEvent> ZhouShenSkillPresentation::consumeFeedbackEvents() {
    auto res = pendingFeedbackEvents_;
    pendingFeedbackEvents_.clear();
    return res;
}

} // namespace battle
