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

#include "../vn/vn_script_catalog.h"

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

/**
 * @brief Normalize a SaveGame to the current on-disk format and sane defaults.
 *
 * Ensures the save uses the current save version, fills a missing timestamp with
 * an ISO-8601 UTC timestamp, clamps audio volumes to the range 0–100, enforces
 * a minimum text speed of 1, normalizes player progression using the
 * StarterRoster fallback policy, and marks the save as containing progression data.
 *
 * @param saveGame A copy of the save to normalize.
 * @return SaveGame The normalized save game object.
 */
SaveGame normalizedSaveGame(SaveGame saveGame) {
    saveGame.version = SAVE_VERSION;
    if (saveGame.timestamp.empty()) {
        saveGame.timestamp = makeIsoUtcTimestamp();
    }
    const std::string storedScriptId =
        !saveGame.scriptId.empty() ? vn::canonicalScriptId(saveGame.scriptId) : std::string();
    const std::string legacyChapterId = vn::canonicalScriptIdFromLegacyAlias(saveGame.chapter);
    saveGame.scriptId = !storedScriptId.empty() ? storedScriptId : legacyChapterId;
    if (!saveGame.scriptId.empty()) {
        saveGame.chapter = saveGame.scriptId;
    }
    saveGame.settings.musicVolume = std::clamp(saveGame.settings.musicVolume, 0, 100);
    saveGame.settings.voiceVolume = std::clamp(saveGame.settings.voiceVolume, 0, 100);
    saveGame.settings.textSpeed = std::max(1, saveGame.settings.textSpeed);
    battle::normalizePlayerProgression(saveGame.progression, battle::ProgressionFallbackPolicy::StarterRoster);
    saveGame.hasProgressionData = true;
    return saveGame;
}

json progressionJson(const battle::PlayerProgression& progression) {
    return json{
        {"unlockedCharacterKeys", progression.unlockedCharacterKeys},
        {"currentPartyLineup", progression.currentPartyLineup},
        {"clearedBattleKeys", progression.clearedBattleKeys},
        {"stageBuffAssignments", progression.stageBuffAssignments}
    };
}

bool parseProfileProgression(const json& root, battle::PlayerProgression& outProgression) {
    if (!root.is_object()) {
        return false;
    }

    const auto progressionIt = root.find("progression");
    if (progressionIt == root.end() || !progressionIt->is_object()) {
        return false;
    }

    outProgression = battle::PlayerProgression{};
    outProgression.unlockedCharacterKeys =
        progressionIt->value("unlockedCharacterKeys", std::vector<std::string>{});
    outProgression.currentPartyLineup =
        progressionIt->value("currentPartyLineup", std::vector<std::string>{});
    outProgression.clearedBattleKeys =
        progressionIt->value("clearedBattleKeys", std::vector<std::string>{});
    outProgression.stageBuffAssignments =
        progressionIt->value("stageBuffAssignments", battle::StageBuffAssignments{});
    return true;
}

/**
 * @brief Construct the default profile JSON used when no profile exists.
 *
 * Produces a JSON object containing the profile version, default settings,
 * basic play statistics, and an initial player progression snapshot.
 *
 * @return json JSON object with the following structure:
 * - "version": integer set to 1
 * - "settings": object with keys "fullscreen" (bool), "musicVolume" (int, 0–100),
 *   "voiceVolume" (int, 0–100), and "textSpeed" (int)
 * - "playStats": object with keys "launches" (int) and "manualSaves" (int)
 * - "progression": object representing the default player progression
 */
json makeDefaultProfileJson() {
    const battle::PlayerProgression defaultProgression = battle::makeDefaultPlayerProgression();
    return json{
        {"version", 1},
        {"settings", {
            {"fullscreen", false},
            {"musicVolume", 100},
            {"voiceVolume", 82},
            {"textSpeed", 42}
        }},
        {"playStats", {
            {"launches", 0},
            {"manualSaves", 0}
        }},
        {"progression", progressionJson(defaultProgression)}
    };
}

