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

std::string lowercaseTrackStem(const std::string& path) {
    std::string stem = std::filesystem::path(path).stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return stem;
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

bool isPauseStoryScreen(const AppState& state) {
    return (state.screen == ScreenState::PauseMenu ||
            state.screen == ScreenState::PauseConfirmExit ||
            state.screen == ScreenState::PauseConfirmOverwriteSave) &&
           state.pauseContext == PauseContext::Story;
}

bool isPauseChildScreen(const AppState& state) {
    if (state.screen == ScreenState::Settings) {
        return state.settingsReturnScreen == ScreenState::PauseMenu &&
               state.pauseContext == PauseContext::Story;
    }

    if (state.screen == ScreenState::LoadMenu || state.screen == ScreenState::LoadConfirmDelete) {
        return state.loadReturnScreen == ScreenState::PauseMenu &&
               state.pauseContext == PauseContext::Story;
    }

    return false;
}

bool isPauseSurfaceReadyForMusic(const AppState& state) {
    if (!(isPauseStoryScreen(state) || isPauseChildScreen(state))) {
        return true;
    }

    return state.pauseIntroTime >= kPauseIntroMaxTime - 0.0001f;
}

}  // namespace

bool UiMusicController::initialize() {
    shutdown();
    initializeAudio();
    loadTrackPool();
    initialized_ = true;
    return true;
}

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
    deferredStartSurface_ = UiMusicSurface::None;
}

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

UiMusicSurface UiMusicController::resolveSurface(const AppState& state) const {
    if (state.screen == ScreenState::MainMenu) {
        return UiMusicSurface::MainMenu;
    }

    if (state.screen == ScreenState::BossSelector) {
        return UiMusicSurface::BattleSelector;
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

    if ((state.screen == ScreenState::LoadMenu || state.screen == ScreenState::LoadConfirmDelete) &&
        state.loadReturnScreen == ScreenState::MainMenu) {
        return UiMusicSurface::MainMenu;
    }

    return UiMusicSurface::None;
}

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
        loadingTransitionActive);
    refreshVisualState(deltaSeconds);
}

void UiMusicController::updateMusicPlayback(float deltaSeconds,
                                            UiMusicSurface desiredSurface,
                                            bool loadingTransitionActive) {
    requestedSurface_ = desiredSurface;

    if (!player_.isPlaying()) {
        currentGain_ = 0.0f;
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
    player_.setVolume(currentGain_ * currentTrackGain_);

    if (targetGain <= 0.0f && currentGain_ <= 0.001f) {
        player_.stop();
        currentGain_ = 0.0f;
        currentSurface_ = UiMusicSurface::None;
        fadeOutSeconds_ = kUiMusicFadeOutSeconds;
        currentTrackGain_ = 1.0f;

        if (requestedSurface_ != UiMusicSurface::None && !loadingTransitionActive) {
            startTrackForSurface(requestedSurface_);
        }
    }
}

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

float UiMusicController::randomStartFraction() {
    std::uniform_real_distribution<float> distribution(0.0f, 0.5f);
    return distribution(rng_);
}

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

std::array<float, kUiMusicBarCount> UiMusicController::analyzeCurrentTrack() const {
    std::array<float, kUiMusicBarCount> analyzed{};
    if (!player_.hasDecodedSamples() || currentGain_ <= 0.001f) {
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
        analyzed[i] = std::clamp(normalized * currentGain_, 0.0f, 1.0f);
    }

    return analyzed;
}

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
            target = std::max(target, 0.032f * currentGain_);
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
        ? std::clamp(0.18f + currentGain_ * 0.34f, 0.0f, 0.52f)
        : 0.0f;
}

}  // namespace game::audio
