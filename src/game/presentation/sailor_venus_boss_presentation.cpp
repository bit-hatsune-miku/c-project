#include "sailor_venus_boss_presentation.h"

#include "../core/easing.h"
#include "../render/battle_camera_staging.h"
#include "../../platform/path_resolution.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr int kFrameWindowRadius = 6;
constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kBossDamageReductionRatio = 0.8f;
constexpr float kBossMinDamageMultiplier = 0.2f;
constexpr float kImpactShotDurationSeconds = 1.0f;
constexpr float kImpactFlashFadeInEndRatio = 0.05f;
constexpr float kImpactFlashFadeOutEndRatio = 0.16f;
constexpr float kImpactFrameFadeInStartRatio = 0.05f;
constexpr float kImpactFrameHoldEndRatio = 0.26f;
constexpr float kImpactFrameFadeOutEndRatio = 0.42f;
constexpr float kImpactStartFocal = 76000.0f;
constexpr float kImpactZoomOutFocal = 20500.0f;
constexpr float kImpactCameraLift = -18.0f;
constexpr float kImpactCameraPullback = -92.0f;
constexpr float kImpactCameraTightenY = -72.0f;
constexpr float kImpactCameraTightenZ = 34.0f;
constexpr float kImpactPitchOffset = -8.5f;
constexpr float kImpactCameraWhipEndRatio = 0.16f;
constexpr float kMinimumDepth = 1.0f;

struct SequenceSpec {
    const char* frameDirectory = "";
    const char* voicePath = "";
    int frameCount = 1;
    float durationSeconds = 1.0f;
    bool allowsSpaceInput = false;
    bool emitsHit = true;
};

const SequenceSpec& sequenceSpecFor(SailorVenusBossPresentation::Variant variant) {
    static const SequenceSpec kBossParry{
        "loveAndBeautyShock",
        "assets/combat/voices/sailorVenus/loveandbeautyshock.wav",
        223,
        9.045333f,
        true,
        true
    };
    static const SequenceSpec kLoveAndBeautyShock{
        "loveAndBeautyShock",
        "assets/combat/voices/sailorVenus/loveandbeautyshock.wav",
        223,
        9.045333f,
        true,
        true
    };
    static const SequenceSpec kTransformation{
        "transformation",
        "assets/combat/voices/sailorVenus/transformation.wav",
        738,
        24.633500f,
        false,
        false
    };
    static const SequenceSpec kCrescentBeam{
        "crescentBeam",
        "assets/combat/voices/sailorVenus/crescentBeam.wav",
        179,
        6.058667f,
        false,
        true
    };
    static const SequenceSpec kLoveMeChain{
        "loveMeChain",
        "assets/combat/voices/sailorVenus/loveMeChain.wav",
        230,
        9.633500f,
        false,
        true
    };

    switch (variant) {
        case SailorVenusBossPresentation::Variant::Transformation:
            return kTransformation;
        case SailorVenusBossPresentation::Variant::CrescentBeam:
            return kCrescentBeam;
        case SailorVenusBossPresentation::Variant::LoveMeChain:
            return kLoveMeChain;
        case SailorVenusBossPresentation::Variant::LoveAndBeautyShock:
            return kLoveAndBeautyShock;
        case SailorVenusBossPresentation::Variant::BossParry:
        default:
            return kBossParry;
    }
}

std::string resolveFramePath(SailorVenusBossPresentation::Variant variant, int frameNumber) {
    const SequenceSpec& spec = sequenceSpecFor(variant);
    char relativePath[256];
    std::snprintf(
        relativePath,
        sizeof(relativePath),
        "assets/combat/presentations/sailorVenus/%s/frame_%06d.png",
        spec.frameDirectory,
        frameNumber
    );
    return platform::path::resolvePath(relativePath);
}

std::string resolveVoicePath(SailorVenusBossPresentation::Variant variant) {
    return platform::path::resolvePath(sequenceSpecFor(variant).voicePath);
}

