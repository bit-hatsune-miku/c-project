#include "ui_music_controller.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <system_error>

#include "../../platform/path_resolution.h"
#include "../vn/vn_system.h"

namespace game::audio {
namespace {

constexpr const char* kUiClassicsRelativeDirectory = "assets/ui/classics";
constexpr float kUiMusicFadeInSeconds = 2.5f;
constexpr float kUiMusicFadeOutSeconds = 1.9f;
constexpr float kPauseStoryFadeOutSeconds = 1.15f;
constexpr float kPauseStoryFadeInSeconds = 1.0f;
constexpr std::size_t kAnalysisWindowSize = 2048;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kUiMusicMasterGain = 0.82f;

/**
 * @brief Extracts the filename stem from a filesystem path and returns it in lowercase.
 *
 * @param path File system path to a file.
 * @return std::string Lowercase filename stem (filename without directory or extension).
 */
std::string lowercaseTrackStem(const std::string& path) {
    std::string stem = std::filesystem::path(path).stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return stem;
}

/**
 * @brief Clamp a floating-point value to the range [0, 1].
 *
 * @param value Input value to clamp.
 * @return float `value` clamped to the range [0, 1].
 */
float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

/**
 * @brief Determines whether the application is currently showing a story-mode pause screen.
 *
 * Evaluates whether the current screen is one of the pause-related screens and the pause
 * context is `PauseContext::Story`.
 *
 * @param state Current application state to evaluate.
 * @return `true` if the screen is `PauseMenu`, `PauseConfirmExit`, or `PauseConfirmOverwriteSave`
 *         and `pauseContext` equals `PauseContext::Story`, `false` otherwise.
 */
bool isPauseStoryScreen(const AppState& state) {
    return (state.screen == ScreenState::PauseMenu ||
            state.screen == ScreenState::PauseConfirmExit ||
            state.screen == ScreenState::PauseConfirmOverwriteSave) &&
           state.pauseContext == PauseContext::Story;
}

/**
 * @brief Determines whether the current screen should be treated as a pause-child screen for story pause music behavior.
 *
 * Evaluates Settings, LoadGameMenu, and LoadConfirmDelete screens to see if they will return to PauseMenu and the pause context is Story.
 *
 * @param state Current application state used to inspect screen, return-target fields, and pause context.
 * @return true if the current screen is a pause-child (i.e., will return to PauseMenu and pause context is Story), false otherwise.
 */
bool isPauseChildScreen(const AppState& state) {
    if (state.screen == ScreenState::Settings) {
        return state.settingsReturnScreen == ScreenState::PauseMenu &&
               state.pauseContext == PauseContext::Story;
    }

    if (state.screen == ScreenState::LoadGameMenu || state.screen == ScreenState::LoadConfirmDelete) {
        return state.loadReturnScreen == ScreenState::PauseMenu &&
               state.pauseContext == PauseContext::Story;
    }

    return false;
}

/**
 * @brief Determines whether the current UI surface is ready for UI music playback.
 *
 * For regular (non-pause) surfaces this is always true. For pause-related
 * surfaces (story pause or its child screens) the surface is ready only when
 * the pause intro timer has reached its maximum threshold (within 0.0001 seconds).
 *
 * @param state Current application state used to evaluate screen and pause timing.
 * @return `true` if music may start or unpause on the current surface, `false` otherwise.
 */
bool isPauseSurfaceReadyForMusic(const AppState& state) {
    if (!(isPauseStoryScreen(state) || isPauseChildScreen(state))) {
        return true;
    }

    return state.pauseIntroTime >= kPauseIntroMaxTime - 0.0001f;
}

}  /**
 * @brief Initializes the UI music controller, preparing audio and loading the UI BGM track pool.
 *
 * Performs shutdown of any prior state, initializes the audio subsystem, loads the track pool,
 * and marks the controller as initialized.
 *
 * @return true if the controller completed initialization.
 */

bool UiMusicController::initialize() {
    shutdown();
    initializeAudio();
    loadTrackPool();
    initialized_ = true;
    return true;
}

/**
 * @brief Stops playback and resets the controller to an uninitialized default state.
 *
 * Clears loaded tracks and the draw bag, stops the audio player, resets visual/analysis
 * state and smoothed bars, and restores all gain/volume, surface, and story-BGM bridge
 * flags/fields to their default values so the controller behaves as if not initialized.
 */
void UiMusicController::shutdown() {
    player_.stop();
    tracks_.clear();
    drawBag_.clear();
    currentSurface_ = UiMusicSurface::None;
    requestedSurface_ = UiMusicSurface::None;
    lastResolvedSurface_ = UiMusicSurface::None;
    visualState_ = UiMusicVisualState{};
    smoothedBars_.fill(0.0f);
    currentGain_ = 0.0f;
    hasLastTrackIndex_ = false;
    audioReady_ = false;
    initialized_ = false;
    storyBgmPauseMode_ = StoryBgmPauseMode::Idle;
    storyBgmShouldResume_ = false;
    storyBgmResumeTarget_ = 0.0f;
    storyBgmFadeFrom_ = 0.0f;
    storyBgmFadeElapsed_ = 0.0f;
    fadeOutSeconds_ = kUiMusicFadeOutSeconds;
    currentTrackGain_ = 1.0f;
    currentAudibleGain_ = 0.0f;
    deferredStartSurface_ = UiMusicSurface::None;
}

/**
 * @brief Ensures the SDL audio subsystem is initialized and marks the controller as audio-ready.
 *
 * Attempts to initialize SDL audio if it is not already initialized. On success the controller's
 * internal audio-ready flag is set to true; on failure it is set to false and an error is reported.
 *
 * @return true if the SDL audio subsystem is available and the controller is marked audio-ready, false otherwise.
 */
bool UiMusicController::initializeAudio() {
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "[UiMusic] Audio init failed: " << SDL_GetError() << "\n";
            audioReady_ = false;
            return false;
        }
    }

    audioReady_ = true;
    return true;
}

