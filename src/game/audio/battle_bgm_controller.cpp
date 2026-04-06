#include "battle_bgm_controller.h"

#include <algorithm>

namespace game::audio {
namespace {

constexpr float kFadeDurationSeconds = 0.35f;
constexpr float kCrossfadeDurationSeconds = 0.85f;

/**
 * @brief Maps an input to a smooth 0→1 interpolation using a cubic Hermite curve.
 *
 * Clamps `value` to the range [0, 1] and returns the smoothstep result for that clamped value.
 *
 * @param value Input value to map; values less than 0 become 0, greater than 1 become 1.
 * @return float Interpolated value in [0, 1] computed as `t*t*(3 - 2*t)` where `t` is the clamped input.
 */
float smoothstep01(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - (2.0f * t));
}

}

/**
 * @brief Attaches a BgmPlayer instance to the controller for subsequent playback control.
 *
 * Stores a pointer to the provided `player` so the controller can call play/pause/stop on it.
 * The controller does not take ownership of `player`; the caller is responsible for its lifetime
 * and must ensure it remains valid while attached.
 *
 * @param player Reference to the BgmPlayer to attach.
 */
void BattleBgmController::attach(BgmPlayer& primaryPlayer, BgmPlayer& secondaryPlayer) {
    slots_[0].player = &primaryPlayer;
    slots_[1].player = &secondaryPlayer;
    activeSlotIndex_ = 0;
    incomingSlotIndex_ = -1;
}

/**
 * @brief Detaches any attached BgmPlayer and resets the controller state.
 *
 * Clears current and queued track paths, restores base, queued and master volumes and
 * fade gain to their defaults, resets fade timing and start gain, sets the transition
 * state to Idle, and clears the paused flag.
 */
void BattleBgmController::detach() {
    slots_[0] = PlaybackSlot{};
    slots_[1] = PlaybackSlot{};
    activeSlotIndex_ = 0;
    incomingSlotIndex_ = -1;
    masterVolume_ = 1.0f;
    fadeDurationSeconds_ = kFadeDurationSeconds;
    transitionElapsedSeconds_ = 0.0f;
    activeStartGain_ = 1.0f;
    incomingStartGain_ = 0.0f;
    paused_ = false;
    transitionState_ = TransitionState::Idle;
}

/**
 * @brief Immediately stops playback and resets controller state.
 *
 * Stops the attached BGM player (if any), clears the current and queued track information,
 * and resets base volumes, fade/gain/timing values, pause flag, and transition state to defaults.
 */
void BattleBgmController::stop() {
    stopSlot(slots_[0]);
    stopSlot(slots_[1]);
    activeSlotIndex_ = 0;
    incomingSlotIndex_ = -1;
    fadeDurationSeconds_ = kFadeDurationSeconds;
    transitionElapsedSeconds_ = 0.0f;
    activeStartGain_ = 1.0f;
    incomingStartGain_ = 0.0f;
    paused_ = false;
    transitionState_ = TransitionState::Idle;
}

/**
 * @brief Initiates a fade-out of the current track and then stops playback.
 *
 * Clamps the requested fade duration to at least 0.01 seconds, clears any queued track,
 * and, if a player is attached and currently playing a track (and not paused), begins a fade-out
 * by resetting transition timing and setting the transition state to FadingOut.
 * If no player is attached, the player is not playing, there is no current track, or playback is paused,
 * the controller stops playback immediately.
 *
 * @param fadeDurationSeconds Desired duration of the fade-out in seconds; values less than 0.01 are raised to 0.01.
 */
