#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "audio_clip_loader.h"

namespace game::audio {

class WavOneShotPlayer {
public:
    struct PlaybackHandle {
        std::uint64_t id = 0;

        [[nodiscard]] bool valid() const {
            return id != 0;
        }
    };

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

    bool playWavOneShot(const std::string& wavPath, float volume = 1.0f, bool replaceExisting = true) {
        return playTrackedWavOneShot(wavPath, volume, replaceExisting).has_value();
    }

    std::optional<PlaybackHandle> playTrackedWavOneShot(const std::string& wavPath,
                                                        float volume = 1.0f,
                                                        bool replaceExisting = true) {
        cleanupFinishedPlayback();

        DecodedAudioClip clip;
        std::string resolvedPath;
        if (!loadDecodedAudioClip(wavPath, clip, &resolvedPath)) {
            return std::nullopt;
        }
        const std::string playbackPath = resolvedPath.empty() ? wavPath : resolvedPath;

        if (replaceExisting) {
            for (auto it = activePlayback_.begin(); it != activePlayback_.end(); ) {
                if (it->wavPath == playbackPath) {
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

        SDL_AudioDeviceID device = SDL_OpenAudioDevice(nullptr, 0, &clip.spec, nullptr, 0);
        if (device == 0) {
            return std::nullopt;
        }

        const int mixVolume = std::clamp(static_cast<int>(std::lround(std::clamp(volume, 0.0f, 1.0f) * SDL_MIX_MAXVOLUME)),
                                         0,
                                         SDL_MIX_MAXVOLUME);
        const Uint32 audioLength = static_cast<Uint32>(clip.audioData.size());
        std::vector<Uint8> playbackBuffer(static_cast<size_t>(audioLength), 0);
        SDL_MixAudioFormat(playbackBuffer.data(), clip.audioData.data(), clip.spec.format, audioLength, mixVolume);
        const int queueResult = SDL_QueueAudio(device, playbackBuffer.data(), audioLength);
        if (queueResult != 0) {
            SDL_CloseAudioDevice(device);
            return std::nullopt;
        }

        SDL_PauseAudioDevice(device, 0);
        const PlaybackHandle handle{nextPlaybackId_++};
        activePlayback_.push_back(ActivePlayback{handle.id, device, playbackPath});
        return handle;
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

    void stopPlayback(PlaybackHandle handle) {
        if (!handle.valid()) {
            return;
        }

        for (auto it = activePlayback_.begin(); it != activePlayback_.end(); ) {
            if (it->id == handle.id) {
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

    void stopAllPlayback() {
        for (const ActivePlayback& playback : activePlayback_) {
            if (playback.device != 0) {
                SDL_ClearQueuedAudio(playback.device);
                SDL_CloseAudioDevice(playback.device);
            }
        }
        activePlayback_.clear();
    }

    bool isPlaying(const std::string& wavPath) const {
        for (const ActivePlayback& playback : activePlayback_) {
            if (playback.device != 0 && playback.wavPath == wavPath) {
                return true;
            }
        }
        return false;
    }

    bool isPlaying(PlaybackHandle handle) const {
        if (!handle.valid()) {
            return false;
        }

        for (const ActivePlayback& playback : activePlayback_) {
            if (playback.device != 0 && playback.id == handle.id) {
                return true;
            }
        }
        return false;
    }

private:
    struct ActivePlayback {
        std::uint64_t id = 0;
        SDL_AudioDeviceID device = 0;
        std::string wavPath;
    };

    std::vector<ActivePlayback> activePlayback_;
    std::uint64_t nextPlaybackId_ = 1;
};

} // namespace game::audio
