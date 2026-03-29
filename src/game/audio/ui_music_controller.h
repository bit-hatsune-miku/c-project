#pragma once

#include <array>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "../../GameMenu/menu_shared.h"
#include "bgm_player.h"
#include "ui_music_types.h"

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
