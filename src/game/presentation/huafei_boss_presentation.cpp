#include "huafei_boss_presentation.h"

#include "../../platform/path_resolution.h"
#include "../render/battle_asset_loading.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace battle {

namespace {

constexpr int kFrameWindowRadius = 6;
constexpr int kOpeningFrameCount = 74;
constexpr int kFailureFrameCount = 25;
constexpr float kOpeningDurationSeconds = 7.552f;
constexpr float kFailureDurationSeconds = 2.624f;
constexpr float kFailureHitTimeSeconds = 0.92f;
constexpr float kOkaySignalScore = 0.30f;
constexpr float kGoodSignalScore = 0.70f;
constexpr float kMaxDamageMultiplier = 3.2f;
constexpr float kMinDamageMultiplier = 0.45f;
constexpr float kFakeIconSize = 220.0f;
constexpr float kFakeIconFramePadding = 18.0f;
constexpr float kFakeIconCenterXRatio = 0.28f;
constexpr float kFakeIconCenterYRatio = 0.50f;

std::string resolveVoicePath(const char* relativePath) {
    return platform::path::resolveAudioPath(relativePath).value_or(platform::path::resolvePath(relativePath));
}

std::string resolveFramePath(HuafeiBossPresentation::Sequence sequence, int frameNumber) {
    char relativePath[256];
    std::snprintf(
        relativePath,
        sizeof(relativePath),
        sequence == HuafeiBossPresentation::Sequence::Opening
            ? "assets/combat/presentations/huafeiBoss/1/output_%04d.png"
            : "assets/combat/presentations/huafeiBoss/2/output_%04d.png",
        frameNumber
    );
    return relativePath;
}

int frameCountForSequence(HuafeiBossPresentation::Sequence sequence) {
    return sequence == HuafeiBossPresentation::Sequence::Opening
        ? kOpeningFrameCount
        : kFailureFrameCount;
}

float durationForSequence(HuafeiBossPresentation::Sequence sequence) {
    return sequence == HuafeiBossPresentation::Sequence::Opening
        ? kOpeningDurationSeconds
        : kFailureDurationSeconds;
}

std::vector<HuafeiBossPresentation::FrameSlot>& slotsForSequence(
    HuafeiBossPresentation::Sequence sequence,
    std::vector<HuafeiBossPresentation::FrameSlot>& openingFrames,
    std::vector<HuafeiBossPresentation::FrameSlot>& failureFrames) {
    return sequence == HuafeiBossPresentation::Sequence::Opening ? openingFrames : failureFrames;
}

const std::vector<HuafeiBossPresentation::FrameSlot>& slotsForSequence(
    HuafeiBossPresentation::Sequence sequence,
    const std::vector<HuafeiBossPresentation::FrameSlot>& openingFrames,
    const std::vector<HuafeiBossPresentation::FrameSlot>& failureFrames) {
    return sequence == HuafeiBossPresentation::Sequence::Opening ? openingFrames : failureFrames;
}

} // namespace

HuafeiBossPresentation::HuafeiBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) : casterX_(casterWorldX)
  , casterY_(casterWorldY)
  , casterZ_(casterWorldZ)
  , targetX_(targetWorldX)
  , targetY_(targetWorldY)
  , targetZ_(targetWorldZ)
  , openingFrames_(static_cast<std::size_t>(kOpeningFrameCount))
  , failureFrames_(static_cast<std::size_t>(kFailureFrameCount)) {
    totalDuration_ = kOpeningDurationSeconds + kFailureDurationSeconds;
}

HuafeiBossPresentation::~HuafeiBossPresentation() {
    releaseTextures();
}

void HuafeiBossPresentation::start() {
    elapsedTime_ = 0.0f;
    openingElapsed_ = 0.0f;
    failureElapsed_ = 0.0f;
    phase_ = Phase::Opening;
    validPressCount_ = 0;
    failureHitReady_ = false;
    hitDispatched_ = false;
    recentKeys_.clear();
    pendingFeedbackEvents_.clear();
    pendingAudioCommands_.clear();
    inputWindow_.startTime = 0.0f;
    inputWindow_.endTime = kOpeningDurationSeconds;
    inputWindow_.active = true;
    pendingAudioCommands_.push_back(PresentationAudioCommand{
        PresentationAudioCommandType::PlayVoiceOneShot,
        resolveVoicePath("assets/combat/voices/huafeiBoss/hostage.opus"),
        1.0f
    });
}

void HuafeiBossPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (phase_ == Phase::Opening) {
        openingElapsed_ = std::min(kOpeningDurationSeconds, openingElapsed_ + deltaTime);
        if (openingElapsed_ >= kOpeningDurationSeconds) {
            inputWindow_.active = false;
            if (validPressCount_ >= quota_) {
                phase_ = Phase::SuccessComplete;
            } else {
                phase_ = Phase::Failure;
                failureElapsed_ = 0.0f;
                failureHitReady_ = false;
                pendingAudioCommands_.push_back(PresentationAudioCommand{
                    PresentationAudioCommandType::PlayVoiceOneShot,
                    resolveVoicePath("assets/combat/voices/huafeiBoss/scream.opus"),
                    1.0f
                });
            }
        }
        return;
    }

    if (phase_ == Phase::Failure) {
        failureElapsed_ = std::min(kFailureDurationSeconds, failureElapsed_ + deltaTime);
        if (!failureHitReady_ && failureElapsed_ >= kFailureHitTimeSeconds) {
            failureHitReady_ = true;
        }
        if (failureElapsed_ >= kFailureDurationSeconds) {
            phase_ = Phase::FailureComplete;
        }
    }
}

void HuafeiBossPresentation::preload(SDL_Renderer* renderer) {
    ensureFrameLoaded(renderer, Sequence::Opening, 0);
    ensureFrameLoaded(renderer, Sequence::Failure, 0);
    ensureTargetIconLoaded(renderer);
}

void HuafeiBossPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)camera;
    if (renderer == nullptr) {
        return;
    }

    const Sequence activeSequence =
        phase_ == Phase::Failure || phase_ == Phase::FailureComplete
            ? Sequence::Failure
            : Sequence::Opening;
    const float sequenceElapsed =
        activeSequence == Sequence::Opening ? openingElapsed_ : failureElapsed_;
    const float frameDuration = durationForSequence(activeSequence) /
        static_cast<float>(std::max(1, frameCountForSequence(activeSequence)));
    const int frameIndex = std::clamp(
        static_cast<int>(sequenceElapsed / std::max(0.001f, frameDuration)),
        0,
        frameCountForSequence(activeSequence) - 1
    );

    maintainFrameWindow(renderer, activeSequence, frameIndex);
    ensureTargetIconLoaded(renderer);

    SDL_Texture* frameTexture = currentFrameTexture();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (frameTexture != nullptr) {
        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(frameTexture, nullptr, nullptr, &texW, &texH);
        SDL_FRect destination{
            0.0f,
            0.0f,
            static_cast<float>(screenW),
            static_cast<float>(screenH)
        };
        if (texW > 0 && texH > 0) {
            const float textureAspect = static_cast<float>(texW) / static_cast<float>(texH);
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
        SDL_RenderCopyF(renderer, frameTexture, nullptr, &destination);
    } else {
        SDL_SetRenderDrawColor(renderer, 16, 16, 20, 255);
        SDL_Rect fallbackRect{0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &fallbackRect);
    }

    const float iconFrameSize = kFakeIconSize + (kFakeIconFramePadding * 2.0f);
    const SDL_FRect frameRect{
        static_cast<float>(screenW) * kFakeIconCenterXRatio - iconFrameSize * 0.5f,
        static_cast<float>(screenH) * kFakeIconCenterYRatio - iconFrameSize * 0.5f,
        iconFrameSize,
        iconFrameSize
    };
    SDL_SetRenderDrawColor(renderer, 10, 10, 12, 210);
    SDL_RenderFillRectF(renderer, &frameRect);
    SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255);
    SDL_RenderDrawRectF(renderer, &frameRect);

    const SDL_FRect iconRect{
        frameRect.x + kFakeIconFramePadding,
        frameRect.y + kFakeIconFramePadding,
        kFakeIconSize,
        kFakeIconSize
    };
    if (targetIconTexture_ != nullptr) {
        SDL_RenderCopyF(renderer, targetIconTexture_, nullptr, &iconRect);
    } else {
        const SDL_Color fallback = render::colorFromKey(targetAssetName_, false);
        SDL_SetRenderDrawColor(renderer, fallback.r, fallback.g, fallback.b, 255);
        SDL_RenderFillRectF(renderer, &iconRect);
        SDL_SetRenderDrawColor(renderer, 12, 12, 12, 255);
        SDL_RenderDrawRectF(renderer, &iconRect);
    }
}

