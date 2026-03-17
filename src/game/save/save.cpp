#include "save.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <system_error>

#include <nlohmann/json.hpp>

namespace save {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

constexpr const char* kSavePrefix = "Idolsmtsmt_";
constexpr const char* kGameDirName = "BitUndergroundIdol";
constexpr const char* kAutosaveFileName = "autosave.idol";
constexpr const char* kProfileFileName = "profile.json";

fs::path gDocumentsRoot;
fs::path gGameDataDir;
fs::path gSavesDir;
bool gInitialized = false;

std::tm utcTime(std::time_t value) {
    std::tm tmValue{};
#if defined(_WIN32)
    gmtime_s(&tmValue, &value);
#else
    gmtime_r(&value, &tmValue);
#endif
    return tmValue;
}

std::tm localTime(std::time_t value) {
    std::tm tmValue{};
#if defined(_WIN32)
    localtime_s(&tmValue, &value);
#else
    localtime_r(&value, &tmValue);
#endif
    return tmValue;
}

std::string formatTm(const std::tm& tmValue, const char* pattern) {
    std::ostringstream stream;
    stream << std::put_time(&tmValue, pattern);
    return stream.str();
}

std::time_t systemClockNow() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

SaveGame normalizedSaveGame(SaveGame saveGame) {
    saveGame.version = SAVE_VERSION;
    if (saveGame.timestamp.empty()) {
        saveGame.timestamp = makeIsoUtcTimestamp();
    }
    saveGame.settings.voiceVolume = std::clamp(saveGame.settings.voiceVolume, 0, 100);
    saveGame.settings.textSpeed = std::max(1, saveGame.settings.textSpeed);
    return saveGame;
}

bool writeJsonFile(const fs::path& path, const json& value) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << value.dump(2) << '\n';
    return static_cast<bool>(file);
}

fs::path uniqueManualSavePath() {
    const std::string baseName = std::string(kSavePrefix) + makeTimestampFilenameStem();
    std::error_code error;

    for (int suffix = 0; suffix < 1000; ++suffix) {
        std::string filename = baseName;
        if (suffix > 0) {
            filename += "_" + std::to_string(suffix);
        }
        filename += ".idol";

        const fs::path candidate = gSavesDir / filename;
        if (!fs::exists(candidate, error)) {
            return candidate;
        }
        error.clear();
    }

    return fs::path();
}

std::map<std::string, std::string> chapterSceneMap(const std::string& chapterId) {
    if (chapterId == "ch0") {
        return {
            {"assets/vn/backgrounds/ch0/0.jpg", "Concert Dream"},
            {"assets/vn/backgrounds/ch0/1.jpg", "Dorm Room"},
            {"assets/vn/backgrounds/ch0/2.png", "Meeting Cupcakke"},
            {"assets/vn/backgrounds/ch0/3.png", "Road to Class"},
            {"assets/vn/backgrounds/ch0/4.png", "Dog Crash"},
            {"assets/vn/backgrounds/ch0/6.png", "Chasing the Dog"},
            {"assets/vn/backgrounds/ch0/5.png", "Underground Entrance"}
        };
    }
    return {};
}

std::string currentSceneBackground(const vn::Script& script, std::size_t entryIndex) {
    if (script.entries.empty()) {
        return std::string();
    }

    const std::size_t clampedIndex = std::min(entryIndex, script.entries.size() - 1);
    for (std::size_t i = clampedIndex + 1; i > 0; --i) {
        const vn::ScriptEntry& entry = script.entries[i - 1];
        if (!entry.background.empty()) {
            return entry.background;
        }
    }

    return std::string();
}

fs::path detectDocumentsDir() {
#if defined(_WIN32)
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile != nullptr && *userProfile != '\0') {
        return fs::path(userProfile) / "Documents";
    }
#else
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return fs::path(home) / "Documents";
    }
#endif
    return fs::current_path() / "Documents";
}

bool ensureGameDirectoriesExist() {
    std::error_code error;
    if (!fs::exists(gGameDataDir, error)) {
        fs::create_directories(gGameDataDir, error);
    }
    if (!error && !fs::exists(gSavesDir, error)) {
        fs::create_directories(gSavesDir, error);
    }
    return !error && fs::exists(gGameDataDir) && fs::exists(gSavesDir);
}