void BattleBgmController::fadeOutAndStop(float fadeDurationSeconds) {
    incomingSlotIndex_ = -1;
    fadeDurationSeconds_ = std::max(0.01f, fadeDurationSeconds);

    if (!anySlotPlaying()) {
        return;
    }

    if (paused_) {
        stop();
        return;
    }

    transitionElapsedSeconds_ = 0.0f;
    activeStartGain_ = std::clamp(slots_[activeSlotIndex_].fadeGain, 0.0f, 1.0f);
    incomingStartGain_ = 0.0f;
    if (const int otherSlotIndex = inactiveSlotIndex();
        slots_[otherSlotIndex].player != nullptr && slots_[otherSlotIndex].player->isPlaying()) {
        incomingStartGain_ = std::clamp(slots_[otherSlotIndex].fadeGain, 0.0f, 1.0f);
    }
    transitionState_ = TransitionState::FadingOutToStop;
}

/**
 * @brief Immediately starts playback of the specified track, cancelling any queued track.
 *
 * Begins playing the given track at an effective volume equal to the clamped `baseVolume` multiplied by the controller's master volume. If `trackPath` is empty or the player fails to start the track, playback is stopped and controller state is cleared. If the controller is currently paused, the player will be paused immediately after starting.
 *
 * @param trackPath Path or identifier of the track to play; an empty string stops playback.
 * @param baseVolume Desired base volume for the track, clamped to the range [0, 1].
 */
void BattleBgmController::playImmediate(const std::string& trackPath, float baseVolume) {
    if (slots_[activeSlotIndex_].player == nullptr) {
        return;
    }

    const float clampedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
    if (trackPath.empty()) {
        stop();
        return;
    }

    stopSlot(slots_[0]);
    stopSlot(slots_[1]);
    activeSlotIndex_ = 0;
    incomingSlotIndex_ = -1;

    if (!startSlot(slots_[activeSlotIndex_], trackPath, clampedBaseVolume, 1.0f)) {
        stop();
        return;
    }

    fadeDurationSeconds_ = kFadeDurationSeconds;
    transitionElapsedSeconds_ = 0.0f;
    activeStartGain_ = 1.0f;
    incomingStartGain_ = 0.0f;
    transitionState_ = TransitionState::Idle;
}

/**
 * @brief Starts playback of a track at zero volume and fades it in to the specified base volume.
 *
 * If no player is attached this function does nothing. The provided base volume is clamped to
 * the range [0, 1]; the effective fade duration is clamped to at least 0.01 seconds. Any
 * previously queued track is cleared. If `trackPath` is empty or starting playback fails, the
 * controller stops playback and resets state. When playback successfully starts, the controller
 * initializes a fade-in transition and applies the initial (zero) volume. If the controller is
 * currently paused, playback is started then immediately paused to preserve the paused state.
 *
 * @param trackPath Path or identifier of the track to play; if empty, playback is stopped.
 * @param baseVolume Desired base volume for the track; values are clamped to [0, 1].
 * @param fadeDurationSeconds Duration of the fade-in in seconds; values less than 0.01 are
 *        treated as 0.01 seconds.
 */
void BattleBgmController::playWithFadeIn(const std::string& trackPath,
                                         float baseVolume,
                                         float fadeDurationSeconds) {
    if (slots_[activeSlotIndex_].player == nullptr) {
        return;
    }

    const float clampedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
    if (trackPath.empty()) {
        stop();
        return;
    }

    stopSlot(slots_[0]);
    stopSlot(slots_[1]);
    activeSlotIndex_ = 0;
    incomingSlotIndex_ = -1;

    if (!startSlot(slots_[activeSlotIndex_], trackPath, clampedBaseVolume, 0.0f)) {
        stop();
        return;
    }

    fadeDurationSeconds_ = std::max(0.01f, fadeDurationSeconds);
    transitionElapsedSeconds_ = 0.0f;
    activeStartGain_ = 0.0f;
    incomingStartGain_ = 0.0f;
    transitionState_ = TransitionState::FadingIn;
    applyVolumes();
}

