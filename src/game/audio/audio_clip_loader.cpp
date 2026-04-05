#include "audio_clip_loader.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "../../platform/path_resolution.h"

#if defined(GAME_AUDIO_ENABLE_OPUS)
#if __has_include(<opus/opusfile.h>)
#include <opus/opusfile.h>
#elif __has_include(<opusfile.h>)
#include <opusfile.h>
#else
#error "GAME_AUDIO_ENABLE_OPUS requires opusfile headers"
#endif
#endif

namespace game::audio {
namespace {

constexpr int kOpusSampleRate = 48000;
constexpr int kOpusChannels = 2;
constexpr int kOpusDecodeChunkFrames = 5760;

void setError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

float readSampleNormalized(const Uint8* data, SDL_AudioFormat format) {
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
        return static_cast<float>(
            std::clamp((static_cast<double>(raw) - 2147483648.0) / 2147483648.0, -1.0, 1.0));
    }

    return 0.0f;
}

std::vector<float> decodeMonoSamples(const std::vector<Uint8>& audioData,
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

bool finalizeDecodedClip(DecodedAudioClip& clip, std::string* errorMessage) {
    clip.channels = std::max<int>(1, clip.spec.channels);
    const std::size_t bytesPerSample = static_cast<std::size_t>(SDL_AUDIO_BITSIZE(clip.spec.format) / 8);
    clip.bytesPerFrame = bytesPerSample * static_cast<std::size_t>(clip.channels);
    if (bytesPerSample == 0 || clip.bytesPerFrame == 0 || clip.audioData.size() < clip.bytesPerFrame) {
        setError(errorMessage, "Decoded clip did not contain a playable PCM buffer.");
        clip = DecodedAudioClip{};
        return false;
    }

    clip.sampleRate = clip.spec.freq;
    clip.frameCount = clip.audioData.size() / clip.bytesPerFrame;
    clip.monoSamples = decodeMonoSamples(clip.audioData, clip.spec, clip.frameCount, clip.bytesPerFrame);
    return clip.valid();
}

bool loadWavClip(const std::string& resolvedPath, DecodedAudioClip& outClip, std::string* errorMessage) {
    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 length = 0;
    if (SDL_LoadWAV(resolvedPath.c_str(), &spec, &buffer, &length) == nullptr) {
        setError(errorMessage, SDL_GetError());
        return false;
    }

    outClip = DecodedAudioClip{};
    outClip.spec = spec;
    outClip.audioData.assign(buffer, buffer + length);
    SDL_FreeWAV(buffer);
    return finalizeDecodedClip(outClip, errorMessage);
}

bool loadOpusClip(const std::string& resolvedPath, DecodedAudioClip& outClip, std::string* errorMessage) {
#if defined(GAME_AUDIO_ENABLE_OPUS)
    int openError = 0;
    OggOpusFile* file = op_open_file(resolvedPath.c_str(), &openError);
    if (file == nullptr) {
        setError(errorMessage, "opusfile could not open the clip.");
        return false;
    }

    DecodedAudioClip clip;
    clip.spec.freq = kOpusSampleRate;
    clip.spec.format = AUDIO_S16SYS;
    clip.spec.channels = kOpusChannels;
    clip.spec.samples = 4096;
    clip.spec.callback = nullptr;
    clip.spec.userdata = nullptr;
    clip.spec.silence = 0;
    clip.spec.padding = 0;
    clip.spec.size = 0;

    std::vector<opus_int16> decodeBuffer(static_cast<std::size_t>(kOpusDecodeChunkFrames) * kOpusChannels);
    for (;;) {
        const int decodedFrames = op_read_stereo(file, decodeBuffer.data(), static_cast<int>(decodeBuffer.size()));
        if (decodedFrames == 0) {
            break;
        }
        if (decodedFrames < 0) {
            op_free(file);
            setError(errorMessage, "opusfile failed while decoding the clip.");
            return false;
        }

        const std::size_t sampleCount = static_cast<std::size_t>(decodedFrames) * kOpusChannels;
        const Uint8* bytes = reinterpret_cast<const Uint8*>(decodeBuffer.data());
        clip.audioData.insert(clip.audioData.end(),
                              bytes,
                              bytes + sampleCount * sizeof(opus_int16));
    }

    op_free(file);
    outClip = std::move(clip);
    return finalizeDecodedClip(outClip, errorMessage);
#else
    (void)resolvedPath;
    outClip = DecodedAudioClip{};
    setError(errorMessage, "Opus support is not enabled; install opusfile and rebuild.");
    return false;
#endif
}

} // namespace

bool loadDecodedAudioClip(const std::string& requestedPath,
                          DecodedAudioClip& outClip,
                          std::string* resolvedPath,
                          std::string* errorMessage) {
    outClip = DecodedAudioClip{};
    if (resolvedPath != nullptr) {
        resolvedPath->clear();
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    const std::optional<std::string> resolved = platform::path::resolveAudioPath(requestedPath);
    if (!resolved.has_value()) {
        setError(errorMessage, "Audio asset was not found.");
        return false;
    }

    if (resolvedPath != nullptr) {
        *resolvedPath = *resolved;
    }

    const std::string extension = platform::path::lowercaseExtension(*resolved);
    if (extension == ".opus") {
        return loadOpusClip(*resolved, outClip, errorMessage);
    }

    return loadWavClip(*resolved, outClip, errorMessage);
}

} // namespace game::audio
