#pragma once

#include <array>
#include <string>
#include <unordered_map>

#include "bgm_player.h"

/**
 * Controller that manages battle background music playback and transitions.
 */

/**
 * Attach the controller to a BgmPlayer so playback commands affect that player.
 * @param player Target BgmPlayer to control.
 */

/**
 * Detach the controller from its current BgmPlayer.
 */

/**
 * Stop playback immediately and clear any queued track transitions.
 */

/**
 * Fade out the current playback over the given duration and stop when the fade completes.
 * @param fadeDurationSeconds Duration of the fade-out in seconds.
 */

/**
 * Start playing the specified track immediately at the given base volume.
 * @param trackPath Path or identifier of the track to play.
 * @param baseVolume Base volume multiplier for the track (0.0 = silent, 1.0 = original).
 */

/**
 * Start playing the specified track and fade in to the given base volume over the provided duration.
 * @param trackPath Path or identifier of the track to play.
 * @param baseVolume Target base volume after fade-in (0.0 = silent, 1.0 = original).
 * @param fadeDurationSeconds Duration of the fade-in in seconds.
 */

/**
 * Queue a track and base volume as the next target for a transition without starting playback immediately.
 * @param trackPath Path or identifier of the track to queue.
 * @param baseVolume Base volume to use when the queued track becomes active.
 */

/**
 * Advance the controller's internal timing and process fades/transitions.
 * @param deltaSeconds Time elapsed since the last update, in seconds.
 */

/**
 * Set the master volume multiplier applied on top of track base volumes and fade gain.
 * @param masterVolume Master volume multiplier (0.0 = silent, 1.0 = unchanged).
 */

/**
 * Pause playback; current state is preserved for resume.
 */

/**
 * Resume playback if previously paused.
 */

/**
 * Determine whether a track is currently playing or in an active transition.
 * @returns `true` if playback or an active transition is in progress, `false` otherwise.
 */

/**
 * Check whether playback is currently paused.
 * @returns `true` if paused, `false` otherwise.
 */

/**
 * Apply the computed effective volume (master volume × base volume × fade gain) to the attached BgmPlayer.
 */
namespace game::audio {

class BattleBgmController {
public:
    void attach(BgmPlayer& primaryPlayer, BgmPlayer& secondaryPlayer);
    void detach();

    void stop();
    void fadeOutAndStop(float fadeDurationSeconds = 1.2f);
    bool preloadTrack(const std::string& trackPath);
    void clearPreloadedTracks();
    void playImmediate(const std::string& trackPath, float baseVolume);
    void playWithFadeIn(const std::string& trackPath, float baseVolume, float fadeDurationSeconds = 0.35f);
    void requestTrack(const std::string& trackPath, float baseVolume);
    void update(float deltaSeconds);

    void setMasterVolume(float masterVolume);
    void pauseWithFade(float fadeDurationSeconds = 0.22f);
    void resumeWithFade(float fadeDurationSeconds = 0.18f);
    void pause();
    void resume();

    bool isPlaying() const;
    bool isPaused() const;

private:
    struct PlaybackSlot {
        BgmPlayer* player = nullptr;
        std::string trackPath;
        float baseVolume = 1.0f;
        float fadeGain = 1.0f;
    };

    enum class TransitionState {
        Idle,
        FadingIn,
        FadingOutToPause,
        FadingOutToStop,
        Crossfading
    };

    void applyVolumes() const;
    void stopSlot(PlaybackSlot& slot);
    bool startSlot(PlaybackSlot& slot,
                   const std::string& trackPath,
                   float baseVolume,
                   float initialGain,
                   float startSeconds = 0.0f);
    const DecodedAudioClip* findPreloadedClip(const std::string& trackPath) const;
    bool anySlotPlaying() const;
    int inactiveSlotIndex() const;

    std::array<PlaybackSlot, 2> slots_{};
    int activeSlotIndex_ = 0;
    int incomingSlotIndex_ = -1;
    float masterVolume_ = 1.0f;
    float fadeDurationSeconds_ = 0.35f;
    float transitionElapsedSeconds_ = 0.0f;
    float activeStartGain_ = 1.0f;
    float incomingStartGain_ = 0.0f;
    bool paused_ = false;
    TransitionState transitionState_ = TransitionState::Idle;
    std::unordered_map<std::string, DecodedAudioClip> preloadedClips_;
};

} // namespace game::audio
