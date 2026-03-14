#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace platform::path {

inline std::string resolvePath(const std::string& relativePath) {
    const std::array<std::string, 3> candidates = {
        relativePath,
        "../" + relativePath,
        "../../" + relativePath
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return relativePath;
}

inline std::string findFontPath() {
    const std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        resolvePath("assets/rmlui/DejaVuSans.ttf"),
        resolvePath("assets/rmlui/DejaVuSans-Bold.ttf")
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return std::string();
}

inline std::string findCombatImagePath(const std::string& folder, const std::string& assetName) {
    const std::array<std::pair<std::string, std::string>, 2> candidates = {
        std::pair<std::string, std::string>{
            resolvePath("assets/combat/" + folder + "/" + assetName + ".png"),
            "../combat/" + folder + "/" + assetName + ".png"
        },
        std::pair<std::string, std::string>{
            resolvePath("assets/combat/" + folder + "/" + assetName + ".webp"),
            "../combat/" + folder + "/" + assetName + ".webp"
        }
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate.first)) {
            return candidate.second;
        }
    }
    return std::string();
}

inline std::optional<std::string> resolveCombatVoicePath(const std::string& assetName, const std::string& clipName) {
    if (assetName.empty() || clipName.empty()) {
        return std::nullopt;
    }

    const std::array<std::string, 2> candidates = {
        resolvePath("assets/combat/voices/" + assetName + "/" + clipName + ".wav"),
        resolvePath("assets/combat/voices/" + assetName + "." + clipName + ".wav")
    };
    for (const std::string& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

// Resolve a BGM clip name to a file path (.wav preferred, .mp3 fallback).
inline std::optional<std::string> resolveCombatBgmPath(const std::string& bgmName) {
    if (bgmName.empty()) {
        return std::nullopt;
    }

    const std::array<std::string, 2> candidates = {
        resolvePath("assets/combat/bgm/" + bgmName + ".wav"),
        resolvePath("assets/combat/bgm/" + bgmName + ".mp3")
    };
    for (const std::string& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace platform::path