/**
 * @brief Loads UI "classic" WAV tracks into the controller's track pool.
 *
 * Clears any existing pool and attempts to resolve and enumerate the configured
 * classics directory. Regular files with a `.wav` extension are added to the
 * controller's track list as usable entries and the draw bag is refilled.
 *
 * @return `true` if one or more tracks were successfully loaded into the pool;
 * `false` if the directory could not be resolved, directory iteration failed,
 * or no `.wav` tracks were found.
 */
bool UiMusicController::loadTrackPool() {
    tracks_.clear();
    drawBag_.clear();

    const std::string classicsDirectory = platform::path::resolvePath(kUiClassicsRelativeDirectory);
    std::error_code filesystemError;
    if (classicsDirectory.empty() ||
        !std::filesystem::exists(classicsDirectory, filesystemError) ||
        filesystemError) {
        return false;
    }

    std::vector<std::filesystem::path> candidates;
    std::filesystem::directory_iterator iterator(classicsDirectory, filesystemError);
    const std::filesystem::directory_iterator end;
    if (filesystemError) {
        return false;
    }

    for (; iterator != end; iterator.increment(filesystemError)) {
        if (filesystemError) {
            break;
        }

        const std::filesystem::directory_entry& entry = *iterator;
        if (!entry.is_regular_file(filesystemError)) {
            filesystemError.clear();
            continue;
        }

        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (extension == ".wav") {
            candidates.push_back(entry.path());
        }
    }

    std::sort(candidates.begin(), candidates.end());
    for (const std::filesystem::path& path : candidates) {
        tracks_.push_back(TrackEntry{path.string(), true});
    }

    refillDrawBag();
    return !tracks_.empty();
}

/**
 * @brief Determines which UI music surface should be active for the given application state.
 *
 * Inspects the current screen, pause context, and certain return-screen fields to map the
 * application's UI state to a UiMusicSurface value that drives UI BGM selection and playback.
 *
 * @param state Current application state used to resolve the appropriate music surface.
 * @return UiMusicSurface The resolved music surface: MainMenu, BattleSelector, PauseMenu, or None.
 */