float clampRatio(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float remap01(float value, float start, float end) {
    if (std::fabs(end - start) <= 0.0001f) {
        return value >= end ? 1.0f : 0.0f;
    }
    return clampRatio((value - start) / (end - start));
}

bool shouldPauseBgmForVariant(SailorVenusBossPresentation::Variant variant) {
    return variant != SailorVenusBossPresentation::Variant::CrescentBeam;
}

} // namespace

SailorVenusBossPresentation::SailorVenusBossPresentation(
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
    totalDuration_ = sequencePlaybackDuration() + (usesImpactShot() ? kImpactShotDurationSeconds : 0.0f);
}

SailorVenusBossPresentation::~SailorVenusBossPresentation() {
    releaseFrames();
}

void SailorVenusBossPresentation::start() {
    elapsedTime_ = 0.0f;
    spacesPressed_ = 0;
    presentationValue_ = std::max(0, presentationValue_);
    hasHit_ = false;
    bgmPaused_ = false;
    bgmResumeQueued_ = false;
    audioCmds_.clear();
    pendingFeedbackEvents_.clear();
    inputWindow_.startTime = 0.0f;
    inputWindow_.endTime = sequencePlaybackDuration();
    inputWindow_.active = sequenceSpecFor(variant_).allowsSpaceInput;
    if (shouldPauseBgmForVariant(variant_)) {
        audioCmds_.push_back(PresentationAudioCommand{
            PresentationAudioCommandType::PauseBgm,
            {},
            1.0f
        });
        bgmPaused_ = true;
    }
    audioCmds_.push_back(PresentationAudioCommand{
        PresentationAudioCommandType::PlayVoiceOneShot,
        resolveVoicePath(variant_),
        1.0f
    });
}

void SailorVenusBossPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;
    if (inputWindow_.active && elapsedTime_ >= inputWindow_.endTime) {
        inputWindow_.active = false;
    }
    const float bgmResumeTime = usesImpactShot() ? sequencePlaybackDuration() : totalDuration_;
    if (bgmPaused_ && !bgmResumeQueued_ && elapsedTime_ >= bgmResumeTime) {
        audioCmds_.push_back(PresentationAudioCommand{
            PresentationAudioCommandType::ResumeBgm,
            {},
            1.0f
        });
        bgmResumeQueued_ = true;
    }
    if (elapsedTime_ >= totalDuration_) {
        inputWindow_.active = false;
    }
}

void SailorVenusBossPresentation::preload(SDL_Renderer* renderer) {
    ensureFrameLoaded(renderer, 0);
}

