#include "battle_bgm_controller.h"

#include <algorithm>

namespace game::audio {
namespace {

constexpr float kFadeDurationSeconds = 0.35f;

}

void BattleBgmController::attach(BgmPlayer& player) {
    player_ = &player;
}

void BattleBgmController::detach() {
    player_ = nullptr;
    currentTrackPath_.clear();
    queuedTrackPath_.clear();
    currentBaseVolume_ = 1.0f;
    queuedBaseVolume_ = 1.0f;
    masterVolume_ = 1.0f;
    fadeGain_ = 1.0f;
    paused_ = false;
    transitionState_ = TransitionState::Idle;
}

void BattleBgmController::stop() {
    queuedTrackPath_.clear();
    if (player_ != nullptr) {
        player_->stop();
    }
    currentTrackPath_.clear();
    currentBaseVolume_ = 1.0f;
    queuedBaseVolume_ = 1.0f;
    fadeGain_ = 1.0f;
    paused_ = false;
    transitionState_ = TransitionState::Idle;
}

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
    transitionState_ = TransitionState::Idle;
    if (paused_) {
        player_->pause();
    }
}

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
        applyVolume();
        return;
    }

    queuedTrackPath_ = trackPath;
    queuedBaseVolume_ = clampedBaseVolume;
    transitionState_ = TransitionState::FadingOut;
}

void BattleBgmController::update(float deltaSeconds) {
    if (player_ == nullptr) {
        return;
    }

    const float step = kFadeDurationSeconds <= 0.0f
        ? 1.0f
        : std::max(deltaSeconds, 0.0f) / kFadeDurationSeconds;

    switch (transitionState_) {
        case TransitionState::Idle:
            applyVolume();
            return;
        case TransitionState::FadingOut:
            if (!player_->isPlaying()) {
                transitionState_ = TransitionState::Idle;
                fadeGain_ = 1.0f;
                return;
            }

            fadeGain_ = std::max(0.0f, fadeGain_ - step);
            applyVolume();
            if (fadeGain_ > 0.001f) {
                return;
            }

            player_->stop();
            currentTrackPath_.clear();
            currentBaseVolume_ = 1.0f;
            if (queuedTrackPath_.empty()) {
                fadeGain_ = 1.0f;
                transitionState_ = TransitionState::Idle;
                return;
            }

            if (!player_->play(queuedTrackPath_, 0.0f)) {
                queuedTrackPath_.clear();
                queuedBaseVolume_ = 1.0f;
                fadeGain_ = 1.0f;
                transitionState_ = TransitionState::Idle;
                return;
            }

            currentTrackPath_ = queuedTrackPath_;
            currentBaseVolume_ = queuedBaseVolume_;
            queuedTrackPath_.clear();
            queuedBaseVolume_ = 1.0f;
            fadeGain_ = 0.0f;
            transitionState_ = TransitionState::FadingIn;
            if (paused_) {
                player_->pause();
            }
            applyVolume();
            return;
        case TransitionState::FadingIn:
            if (!player_->isPlaying()) {
                transitionState_ = TransitionState::Idle;
                fadeGain_ = 1.0f;
                return;
            }

            fadeGain_ = std::min(1.0f, fadeGain_ + step);
            applyVolume();
            if (fadeGain_ >= 0.999f) {
                fadeGain_ = 1.0f;
                transitionState_ = TransitionState::Idle;
            }
            return;
    }
}

void BattleBgmController::setMasterVolume(float masterVolume) {
    masterVolume_ = std::clamp(masterVolume, 0.0f, 1.0f);
    applyVolume();
}

void BattleBgmController::pause() {
    paused_ = true;
    if (player_ != nullptr && player_->isPlaying() && !player_->isPaused()) {
        player_->pause();
    }
}

void BattleBgmController::resume() {
    paused_ = false;
    if (player_ != nullptr && player_->isPlaying() && player_->isPaused()) {
        player_->resume();
    }
}

bool BattleBgmController::isPlaying() const {
    return player_ != nullptr && player_->isPlaying();
}

bool BattleBgmController::isPaused() const {
    return player_ != nullptr && player_->isPaused();
}

void BattleBgmController::applyVolume() {
    if (player_ == nullptr || !player_->isPlaying()) {
        return;
    }

    player_->setVolume(currentBaseVolume_ * fadeGain_ * masterVolume_);
}

} // namespace game::audio