bool HuafeiBossPresentation::isComplete() const {
    return phase_ == Phase::SuccessComplete ||
           (phase_ == Phase::FailureComplete && hitDispatched_);
}

bool HuafeiBossPresentation::onKeyPressed(SDL_Keycode key) {
    if (!inputWindow_.active || phase_ != Phase::Opening) {
        (void)key;
        return true;
    }

    const bool isRecent = std::find(recentKeys_.begin(), recentKeys_.end(), key) != recentKeys_.end();
    recentKeys_.push_back(key);
    while (recentKeys_.size() > 5) {
        recentKeys_.pop_front();
    }

    if (isRecent) {
        return true;
    }

    ++validPressCount_;
    PresentationFeedbackEvent event;
    const float ratio = escapeRatio();
    if (ratio >= 1.0f) {
        event.signal = PresentationFeedbackSignal::forcedPerfect();
    } else if (ratio >= 0.5f) {
        event.signal = PresentationFeedbackSignal::graded(kGoodSignalScore);
    } else {
        event.signal = PresentationFeedbackSignal::graded(kOkaySignalScore);
    }
    event.comboEligible = true;
    pendingFeedbackEvents_.push_back(event);
    return true;
}

void HuafeiBossPresentation::setTuningProfile(const PresentationTuningProfile& profile) {
    if (const auto it = profile.intParams.find("spamQuota"); it != profile.intParams.end()) {
        quota_ = std::max(1, it->second);
    }
}

void HuafeiBossPresentation::setTargetPartyIndex(int index) {
    focusedPartyIndex_ = index;
}

int HuafeiBossPresentation::getFocusedPartyIndex() const {
    return focusedPartyIndex_;
}

void HuafeiBossPresentation::setPartyAssetNames(const std::vector<std::string>& assetNames) {
    if (focusedPartyIndex_ >= 0 && focusedPartyIndex_ < static_cast<int>(assetNames.size())) {
        targetAssetName_ = assetNames[static_cast<std::size_t>(focusedPartyIndex_)];
    } else {
        targetAssetName_.clear();
    }
}

float HuafeiBossPresentation::getInputMultiplier() const {
    return escapeRatio();
}

float HuafeiBossPresentation::consumeHitDamageMultiplier() {
    return failureDamageMultiplier();
}

PresentationFeedbackSignal HuafeiBossPresentation::getFeedbackSignal() const {
    return {};
}

std::vector<PresentationFeedbackEvent> HuafeiBossPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

int HuafeiBossPresentation::consumeHitEvents() {
    if (!failureHitReady_ || hitDispatched_) {
        return 0;
    }

    hitDispatched_ = true;
    return 1;
}

int HuafeiBossPresentation::getDamageLabelHitCount() const {
    return 1;
}

std::string HuafeiBossPresentation::getInputResultText() const {
    char buffer[192];
    if (validPressCount_ >= quota_) {
        std::snprintf(
            buffer,
            sizeof(buffer),
            "Escaped RongMoMo's grab with %d valid keys.",
            validPressCount_
        );
        return buffer;
    }

    std::snprintf(
        buffer,
        sizeof(buffer),
        "Valid keys: %d/%d. Damage multiplier %.2fx.",
        validPressCount_,
        std::max(1, quota_),
        failureDamageMultiplier()
    );
    return buffer;
}

std::vector<PresentationAudioCommand> HuafeiBossPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

bool HuafeiBossPresentation::shouldHideNonCasterCharacters() const {
    return true;
}

bool HuafeiBossPresentation::shouldRenderCasterEntity() const {
    return false;
}

bool HuafeiBossPresentation::shouldRenderBossEntity() const {
    return false;
}

bool HuafeiBossPresentation::shouldRenderFocusedTargetEntity() const {
    return false;
}