void SailorVenusBossPresentation::render(SDL_Renderer* renderer,
                                         int screenW,
                                         int screenH,
                                         const Camera3D& camera) {
    if (renderer == nullptr) {
        return;
    }

    const SequenceSpec& spec = sequenceSpecFor(variant_);
    const bool impactShotActive = isImpactShotActive();
    const float frameDurationSeconds = sequencePlaybackDuration() / static_cast<float>(std::max(1, spec.frameCount));
    const int frameIndex = impactShotActive
        ? std::max(0, spec.frameCount - 1)
        : std::clamp(
            static_cast<int>(elapsedTime_ / std::max(0.001f, frameDurationSeconds)),
            0,
            std::max(0, spec.frameCount - 1)
        );
    maintainFrameWindow(renderer, frameIndex);
    const FrameSlot& frame = frames_[static_cast<size_t>(frameIndex)];

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_FRect destination{
        0.0f,
        0.0f,
        static_cast<float>(screenW),
        static_cast<float>(screenH)
    };

    if (frame.texture != nullptr) {
        int textureWidth = 0;
        int textureHeight = 0;
        SDL_QueryTexture(frame.texture, nullptr, nullptr, &textureWidth, &textureHeight);
        if (textureWidth > 0 && textureHeight > 0) {
            const float textureAspect = static_cast<float>(textureWidth) / static_cast<float>(textureHeight);
            float drawWidth = static_cast<float>(screenW);
            float drawHeight = drawWidth / textureAspect;
            if (drawHeight < static_cast<float>(screenH)) {
                drawHeight = static_cast<float>(screenH);
                drawWidth = drawHeight * textureAspect;
            }
            destination = SDL_FRect{
                (static_cast<float>(screenW) - drawWidth) * 0.5f,
                (static_cast<float>(screenH) - drawHeight) * 0.5f,
                drawWidth,
                drawHeight
            };
        }
    }

    if (!impactShotActive) {
        if (frame.texture == nullptr) {
            SDL_SetRenderDrawColor(renderer, 255, 241, 204, 220);
            SDL_Rect fallback{0, 0, screenW, screenH};
            SDL_RenderFillRect(renderer, &fallback);
            return;
        }

        SDL_RenderCopyF(renderer, frame.texture, nullptr, &destination);
        return;
    }

    const float impactProgress = impactShotProgress();
    const float whiteFlashAlpha =
        impactProgress >= kImpactFlashFadeOutEndRatio
        ? 0.0f
        : (impactProgress <= kImpactFlashFadeInEndRatio
            ? remap01(impactProgress, 0.0f, kImpactFlashFadeInEndRatio)
            : (1.0f - remap01(impactProgress, kImpactFlashFadeInEndRatio, kImpactFlashFadeOutEndRatio)));
    const float impactFrameAlpha =
        impactProgress < kImpactFrameFadeInStartRatio
        ? 0.0f
        :
        impactProgress >= kImpactFrameFadeOutEndRatio
        ? 0.0f
        : (impactProgress <= kImpactFrameHoldEndRatio
            ? 1.0f
            : (1.0f - remap01(impactProgress, kImpactFrameHoldEndRatio, kImpactFrameFadeOutEndRatio)));

    if (impactFrameAlpha > 0.001f) {
        const Uint8 impactAlpha = static_cast<Uint8>(
            std::lround(std::clamp(impactFrameAlpha, 0.0f, 1.0f) * 255.0f)
        );
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, impactAlpha);
        SDL_Rect impactRect{0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &impactRect);

        SDL_Texture* casterImpactTexture = overlayCasterSpriteTexture_ != nullptr
            ? overlayCasterSpriteTexture_
            : casterSpriteTexture_;
        SDL_Texture* targetImpactTexture = overlayTargetSpriteTexture_ != nullptr
            ? overlayTargetSpriteTexture_
            : targetSpriteTexture_;
        drawImpactSilhouette(renderer, camera, casterImpactTexture, casterX_, casterY_, casterZ_, false, impactFrameAlpha);
        drawImpactSilhouette(renderer, camera, targetImpactTexture, targetX_, targetY_, targetZ_, true, impactFrameAlpha);
    }

    if (whiteFlashAlpha > 0.001f) {
        SDL_SetRenderDrawColor(
            renderer,
            255,
            255,
            255,
            static_cast<Uint8>(std::lround(std::clamp(whiteFlashAlpha, 0.0f, 1.0f) * 255.0f))
        );
        SDL_Rect flashRect{0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &flashRect);
    }
}

bool SailorVenusBossPresentation::isComplete() const {
    return elapsedTime_ >= totalDuration_;
}

void SailorVenusBossPresentation::onSpacePressed() {
    const SequenceSpec& spec = sequenceSpecFor(variant_);
    if (!spec.allowsSpaceInput || !inputWindow_.active || elapsedTime_ > totalDuration_) {
        return;
    }

    ++spacesPressed_;

    PresentationFeedbackEvent event;
    event.signal = PresentationFeedbackSignal::forcedPerfect();
    event.multiplier = getInputMultiplier();

    switch (variant_) {
        case Variant::BossParry: {
            event.comboEligible = true;
            const int reductionPercent = std::max(
                0,
                static_cast<int>(std::lround((1.0f - event.multiplier) * 100.0f))
            );
            event.rewardText = "-" + std::to_string(reductionPercent) + "% DMG TAKEN";
            pendingFeedbackEvents_.push_back(std::move(event));
            break;
        }
        case Variant::LoveAndBeautyShock: {
            event.comboEligible = true;
            const int bonusPercent = std::max(
                0,
                static_cast<int>(std::lround((event.multiplier - 1.0f) * 100.0f))
            );
            event.rewardText = "+" + std::to_string(bonusPercent) + "% DMG";
            pendingFeedbackEvents_.push_back(std::move(event));
            break;
        }
        default:
            break;
    }
}