UiMusicSurface UiMusicController::resolveSurface(const AppState& state) const {
    if (state.screen == ScreenState::MainMenu) {
        return UiMusicSurface::MainMenu;
    }

    if (state.screen == ScreenState::BossSelector) {
        return UiMusicSurface::BattleSelector;
    }

    if (state.screen == ScreenState::PartyLoader) {
        return UiMusicSurface::PartyLoader;
    }

    if (isPauseStoryScreen(state) || isPauseChildScreen(state)) {
        if (!isPauseSurfaceReadyForMusic(state)) {
            return UiMusicSurface::None;
        }
        return UiMusicSurface::PauseMenu;
    }

    if (state.screen == ScreenState::Settings &&
        state.settingsReturnScreen == ScreenState::MainMenu) {
        return UiMusicSurface::MainMenu;
    }

    if ((state.screen == ScreenState::LoadGameMenu || state.screen == ScreenState::LoadConfirmDelete) &&
        state.loadReturnScreen == ScreenState::MainMenu) {
        return UiMusicSurface::MainMenu;
    }

    return UiMusicSurface::None;
}

/**
 * @brief Handle a transition between UI music "surfaces", configuring fade timers
 *        and coordinating pause/resume behavior for visual-novel (VN) story BGM.
 *
 * Adjusts internal fade-out duration, sets a deferred start for entering the PauseMenu,
 * and manages the story-BGM bridge used when the application is in a pause-story context:
 * - When entering the PauseMenu from a non-pause surface and the app is in a pause-story
 *   screen, begins fading the VN BGM out (and records whether it should be resumed).
 * - When leaving the PauseMenu, optionally begins fading the VN BGM back in and unpauses
 *   VN playback if it was previously marked to resume; otherwise clears resume state.
 *
 * @param previousSurface The music surface that was active before the transition.
 * @param nextSurface The music surface that will be active after the transition.
 * @param state The current application state used to decide pause-story conditions
 *              and whether to resume VN story BGM.
 */
void UiMusicController::handleSurfaceTransition(UiMusicSurface previousSurface,
                                                UiMusicSurface nextSurface,
                                                const AppState& state) {
    fadeOutSeconds_ = kUiMusicFadeOutSeconds;
    deferredStartSurface_ = UiMusicSurface::None;

    if (previousSurface != UiMusicSurface::PauseMenu &&
        nextSurface == UiMusicSurface::PauseMenu) {
        deferredStartSurface_ = UiMusicSurface::PauseMenu;
    }

    if (previousSurface != UiMusicSurface::PauseMenu &&
        nextSurface == UiMusicSurface::PauseMenu &&
        isPauseStoryScreen(state)) {
        if (vn::hasBgmPlayback() && !vn::isBgmPlaybackPaused() && vn::getBgmVolume() > 0.001f) {
            storyBgmShouldResume_ = true;
            storyBgmResumeTarget_ = vn::getBgmVolume();
            storyBgmFadeFrom_ = storyBgmResumeTarget_;
            storyBgmFadeElapsed_ = 0.0f;
            storyBgmPauseMode_ = StoryBgmPauseMode::FadingOutForPause;
        } else {
            storyBgmShouldResume_ = false;
            storyBgmPauseMode_ = StoryBgmPauseMode::Idle;
        }
        return;
    }

    if (previousSurface == UiMusicSurface::PauseMenu &&
        nextSurface != UiMusicSurface::PauseMenu) {
        if (nextSurface == UiMusicSurface::None &&
            state.screen == ScreenState::Playing &&
            storyBgmShouldResume_) {
            fadeOutSeconds_ = kPauseStoryFadeOutSeconds;
            storyBgmFadeFrom_ = vn::getBgmVolume();
            vn::setBgmPaused(false);
            vn::setBgmVolume(storyBgmFadeFrom_);
            storyBgmFadeElapsed_ = 0.0f;
            storyBgmPauseMode_ = StoryBgmPauseMode::FadingInFromPause;
        } else {
            storyBgmShouldResume_ = false;
            storyBgmPauseMode_ = StoryBgmPauseMode::Idle;
            storyBgmFadeElapsed_ = 0.0f;
        }
    }
}

/**
 * @brief Advances the VN (story) background-music bridge state machine to fade the story BGM out or back in while the UI enters or leaves pause.
 *
 * Progresses the current StoryBgmPauseMode using the elapsed time to:
 * - fade the VN BGM volume to 0 and pause it when entering a pause, or
 * - interpolate the VN BGM volume back to the recorded resume target when leaving a pause.
 * The method also clears the resume intent and resets the pause mode when VN BGM is unavailable.
 *
 * @param deltaSeconds Elapsed time in seconds since the last update (non-negative values are used).
 */
