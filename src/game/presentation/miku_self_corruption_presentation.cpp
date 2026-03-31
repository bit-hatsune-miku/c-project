#include "miku_self_corruption_presentation.h"

#include "../core/easing.h"
#include "../../platform/path_resolution.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kCheerLineDuration = 5.8f;
constexpr float kWhiteoutDuration = 0.45f;
constexpr float kBirdViewDuration = 3.6f;
constexpr float kIntroFramesDuration = 2.4f;
constexpr float kGlitchFramesDuration = 1.0f;
constexpr float kCorruptionLoopDuration = 4.0f;
constexpr float kReturnToWorldDuration = 1.6f;
constexpr float kFinalHitsDuration = 3.8f;
constexpr int kFinalHitCount = 10;
constexpr float kPi = 3.14159265f;

constexpr float kWhiteoutStart = kCheerLineDuration;
constexpr float kBirdViewStart = kWhiteoutStart + kWhiteoutDuration;
constexpr float kIntroFramesStart = kBirdViewStart + kBirdViewDuration;
constexpr float kGlitchFramesStart = kIntroFramesStart + kIntroFramesDuration;
constexpr float kCorruptionLoopStart = kGlitchFramesStart + kGlitchFramesDuration;
constexpr float kReturnToWorldStart = kCorruptionLoopStart + kCorruptionLoopDuration;
constexpr float kFinalHitsStart = kReturnToWorldStart + kReturnToWorldDuration;
constexpr float kCompleteTime = kFinalHitsStart + kFinalHitsDuration;

std::string resolvePresentationFramePath(const char* name) {
    return platform::path::resolvePath(std::string("assets/combat/presentations/mikuBoss/") + name);
}

float clamp01Local(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float easeInCubicLocal(float value) {
    const float t = clamp01Local(value);
    return t * t * t;
}

} // namespace

MikuSelfCorruptionPresentation::MikuSelfCorruptionPresentation(
    float casterWorldX,
    float casterWorldY,
    float casterWorldZ,
    float targetWorldX,
    float targetWorldY,
    float targetWorldZ)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = kCompleteTime;
}

MikuSelfCorruptionPresentation::~MikuSelfCorruptionPresentation() {
    releaseFallbackAssets();
}

void MikuSelfCorruptionPresentation::start() {
    elapsedTime_ = 0.0f;
    pendingAudioCommands_.clear();
    pendingAbilityAudioCues_ = 1;
    pendingHitEvents_ = 0;
    emittedHitBursts_ = 0;
    nextCheerVoiceIndex_ = 0;
}

void MikuSelfCorruptionPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    const NativePhase phase = currentPhase();
    if (phase == NativePhase::CheerLine && !partyAssetNames_.empty()) {
        const float localTime = phaseElapsed();
        const float audioWindowStart = 0.35f;
        const float audioWindowDuration = std::max(0.2f, kCheerLineDuration - 0.8f);
        while (nextCheerVoiceIndex_ < static_cast<int>(partyAssetNames_.size())) {
            const float voiceTime =
                audioWindowStart +
                (audioWindowDuration * (static_cast<float>(nextCheerVoiceIndex_) + 0.5f) /
                 static_cast<float>(partyAssetNames_.size()));
            if (localTime < voiceTime) {
                break;
            }
            queueAudioCommand(
                PresentationAudioCommandType::PlayOneShotAllowOverlap,
                "assets/combat/voices/" + partyAssetNames_[static_cast<std::size_t>(nextCheerVoiceIndex_)] + "/special.wav",
                0.92f);
            ++nextCheerVoiceIndex_;
        }
    }

    if (phase == NativePhase::FinalHits) {
        const float localTime = phaseElapsed();
        const float hitInterval = kFinalHitsDuration / static_cast<float>(kFinalHitCount);
        const int desiredHits = std::clamp(
            static_cast<int>(std::floor(localTime / std::max(0.001f, hitInterval))) + 1,
            0,
            kFinalHitCount);
        if (desiredHits > emittedHitBursts_) {
            pendingHitEvents_ += desiredHits - emittedHitBursts_;
            emittedHitBursts_ = desiredHits;
        }
    }
}