float SailorVenusBossPresentation::bossParryInputRatio() const {
    return clampRatio(
        static_cast<float>(spacesPressed_) / static_cast<float>(std::max(1, quota_))
    );
}

float SailorVenusBossPresentation::loveAndBeautyShockFinalMultiplier() const {
    return std::min(3.15f, 2.10f + (0.035f * static_cast<float>(spacesPressed_)));
}

float SailorVenusBossPresentation::loveMeChainFinalMultiplier() const {
    return std::min(8.00f, 4.40f + (0.06f * static_cast<float>(std::max(0, presentationValue_))));
}

bool SailorVenusBossPresentation::usesImpactShot() const {
    return variant_ == Variant::LoveAndBeautyShock ||
        variant_ == Variant::CrescentBeam ||
        variant_ == Variant::LoveMeChain;
}

bool SailorVenusBossPresentation::isImpactShotActive() const {
    if (!usesImpactShot()) {
        return false;
    }
    return elapsedTime_ >= sequencePlaybackDuration();
}

float SailorVenusBossPresentation::sequencePlaybackDuration() const {
    return sequenceSpecFor(variant_).durationSeconds;
}

float SailorVenusBossPresentation::hitTriggerTime() const {
    return usesImpactShot() ? sequencePlaybackDuration() : totalDuration_;
}

float SailorVenusBossPresentation::impactShotProgress() const {
    if (!usesImpactShot()) {
        return 0.0f;
    }

    return clampRatio(
        (elapsedTime_ - sequencePlaybackDuration()) / std::max(0.001f, kImpactShotDurationSeconds)
    );
}

void SailorVenusBossPresentation::setExternalTextures(SDL_Texture* caster, SDL_Texture* target) {
    casterSpriteTexture_ = caster;
    targetSpriteTexture_ = target;
}

void SailorVenusBossPresentation::setOverlayTextures(SDL_Texture* caster, SDL_Texture* target) {
    overlayCasterSpriteTexture_ = caster;
    overlayTargetSpriteTexture_ = target;
}

void SailorVenusBossPresentation::setTargetWorldPosition(float x, float y, float z) {
    targetX_ = x;
    targetY_ = y;
    targetZ_ = z;
}

void SailorVenusBossPresentation::drawImpactSilhouette(SDL_Renderer* renderer,
                                                       const Camera3D& camera,
                                                       SDL_Texture* texture,
                                                       float worldX,
                                                       float worldY,
                                                       float worldZ,
                                                       bool isBoss,
                                                       float alpha) const {
    if (renderer == nullptr) {
        return;
    }

    const float depth = camera.getDepth(worldX, worldY, worldZ);
    if (depth <= kMinimumDepth) {
        return;
    }

    const SDL_FPoint screen = camera.worldToScreen(worldX, worldY, worldZ);
    const float scale = camera.getPerspectiveScale(worldX, worldY, worldZ);
    if (scale <= 0.0f) {
        return;
    }

    const float entityScale = isBoss ? 1.28f : 1.0f;
    const int drawHeight = std::max(
        8,
        static_cast<int>(std::lround(kSuggestedBaseSpriteHeight * scale * entityScale))
    );

    SDL_Rect srcRect{0, 0, 0, 0};
    int drawWidth = std::max(
        8,
        static_cast<int>(std::lround(kSuggestedBaseSpriteWidth * scale * entityScale))
    );

    if (texture != nullptr) {
        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);
        if (texW > 0 && texH > 0) {
            const int frameCount = texW / kSuggestedBaseSpriteWidth;
            const bool isAnimated = frameCount > 1 && (texW % kSuggestedBaseSpriteWidth == 0);
            srcRect = isAnimated
                ? SDL_Rect{0, 0, kSuggestedBaseSpriteWidth, texH}
                : SDL_Rect{0, 0, texW, texH};
            drawWidth = std::max(
                8,
                static_cast<int>(std::lround(
                    static_cast<float>(drawHeight) *
                    (static_cast<float>(srcRect.w) / static_cast<float>(std::max(1, srcRect.h)))
                ))
            );
        }
    }

    SDL_Rect dstRect{
        static_cast<int>(std::lround(screen.x)) - (drawWidth / 2),
        static_cast<int>(std::lround(screen.y)) - drawHeight,
        drawWidth,
        drawHeight
    };

    const Uint8 silhouetteAlpha = static_cast<Uint8>(
        std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)
    );
    if (texture != nullptr && srcRect.w > 0 && srcRect.h > 0) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(texture, 0, 0, 0);
        SDL_SetTextureAlphaMod(texture, silhouetteAlpha);
        SDL_RenderCopy(renderer, texture, &srcRect, &dstRect);
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(texture, 255);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, silhouetteAlpha);
    SDL_RenderFillRect(renderer, &dstRect);
}