void UiMusicController::updateStoryBgmBridge(float deltaSeconds) {
    switch (storyBgmPauseMode_) {
        case StoryBgmPauseMode::Idle:
        case StoryBgmPauseMode::PausedForPause:
            return;

        case StoryBgmPauseMode::FadingOutForPause: {
            if (!storyBgmShouldResume_ || !vn::hasBgmPlayback()) {
                storyBgmPauseMode_ = StoryBgmPauseMode::Idle;
                storyBgmShouldResume_ = false;
                return;
            }

            storyBgmFadeElapsed_ += std::max(deltaSeconds, 0.0f);
            const float progress = clamp01(storyBgmFadeElapsed_ / kPauseStoryFadeOutSeconds);
            const float nextVolume = storyBgmFadeFrom_ * (1.0f - progress);
            vn::setBgmVolume(nextVolume);
            if (progress >= 1.0f) {
                vn::setBgmVolume(0.0f);
                vn::setBgmPaused(true);
                storyBgmPauseMode_ = StoryBgmPauseMode::PausedForPause;
            }
            return;
        }

        case StoryBgmPauseMode::FadingInFromPause: {
            if (!storyBgmShouldResume_ || !vn::hasBgmPlayback()) {
                storyBgmPauseMode_ = StoryBgmPauseMode::Idle;
                storyBgmShouldResume_ = false;
                return;
            }

            storyBgmFadeElapsed_ += std::max(deltaSeconds, 0.0f);
            const float progress = clamp01(storyBgmFadeElapsed_ / kPauseStoryFadeInSeconds);
            const float nextVolume = storyBgmFadeFrom_ +
                (storyBgmResumeTarget_ - storyBgmFadeFrom_) * progress;
            vn::setBgmVolume(nextVolume);
            if (progress >= 1.0f) {
                vn::setBgmVolume(storyBgmResumeTarget_);
                storyBgmPauseMode_ = StoryBgmPauseMode::Idle;
                storyBgmShouldResume_ = false;
            }
            return;
        }
    }
}

/**
 * @brief Advance the UI music controller for a single frame.
 *
 * Resolves the current UI music surface from application state, handles any surface
 * transition (including bridging story BGM pause/resume), updates music playback
 * (start/stop/fade and volume) and refreshes audio-driven visual state.
 *
 * This will call initialize() if the controller has not yet been initialized.
 *
 * @param state Current application state used to determine the active UI surface and settings.
 * @param loadingTransitionActive When true, music playback is suppressed for loading transitions.
 * @param deltaSeconds Time elapsed since the last update, in seconds.
 */
void UiMusicController::update(const AppState& state, bool loadingTransitionActive, float deltaSeconds) {
    if (!initialized_) {
        initialize();
    }

    const UiMusicSurface resolvedSurface = resolveSurface(state);
    if (resolvedSurface != lastResolvedSurface_) {
        handleSurfaceTransition(lastResolvedSurface_, resolvedSurface, state);
        lastResolvedSurface_ = resolvedSurface;
    }

    updateStoryBgmBridge(deltaSeconds);
    updateMusicPlayback(
        deltaSeconds,
        loadingTransitionActive ? UiMusicSurface::None : resolvedSurface,
        loadingTransitionActive,
        std::clamp(state.settings.musicVolume, 0.0f, 1.0f));
    refreshVisualState(deltaSeconds);
}

/**
 * @brief Update UI music player's playback state, fades, and audible volume for the current frame.
 *
 * Updates the controller's requested surface, ensures a track is started when appropriate (respecting
 * deferred starts and loading transitions), advances fade-in/out toward the target surface, computes
 * the audible gain using the per-track gain and the provided music volume, applies the volume to the
 * player, and stops playback when fully faded out (optionally starting the next requested track).
 *
 * @param deltaSeconds Time elapsed since the last update, in seconds.
 * @param desiredSurface The UI music surface that should be active this frame.
 * @param loadingTransitionActive If true, suppresses starting new tracks while a loading transition is active.
 * @param musicVolume Master music volume in the range [0.0, 1.0]; used to scale the audible gain.
 */
