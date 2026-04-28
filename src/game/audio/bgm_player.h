#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <SDL2/SDL.h>

#include "audio_clip_loader.h"

/**
 * Background-music player that feeds audio to SDL2 via an audio callback.
 * Loads an entire decoded clip into memory when playback starts and can either
 * loop continuously or stop at the end of the clip.
 */

/**
 * Destructor that stops playback and releases resources.
 */

/**
 * Load the audio clip at wavPath, begin playback, and initialize playback state.
 *
 * @param wavPath Path to the WAV file to load.
 * @param volume Initial playback volume, clamped to the range [0, 1].
 * @param startFraction Fractional start position within the track, clamped to [0, 1]; 0.0 starts at the beginning, 1.0 at the end.
 * @returns `true` if the clip was loaded and the audio device opened successfully, `false` on failure.
 */

/**
 * Load the audio clip at wavPath, begin playback, and initialize playback state.
 *
 * @param wavPath Path to the WAV file to load.
 * @param volume Initial playback volume, clamped to the range [0, 1].
 * @param startSeconds Start position within the track in seconds. Negative values start at 0. If the requested
 *        offset is at or beyond the track duration, playback starts at 0.
 * @returns `true` if the clip was loaded and the audio device opened successfully, `false` on failure.
 */

/**
 * Stop playback, close the audio device (if open), and clear all loaded/decoded audio state.
 */

/**
 * Set playback volume; value is clamped to [0, 1]. This method is safe to call from other threads while playing.
 *
 * @param volume Desired volume level.
 */

/**
 * Pause playback if currently playing.
 */

/**
 * Resume playback if currently paused.
 */

/**
 * @returns `true` if an audio device is open and playback is active, `false` otherwise.
 */

/**
 * @returns `true` if playback is paused, `false` otherwise.
 */

/**
 * @returns The index of the current audio frame within the decoded frames.
 */

/**
 * @returns The total number of audio frames derived from the loaded clip, or 0 if none.
 */

/**
 * @returns The sample rate (Hz) of the loaded WAV, or 0 if none.
 */

/**
 * @returns The channel count used for decoding (at least 1).
 */

/**
 * @returns Reference to the decoded mono float samples (one value per frame). May be empty if decoding failed or no data is loaded.
 */

/**
 * @returns `true` if decoded mono samples, a positive frame count, and a valid sample rate are available, `false` otherwise.
 */

/**
 * SDL audio callback entry point that forwards to the instance's fillStream.
 *
 * @param userdata Pointer to the BgmPlayer instance.
 * @param stream Destination audio buffer provided by SDL.
 * @param len Length in bytes of the destination buffer.
 */

/**
 * Fill SDL's output buffer by mixing the loaded audio data into stream.
 * Updates the internal play position and current frame as audio is consumed.
 *
 * @param stream Destination audio buffer provided by SDL.
 * @param len Length in bytes of the destination buffer.
 */
namespace game::audio {

// BGM player backed by SDL2's audio callback.
// Loads the full decoded clip into memory once, then either loops it forever
// or stops at the end of the clip until stop() is called.
// setVolume() is thread-safe and can be called while playing.
class BgmPlayer {
public:
    ~BgmPlayer() {
        stop();
    }

    // Load wavPath and begin playback immediately at volume [0, 1].
    bool play(const std::string& wavPath,
              float volume = 1.0f,
              float startFraction = 0.0f,
              bool loop = true) {
        stop();

        DecodedAudioClip clip;
        if (!loadDecodedAudioClip(wavPath, clip)) {
            return false;
        }

        const float clampedStartFraction = std::clamp(startFraction, 0.0f, 1.0f);
        std::size_t startFrame = static_cast<std::size_t>(std::floor(clampedStartFraction * static_cast<float>(clip.frameCount)));
        if (clip.frameCount > 0) {
            startFrame = std::min(startFrame, clip.frameCount - 1);
        } else {
            startFrame = 0;
        }

        return startClipPlayback(std::move(clip), volume, startFrame, loop);
    }

