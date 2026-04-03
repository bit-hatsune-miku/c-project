#pragma once

#include <string>

#include "bgm_player.h"

namespace game::audio {

class BattleBgmController {
public:
    void attach(BgmPlayer& player);
    void detach();

    void stop();
    void fadeOutAndStop(float fadeDurationSeconds = 1.2f);
    void playImmediate(const std::string& trackPath, float baseVolume);
    void playWithFadeIn(const std::string& trackPath, float baseVolume, float fadeDurationSeconds = 0.35f);
    void requestTrack(const std::string& trackPath, float baseVolume);
    void update(float deltaSeconds);

    void setMasterVolume(float masterVolume);
    void pause();
    void resume();

    bool isPlaying() const;
    bool isPaused() const;

private:
    enum class TransitionState {
        Idle,
        FadingOut,
        FadingIn
    };

    void applyVolume();

    BgmPlayer* player_ = nullptr;
    std::string currentTrackPath_;
    float currentBaseVolume_ = 1.0f;
    std::string queuedTrackPath_;
    float queuedBaseVolume_ = 1.0f;
    float masterVolume_ = 1.0f;
    float fadeGain_ = 1.0f;
    float fadeDurationSeconds_ = 0.35f;
    float transitionElapsedSeconds_ = 0.0f;
    float transitionStartGain_ = 1.0f;
    bool paused_ = false;
    TransitionState transitionState_ = TransitionState::Idle;
};

} // namespace game::audio
