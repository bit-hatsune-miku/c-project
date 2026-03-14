#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
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
    bool play(const std::string& wavPath, float volume = 1.0f) {
        stop();

        SDL_AudioSpec spec{};
        Uint8* buffer = nullptr;
        Uint32 length = 0;
        if (SDL_LoadWAV(wavPath.c_str(), &spec, &buffer, &length) == nullptr) {
            return false;
        }

        audioData_.assign(buffer, buffer + length);
        SDL_FreeWAV(buffer);
        playPos_ = 0;
        volume_.store(std::clamp(volume, 0.0f, 1.0f));
        spec_ = spec;

        SDL_AudioSpec desired = spec;
        desired.callback = &BgmPlayer::audioCallback;
        desired.userdata = this;

        device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
        if (device_ == 0) {
            audioData_.clear();
            return false;
        }

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
        playPos_ = 0;
    }

    // Change volume [0, 1] while playing. Thread-safe.
    void setVolume(float volume) {
        volume_.store(std::clamp(volume, 0.0f, 1.0f));
    }

    bool isPlaying() const { return device_ != 0; }

private:
    static void audioCallback(void* userdata, Uint8* stream, int len) {
        static_cast<BgmPlayer*>(userdata)->fillStream(stream, static_cast<Uint32>(len));
    }

    void fillStream(Uint8* stream, Uint32 len) {
        SDL_memset(stream, 0, len);
        if (audioData_.empty()) return;

        const int mixVol = std::clamp(
            static_cast<int>(std::lround(volume_.load() * SDL_MIX_MAXVOLUME)),
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
        }
    }

    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec spec_{};
    std::vector<Uint8> audioData_;
    Uint32 playPos_ = 0;           // written only by the audio callback thread
    std::atomic<float> volume_{1.0f};
};

} // namespace game::audio
