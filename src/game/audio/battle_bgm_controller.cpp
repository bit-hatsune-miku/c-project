#include "battle_bgm_controller.h"

#include <algorithm>

namespace game::audio {
namespace {

constexpr float kFadeDurationSeconds = 0.35f;

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
void BattleBgmController::attach(BgmPlayer& player) {
    player_ = &player;
}

/**
 * @brief Detaches any attached BgmPlayer and resets the controller state.
 *
 * Clears current and queued track paths, restores base, queued and master volumes and
 * fade gain to their defaults, resets fade timing and start gain, sets the transition
 * state to Idle, and clears the paused flag.
 */
void BattleBgmController::detach() {
    player_ = nullptr;
    currentTrackPath_.clear();
    queuedTrackPath_.clear();
    currentBaseVolume_ = 1.0f;
    queuedBaseVolume_ = 1.0f;
    masterVolume_ = 1.0f;
    fadeGain_ = 1.0f;
    fadeDurationSeconds_ = kFadeDurationSeconds;
    transitionElapsedSeconds_ = 0.0f;
    transitionStartGain_ = 1.0f;
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
    queuedTrackPath_.clear();
    if (player_ != nullptr) {
        player_->stop();
    }
    currentTrackPath_.clear();
    currentBaseVolume_ = 1.0f;
    queuedBaseVolume_ = 1.0f;
    fadeGain_ = 1.0f;
    fadeDurationSeconds_ = kFadeDurationSeconds;
    transitionElapsedSeconds_ = 0.0f;
    transitionStartGain_ = 1.0f;
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
    queuedTrackPath_.clear();
    queuedBaseVolume_ = 1.0f;
    fadeDurationSeconds_ = std::max(0.01f, fadeDurationSeconds);

    if (player_ == nullptr) {
        return;
    }

    if (!player_->isPlaying() || currentTrackPath_.empty()) {
        stop();
        return;
    }

    if (paused_) {
        stop();
        return;
    }

    transitionElapsedSeconds_ = 0.0f;
    transitionStartGain_ = std::clamp(fadeGain_, 0.0f, 1.0f);
    transitionState_ = TransitionState::FadingOut;
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
    if (player_ == nullptr) {
        return;
    }

    const float clampedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
    queuedTrackPath_.clear();
    queuedBaseVolume_ = clampedBaseVolume;

    if (trackPath.empty()) {
        stop();
        return;
    }

    if (!player_->play(trackPath, clampedBaseVolume * masterVolume_)) {
        stop();
        return;
    }

    currentTrackPath_ = trackPath;
    currentBaseVolume_ = clampedBaseVolume;
    fadeGain_ = 1.0f;
    fadeDurationSeconds_ = kFadeDurationSeconds;
    transitionElapsedSeconds_ = 0.0f;
    transitionStartGain_ = 1.0f;
    transitionState_ = TransitionState::Idle;
    if (paused_) {
        player_->pause();
    }
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
    if (player_ == nullptr) {
        return;
    }

    const float clampedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
    queuedTrackPath_.clear();
    queuedBaseVolume_ = clampedBaseVolume;

    if (trackPath.empty()) {
        stop();
        return;
    }

    if (!player_->play(trackPath, 0.0f)) {
        stop();
        return;
    }

    currentTrackPath_ = trackPath;
    currentBaseVolume_ = clampedBaseVolume;
    fadeGain_ = 0.0f;
    fadeDurationSeconds_ = std::max(0.01f, fadeDurationSeconds);
    transitionElapsedSeconds_ = 0.0f;
    transitionStartGain_ = 0.0f;
    transitionState_ = TransitionState::FadingIn;
    if (paused_) {
        player_->pause();
    }
    applyVolume();
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
    if (player_ == nullptr) {
        return;
    }

    const float clampedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
    if (trackPath.empty()) {
        return;
    }

    if (!player_->isPlaying() || currentTrackPath_.empty()) {
        playImmediate(trackPath, clampedBaseVolume);
        return;
    }

    if (trackPath == currentTrackPath_) {
        currentBaseVolume_ = clampedBaseVolume;
        queuedTrackPath_.clear();
        queuedBaseVolume_ = clampedBaseVolume;
        transitionState_ = TransitionState::Idle;
        fadeGain_ = 1.0f;
        fadeDurationSeconds_ = kFadeDurationSeconds;
        transitionElapsedSeconds_ = 0.0f;
        transitionStartGain_ = 1.0f;
        applyVolume();
        return;
    }

    queuedTrackPath_ = trackPath;
    queuedBaseVolume_ = clampedBaseVolume;
    fadeDurationSeconds_ = kFadeDurationSeconds;
    transitionElapsedSeconds_ = 0.0f;
    transitionStartGain_ = std::clamp(fadeGain_, 0.0f, 1.0f);
    transitionState_ = TransitionState::FadingOut;
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
    if (player_ == nullptr) {
        return;
    }

    switch (transitionState_) {
        case TransitionState::Idle:
            applyVolume();
            return;
        case TransitionState::FadingOut: {
            if (!player_->isPlaying()) {
                transitionState_ = TransitionState::Idle;
                fadeGain_ = 1.0f;
                transitionElapsedSeconds_ = 0.0f;
                transitionStartGain_ = 1.0f;
                return;
            }

            transitionElapsedSeconds_ += std::max(deltaSeconds, 0.0f);
            const float fadeOutProgress = fadeDurationSeconds_ <= 0.0f
                ? 1.0f
                : std::clamp(transitionElapsedSeconds_ / fadeDurationSeconds_, 0.0f, 1.0f);
            fadeGain_ = transitionStartGain_ * (1.0f - smoothstep01(fadeOutProgress));
            applyVolume();
            if (fadeOutProgress < 1.0f) {
                return;
            }

            player_->stop();
            currentTrackPath_.clear();
            currentBaseVolume_ = 1.0f;
            if (queuedTrackPath_.empty()) {
                fadeGain_ = 1.0f;
                fadeDurationSeconds_ = kFadeDurationSeconds;
                transitionElapsedSeconds_ = 0.0f;
                transitionStartGain_ = 1.0f;
                transitionState_ = TransitionState::Idle;
                return;
            }

            if (!player_->play(queuedTrackPath_, 0.0f)) {
                queuedTrackPath_.clear();
                queuedBaseVolume_ = 1.0f;
                fadeGain_ = 1.0f;
                fadeDurationSeconds_ = kFadeDurationSeconds;
                transitionElapsedSeconds_ = 0.0f;
                transitionStartGain_ = 1.0f;
                transitionState_ = TransitionState::Idle;
                return;
            }

            currentTrackPath_ = queuedTrackPath_;
            currentBaseVolume_ = queuedBaseVolume_;
            queuedTrackPath_.clear();
            queuedBaseVolume_ = 1.0f;
            fadeGain_ = 0.0f;
            fadeDurationSeconds_ = kFadeDurationSeconds;
            transitionElapsedSeconds_ = 0.0f;
            transitionStartGain_ = 0.0f;
            transitionState_ = TransitionState::FadingIn;
            if (paused_) {
                player_->pause();
            }
            applyVolume();
            return;
        }
        case TransitionState::FadingIn: {
            if (!player_->isPlaying()) {
                transitionState_ = TransitionState::Idle;
                fadeGain_ = 1.0f;
                transitionElapsedSeconds_ = 0.0f;
                transitionStartGain_ = 1.0f;
                return;
            }

            transitionElapsedSeconds_ += std::max(deltaSeconds, 0.0f);
            const float fadeInProgress = fadeDurationSeconds_ <= 0.0f
                ? 1.0f
                : std::clamp(transitionElapsedSeconds_ / fadeDurationSeconds_, 0.0f, 1.0f);
            fadeGain_ = smoothstep01(fadeInProgress);
            applyVolume();
            if (fadeInProgress >= 1.0f) {
                fadeGain_ = 1.0f;
                transitionElapsedSeconds_ = 0.0f;
                transitionStartGain_ = 1.0f;
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
    applyVolume();
}

/**
 * @brief Pauses battle BGM control and, if applicable, the attached player.
 *
 * Marks the controller as paused and pauses the attached BgmPlayer when it is currently playing and not already paused.
 */
void BattleBgmController::pause() {
    paused_ = true;
    if (player_ != nullptr && player_->isPlaying() && !player_->isPaused()) {
        player_->pause();
    }
}

/**
 * @brief Resumes BGM playback managed by the controller.
 *
 * Clears the controller's paused flag and, if a player is attached and is currently playing but paused, instructs the player to resume.
 */
void BattleBgmController::resume() {
    paused_ = false;
    if (player_ != nullptr && player_->isPlaying() && player_->isPaused()) {
        player_->resume();
    }
}

/**
 * @brief Reports whether a background music track is currently playing.
 *
 * @return `true` if a BGM player is attached and is currently playing, `false` otherwise.
 */
bool BattleBgmController::isPlaying() const {
    return player_ != nullptr && player_->isPlaying();
}

/**
 * @brief Reports whether the attached BGM player is currently paused.
 *
 * @return `true` if a player is attached and it is paused, `false` otherwise.
 */
bool BattleBgmController::isPaused() const {
    return player_ != nullptr && player_->isPaused();
}

/**
 * @brief Apply the controller's effective volume to the attached player.
 *
 * If a player is attached and currently playing, sets its volume to
 * currentBaseVolume_ * fadeGain_ * masterVolume_. Does nothing when there
 * is no attached player or the player is not playing.
 */
void BattleBgmController::applyVolume() {
    if (player_ == nullptr || !player_->isPlaying()) {
        return;
    }

    player_->setVolume(currentBaseVolume_ * fadeGain_ * masterVolume_);
}

} // namespace game::audio