/**
 * @brief Requests playback of a battle BGM track, optionally queuing it for a fade transition.
 *
 * If no player is attached or `trackPath` is empty the request is ignored. If nothing is
 * currently playing, the requested track starts immediately. If the requested track matches
 * the current track, the controller updates the current base volume and cancels any queued
 * transition. Otherwise the controller queues the requested track and begins fading out the
 * current track to transition to the queued track.
 *
 * @param trackPath Filesystem path or identifier of the track to play; ignored if empty.
 * @param baseVolume Desired base volume in the range [0, 1]; values outside this range are clamped.
 */
void BattleBgmController::requestTrack(const std::string& trackPath, float baseVolume) {
    if (slots_[activeSlotIndex_].player == nullptr) {
        return;
    }

    const float clampedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
    if (trackPath.empty()) {
        return;
    }

    PlaybackSlot& activeSlot = slots_[activeSlotIndex_];
    if (incomingSlotIndex_ >= 0 && incomingSlotIndex_ < static_cast<int>(slots_.size())) {
        PlaybackSlot& incomingSlot = slots_[incomingSlotIndex_];
        if (incomingSlot.trackPath == trackPath) {
            incomingSlot.baseVolume = clampedBaseVolume;
            applyVolumes();
            return;
        }
        if (activeSlot.trackPath == trackPath) {
            stopSlot(incomingSlot);
            incomingSlotIndex_ = -1;
            activeSlot.fadeGain = 1.0f;
            transitionState_ = TransitionState::Idle;
            transitionElapsedSeconds_ = 0.0f;
            activeStartGain_ = 1.0f;
            incomingStartGain_ = 0.0f;
            applyVolumes();
            return;
        }
    }

    if (!activeSlot.player->isPlaying() || activeSlot.trackPath.empty()) {
        playImmediate(trackPath, clampedBaseVolume);
        return;
    }

    if (trackPath == activeSlot.trackPath) {
        activeSlot.baseVolume = clampedBaseVolume;
        transitionState_ = TransitionState::Idle;
        activeSlot.fadeGain = 1.0f;
        incomingSlotIndex_ = -1;
        fadeDurationSeconds_ = kCrossfadeDurationSeconds;
        transitionElapsedSeconds_ = 0.0f;
        activeStartGain_ = 1.0f;
        incomingStartGain_ = 0.0f;
        applyVolumes();
        return;
    }

    float incomingStartSeconds = 0.0f;
    if (activeSlot.player != nullptr && activeSlot.player->sampleRate() > 0) {
        incomingStartSeconds =
            static_cast<float>(activeSlot.player->currentFrame()) /
            static_cast<float>(activeSlot.player->sampleRate());
    }

    const int nextSlotIndex = inactiveSlotIndex();
    PlaybackSlot& nextSlot = slots_[nextSlotIndex];
    stopSlot(nextSlot);
    if (!startSlot(nextSlot, trackPath, clampedBaseVolume, 0.0f, incomingStartSeconds)) {
        return;
    }

    incomingSlotIndex_ = nextSlotIndex;
    fadeDurationSeconds_ = kCrossfadeDurationSeconds;
    transitionElapsedSeconds_ = 0.0f;
    activeStartGain_ = std::clamp(activeSlot.fadeGain, 0.0f, 1.0f);
    incomingStartGain_ = 0.0f;
    transitionState_ = TransitionState::Crossfading;
    applyVolumes();
}

/**
 * @brief Advance the controller's fade/track transition state by a time step.
 *
 * Processes the internal transition state machine using the provided elapsed time:
 * - In Idle, reapplies current effective volume.
 * - In FadingOut, decreases the fade gain over the configured fade duration, stops the current player
 *   when the fade completes, and either returns to Idle or attempts to start the queued track and
 *   transition to FadingIn.
 * - In FadingIn, increases the fade gain until full volume is reached and then returns to Idle.
 *
 * Negative `deltaSeconds` values are treated as zero. Early exits occur if no player is attached.
 *
 * @param deltaSeconds Elapsed time in seconds since the last update; values less than zero are treated as zero.
 */