void UiMusicController::updateMusicPlayback(float deltaSeconds,
                                            UiMusicSurface desiredSurface,
                                            bool loadingTransitionActive,
                                            float musicVolume) {
    requestedSurface_ = desiredSurface;

    if (!player_.isPlaying()) {
        currentGain_ = 0.0f;
        currentAudibleGain_ = 0.0f;
        currentSurface_ = UiMusicSurface::None;
        if (requestedSurface_ != UiMusicSurface::None && !loadingTransitionActive) {
            if (deferredStartSurface_ == requestedSurface_) {
                deferredStartSurface_ = UiMusicSurface::None;
                return;
            }
            startTrackForSurface(requestedSurface_);
        }
    }

    if (!player_.isPlaying()) {
        return;
    }

    const float targetGain =
        currentSurface_ != UiMusicSurface::None && currentSurface_ == requestedSurface_ ? 1.0f : 0.0f;
    const float fadeDuration = targetGain > currentGain_ ? kUiMusicFadeInSeconds : fadeOutSeconds_;
    const float step = fadeDuration <= 0.0f ? 1.0f : std::max(deltaSeconds, 0.0f) / fadeDuration;

    if (targetGain > currentGain_) {
        currentGain_ = std::min(targetGain, currentGain_ + step);
    } else {
        currentGain_ = std::max(targetGain, currentGain_ - step);
    }
    currentAudibleGain_ = currentGain_ * currentTrackGain_ * std::clamp(musicVolume, 0.0f, 1.0f);
    player_.setVolume(currentAudibleGain_);

    if (targetGain <= 0.0f && currentGain_ <= 0.001f) {
        player_.stop();
        currentGain_ = 0.0f;
        currentAudibleGain_ = 0.0f;
        currentSurface_ = UiMusicSurface::None;
        fadeOutSeconds_ = kUiMusicFadeOutSeconds;
        currentTrackGain_ = 1.0f;

        if (requestedSurface_ != UiMusicSurface::None && !loadingTransitionActive) {
            startTrackForSurface(requestedSurface_);
        }
    }
}

/**
 * @brief Attempt to start a randomized, playable UI music track for the given surface.
 *
 * Tries up to the number of currently usable tracks, drawing indices from the internal
 * randomized draw bag to avoid immediate repeats. On a successful play start, the controller's
 * playback state is updated (current surface and gain state) and the player begins playback
 * from a randomized start position. If a track fails to start it is marked unusable and the
 * draw bag is refreshed; the function returns `false` when audio is not ready, there are no
 * usable tracks, or no playable track could be started.
 *
 * @param surface The UI music surface to start music for; must not be `UiMusicSurface::None`.
 * @return `true` if a track was started and playback initiated, `false` otherwise.
 */
bool UiMusicController::startTrackForSurface(UiMusicSurface surface) {
    if (!audioReady_ || surface == UiMusicSurface::None) {
        return false;
    }

    const std::size_t usableTracks = static_cast<std::size_t>(std::count_if(
        tracks_.begin(), tracks_.end(), [](const TrackEntry& track) { return track.usable; }));
    if (usableTracks == 0) {
        return false;
    }

    for (std::size_t attempt = 0; attempt < usableTracks; ++attempt) {
        const std::size_t trackIndex = drawNextTrackIndex();
        if (trackIndex >= tracks_.size() || !tracks_[trackIndex].usable) {
            continue;
        }

        const float startFraction = randomStartFraction();
        if (player_.play(tracks_[trackIndex].path, 0.0f, startFraction)) {
            currentSurface_ = surface;
            currentGain_ = 0.0f;
            currentAudibleGain_ = 0.0f;
            fadeOutSeconds_ = kUiMusicFadeOutSeconds;
            currentTrackGain_ = gainForTrackPath(tracks_[trackIndex].path);
            player_.setVolume(0.0f);
            hasLastTrackIndex_ = true;
            lastTrackIndex_ = trackIndex;
            return true;
        }

        tracks_[trackIndex].usable = false;
        refillDrawBag();
    }

    currentSurface_ = UiMusicSurface::None;
    currentGain_ = 0.0f;
    return false;
}

/**
 * Selects and removes the next track index from the draw bag, refilling the bag if it is empty.
 *
 * This modifies the internal draw bag by popping the chosen index.
 *
 * @return std::size_t The selected track index, or `std::numeric_limits<std::size_t>::max()` if no index is available.
 */
std::size_t UiMusicController::drawNextTrackIndex() {
    if (drawBag_.empty()) {
        refillDrawBag();
    }
    if (drawBag_.empty()) {
        return std::numeric_limits<std::size_t>::max();
    }

    const std::size_t trackIndex = drawBag_.back();
    drawBag_.pop_back();
    return trackIndex;
}