    bool playAtTime(const std::string& wavPath, float volume, float startSeconds, bool loop = true) {
        stop();

        DecodedAudioClip clip;
        if (!loadDecodedAudioClip(wavPath, clip)) {
            return false;
        }

        const float clampedStartSeconds = std::max(0.0f, startSeconds);
        std::size_t startFrame = 0;
        if (clip.frameCount > 0 && clip.sampleRate > 0) {
            const float durationSeconds = static_cast<float>(clip.frameCount) / static_cast<float>(clip.sampleRate);
            if (clampedStartSeconds > 0.0f && clampedStartSeconds < durationSeconds) {
                startFrame = static_cast<std::size_t>(std::floor(clampedStartSeconds * static_cast<float>(clip.sampleRate)));
                startFrame = std::min(startFrame, clip.frameCount - 1);
            }
        }

        return startClipPlayback(std::move(clip), volume, startFrame, loop);
    }

    bool playClip(const DecodedAudioClip& clip,
                  float volume = 1.0f,
                  float startFraction = 0.0f,
                  bool loop = true) {
        stop();

        if (!clip.valid()) {
            return false;
        }

        const float clampedStartFraction = std::clamp(startFraction, 0.0f, 1.0f);
        std::size_t startFrame = static_cast<std::size_t>(
            std::floor(clampedStartFraction * static_cast<float>(clip.frameCount)));
        if (clip.frameCount > 0) {
            startFrame = std::min(startFrame, clip.frameCount - 1);
        } else {
            startFrame = 0;
        }

        DecodedAudioClip copy = clip;
        return startClipPlayback(std::move(copy), volume, startFrame, loop);
    }

    bool playClipAtTime(const DecodedAudioClip& clip,
                        float volume,
                        float startSeconds,
                        bool loop = true) {
        stop();

        if (!clip.valid()) {
            return false;
        }

        const float clampedStartSeconds = std::max(0.0f, startSeconds);
        std::size_t startFrame = 0;
        if (clip.frameCount > 0 && clip.sampleRate > 0) {
            const float durationSeconds =
                static_cast<float>(clip.frameCount) / static_cast<float>(clip.sampleRate);
            if (clampedStartSeconds > 0.0f && clampedStartSeconds < durationSeconds) {
                startFrame = static_cast<std::size_t>(
                    std::floor(clampedStartSeconds * static_cast<float>(clip.sampleRate)));
                startFrame = std::min(startFrame, clip.frameCount - 1);
            }
        }

        DecodedAudioClip copy = clip;
        return startClipPlayback(std::move(copy), volume, startFrame, loop);
    }

    // Stop playback and release audio data.
    void stop() {
        if (device_ != 0) {
            // SDL_CloseAudioDevice waits for any in-flight callback to finish,
            // so it is safe to clear audioData_ immediately after.
            SDL_CloseAudioDevice(device_);
            device_ = 0;
        }
        audioData_.clear();
        monoSamples_.clear();
        spec_ = SDL_AudioSpec{};
        playPos_ = 0;
        paused_ = false;
        frameCount_ = 0;
        sampleRate_ = 0;
        channels_ = 0;
        bytesPerFrame_ = 0;
        currentFrame_.store(0, std::memory_order_relaxed);
        finished_.store(false, std::memory_order_relaxed);
        loop_ = true;
    }