void MikuSelfCorruptionPresentation::render(SDL_Renderer* renderer,
                                            int screenW,
                                            int screenH,
                                            const Camera3D& camera) {
    (void)camera;
    ensureFallbackAssets(renderer);
    if (renderer == nullptr) {
        return;
    }

    const NativePhase phase = currentPhase();
    if (phase == NativePhase::IntroFrames || phase == NativePhase::GlitchFrames ||
        phase == NativePhase::CorruptionLoop || phase == NativePhase::ReturnToWorld) {
        SDL_Texture* texture = nullptr;
        if (phase == NativePhase::IntroFrames) {
            texture = loadedFrameAtIndex(phaseProgress() < 0.5f ? 0 : 1);
        } else if (phase == NativePhase::GlitchFrames) {
            texture = loadedFrameAtIndex(2);
        } else if (phase == NativePhase::CorruptionLoop) {
            texture = loadedFrameAtIndex(3);
        }
        if (texture != nullptr) {
            renderFallbackFrame(renderer, texture, screenW, screenH);
        }
        return;
    }

    if (phase == NativePhase::Whiteout) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect rect{0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &rect);
        return;
    }

    if (phase == NativePhase::FinalHits) {
        const float localTime = phaseElapsed();
        const float hitInterval = kFinalHitsDuration / static_cast<float>(kFinalHitCount);
        const int lastHitIndex = std::clamp(
            static_cast<int>(std::floor(localTime / std::max(0.001f, hitInterval))),
            0,
            std::max(0, emittedHitBursts_ - 1));
        const float lastHitTime = static_cast<float>(lastHitIndex) * hitInterval;
        const float flashProgress = clamp01Local((localTime - lastHitTime) / 0.18f);
        const Uint8 alpha = static_cast<Uint8>(std::lround((1.0f - flashProgress) * 150.0f));
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
        SDL_Rect rect{0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &rect);
    }
}

void MikuSelfCorruptionPresentation::preload(SDL_Renderer* renderer) {
    ensureFallbackAssets(renderer);
}

bool MikuSelfCorruptionPresentation::isComplete() const {
    return elapsedTime_ >= kCompleteTime;
}

bool MikuSelfCorruptionPresentation::overridesCamera() const {
    return true;
}

void MikuSelfCorruptionPresentation::applyCameraState(Camera3D& camera) const {
    const NativePhase phase = currentPhase();
    if (phase == NativePhase::CheerLine || phase == NativePhase::Whiteout) {
        const float t = phase == NativePhase::Whiteout ? 1.0f : easeInCubicLocal(phaseProgress());
        const float castExtent = static_cast<float>(std::max<std::size_t>(1, partyAssetNames_.size())) - 1.0f;
        const float halfWidth = castExtent * 146.0f;
        const float focusX = easing::lerp(-halfWidth, halfWidth, t);
        camera.posX = focusX + 8.0f;
        camera.posY = 855.0f;
        camera.posZ = -128.0f;
        camera.focalLength = 47000.0f;
        applyLookAt(camera, focusX, 510.0f, -22.0f);
        return;
    }

    if (phase == NativePhase::BirdView) {
        camera.posX = 0.0f;
        camera.posY = 265.0f;
        camera.posZ = -360.0f;
        camera.focalLength = 41000.0f;
        applyLookAt(camera, 0.0f, 675.0f, -58.0f);
        return;
    }

    if (phase == NativePhase::IntroFrames || phase == NativePhase::GlitchFrames || phase == NativePhase::CorruptionLoop) {
        camera.posX = 0.0f;
        camera.posY = 500.0f;
        camera.posZ = -230.0f;
        camera.focalLength = 30000.0f;
        applyLookAt(camera, 0.0f, 720.0f, -40.0f);
        return;
    }

    if (phase == NativePhase::ReturnToWorld) {
        const float t = phaseProgress();
        const float violentT = easing::easeOutCubic(t);
        camera.posX = easing::lerp(0.0f, -110.0f, violentT);
        camera.posY = easing::lerp(1060.0f, 120.0f, violentT);
        camera.posZ = easing::lerp(-112.0f, -175.0f, violentT);
        camera.focalLength = easing::lerp(56000.0f, 28500.0f, violentT);
        applyLookAt(camera, 0.0f, 720.0f, -55.0f);
        applyShake(camera, phaseElapsed() * 1.2f, 18.0f * (1.0f - t));
        return;
    }

    camera.posX = 0.0f;
    camera.posY = 90.0f;
    camera.posZ = -170.0f;
    camera.focalLength = 28500.0f;
    applyLookAt(camera, 0.0f, 710.0f, -60.0f);
    if (phase == NativePhase::FinalHits) {
        const float localTime = phaseElapsed();
        const float progress = phaseProgress();
        const float baseMagnitude = 28.0f + static_cast<float>(emittedHitBursts_) * 9.0f;
        applyShake(camera, localTime, baseMagnitude * (0.55f + progress * 0.70f));
        applyShake(camera, localTime * 1.68f, baseMagnitude * 0.52f);
    }
}