bool readProfileJson(json& outProfile) {
    std::ifstream file(profilePath());
    if (!file.is_open()) {
        return false;
    }

    try {
        file >> outProfile;
        return outProfile.is_object();
    } catch (const json::exception&) {
        return false;
    }
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

std::map<std::string, std::string> chapterSceneMap(const vn::Script& script) {
    if (script.scriptId == "ch0001" || script.scriptId == "ch0099") {
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
    (void)writeJsonFile(path, makeDefaultProfileJson());
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

    std::error_code error;
    const fs::path canonicalSavesDir = fs::weakly_canonical(gSavesDir, error);
    if (error) {
        return false;
    }
    const fs::path canonicalTarget = fs::weakly_canonical(path, error);
    if (error) {
        return false;
    }
    const auto [end, _] = std::mismatch(canonicalSavesDir.begin(), canonicalSavesDir.end(), canonicalTarget.begin());
    if (end != canonicalSavesDir.end()) {
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

/**
 * @brief Load and validate a save file from disk.
 *
 * Parses the JSON at the given path into a SaveGame, validates that the save version
 * matches SAVE_VERSION and that required fields (`chapter` non-empty and `entryIndex`
 * >= 0) are present, and clamps `musicVolume` and `voiceVolume` to [0, 100] and
 * ensures `textSpeed` is at least 1 before returning.
 *
 * @param path Filesystem path to the save file to read.
 * @return std::optional<SaveGame> containing the validated save when successful; `std::nullopt`
 *         if the file cannot be opened, the JSON fails to parse, the saved version mismatches,
 *         or required fields are invalid.
 */
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
        return normalizedSaveGame(std::move(saveGame));
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
        if (candidate->chapter != saveGame.chapter ||
            candidate->saveContext != saveGame.saveContext ||
            candidate->entryIndex != saveGame.entryIndex) {
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

battle::PlayerProgression loadCurrentProgression() {
    init();

    battle::PlayerProgression progression;
    if (loadProfileProgression(progression)) {
        battle::normalizePlayerProgression(progression, battle::ProgressionFallbackPolicy::StarterRoster);
        return progression;
    }
    if (loadLatestProgressionSnapshot(progression)) {
        battle::normalizePlayerProgression(progression, battle::ProgressionFallbackPolicy::StarterRoster);
        return progression;
    }

    battle::normalizePlayerProgression(progression, battle::ProgressionFallbackPolicy::StarterRoster);
    return progression;
}

bool loadProfileProgression(battle::PlayerProgression& outProgression) {
    init();

    json profile;
    if (!readProfileJson(profile)) {
        return false;
    }
    if (!parseProfileProgression(profile, outProgression)) {
        return false;
    }

    battle::normalizePlayerProgression(outProgression, battle::ProgressionFallbackPolicy::StarterRoster);
    return true;
}

bool writeProfileProgression(const battle::PlayerProgression& progression) {
    init();
    if (!ensureGameDirectoriesExist()) {
        return false;
    }

    json profile = makeDefaultProfileJson();
    json existing;
    if (readProfileJson(existing) && existing.is_object()) {
        profile = std::move(existing);
    }

    battle::PlayerProgression normalized = progression;
    battle::normalizePlayerProgression(normalized, battle::ProgressionFallbackPolicy::StarterRoster);
    profile["progression"] = progressionJson(normalized);
    if (!profile.contains("version")) {
        profile["version"] = 1;
    }

    return writeJsonFile(profilePath(), profile);
}

bool loadLatestProgressionSnapshot(battle::PlayerProgression& outProgression) {
    init();

    std::optional<SaveGame> bestSave;
    for (const SlotInfo& slot : listSlots()) {
        const std::optional<SaveGame> saveGame = load(slot.path);
        if (!saveGame.has_value() || !saveGame->hasProgressionData) {
            continue;
        }

        if (!bestSave.has_value() || saveGame->timestamp > bestSave->timestamp) {
            bestSave = saveGame;
        }
    }

    if (!bestSave.has_value()) {
        return false;
    }

    outProgression = bestSave->progression;
    battle::normalizePlayerProgression(outProgression, battle::ProgressionFallbackPolicy::StarterRoster);
    return true;
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

    return formatTm(localTime(utcValue), "%Y/%m/%d %H:%M");
}

std::string chapterIdFromScript(const vn::Script& script) {
    if (!script.scriptId.empty()) {
        return vn::canonicalScriptId(script.scriptId);
    }
    return vn::canonicalScriptId("ch" + std::to_string(std::max(0, script.chapter)));
}

std::string chapterScriptPathFromId(const std::string& chapterId) {
    return vn::resolveScriptPath(chapterId);
}

std::string generateLabel(const vn::Script& script, std::size_t entryIndex) {
    const std::string chapterTitle = script.title.empty() ? ("Chapter " + std::to_string(std::max(0, script.chapter))) : script.title;
    const std::string background = currentSceneBackground(script, entryIndex);
    const std::map<std::string, std::string> scenes = chapterSceneMap(script);

    const auto sceneIt = scenes.find(background);
    if (sceneIt != scenes.end()) {
        return "Chapter " + std::to_string(std::max(0, script.chapter)) + " - " + sceneIt->second;
    }

    return "Chapter " + std::to_string(std::max(0, script.chapter)) + " - " + chapterTitle;
}

} // namespace save
