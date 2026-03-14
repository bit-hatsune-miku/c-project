#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace platform::path {

inline std::string resolvePath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    if (relativePath.empty()) {
        return relativePath;
    }

    const fs::path input(relativePath);
    if (input.is_absolute() && fs::exists(input)) {
        return input.string();
    }

    std::vector<fs::path> roots;
    roots.reserve(12);

    // 1) Legacy behavior (relative to current working directory).
    roots.emplace_back(".");
    roots.emplace_back("..");
    roots.emplace_back("../..");

    // 2) Relative to current_path() and its parents.
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec && !cwd.empty()) {
        roots.push_back(cwd);
        fs::path parent = cwd;
        for (int i = 0; i < 3; ++i) {
            parent = parent.parent_path();
            if (parent.empty()) {
                break;
            }
            roots.push_back(parent);
        }
    }

    // 3) Relative to executable location (Linux: /proc/self/exe) and parents.
    const fs::path procExe("/proc/self/exe");
    if (fs::exists(procExe)) {
        const fs::path exePath = fs::read_symlink(procExe, ec);
        if (!ec && !exePath.empty()) {
            fs::path exeDir = exePath.parent_path();
            if (!exeDir.empty()) {
                roots.push_back(exeDir);
                for (int i = 0; i < 4; ++i) {
                    exeDir = exeDir.parent_path();
                    if (exeDir.empty()) {
                        break;
                    }
                    roots.push_back(exeDir);
                }
            }
        }
    }

    for (const fs::path& root : roots) {
        const fs::path candidate = root / input;
        if (fs::exists(candidate)) {
            return candidate.lexically_normal().string();
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