void MikuSelfCorruptionPresentation::setPartyAssetNames(const std::vector<std::string>& assetNames) {
    partyAssetNames_.clear();
    for (const std::string& assetName : assetNames) {
        if (assetName.empty()) {
            continue;
        }
        partyAssetNames_.push_back(assetName);
    }
    if (std::find(partyAssetNames_.begin(), partyAssetNames_.end(), "lyoo") == partyAssetNames_.end()) {
        partyAssetNames_.push_back("lyoo");
    }
}

bool MikuSelfCorruptionPresentation::shouldHideNonCasterCharacters() const {
    const NativePhase phase = currentPhase();
    return phase != NativePhase::ReturnToWorld &&
           phase != NativePhase::FinalHits &&
           phase != NativePhase::Complete;
}

bool MikuSelfCorruptionPresentation::shouldRenderCasterEntity() const {
    const NativePhase phase = currentPhase();
    return phase == NativePhase::ReturnToWorld || phase == NativePhase::FinalHits;
}

bool MikuSelfCorruptionPresentation::shouldRenderAboveHud() const {
    return true;
}

bool MikuSelfCorruptionPresentation::shouldRenderFloor() const {
    return true;
}

int MikuSelfCorruptionPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int MikuSelfCorruptionPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

int MikuSelfCorruptionPresentation::getDamageLabelHitCount() const {
    return kFinalHitCount;
}

std::vector<PresentationAudioCommand> MikuSelfCorruptionPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

MikuSelfCorruptionPresentation::NativeState MikuSelfCorruptionPresentation::buildNativeState() const {
    return NativeState{
        currentPhase(),
        elapsedTime_,
        phaseProgress(),
        emittedHitBursts_
    };
}

void MikuSelfCorruptionPresentation::ensureFallbackAssets(SDL_Renderer* renderer) {
    if (renderer == nullptr) {
        return;
    }
    if (fallbackRenderer_ != nullptr && fallbackRenderer_ != renderer) {
        releaseFallbackAssets();
    }
    if (attemptedFallbackLoad_) {
        return;
    }
    attemptedFallbackLoad_ = true;
    fallbackRenderer_ = renderer;
    fallbackFrames_.assign(4, nullptr);

#ifdef BATTLE_ENABLE_IMAGE
    const std::array<const char*, 4> names{{"01.png", "02.png", "03.png", "04.png"}};
    for (std::size_t i = 0; i < names.size(); ++i) {
        const std::string path = resolvePresentationFramePath(names[i]);
        if (!std::filesystem::exists(path)) {
            continue;
        }
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface == nullptr) {
            continue;
        }
        fallbackFrames_[i] = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
#else
    (void)renderer;
#endif
}

void MikuSelfCorruptionPresentation::releaseFallbackAssets() {
    for (SDL_Texture*& texture : fallbackFrames_) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }
    fallbackFrames_.clear();
    fallbackRenderer_ = nullptr;
    attemptedFallbackLoad_ = false;
}

void MikuSelfCorruptionPresentation::queueAudioCommand(PresentationAudioCommandType type,
                                                       const std::string& id,
                                                       float volume) {
    pendingAudioCommands_.push_back(PresentationAudioCommand{type, id, volume});
}

MikuSelfCorruptionPresentation::NativePhase MikuSelfCorruptionPresentation::currentPhase() const {
    if (elapsedTime_ < kWhiteoutStart) {
        return NativePhase::CheerLine;
    }
    if (elapsedTime_ < kBirdViewStart) {
        return NativePhase::Whiteout;
    }
    if (elapsedTime_ < kIntroFramesStart) {
        return NativePhase::BirdView;
    }
    if (elapsedTime_ < kGlitchFramesStart) {
        return NativePhase::IntroFrames;
    }
    if (elapsedTime_ < kCorruptionLoopStart) {
        return NativePhase::GlitchFrames;
    }
    if (elapsedTime_ < kReturnToWorldStart) {
        return NativePhase::CorruptionLoop;
    }
    if (elapsedTime_ < kFinalHitsStart) {
        return NativePhase::ReturnToWorld;
    }
    if (elapsedTime_ < kCompleteTime) {
        return NativePhase::FinalHits;
    }
    return NativePhase::Complete;
}