bool HuafeiBossPresentation::shouldRenderAboveHud() const {
    return false;
}

bool HuafeiBossPresentation::shouldRenderFloor() const {
    return false;
}

void HuafeiBossPresentation::releaseTextures() {
    if (targetIconTexture_ != nullptr) {
        SDL_DestroyTexture(targetIconTexture_);
        targetIconTexture_ = nullptr;
    }

    auto releaseSlots = [](std::vector<FrameSlot>& slots) {
        for (FrameSlot& slot : slots) {
            if (slot.texture != nullptr) {
                SDL_DestroyTexture(slot.texture);
                slot.texture = nullptr;
            }
            slot.attemptedLoad = false;
        }
    };

    releaseSlots(openingFrames_);
    releaseSlots(failureFrames_);
}

void HuafeiBossPresentation::ensureTargetIconLoaded(SDL_Renderer* renderer) {
    if (renderer == nullptr || targetIconTexture_ != nullptr || targetAssetName_.empty()) {
        return;
    }

    if (const auto loaded = render::tryLoadCombatIconTexture(renderer, targetAssetName_); loaded.has_value()) {
        targetIconTexture_ = *loaded;
    }
}

void HuafeiBossPresentation::ensureFrameLoaded(SDL_Renderer* renderer, Sequence sequence, int frameIndex) {
    if (renderer == nullptr) {
        return;
    }

    std::vector<FrameSlot>& slots = slotsForSequence(sequence, openingFrames_, failureFrames_);
    if (frameIndex < 0 || frameIndex >= static_cast<int>(slots.size())) {
        return;
    }

    FrameSlot& slot = slots[static_cast<std::size_t>(frameIndex)];
    if (slot.attemptedLoad) {
        return;
    }
    slot.attemptedLoad = true;

    const auto texture = render::tryLoadTextureFromPath(
        renderer,
        resolveFramePath(sequence, frameIndex + 1)
    );
    if (texture.has_value()) {
        slot.texture = *texture;
    }
}

void HuafeiBossPresentation::maintainFrameWindow(SDL_Renderer* renderer, Sequence sequence, int frameIndex) {
    std::vector<FrameSlot>& slots = slotsForSequence(sequence, openingFrames_, failureFrames_);
    for (int index = std::max(0, frameIndex - kFrameWindowRadius);
         index <= std::min(frameCountForSequence(sequence) - 1, frameIndex + kFrameWindowRadius);
         ++index) {
        ensureFrameLoaded(renderer, sequence, index);
    }

    for (int index = 0; index < static_cast<int>(slots.size()); ++index) {
        if (std::abs(index - frameIndex) <= kFrameWindowRadius) {
            continue;
        }
        FrameSlot& slot = slots[static_cast<std::size_t>(index)];
        if (slot.texture != nullptr) {
            SDL_DestroyTexture(slot.texture);
            slot.texture = nullptr;
        }
    }
}

float HuafeiBossPresentation::escapeRatio() const {
    return std::clamp(
        static_cast<float>(validPressCount_) / static_cast<float>(std::max(1, quota_)),
        0.0f,
        1.0f
    );
}

float HuafeiBossPresentation::failureDamageMultiplier() const {
    const float ratio = escapeRatio();
    return kMaxDamageMultiplier + ((kMinDamageMultiplier - kMaxDamageMultiplier) * ratio);
}

SDL_Texture* HuafeiBossPresentation::currentFrameTexture() const {
    const Sequence activeSequence =
        phase_ == Phase::Failure || phase_ == Phase::FailureComplete
            ? Sequence::Failure
            : Sequence::Opening;
    const float sequenceElapsed =
        activeSequence == Sequence::Opening ? openingElapsed_ : failureElapsed_;
    const float frameDuration = durationForSequence(activeSequence) /
        static_cast<float>(std::max(1, frameCountForSequence(activeSequence)));
    const int frameIndex = std::clamp(
        static_cast<int>(sequenceElapsed / std::max(0.001f, frameDuration)),
        0,
        frameCountForSequence(activeSequence) - 1
    );
    const std::vector<FrameSlot>& slots = slotsForSequence(activeSequence, openingFrames_, failureFrames_);
    return slots[static_cast<std::size_t>(frameIndex)].texture;
}

} // namespace battle
