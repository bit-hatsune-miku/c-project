#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

namespace game::audio {

// Looping BGM player backed by SDL2's audio callback.
// Loads the full WAV into memory once, then loops it forever via the
// audio callback until stop() is called.
// setVolume() is thread-safe and can be called while playing.
class BgmPlayer {
public:
    ~BgmPlayer() {
        stop();
    }

    // Load wavPath and begin looping immediately at volume [0, 1].
    bool play(const std::string& wavPath, float volume = 1.0f, float startFraction = 0.0f) {
        stop();

        SDL_AudioSpec spec{};
        Uint8* buffer = nullptr;
        Uint32 length = 0;
        if (SDL_LoadWAV(wavPath.c_str(), &spec, &buffer, &length) == nullptr) {
            return false;
        }

        audioData_.assign(buffer, buffer + length);
        SDL_FreeWAV(buffer);
        spec_ = spec;
        channels_ = std::max<int>(1, spec_.channels);
        const std::size_t bytesPerSample = static_cast<std::size_t>(SDL_AUDIO_BITSIZE(spec_.format) / 8);
        bytesPerFrame_ = bytesPerSample * static_cast<std::size_t>(channels_);
        if (bytesPerSample == 0 || bytesPerFrame_ == 0 || audioData_.size() < bytesPerFrame_) {
            audioData_.clear();
            return false;
        }

        frameCount_ = audioData_.size() / bytesPerFrame_;
        sampleRate_ = spec_.freq;
        monoSamples_ = decodeMonoSamples(audioData_, spec_, frameCount_, bytesPerFrame_);

        const float clampedStartFraction = std::clamp(startFraction, 0.0f, 1.0f);
        std::size_t startFrame = static_cast<std::size_t>(std::floor(clampedStartFraction * static_cast<float>(frameCount_)));
        if (frameCount_ > 0) {
            startFrame = std::min(startFrame, frameCount_ - 1);
        } else {
            startFrame = 0;
        }

        playPos_ = static_cast<Uint32>(startFrame * bytesPerFrame_);
        currentFrame_.store(startFrame, std::memory_order_relaxed);
        volume_.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);

        SDL_AudioSpec desired = spec;
        desired.callback = &BgmPlayer::audioCallback;
        desired.userdata = this;

        device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
        if (device_ == 0) {
            audioData_.clear();
            monoSamples_.clear();
            frameCount_ = 0;
            sampleRate_ = 0;
            channels_ = 0;
            bytesPerFrame_ = 0;
            currentFrame_.store(0, std::memory_order_relaxed);
            return false;
        }

        paused_ = false;
        SDL_PauseAudioDevice(device_, 0);
        return true;
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
        playPos_ = 0;
        paused_ = false;
        frameCount_ = 0;
        sampleRate_ = 0;
        channels_ = 0;
        bytesPerFrame_ = 0;
        currentFrame_.store(0, std::memory_order_relaxed);
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

private:
    static float readSampleNormalized(const Uint8* data, SDL_AudioFormat format) {
        const int bits = SDL_AUDIO_BITSIZE(format);
        const bool isFloat = SDL_AUDIO_ISFLOAT(format) != 0;
        const bool isSigned = SDL_AUDIO_ISSIGNED(format) != 0;
        const bool isBigEndian = SDL_AUDIO_ISBIGENDIAN(format) != 0;

        if (bits == 8) {
            if (isSigned) {
                return std::clamp(static_cast<float>(*reinterpret_cast<const int8_t*>(data)) / 128.0f, -1.0f, 1.0f);
            }
            return std::clamp((static_cast<float>(*data) - 128.0f) / 128.0f, -1.0f, 1.0f);
        }

        if (bits == 16) {
            const uint16_t raw = isBigEndian
                ? static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) | static_cast<uint16_t>(data[1]))
                : static_cast<uint16_t>((static_cast<uint16_t>(data[1]) << 8U) | static_cast<uint16_t>(data[0]));
            if (isSigned) {
                int16_t sample = 0;
                std::memcpy(&sample, &raw, sizeof(sample));
                return std::clamp(static_cast<float>(sample) / 32768.0f, -1.0f, 1.0f);
            }
            return std::clamp((static_cast<float>(raw) - 32768.0f) / 32768.0f, -1.0f, 1.0f);
        }

        if (bits == 32 && isFloat) {
            uint32_t raw = isBigEndian
                ? (static_cast<uint32_t>(data[0]) << 24U) |
                  (static_cast<uint32_t>(data[1]) << 16U) |
                  (static_cast<uint32_t>(data[2]) << 8U) |
                  static_cast<uint32_t>(data[3])
                : (static_cast<uint32_t>(data[3]) << 24U) |
                  (static_cast<uint32_t>(data[2]) << 16U) |
                  (static_cast<uint32_t>(data[1]) << 8U) |
                  static_cast<uint32_t>(data[0]);
            float sample = 0.0f;
            std::memcpy(&sample, &raw, sizeof(sample));
            return std::clamp(sample, -1.0f, 1.0f);
        }

        if (bits == 32) {
            const uint32_t raw = isBigEndian
                ? (static_cast<uint32_t>(data[0]) << 24U) |
                  (static_cast<uint32_t>(data[1]) << 16U) |
                  (static_cast<uint32_t>(data[2]) << 8U) |
                  static_cast<uint32_t>(data[3])
                : (static_cast<uint32_t>(data[3]) << 24U) |
                  (static_cast<uint32_t>(data[2]) << 16U) |
                  (static_cast<uint32_t>(data[1]) << 8U) |
                  static_cast<uint32_t>(data[0]);
            if (isSigned) {
                int32_t sample = 0;
                std::memcpy(&sample, &raw, sizeof(sample));
                return std::clamp(static_cast<float>(sample) / 2147483648.0f, -1.0f, 1.0f);
            }
            return std::clamp((static_cast<double>(raw) - 2147483648.0) / 2147483648.0, -1.0, 1.0);
        }

        return 0.0f;
    }

    static std::vector<float> decodeMonoSamples(const std::vector<Uint8>& audioData,
                                                const SDL_AudioSpec& spec,
                                                std::size_t frameCount,
                                                std::size_t bytesPerFrame) {
        std::vector<float> monoSamples;
        monoSamples.reserve(frameCount);

        const std::size_t bytesPerSample = static_cast<std::size_t>(SDL_AUDIO_BITSIZE(spec.format) / 8);
        if (bytesPerSample == 0) {
            return monoSamples;
        }

        const std::size_t channelCount = std::max<int>(1, spec.channels);
        for (std::size_t frame = 0; frame < frameCount; ++frame) {
            const Uint8* framePtr = audioData.data() + frame * bytesPerFrame;
            float mixed = 0.0f;
            for (std::size_t channel = 0; channel < channelCount; ++channel) {
                mixed += readSampleNormalized(framePtr + channel * bytesPerSample, spec.format);
            }
            monoSamples.push_back(mixed / static_cast<float>(channelCount));
        }

        return monoSamples;
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
            const Uint32 available = dataSize - playPos_;
            const Uint32 toCopy = std::min(remaining, available);
            SDL_MixAudioFormat(dst, audioData_.data() + playPos_, spec_.format, toCopy, mixVol);
            dst += toCopy;
            playPos_ += toCopy;
            remaining -= toCopy;
            if (playPos_ >= dataSize) {
                playPos_ = 0; // loop
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
    bool paused_ = false;
    std::size_t frameCount_ = 0;
    std::size_t bytesPerFrame_ = 0;
    int sampleRate_ = 0;
    int channels_ = 0;
};

} // namespace game::audio
