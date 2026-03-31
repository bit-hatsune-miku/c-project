#pragma once

#include <array>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace platform::path {

inline std::string findCjkFontPath();

inline std::string portablePathString(const std::filesystem::path& path) {
    return path.lexically_normal().generic_string();
}

inline std::optional<std::filesystem::path> executablePath() {
    namespace fs = std::filesystem;

#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::nullopt;
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return fs::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::error_code ec;
    const fs::path procExe("/proc/self/exe");
    if (!fs::exists(procExe, ec) || ec) {
        return std::nullopt;
    }

    fs::path exePath = fs::read_symlink(procExe, ec);
    if (ec || exePath.empty()) {
        return std::nullopt;
    }
    return exePath;
#endif
}

inline std::string resolvePath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    if (relativePath.empty()) {
        return relativePath;
    }

    const fs::path input(relativePath);
    if (input.is_absolute() && fs::exists(input)) {
        return portablePathString(input);
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

    // 3) Relative to executable location and parents.
    if (const auto exePath = executablePath(); exePath.has_value()) {
        fs::path exeDir = exePath->parent_path();
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

    for (const fs::path& root : roots) {
        const fs::path candidate = root / input;
        if (fs::exists(candidate)) {
            return portablePathString(candidate);
        }
    }

    return portablePathString(input);
}

inline std::string resolvePathForRml(const std::string& assetPath) {
    namespace fs = std::filesystem;

    if (assetPath.empty()) {
        return assetPath;
    }

    std::error_code ec;
    const fs::path resolved(resolvePath(assetPath));
    if (fs::exists(resolved, ec) && !ec) {
        const fs::path absolutePath = fs::absolute(resolved, ec);
        if (!ec) {
            return portablePathString(absolutePath);
        }
    }

    return portablePathString(resolved);
}

inline std::string findFontPath() {
    const std::vector<std::string> candidates = {
        resolvePath("assets/fonts/SpaceMono-Regular.ttf"),
        resolvePath("assets/fonts/SpaceMono-Bold.ttf"),
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

inline std::string findCjkFontPath() {
    const std::vector<std::string> candidates = {
        resolvePath("assets/fonts/NotoSansCJK-Regular.ttc"),
        resolvePath("assets/fonts/NotoSansCJK-Bold.ttc"),
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return std::string();
}

inline std::vector<std::string> preferredLatinFontPaths(const std::vector<std::string>& preferredPaths = {}) {
    auto appendUnique = [](std::vector<std::string>& out, const std::string& path) {
        if (path.empty()) {
            return;
        }
        if (std::find(out.begin(), out.end(), path) == out.end()) {
            out.push_back(path);
        }
    };

    std::vector<std::string> candidates;
    candidates.reserve(preferredPaths.size() + 12);

    for (const std::string& preferredPath : preferredPaths) {
        appendUnique(candidates, resolvePath(preferredPath));
    }

    appendUnique(candidates, resolvePath("assets/fonts/SpaceMono-Regular.ttf"));
    appendUnique(candidates, resolvePath("assets/fonts/SpaceMono-Bold.ttf"));
    appendUnique(candidates, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    appendUnique(candidates, "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    appendUnique(candidates, "/usr/share/fonts/TTF/DejaVuSans.ttf");
    appendUnique(candidates, "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf");
    appendUnique(candidates, resolvePath("assets/rmlui/DejaVuSans.ttf"));
    appendUnique(candidates, resolvePath("assets/rmlui/DejaVuSans-Bold.ttf"));

    return candidates;
}

inline std::vector<std::string> preferredCjkFontPaths(const std::vector<std::string>& preferredPaths = {}) {
    auto candidates = preferredLatinFontPaths(preferredPaths);
    const std::string cjkFontPath = findCjkFontPath();
    if (!cjkFontPath.empty() &&
        std::find(candidates.begin(), candidates.end(), cjkFontPath) == candidates.end()) {
        candidates.insert(candidates.begin(), cjkFontPath);
    }
    return candidates;
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