void BattleBgmController::update(float deltaSeconds) {
    if (slots_[activeSlotIndex_].player == nullptr) {
        return;
    }

    switch (transitionState_) {
        case TransitionState::Idle:
            applyVolumes();
            return;
        case TransitionState::FadingIn: {
            PlaybackSlot& activeSlot = slots_[activeSlotIndex_];
            if (activeSlot.player == nullptr || !activeSlot.player->isPlaying()) {
                transitionState_ = TransitionState::Idle;
                activeSlot.fadeGain = 1.0f;
                transitionElapsedSeconds_ = 0.0f;
                activeStartGain_ = 1.0f;
                return;
            }

            transitionElapsedSeconds_ += std::max(deltaSeconds, 0.0f);
            const float fadeInProgress = fadeDurationSeconds_ <= 0.0f
                ? 1.0f
                : std::clamp(transitionElapsedSeconds_ / fadeDurationSeconds_, 0.0f, 1.0f);
            activeSlot.fadeGain = activeStartGain_ + ((1.0f - activeStartGain_) * smoothstep01(fadeInProgress));
            applyVolumes();
            if (fadeInProgress >= 1.0f) {
                activeSlot.fadeGain = 1.0f;
                transitionElapsedSeconds_ = 0.0f;
                activeStartGain_ = 1.0f;
                transitionState_ = TransitionState::Idle;
            }
            return;
        }
        case TransitionState::FadingOutToPause: {
            transitionElapsedSeconds_ += std::max(deltaSeconds, 0.0f);
            const float fadeOutProgress = fadeDurationSeconds_ <= 0.0f
                ? 1.0f
                : std::clamp(transitionElapsedSeconds_ / fadeDurationSeconds_, 0.0f, 1.0f);
            const float shapedProgress = smoothstep01(fadeOutProgress);
            slots_[activeSlotIndex_].fadeGain = activeStartGain_ * (1.0f - shapedProgress);
            if (incomingSlotIndex_ >= 0 && incomingSlotIndex_ < static_cast<int>(slots_.size())) {
                slots_[incomingSlotIndex_].fadeGain = incomingStartGain_ * (1.0f - shapedProgress);
            }
            applyVolumes();
            if (fadeOutProgress < 1.0f) {
                return;
            }

            paused_ = true;
            for (PlaybackSlot& slot : slots_) {
                if (slot.player != nullptr && slot.player->isPlaying() && !slot.player->isPaused()) {
                    slot.player->pause();
                }
            }
            transitionElapsedSeconds_ = 0.0f;
            transitionState_ = TransitionState::Idle;
            return;
        }
        case TransitionState::FadingOutToStop: {
            transitionElapsedSeconds_ += std::max(deltaSeconds, 0.0f);
            const float fadeOutProgress = fadeDurationSeconds_ <= 0.0f
                ? 1.0f
                : std::clamp(transitionElapsedSeconds_ / fadeDurationSeconds_, 0.0f, 1.0f);
            const float shapedProgress = smoothstep01(fadeOutProgress);
            slots_[activeSlotIndex_].fadeGain = activeStartGain_ * (1.0f - shapedProgress);
            if (const int otherSlotIndex = inactiveSlotIndex();
                slots_[otherSlotIndex].player != nullptr && slots_[otherSlotIndex].player->isPlaying()) {
                slots_[otherSlotIndex].fadeGain = incomingStartGain_ * (1.0f - shapedProgress);
            }
            applyVolumes();
            if (fadeOutProgress < 1.0f) {
                return;
            }

            stop();
            return;
        }
        case TransitionState::Crossfading: {
            if (incomingSlotIndex_ < 0 || incomingSlotIndex_ >= static_cast<int>(slots_.size())) {
                transitionState_ = TransitionState::Idle;
                slots_[activeSlotIndex_].fadeGain = 1.0f;
                transitionElapsedSeconds_ = 0.0f;
                activeStartGain_ = 1.0f;
                incomingStartGain_ = 0.0f;
                return;
            }

            PlaybackSlot& activeSlot = slots_[activeSlotIndex_];
            PlaybackSlot& incomingSlot = slots_[incomingSlotIndex_];
            if (activeSlot.player == nullptr || incomingSlot.player == nullptr ||
                !activeSlot.player->isPlaying() || !incomingSlot.player->isPlaying()) {
                stopSlot(incomingSlot);
                incomingSlotIndex_ = -1;
                transitionState_ = TransitionState::Idle;
                activeSlot.fadeGain = 1.0f;
                transitionElapsedSeconds_ = 0.0f;
                activeStartGain_ = 1.0f;
                incomingStartGain_ = 0.0f;
                applyVolumes();
                return;
            }

            transitionElapsedSeconds_ += std::max(deltaSeconds, 0.0f);
            const float crossfadeProgress = fadeDurationSeconds_ <= 0.0f
                ? 1.0f
                : std::clamp(transitionElapsedSeconds_ / fadeDurationSeconds_, 0.0f, 1.0f);
            const float shapedProgress = smoothstep01(crossfadeProgress);
            activeSlot.fadeGain = activeStartGain_ * (1.0f - shapedProgress);
            incomingSlot.fadeGain = incomingStartGain_ + ((1.0f - incomingStartGain_) * shapedProgress);
            applyVolumes();
            if (crossfadeProgress >= 1.0f) {
                stopSlot(activeSlot);
                activeSlotIndex_ = incomingSlotIndex_;
                incomingSlotIndex_ = -1;
                slots_[activeSlotIndex_].fadeGain = 1.0f;
                transitionElapsedSeconds_ = 0.0f;
                activeStartGain_ = 1.0f;
                incomingStartGain_ = 0.0f;
                transitionState_ = TransitionState::Idle;
            }
            return;
        }
    }
}