/**
 * @brief Refills the randomized draw bag with indices of currently usable tracks.
 *
 * Clears the existing draw bag, appends the index of each track whose `usable` flag is true,
 * and returns early if no usable tracks are available. When populated, the bag is shuffled
 * using the controller's RNG. If the controller knows the last-played track and the last
 * element after shuffling would repeat it while the bag contains more than one entry, the
 * function swaps that last element with an earlier, different index to reduce immediate repeats.
 */
void UiMusicController::refillDrawBag() {
    drawBag_.clear();
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].usable) {
            drawBag_.push_back(i);
        }
    }

    if (drawBag_.empty()) {
        return;
    }

    std::shuffle(drawBag_.begin(), drawBag_.end(), rng_);
    if (hasLastTrackIndex_ && drawBag_.size() > 1 && drawBag_.back() == lastTrackIndex_) {
        for (std::size_t i = 0; i + 1 < drawBag_.size(); ++i) {
            if (drawBag_[i] != lastTrackIndex_) {
                std::swap(drawBag_[i], drawBag_.back());
                break;
            }
        }
    }
}

/**
 * @brief Choose a random start position fraction within the first half of a track.
 *
 * @return A float in the range [0.0, 0.5] representing the start fraction of the track.
 */
float UiMusicController::randomStartFraction() {
    std::uniform_real_distribution<float> distribution(0.0f, 0.5f);
    return distribution(rng_);
}

/**
 * @brief Computes the master gain multiplier to apply for a specific UI music file.
 *
 * Determines a per-track master gain based on the file's filename stem to normalize perceived loudness
 * between tracks; returns a scaled multiplier for known tracks or the default master gain otherwise.
 *
 * @param path Path to the audio file (used to derive the filename stem).
 * @return float Master gain multiplier to apply for the given track path.
 */
float UiMusicController::gainForTrackPath(const std::string& path) const {
    const std::string stem = lowercaseTrackStem(path);

    if (stem == "matryoshka" ||
        stem == "ghostrule" ||
        stem == "melt" ||
        stem == "tetoterritory" ||
        stem == "fukireta" ||
        stem == "unhappyrefrain" ||
        stem == "gochagocha") {
        return kUiMusicMasterGain * 0.58f;
    }

    if (stem == "rollinggirl" ||
        stem == "triplebaka" ||
        stem == "worldismine") {
        return kUiMusicMasterGain * 0.66f;
    }

    if (stem == "popipo") {
        return kUiMusicMasterGain * 0.84f;
    }

    return kUiMusicMasterGain;
}

/**
 * @brief Analyze the currently decoded audio and produce per-band bar levels for UI visualization.
 *
 * Computes frequency-band magnitudes from the player's decoded mono samples (using a Hann window
 * and a Goertzel-style probe across logarithmically spaced bands), applies tilting and nonlinear
 * normalization, scales results by the controller's current audible gain, and clamps values to
 * the range [0.0, 1.0]. If no decoded samples are available, the sample rate is invalid, the
 * analysis window is too small, or the audible gain is effectively zero, the function returns an
 * array of zeros.
 *
 * @return std::array<float, kUiMusicBarCount> Array of normalized bar levels (one per visual band),
 *         each in the range [0.0, 1.0], scaled by the current audible gain.
 */