    // Change volume [0, 1] while playing. Thread-safe.
    void setVolume(float volume) {
        volume_.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    void pause() {
        if (device_ == 0 || paused_) {
            return;
        }
        SDL_PauseAudioDevice(device_, 1);
        paused_ = true;
    }

    void resume() {
        if (device_ == 0 || !paused_) {
            return;
        }
        SDL_PauseAudioDevice(device_, 0);
        paused_ = false;
    }

    bool isPlaying() const { return device_ != 0; }
    bool isPaused() const { return device_ != 0 && paused_; }
    std::size_t currentFrame() const { return currentFrame_.load(std::memory_order_relaxed); }
    std::size_t frameCount() const { return frameCount_; }
    int sampleRate() const { return sampleRate_; }
    int channelCount() const { return channels_; }
    const std::vector<float>& monoSamples() const { return monoSamples_; }
    bool hasDecodedSamples() const { return !monoSamples_.empty() && frameCount_ > 0 && sampleRate_ > 0; }
    bool isFinished() const { return finished_.load(std::memory_order_relaxed); }
    float durationSeconds() const {
        if (sampleRate_ <= 0 || frameCount_ == 0) {
            return 0.0f;
        }
        return static_cast<float>(frameCount_) / static_cast<float>(sampleRate_);
    }
    float playbackSeconds() const {
        if (sampleRate_ <= 0) {
            return 0.0f;
        }
        return static_cast<float>(std::min(currentFrame(), frameCount_)) / static_cast<float>(sampleRate_);
    }

private:
    bool startClipPlayback(DecodedAudioClip&& clip, float volume, std::size_t startFrame, bool loop) {
        if (!clip.valid()) {
            return false;
        }

        audioData_ = std::move(clip.audioData);
        monoSamples_ = std::move(clip.monoSamples);
        spec_ = clip.spec;
        channels_ = clip.channels;
        bytesPerFrame_ = clip.bytesPerFrame;
        frameCount_ = clip.frameCount;
        sampleRate_ = clip.sampleRate;

        playPos_ = static_cast<Uint32>(startFrame * bytesPerFrame_);
        currentFrame_.store(startFrame, std::memory_order_relaxed);
        volume_.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
        finished_.store(false, std::memory_order_relaxed);
        loop_ = loop;

        SDL_AudioSpec desired = spec_;
        desired.callback = &BgmPlayer::audioCallback;
        desired.userdata = this;

        device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
        if (device_ == 0) {
            stop();
            return false;
        }

        paused_ = false;
        SDL_PauseAudioDevice(device_, 0);
        return true;
    }

    static void audioCallback(void* userdata, Uint8* stream, int len) {
        static_cast<BgmPlayer*>(userdata)->fillStream(stream, static_cast<Uint32>(len));
    }

    void fillStream(Uint8* stream, Uint32 len) {
        SDL_memset(stream, 0, len);
        if (audioData_.empty()) return;

        const int mixVol = std::clamp(
            static_cast<int>(std::lround(volume_.load(std::memory_order_relaxed) * SDL_MIX_MAXVOLUME)),
            0, SDL_MIX_MAXVOLUME
        );

        const Uint32 dataSize = static_cast<Uint32>(audioData_.size());
        Uint32 remaining = len;
        Uint8* dst = stream;

        while (remaining > 0) {
            if (!loop_ && playPos_ >= dataSize) {
                finished_.store(true, std::memory_order_relaxed);
                currentFrame_.store(frameCount_, std::memory_order_relaxed);
                break;
            }

            const Uint32 available = dataSize - playPos_;
            const Uint32 toCopy = std::min(remaining, available);
            SDL_MixAudioFormat(dst, audioData_.data() + playPos_, spec_.format, toCopy, mixVol);
            dst += toCopy;
            playPos_ += toCopy;
            remaining -= toCopy;
            if (playPos_ >= dataSize) {
                if (loop_) {
                    playPos_ = 0;
                } else {
                    finished_.store(true, std::memory_order_relaxed);
                    currentFrame_.store(frameCount_, std::memory_order_relaxed);
                    break;
                }
            }
            if (bytesPerFrame_ != 0 && frameCount_ != 0) {
                currentFrame_.store(
                    std::min<std::size_t>(playPos_ / bytesPerFrame_, frameCount_ - 1),
                    std::memory_order_relaxed);
            }
        }
    }

    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec spec_{};
    std::vector<Uint8> audioData_;
    std::vector<float> monoSamples_;
    Uint32 playPos_ = 0;           // written only by the audio callback thread
    std::atomic<float> volume_{1.0f};
    std::atomic<std::size_t> currentFrame_{0};
    std::atomic<bool> finished_{false};
    bool paused_ = false;
    bool loop_ = true;
    std::size_t frameCount_ = 0;
    std::size_t bytesPerFrame_ = 0;
    int sampleRate_ = 0;
    int channels_ = 0;
};

} // namespace game::audio