/**
 * @brief Sets the master volume multiplier for battle BGM and applies it immediately.
 *
 * The value is clamped to the range [0, 1] and applied to the currently playing track.
 * This affects the effective output volume computed as current base volume × fade gain × master volume.
 *
 * @param masterVolume Master volume multiplier in the range [0, 1]; values outside the range are clamped.
 */
void BattleBgmController::setMasterVolume(float masterVolume) {
    masterVolume_ = std::clamp(masterVolume, 0.0f, 1.0f);
    applyVolumes();
}

void BattleBgmController::pauseWithFade(float fadeDurationSeconds) {
    if (!anySlotPlaying()) {
        paused_ = true;
        return;
    }

    if (paused_ || transitionState_ == TransitionState::FadingOutToPause) {
        return;
    }

    fadeDurationSeconds_ = std::max(0.01f, fadeDurationSeconds);
    transitionElapsedSeconds_ = 0.0f;
    activeStartGain_ = std::clamp(slots_[activeSlotIndex_].fadeGain, 0.0f, 1.0f);
    incomingStartGain_ = 0.0f;
    if (incomingSlotIndex_ >= 0 && incomingSlotIndex_ < static_cast<int>(slots_.size())) {
        incomingStartGain_ = std::clamp(slots_[incomingSlotIndex_].fadeGain, 0.0f, 1.0f);
    }
    transitionState_ = TransitionState::FadingOutToPause;
}