float MikuSelfCorruptionPresentation::phaseElapsed() const {
    switch (currentPhase()) {
        case NativePhase::CheerLine:
            return elapsedTime_;
        case NativePhase::Whiteout:
            return elapsedTime_ - kWhiteoutStart;
        case NativePhase::BirdView:
            return elapsedTime_ - kBirdViewStart;
        case NativePhase::IntroFrames:
            return elapsedTime_ - kIntroFramesStart;
        case NativePhase::GlitchFrames:
            return elapsedTime_ - kGlitchFramesStart;
        case NativePhase::CorruptionLoop:
            return elapsedTime_ - kCorruptionLoopStart;
        case NativePhase::ReturnToWorld:
            return elapsedTime_ - kReturnToWorldStart;
        case NativePhase::FinalHits:
            return elapsedTime_ - kFinalHitsStart;
        case NativePhase::Complete:
        default:
            return 0.0f;
    }
}

float MikuSelfCorruptionPresentation::phaseProgress() const {
    switch (currentPhase()) {
        case NativePhase::CheerLine:
            return clamp01Local(phaseElapsed() / kCheerLineDuration);
        case NativePhase::Whiteout:
            return clamp01Local(phaseElapsed() / kWhiteoutDuration);
        case NativePhase::BirdView:
            return clamp01Local(phaseElapsed() / kBirdViewDuration);
        case NativePhase::IntroFrames:
            return clamp01Local(phaseElapsed() / kIntroFramesDuration);
        case NativePhase::GlitchFrames:
            return clamp01Local(phaseElapsed() / kGlitchFramesDuration);
        case NativePhase::CorruptionLoop:
            return clamp01Local(phaseElapsed() / kCorruptionLoopDuration);
        case NativePhase::ReturnToWorld:
            return clamp01Local(phaseElapsed() / kReturnToWorldDuration);
        case NativePhase::FinalHits:
            return clamp01Local(phaseElapsed() / kFinalHitsDuration);
        case NativePhase::Complete:
        default:
            return 1.0f;
    }
}

void MikuSelfCorruptionPresentation::applyLookAt(Camera3D& camera,
                                                 float lookX,
                                                 float lookY,
                                                 float lookZ) const {
    const float dx = lookX - camera.posX;
    const float dy = lookY - camera.posY;
    const float dz = lookZ - camera.posZ;
    const float horizontalDist = std::sqrt((dx * dx) + (dy * dy));
    camera.yawDegrees = std::atan2(-dx, dy) * 180.0f / kPi;
    camera.pitchDegrees = std::atan2(dz, std::max(1.0f, horizontalDist)) * 180.0f / kPi;
}

void MikuSelfCorruptionPresentation::applyShake(Camera3D& camera,
                                                float timeSeconds,
                                                float magnitude) const {
    camera.posX += std::sin(timeSeconds * 26.0f) * magnitude;
    camera.posY += std::cos(timeSeconds * 22.0f) * magnitude * 0.72f;
    camera.posZ += std::sin(timeSeconds * 31.0f) * magnitude * 0.55f;
    camera.yawDegrees += std::sin(timeSeconds * 18.0f) * magnitude * 0.06f;
    camera.pitchDegrees += std::cos(timeSeconds * 16.0f) * magnitude * 0.05f;
}

SDL_Texture* MikuSelfCorruptionPresentation::loadedFrameAtIndex(std::size_t index) const {
    if (index < fallbackFrames_.size()) {
        return fallbackFrames_[index];
    }
    return nullptr;
}

void MikuSelfCorruptionPresentation::renderFallbackFrame(SDL_Renderer* renderer,
                                                         SDL_Texture* texture,
                                                         int screenW,
                                                         int screenH) const {
    if (renderer == nullptr) {
        return;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect dst{0, 0, screenW, screenH};
    if (texture == nullptr) {
        SDL_RenderFillRect(renderer, &dst);
        return;
    }
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
}

} // namespace battle
