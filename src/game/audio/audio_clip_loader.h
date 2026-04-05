#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

namespace game::audio {

struct DecodedAudioClip {
    SDL_AudioSpec spec{};
    std::vector<Uint8> audioData;
    std::vector<float> monoSamples;
    std::size_t frameCount = 0;
    std::size_t bytesPerFrame = 0;
    int sampleRate = 0;
    int channels = 0;

    [[nodiscard]] bool valid() const {
        return !audioData.empty() && bytesPerFrame > 0 && frameCount > 0 && sampleRate > 0 && channels > 0;
    }
};

bool loadDecodedAudioClip(const std::string& requestedPath,
                          DecodedAudioClip& outClip,
                          std::string* resolvedPath = nullptr,
                          std::string* errorMessage = nullptr);

} // namespace game::audio