void BattleBgmController::resumeWithFade(float fadeDurationSeconds) {
    if (!anySlotPlaying()) {
        paused_ = false;
        return;
    }

    if (paused_) {
        paused_ = false;
        for (PlaybackSlot& slot : slots_) {
            if (slot.player != nullptr && slot.player->isPlaying() && slot.player->isPaused()) {
                slot.player->resume();
            }
        }
    }

    fadeDurationSeconds_ = std::max(0.01f, fadeDurationSeconds);
    transitionElapsedSeconds_ = 0.0f;
    activeStartGain_ = std::clamp(slots_[activeSlotIndex_].fadeGain, 0.0f, 1.0f);
    if (activeStartGain_ >= 0.999f) {
        slots_[activeSlotIndex_].fadeGain = 1.0f;
        transitionState_ = TransitionState::Idle;
        applyVolumes();
        return;
    }

    transitionState_ = TransitionState::FadingIn;
    applyVolumes();
}

/**
 * @brief Pauses battle BGM control and, if applicable, the attached player.
 *
 * Marks the controller as paused and pauses the attached BgmPlayer when it is currently playing and not already paused.
 */
void BattleBgmController::pause() {
    paused_ = true;
    for (PlaybackSlot& slot : slots_) {
        if (slot.player != nullptr && slot.player->isPlaying() && !slot.player->isPaused()) {
            slot.player->pause();
        }
    }
}

/**
 * @brief Resumes BGM playback managed by the controller.
 *
 * Clears the controller's paused flag and, if a player is attached and is currently playing but paused, instructs the player to resume.
 */
void BattleBgmController::resume() {
    paused_ = false;
    for (PlaybackSlot& slot : slots_) {
        if (slot.player != nullptr && slot.player->isPlaying() && slot.player->isPaused()) {
            slot.player->resume();
        }
    }
}

/**
 * @brief Reports whether a background music track is currently playing.
 *
 * @return `true` if a BGM player is attached and is currently playing, `false` otherwise.
 */
bool BattleBgmController::isPlaying() const {
    return anySlotPlaying();
}

/**
 * @brief Reports whether the attached BGM player is currently paused.
 *
 * @return `true` if a player is attached and it is paused, `false` otherwise.
 */
bool BattleBgmController::isPaused() const {
    return paused_ && anySlotPlaying();
}

/**
 * @brief Apply the controller's effective volume to the attached player.
 *
 * If a player is attached and currently playing, sets its volume to
 * currentBaseVolume_ * fadeGain_ * masterVolume_. Does nothing when there
 * is no attached player or the player is not playing.
 */
void BattleBgmController::applyVolumes() const {
    for (const PlaybackSlot& slot : slots_) {
        if (slot.player == nullptr || !slot.player->isPlaying()) {
            continue;
        }
        slot.player->setVolume(slot.baseVolume * slot.fadeGain * masterVolume_);
    }
}

void BattleBgmController::stopSlot(PlaybackSlot& slot) {
    if (slot.player != nullptr) {
        slot.player->stop();
    }
    slot.trackPath.clear();
    slot.baseVolume = 1.0f;
    slot.fadeGain = 1.0f;
}

bool BattleBgmController::startSlot(PlaybackSlot& slot,
                                    const std::string& trackPath,
                                    float baseVolume,
                                    float initialGain,
                                    float startSeconds) {
    if (slot.player == nullptr) {
        return false;
    }

    const float clampedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
    const float clampedInitialGain = std::clamp(initialGain, 0.0f, 1.0f);
    if (!slot.player->playAtTime(trackPath,
                                 clampedBaseVolume * clampedInitialGain * masterVolume_,
                                 startSeconds)) {
        stopSlot(slot);
        return false;
    }

    slot.trackPath = trackPath;
    slot.baseVolume = clampedBaseVolume;
    slot.fadeGain = clampedInitialGain;
    if (paused_) {
        slot.player->pause();
    }
    return true;
}

bool BattleBgmController::anySlotPlaying() const {
    for (const PlaybackSlot& slot : slots_) {
        if (slot.player != nullptr && slot.player->isPlaying()) {
            return true;
        }
    }
    return false;
}

int BattleBgmController::inactiveSlotIndex() const {
    return activeSlotIndex_ == 0 ? 1 : 0;
}

} // namespace game::audio