std::array<float, kUiMusicBarCount> UiMusicController::analyzeCurrentTrack() const {
    std::array<float, kUiMusicBarCount> analyzed{};
    if (!player_.hasDecodedSamples() || currentAudibleGain_ <= 0.001f) {
        return analyzed;
    }

    const std::vector<float>& samples = player_.monoSamples();
    const std::size_t sampleCount = samples.size();
    const int sampleRate = player_.sampleRate();
    if (sampleCount == 0 || sampleRate <= 0) {
        return analyzed;
    }

    const std::size_t windowSize = std::min<std::size_t>(kAnalysisWindowSize, sampleCount);
    if (windowSize < 64) {
        return analyzed;
    }

    const std::size_t currentFrame = player_.currentFrame() % sampleCount;
    const std::size_t startFrame = (currentFrame + sampleCount - windowSize) % sampleCount;
    const float minFrequency = 46.0f;
    const float maxFrequency = std::min(12000.0f, static_cast<float>(sampleRate) * 0.46f);
    if (maxFrequency <= minFrequency + 1.0f) {
        return analyzed;
    }

    const auto sampleAt = [&](std::size_t offset) -> float {
        const std::size_t sampleIndex = (startFrame + offset) % sampleCount;
        const float window =
            0.5f - 0.5f * std::cos((2.0f * kPi * static_cast<float>(offset)) /
                                   static_cast<float>(windowSize - 1));
        return samples[sampleIndex] * window;
    };

    const auto goertzelMagnitude = [&](float frequency) -> float {
        if (frequency <= 0.0f || frequency >= static_cast<float>(sampleRate) * 0.5f) {
            return 0.0f;
        }

        const double omega = (2.0 * kPi * static_cast<double>(frequency)) / static_cast<double>(sampleRate);
        const double coeff = 2.0 * std::cos(omega);
        double s0 = 0.0;
        double s1 = 0.0;
        double s2 = 0.0;
        for (std::size_t i = 0; i < windowSize; ++i) {
            s0 = static_cast<double>(sampleAt(i)) + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const double power = std::max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2);
        return static_cast<float>(std::sqrt(power) / static_cast<double>(windowSize));
    };

    for (std::size_t i = 0; i < analyzed.size(); ++i) {
        const float startRatio = static_cast<float>(i) / static_cast<float>(analyzed.size());
        const float endRatio = static_cast<float>(i + 1) / static_cast<float>(analyzed.size());
        const float bandStart = minFrequency * std::pow(maxFrequency / minFrequency, startRatio);
        const float bandEnd = minFrequency * std::pow(maxFrequency / minFrequency, endRatio);
        const float center = std::sqrt(bandStart * bandEnd);
        const float probeA = std::sqrt(bandStart * center);
        const float probeB = center;
        const float probeC = std::sqrt(center * bandEnd);

        const float magnitude =
            (goertzelMagnitude(probeA) + goertzelMagnitude(probeB) + goertzelMagnitude(probeC)) / 3.0f;
        const float tilt = 1.10f - (static_cast<float>(i) / static_cast<float>(analyzed.size())) * 0.18f;
        const float normalized = std::pow(std::max(0.0f, magnitude * tilt * 14.5f), 0.74f);
        analyzed[i] = std::clamp(normalized * currentAudibleGain_, 0.0f, 1.0f);
    }

    return analyzed;
}

/**
 * @brief Updates smoothed audio-visual bars and overall UI music visibility/opacity.
 *
 * Applies per-frame attack/release smoothing to frequency-band levels produced by analyzeCurrentTrack(),
 * writes the smoothed band values into visualState_.bars, and sets visualState_.visible and
 * visualState_.opacity based on whether any bars are meaningfully active and the current audible gain.
 *
 * @param deltaSeconds Time elapsed since the last update, in seconds; used to compute attack/release blends.
 */
void UiMusicController::refreshVisualState(float deltaSeconds) {
    const std::array<float, kUiMusicBarCount> analyzed = analyzeCurrentTrack();
    const float attackBlend = deltaSeconds > 0.0f
        ? clamp01(1.0f - std::exp(-deltaSeconds * 18.0f))
        : 1.0f;
    const float releaseBlend = deltaSeconds > 0.0f
        ? clamp01(1.0f - std::exp(-deltaSeconds * 5.4f))
        : 1.0f;

    bool anyVisible = false;
    for (std::size_t i = 0; i < smoothedBars_.size(); ++i) {
        float target = analyzed[i];
        if (target > 0.0f) {
            target = std::max(target, 0.032f * currentAudibleGain_);
        }

        const float blend = target > smoothedBars_[i] ? attackBlend : releaseBlend;
        smoothedBars_[i] += (target - smoothedBars_[i]) * blend;
        if (std::fabs(smoothedBars_[i]) < 0.0005f) {
            smoothedBars_[i] = 0.0f;
        }

        visualState_.bars[i] = std::clamp(smoothedBars_[i], 0.0f, 1.0f);
        anyVisible = anyVisible || visualState_.bars[i] > 0.002f;
    }

    visualState_.visible = anyVisible;
    visualState_.opacity = anyVisible
        ? std::clamp(0.18f + currentAudibleGain_ * 0.34f, 0.0f, 0.52f)
        : 0.0f;
}

}  // namespace game::audio