float SailorVenusBossPresentation::getInputMultiplier() const {
    switch (variant_) {
        case Variant::BossParry:
            return std::max(
                kBossMinDamageMultiplier,
                1.0f - (kBossDamageReductionRatio * bossParryInputRatio())
            );
        case Variant::LoveAndBeautyShock:
            return loveAndBeautyShockFinalMultiplier() / 2.10f;
        case Variant::LoveMeChain:
            return loveMeChainFinalMultiplier() / 4.40f;
        case Variant::Transformation:
        case Variant::CrescentBeam:
        default:
            return 1.0f;
    }
}

float SailorVenusBossPresentation::consumeHitDamageMultiplier() {
    return getInputMultiplier();
}

bool SailorVenusBossPresentation::overridesCamera() const {
    return isImpactShotActive();
}

void SailorVenusBossPresentation::applyCameraState(Camera3D& camera) const {
    if (!isImpactShotActive()) {
        return;
    }

    camera = render::makeDefaultBattleActionIntroCamera();
    const Camera3D goalCamera = render::makeDefaultBattleCamera();
    const float rawProgress = impactShotProgress();
    const float progress = rawProgress >= kImpactCameraWhipEndRatio
        ? 1.0f
        : easing::easeOutQuint(remap01(rawProgress, 0.0f, kImpactCameraWhipEndRatio));

    camera.posY += kImpactCameraTightenY;
    camera.posZ += kImpactCameraTightenZ;
    camera.focalLength = kImpactStartFocal;

    camera.posX = easing::lerp(camera.posX, goalCamera.posX, progress);
    camera.posY = easing::lerp(camera.posY, goalCamera.posY + kImpactCameraLift, progress);
    camera.posZ = easing::lerp(camera.posZ, goalCamera.posZ + kImpactCameraPullback, progress);
    camera.pitchDegrees = easing::lerp(camera.pitchDegrees + kImpactPitchOffset, goalCamera.pitchDegrees + kImpactPitchOffset, progress);
    camera.yawDegrees = easing::lerp(camera.yawDegrees, goalCamera.yawDegrees, progress);
    camera.focalLength = easing::lerp(kImpactStartFocal, kImpactZoomOutFocal, progress);
}

PresentationFeedbackSignal SailorVenusBossPresentation::getFeedbackSignal() const {
    switch (variant_) {
        case Variant::BossParry:
            return PresentationFeedbackSignal::graded(bossParryInputRatio());
        case Variant::LoveAndBeautyShock: {
            const float ratio = clampRatio((loveAndBeautyShockFinalMultiplier() - 2.10f) / (3.15f - 2.10f));
            return PresentationFeedbackSignal::graded(ratio);
        }
        case Variant::LoveMeChain: {
            const float ratio = clampRatio((loveMeChainFinalMultiplier() - 4.40f) / (8.00f - 4.40f));
            return PresentationFeedbackSignal::graded(ratio);
        }
        default:
            return {};
    }
}

std::vector<PresentationFeedbackEvent> SailorVenusBossPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

