#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

namespace game::audio {

class WavOneShotPlayer {
public:
    void cleanupFinishedPlayback() {
        std::vector<ActivePlayback> stillPlaying;
        stillPlaying.reserve(activePlayback_.size());
        for (const ActivePlayback& playback : activePlayback_) {
            if (playback.device == 0) {
                continue;
            }
            if (SDL_GetQueuedAudioSize(playback.device) == 0) {
                SDL_CloseAudioDevice(playback.device);
            } else {
                stillPlaying.push_back(playback);
            }
        }
        activePlayback_.swap(stillPlaying);
    }

    void shutdown() {
        for (const ActivePlayback& playback : activePlayback_) {
            if (playback.device != 0) {
                SDL_CloseAudioDevice(playback.device);
            }
        }
        activePlayback_.clear();
    }

    bool playWavOneShot(const std::string& wavPath, float volume = 1.0f) {
        cleanupFinishedPlayback();

        for (auto it = activePlayback_.begin(); it != activePlayback_.end(); ) {
            if (it->wavPath == wavPath) {
                if (it->device != 0) {
                    SDL_ClearQueuedAudio(it->device);
                    SDL_CloseAudioDevice(it->device);
                }
                it = activePlayback_.erase(it);
            } else {
                ++it;
            }
        }

        SDL_AudioSpec wavSpec{};
        Uint8* wavBuffer = nullptr;
        Uint32 wavLength = 0;
        if (SDL_LoadWAV(wavPath.c_str(), &wavSpec, &wavBuffer, &wavLength) == nullptr) {
            return false;
        }

        SDL_AudioDeviceID device = SDL_OpenAudioDevice(nullptr, 0, &wavSpec, nullptr, 0);
        if (device == 0) {
            SDL_FreeWAV(wavBuffer);
            return false;
        }

        const int mixVolume = std::clamp(static_cast<int>(std::lround(std::clamp(volume, 0.0f, 1.0f) * SDL_MIX_MAXVOLUME)),
                                         0,
                                         SDL_MIX_MAXVOLUME);
        std::vector<Uint8> playbackBuffer(static_cast<size_t>(wavLength), 0);
        SDL_MixAudioFormat(playbackBuffer.data(), wavBuffer, wavSpec.format, wavLength, mixVolume);
        const int queueResult = SDL_QueueAudio(device, playbackBuffer.data(), wavLength);
        SDL_FreeWAV(wavBuffer);
        if (queueResult != 0) {
            SDL_CloseAudioDevice(device);
            return false;
        }

        SDL_PauseAudioDevice(device, 0);
        activePlayback_.push_back(ActivePlayback{device, wavPath});
        return true;
    }

    void stopPlayback(const std::string& wavPath) {
        for (auto it = activePlayback_.begin(); it != activePlayback_.end(); ) {
            if (it->wavPath == wavPath) {
                if (it->device != 0) {
                    SDL_ClearQueuedAudio(it->device);
                    SDL_CloseAudioDevice(it->device);
                }
                it = activePlayback_.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool isPlaying(const std::string& wavPath) const {
        for (const ActivePlayback& playback : activePlayback_) {
            if (playback.device != 0 && playback.wavPath == wavPath) {
                return true;
            }
        }
        return false;
    }

private:
    struct ActivePlayback {
        SDL_AudioDeviceID device = 0;
        std::string wavPath;
    };

    std::vector<ActivePlayback> activePlayback_;
};

} // namespace game::audio