void writeDefaultProfileIfMissing() {
    const fs::path path = profilePath();
    if (fs::exists(path)) {
        return;
    }

    json profile = {
        {"version", 1},
        {"settings", {
            {"fullscreen", false},
            {"voiceVolume", 82},
            {"textSpeed", 42}
        }},
        {"playStats", {
            {"launches", 0},
            {"manualSaves", 0}
        }}
    };
    (void)writeJsonFile(path, profile);
}

bool isManagedSaveFile(const fs::path& path) {
    if (path.extension() != ".idol") {
        return false;
    }
    const std::string name = path.filename().string();
    return name == kAutosaveFileName || name.rfind(kSavePrefix, 0) == 0;
}

} // namespace

void init() {
    if (gInitialized) {
        return;
    }

    gDocumentsRoot = detectDocumentsDir();
    gGameDataDir = gDocumentsRoot / kGameDirName;
    gSavesDir = gGameDataDir / "saves";

    // Mark initialized before helper calls that may query save paths.
    gInitialized = true;

    if (!ensureGameDirectoriesExist()) {
        std::cerr << "[Save] Failed to create game data directory at " << gGameDataDir << "\n";
    } else {
        writeDefaultProfileIfMissing();
    }
}

bool autosave(const SaveGame& saveGame) {
    init();
    if (!ensureGameDirectoriesExist()) {
        return false;
    }

    const SaveGame normalized = normalizedSaveGame(saveGame);
    return writeJsonFile(autosavePath(), json(normalized));
}

bool manualSave(const SaveGame& saveGame) {
    init();
    if (!ensureGameDirectoriesExist()) {
        return false;
    }

    const SaveGame normalized = normalizedSaveGame(saveGame);
    const fs::path path = uniqueManualSavePath();
    if (path.empty()) {
        return false;
    }
    return writeJsonFile(path, json(normalized));
}

bool writeToPath(const fs::path& path, const SaveGame& saveGame) {
    init();
    if (!ensureGameDirectoriesExist()) {
        return false;
    }

    const SaveGame normalized = normalizedSaveGame(saveGame);
    return writeJsonFile(path, json(normalized));
}

bool deleteManualSave(const fs::path& path) {
    init();
    if (!ensureGameDirectoriesExist()) {
        return false;
    }

    std::error_code error;
    if (!fs::exists(path, error) || error || !fs::is_regular_file(path, error)) {
        return false;
    }
    if (path.parent_path() != gSavesDir) {
        return false;
    }
    if (path.filename() == kAutosaveFileName || !isManagedSaveFile(path)) {
        return false;
    }

    return fs::remove(path, error) && !error;
}

