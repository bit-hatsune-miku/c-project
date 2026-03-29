#pragma once

#include <array>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "../../GameMenu/menu_shared.h"
#include "bgm_player.h"
#include "ui_music_types.h"

/**
 * Initialize the UI music controller and its audio resources.
 * @returns `true` if initialization succeeded and the controller is ready, `false` otherwise.
 */

/**
 * Shutdown the UI music controller and release any audio resources.
 */

/**
 * Advance the controller one frame: resolve desired surface, handle transitions,
 * update story BGM pause bridging, drive playback, and refresh visual state.
 * @param state Current application state used to resolve the desired UI music surface.
 * @param loadingTransitionActive `true` if a loading transition is active and music behavior should account for it.
 * @param deltaSeconds Time elapsed since the last update call, in seconds.
 */

/**
 * Get the current computed UI music visual state.
 * @returns Reference to the current UiMusicVisualState.
 */

/**
 * Initialize the underlying audio subsystem and prepare the BGM player.
 * @returns `true` if audio initialization succeeded, `false` otherwise.
 */

/**
 * Populate the track pool with available tracks and mark usable entries.
 * @returns `true` if at least one usable track was loaded, `false` otherwise.
 */

/**
 * Map the provided application state to the desired UiMusicSurface for playback.
 * @param state Application state to inspect.
 * @returns The resolved UiMusicSurface for the given state.
 */

/**
 * Handle a transition between two UI music surfaces, potentially deferring starts,
 * changing tracks, or adjusting gains based on the transition and application state.
 * @param previousSurface The surface active before the transition.
 * @param nextSurface The surface requested after the transition.
 * @param state Current application state that may affect transition decisions.
 */

/**
 * Advance the internal story BGM pause/resume state machine and perform fades/resumes as needed.
 * @param deltaSeconds Time elapsed since the last update call, in seconds.
 */

/**
 * Update music playback to approach the desired surface, applying volume rules and
 * accounting for loading transitions.
 * @param deltaSeconds Time elapsed since the last update call, in seconds.
 * @param desiredSurface The surface that playback should be driven toward.
 * @param loadingTransitionActive `true` if a loading transition is active and playback should adapt.
 * @param musicVolume Master music volume multiplier to apply.
 */

/**
 * Select and start a track appropriate for the given surface.
 * @param surface Surface for which to start playback.
 * @returns `true` if a track was successfully started for the surface, `false` otherwise.
 */

/**
 * Draw the next track index from the non-repeating draw bag, refilling it if necessary.
 * @returns The index of the chosen track in the track pool.
 */

/**
 * Rebuild the draw bag from currently usable tracks to support randomized non-repeating selection.
 */

/**
 * Produce a fractional random start offset (0.0 to <1.0) used to seed playback position within a track.
 * @returns Fractional start position within the track.
 */

/**
 * Compute a per-track gain multiplier based on the track's path or identifier.
 * @param path Track path or identifier.
 * @returns Gain multiplier to apply when playing the specified track.
 */

/**
 * Refresh the visualState_ using analyzed track data and smoothing over time.
 * @param deltaSeconds Time elapsed since the last update call, in seconds.
 */

/**
 * Analyze the currently playing track and produce bar-level values for visualization.
 * @returns Array of `kUiMusicBarCount` floats representing current bar values (each typically in a normalized range).
 */
namespace game::audio {

class UiMusicController {
public:
    bool initialize();
    void shutdown();
    void update(const AppState& state, bool loadingTransitionActive, float deltaSeconds);
    const UiMusicVisualState& visualState() const { return visualState_; }

private:
    enum class StoryBgmPauseMode {
        Idle,
        FadingOutForPause,
        PausedForPause,
        FadingInFromPause,
    };

    struct TrackEntry {
        std::string path;
        bool usable = true;
    };

    bool initializeAudio();
    bool loadTrackPool();
    UiMusicSurface resolveSurface(const AppState& state) const;
    void handleSurfaceTransition(UiMusicSurface previousSurface,
                                 UiMusicSurface nextSurface,
                                 const AppState& state);
    void updateStoryBgmBridge(float deltaSeconds);
    void updateMusicPlayback(float deltaSeconds,
                             UiMusicSurface desiredSurface,
                             bool loadingTransitionActive,
                             float musicVolume);
    bool startTrackForSurface(UiMusicSurface surface);
    std::size_t drawNextTrackIndex();
    void refillDrawBag();
    float randomStartFraction();
    float gainForTrackPath(const std::string& path) const;
    void refreshVisualState(float deltaSeconds);
    std::array<float, kUiMusicBarCount> analyzeCurrentTrack() const;

    BgmPlayer player_;
    std::vector<TrackEntry> tracks_;
    std::vector<std::size_t> drawBag_;
    std::mt19937 rng_{std::random_device{}()};
    UiMusicSurface lastResolvedSurface_ = UiMusicSurface::None;
    UiMusicSurface currentSurface_ = UiMusicSurface::None;
    UiMusicSurface requestedSurface_ = UiMusicSurface::None;
    UiMusicVisualState visualState_{};
    std::array<float, kUiMusicBarCount> smoothedBars_{};
    float currentGain_ = 0.0f;
    std::size_t lastTrackIndex_ = 0;
    bool hasLastTrackIndex_ = false;
    bool initialized_ = false;
    bool audioReady_ = false;
    StoryBgmPauseMode storyBgmPauseMode_ = StoryBgmPauseMode::Idle;
    bool storyBgmShouldResume_ = false;
    float storyBgmResumeTarget_ = 0.0f;
    float storyBgmFadeFrom_ = 0.0f;
    float storyBgmFadeElapsed_ = 0.0f;
    float fadeOutSeconds_ = 0.9f;
    float currentTrackGain_ = 1.0f;
    float currentAudibleGain_ = 0.0f;
    UiMusicSurface deferredStartSurface_ = UiMusicSurface::None;
};

}  // namespace game::audio