int SailorVenusBossPresentation::consumeHitEvents() {
    if (hasHit_ || elapsedTime_ < hitTriggerTime() || !sequenceSpecFor(variant_).emitsHit) {
        return 0;
    }

    hasHit_ = true;
    return 1;
}

std::string SailorVenusBossPresentation::getInputResultText() const {
    char buffer[192];
    switch (variant_) {
        case Variant::BossParry: {
            const int reductionPercent = static_cast<int>(std::lround((1.0f - getInputMultiplier()) * 100.0f));
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Pressed %d/%d SPACE. Damage reduced %d%%.",
                spacesPressed_,
                std::max(1, quota_),
                std::max(0, reductionPercent)
            );
            return buffer;
        }
        case Variant::LoveAndBeautyShock:
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Pressed SPACE %d times. Love and Beauty Shock reached %.2fx.",
                spacesPressed_,
                loveAndBeautyShockFinalMultiplier()
            );
            return buffer;
        case Variant::LoveMeChain:
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Stored tally %d. Venus Love Me Chain reached %.2fx.",
                std::max(0, presentationValue_),
                loveMeChainFinalMultiplier()
            );
            return buffer;
        default:
            return {};
    }
}

int SailorVenusBossPresentation::getScoreValue() const {
    return variant_ == Variant::LoveAndBeautyShock ? std::max(0, spacesPressed_) : 0;
}

void SailorVenusBossPresentation::setPresentationValue(int value) {
    presentationValue_ = std::max(0, value);
}

void SailorVenusBossPresentation::setTuningProfile(const PresentationTuningProfile& profile) {
    if (variant_ != Variant::BossParry) {
        return;
    }
    if (const auto it = profile.intParams.find("spamQuota"); it != profile.intParams.end()) {
        quota_ = std::max(1, it->second);
    }
}

std::vector<PresentationAudioCommand> SailorVenusBossPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(audioCmds_);
    return commands;
}

void SailorVenusBossPresentation::releaseFrames() {
    for (FrameSlot& slot : frames_) {
        if (slot.texture != nullptr) {
            SDL_DestroyTexture(slot.texture);
            slot.texture = nullptr;
        }
        slot.attemptedLoad = false;
    }
}

void SailorVenusBossPresentation::maintainFrameWindow(SDL_Renderer* renderer, int frameIndex) {
    const SequenceSpec& spec = sequenceSpecFor(variant_);
    if (frames_.empty()) {
        frames_.resize(static_cast<size_t>(spec.frameCount));
    }

    const int minFrame = std::max(0, frameIndex - kFrameWindowRadius);
    const int maxFrame = std::min(spec.frameCount - 1, frameIndex + kFrameWindowRadius);

    for (int index = minFrame; index <= maxFrame; ++index) {
        ensureFrameLoaded(renderer, index);
    }

    for (int index = 0; index < spec.frameCount; ++index) {
        if (index >= minFrame && index <= maxFrame) {
            continue;
        }

        FrameSlot& slot = frames_[static_cast<size_t>(index)];
        if (slot.texture != nullptr) {
            SDL_DestroyTexture(slot.texture);
            slot.texture = nullptr;
        }
        slot.attemptedLoad = false;
    }
}

void SailorVenusBossPresentation::ensureFrameLoaded(SDL_Renderer* renderer, int frameIndex) {
    const SequenceSpec& spec = sequenceSpecFor(variant_);
    if (renderer == nullptr || frameIndex < 0 || frameIndex >= spec.frameCount) {
        return;
    }

    if (frames_.empty()) {
        frames_.resize(static_cast<size_t>(spec.frameCount));
    }

    FrameSlot& slot = frames_[static_cast<size_t>(frameIndex)];
    if (slot.texture != nullptr || slot.attemptedLoad) {
        return;
    }

    slot.attemptedLoad = true;

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolveFramePath(variant_, frameIndex + 1).c_str());
    if (surface == nullptr) {
        return;
    }

    slot.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
#else
    (void)renderer;
#endif
}

} // namespace battle