std::optional<SaveGame> load(const fs::path& path) {
    init();

    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    try {
        json root;
        file >> root;
        SaveGame saveGame = root.get<SaveGame>();
        if (saveGame.version != SAVE_VERSION) {
            return std::nullopt;
        }
        if (saveGame.chapter.empty() || saveGame.entryIndex < 0) {
            return std::nullopt;
        }
        saveGame.settings.voiceVolume = std::clamp(saveGame.settings.voiceVolume, 0, 100);
        saveGame.settings.textSpeed = std::max(1, saveGame.settings.textSpeed);
        return saveGame;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

std::optional<fs::path> findMatchingManualSavePath(const SaveGame& saveGame) {
    init();

    std::optional<fs::path> bestPath;
    std::string bestTimestamp;
    std::error_code error;
    if (!fs::exists(gSavesDir, error)) {
        return std::nullopt;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(gSavesDir, error)) {
        if (error || !entry.is_regular_file()) {
            continue;
        }
        const fs::path& path = entry.path();
        if (path.filename() == kAutosaveFileName || !isManagedSaveFile(path)) {
            continue;
        }

        const std::optional<SaveGame> candidate = load(path);
        if (!candidate.has_value()) {
            continue;
        }
        if (candidate->chapter != saveGame.chapter || candidate->entryIndex != saveGame.entryIndex) {
            continue;
        }

        if (!bestPath.has_value() || candidate->timestamp > bestTimestamp) {
            bestPath = path;
            bestTimestamp = candidate->timestamp;
        }
    }

    return bestPath;
}

std::vector<SlotInfo> listSlots() {
    init();

    std::vector<SlotInfo> slots;
    std::error_code error;
    if (!fs::exists(gSavesDir, error)) {
        return slots;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(gSavesDir, error)) {
        if (error || !entry.is_regular_file() || !isManagedSaveFile(entry.path())) {
            continue;
        }

        const std::optional<SaveGame> saveGame = load(entry.path());
        if (!saveGame.has_value()) {
            continue;
        }

        SlotInfo slot;
        slot.path = entry.path();
        slot.label = saveGame->label;
        slot.timestamp = saveGame->timestamp;
        slot.isAutosave = entry.path().filename() == kAutosaveFileName;
        slots.push_back(std::move(slot));
    }

    std::sort(slots.begin(), slots.end(), [](const SlotInfo& lhs, const SlotInfo& rhs) {
        if (lhs.isAutosave != rhs.isAutosave) {
            return lhs.isAutosave;
        }
        return lhs.timestamp > rhs.timestamp;
    });

    return slots;
}

fs::path gameDataDir() {
    init();
    return gGameDataDir;
}

fs::path savesDir() {
    init();
    return gSavesDir;
}

fs::path profilePath() {
    init();
    return gameDataDir() / kProfileFileName;
}

fs::path autosavePath() {
    init();
    return savesDir() / kAutosaveFileName;
}

std::string makeIsoUtcTimestamp() {
    return formatTm(utcTime(systemClockNow()), "%Y-%m-%dT%H:%M:%SZ");
}

std::string makeTimestampFilenameStem() {
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream stream;
    stream << formatTm(localTime(std::chrono::system_clock::to_time_t(now)), "%Y%m%d_%H%M%S")
           << '_' << std::setw(3) << std::setfill('0') << millis.count();
    return stream.str();
}

std::string formatTimestampForDisplay(const std::string& isoTimestamp) {
    if (isoTimestamp.size() < 19) {
        return isoTimestamp;
    }

    std::tm tmValue{};
    std::istringstream stream(isoTimestamp.substr(0, 19));
    stream >> std::get_time(&tmValue, "%Y-%m-%dT%H:%M:%S");
    if (stream.fail()) {
        return isoTimestamp;
    }

#if defined(_WIN32)
    const std::time_t utcValue = _mkgmtime(&tmValue);
#else
    const std::time_t utcValue = timegm(&tmValue);
#endif
    if (utcValue == static_cast<std::time_t>(-1)) {
        return isoTimestamp;
    }

    return formatTm(localTime(utcValue), "%b %d, %Y  %I:%M %p");
}

std::string chapterIdFromScript(const vn::Script& script) {
    return "ch" + std::to_string(std::max(0, script.chapter));
}

std::string chapterScriptPathFromId(const std::string& chapterId) {
    std::string normalized = chapterId;
    normalized.erase(
        std::remove_if(normalized.begin(), normalized.end(), [](unsigned char c) {
            return !(std::isalnum(c) || c == '_' || c == '-');
        }),
        normalized.end()
    );
    return "assets/vn/json/" + normalized + ".json";
}

std::string generateLabel(const vn::Script& script, std::size_t entryIndex) {
    const std::string chapterId = chapterIdFromScript(script);
    const std::string chapterTitle = script.title.empty() ? ("Chapter " + std::to_string(std::max(0, script.chapter))) : script.title;
    const std::string background = currentSceneBackground(script, entryIndex);
    const std::map<std::string, std::string> scenes = chapterSceneMap(chapterId);

    const auto sceneIt = scenes.find(background);
    if (sceneIt != scenes.end()) {
        return "Chapter " + std::to_string(std::max(0, script.chapter)) + " - " + sceneIt->second;
    }

    return "Chapter " + std::to_string(std::max(0, script.chapter)) + " - " + chapterTitle;
}

} // namespace save
