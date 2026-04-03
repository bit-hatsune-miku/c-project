#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../core/player_progression.h"
#include "../vn/vn_script.h"

#include <nlohmann/json.hpp>

/**
 * Represents all data required to persist and restore a game save.
 *
 * Contains metadata (version, timestamp, label, chapter, entry index),
 * a snapshot of application settings, and optional player progression data.
 */

/**
 * Settings snapshot stored in a save file.
 *
 * Each field represents user-visible configuration captured at the time of save.
 */

/**
 * Metadata for a save slot on disk.
 */

/**
 * Initialize save system resources and internal state.
 */

/**
 * Write an autosave using the provided save game snapshot.
 *
 * @param saveGame Save data to write.
 * @returns `true` if the autosave was written successfully, `false` otherwise.
 */

/**
 * Write a manual save using the provided save game snapshot.
 *
 * @param saveGame Save data to write.
 * @returns `true` if the manual save was written successfully, `false` otherwise.
 */

/**
 * Serialize and write the provided save game to the specified filesystem path.
 *
 * @param path Destination file path.
 * @param saveGame Save data to write.
 * @returns `true` if the file was written successfully, `false` otherwise.
 */

/**
 * Delete a manual save file at the given path.
 *
 * @param path Path to the manual save file to delete.
 * @returns `true` if the file was deleted successfully, `false` otherwise.
 */

/**
 * Load save data from the specified path.
 *
 * @param path Path to the save file to load.
 * @returns The loaded SaveGame on success, `std::nullopt` on failure.
 */

/**
 * Find an existing manual save file path that matches the provided save snapshot.
 *
 * Matching is performed against identifying fields of the provided snapshot.
 *
 * @param saveGame Save snapshot to match.
 * @returns A filesystem path to a matching manual save if found, `std::nullopt` otherwise.
 */

/**
 * List metadata for all available save slots (manual and autosave).
 *
 * @returns A vector of SlotInfo entries describing each slot.
 */

/**
 * Load the current in-memory player progression snapshot.
 *
 * @returns The current PlayerProgression snapshot.
 */

/**
 * Load profile progression into the provided output parameter.
 *
 * @param outProgression Output reference that will receive the progression data.
 * @returns `true` if progression was loaded successfully, `false` otherwise.
 */

/**
 * Write the provided player progression to persistent profile storage.
 *
 * @param progression Progression data to write.
 * @returns `true` if the write succeeded, `false` otherwise.
 */

/**
 * Load the most recent progression snapshot into the provided output parameter.
 *
 * @param outProgression Output reference that will receive the latest progression snapshot.
 * @returns `true` if a snapshot was loaded successfully, `false` otherwise.
 */

/**
 * Get the application game data directory path.
 *
 * @returns Path to the game data directory.
 */

/**
 * Get the saves directory path inside the game data directory.
 *
 * @returns Path to the saves directory.
 */

/**
 * Get the profile file path used for storing persistent profile data.
 *
 * @returns Path to the profile file.
 */

/**
 * Get the path reserved for the autosave file.
 *
 * @returns Path to the autosave file.
 */

/**
 * Produce an ISO-formatted UTC timestamp string suitable for embedding in save files.
 *
 * @returns ISO UTC timestamp string.
 */

/**
 * Produce a timestamp string suitable for use as a filename stem (filesystem-safe).
 *
 * @returns Filename-stem-friendly timestamp string.
 */

/**
 * Convert an ISO UTC timestamp (as stored in save files) into a user-friendly display string.
 *
 * @param isoTimestamp ISO-formatted UTC timestamp to format.
 * @returns A human-readable representation of the timestamp.
 */

/**
 * Derive a canonical chapter identifier from a script.
 *
 * @param script Script object to derive the chapter id from.
 * @returns Chapter identifier string.
 */

/**
 * Convert a chapter identifier into the corresponding script file path.
 *
 * @param chapterId Chapter identifier to convert.
 * @returns Filesystem path to the chapter script.
 */

/**
 * Generate a display label for a save based on the script and an entry index.
 *
 * @param script Script used to derive the label.
 * @param entryIndex Entry index within the script to include in the label.
 * @returns Generated label string suitable for showing in save lists.
 */

/**
 * Serialize Settings into JSON with keys: "fullscreen", "musicVolume", "voiceVolume", "textSpeed".
 *
 * @param jsonValue JSON output value to populate.
 * @param settings Settings instance to serialize.
 */

/**
 * Deserialize Settings from JSON using defaults when keys are absent:
 * - "fullscreen" default false
 * - "musicVolume" default 100
 * - "voiceVolume" default 82
 * - "textSpeed" default 42
 *
 * @param jsonValue JSON input to read from.
 * @param settings Settings instance to populate.
 */

/**
 * Serialize a SaveGame into JSON, emitting fields:
 * "version", "timestamp", "label", "chapter", "entryIndex", "settings", and
 * a "progression" object containing "unlockedCharacterKeys", "currentPartyLineup", and "clearedBattleKeys".
 *
 * @param jsonValue JSON output value to populate.
 * @param saveGame SaveGame instance to serialize.
 */

/**
 * Deserialize a SaveGame from JSON. Required keys (accessed with `at`) are:
 * "version", "timestamp", "label", "chapter", "entryIndex", and "settings".
 * If a "progression" object is present, its arrays "unlockedCharacterKeys",
 * "currentPartyLineup", and "clearedBattleKeys" are loaded and the
 * SaveGame::hasProgressionData flag is set to `true`; otherwise progression
 * is left empty and the flag remains `false`.
 *
 * @param jsonValue JSON input to read from.
 * @param saveGame SaveGame instance to populate.
 */
namespace save {

constexpr int SAVE_VERSION = 1;

enum class SaveContext {
    Campaign,
    Practice
};

inline std::string saveContextToString(SaveContext saveContext) {
    switch (saveContext) {
    case SaveContext::Practice:
        return "practice";
    case SaveContext::Campaign:
    default:
        return "campaign";
    }
}

inline SaveContext saveContextFromString(const std::string& saveContext) {
    if (saveContext == "practice") {
        return SaveContext::Practice;
    }
    return SaveContext::Campaign;
}

struct SaveGame {
    struct Settings {
        bool fullscreen = false;
        int musicVolume = 100;
        int voiceVolume = 82;
        int textSpeed = 42;
    };

    int version = SAVE_VERSION;
    std::string timestamp;
    std::string label;
    std::string chapter;
    SaveContext saveContext = SaveContext::Campaign;
    int entryIndex = 0;
    Settings settings;
    battle::PlayerProgression progression;
    bool hasProgressionData = false;
};

struct SlotInfo {
    std::filesystem::path path;
    std::string label;
    std::string timestamp;
    bool isAutosave = false;
};

void init();

bool autosave(const SaveGame& saveGame);
bool manualSave(const SaveGame& saveGame);
bool writeToPath(const std::filesystem::path& path, const SaveGame& saveGame);
bool deleteManualSave(const std::filesystem::path& path);
std::optional<SaveGame> load(const std::filesystem::path& path);
std::optional<std::filesystem::path> findMatchingManualSavePath(const SaveGame& saveGame);
std::vector<SlotInfo> listSlots();
battle::PlayerProgression loadCurrentProgression();
bool loadProfileProgression(battle::PlayerProgression& outProgression);
bool writeProfileProgression(const battle::PlayerProgression& progression);
bool loadLatestProgressionSnapshot(battle::PlayerProgression& outProgression);

std::filesystem::path gameDataDir();
std::filesystem::path savesDir();
std::filesystem::path profilePath();
std::filesystem::path autosavePath();

std::string makeIsoUtcTimestamp();
std::string makeTimestampFilenameStem();
std::string formatTimestampForDisplay(const std::string& isoTimestamp);

std::string chapterIdFromScript(const vn::Script& script);
std::string chapterScriptPathFromId(const std::string& chapterId);
std::string generateLabel(const vn::Script& script, std::size_t entryIndex);

inline void to_json(nlohmann::json& jsonValue, const SaveGame::Settings& settings) {
    jsonValue = nlohmann::json{
        {"fullscreen", settings.fullscreen},
        {"musicVolume", settings.musicVolume},
        {"voiceVolume", settings.voiceVolume},
        {"textSpeed", settings.textSpeed}
    };
}

inline void from_json(const nlohmann::json& jsonValue, SaveGame::Settings& settings) {
    settings.fullscreen = jsonValue.value("fullscreen", false);
    settings.musicVolume = jsonValue.value("musicVolume", 100);
    settings.voiceVolume = jsonValue.value("voiceVolume", 82);
    settings.textSpeed = jsonValue.value("textSpeed", 42);
}

inline void to_json(nlohmann::json& jsonValue, const SaveGame& saveGame) {
    jsonValue = nlohmann::json::object();
    jsonValue["version"] = saveGame.version;
    jsonValue["timestamp"] = saveGame.timestamp;
    jsonValue["label"] = saveGame.label;
    jsonValue["chapter"] = saveGame.chapter;
    jsonValue["saveContext"] = saveContextToString(saveGame.saveContext);
    jsonValue["entryIndex"] = saveGame.entryIndex;
    jsonValue["settings"] = saveGame.settings;
    jsonValue["progression"] = nlohmann::json{
        {"unlockedCharacterKeys", saveGame.progression.unlockedCharacterKeys},
        {"currentPartyLineup", saveGame.progression.currentPartyLineup},
        {"clearedBattleKeys", saveGame.progression.clearedBattleKeys}
    };
}

inline void from_json(const nlohmann::json& jsonValue, SaveGame& saveGame) {
    jsonValue.at("version").get_to(saveGame.version);
    jsonValue.at("timestamp").get_to(saveGame.timestamp);
    jsonValue.at("label").get_to(saveGame.label);
    jsonValue.at("chapter").get_to(saveGame.chapter);
    saveGame.saveContext = saveContextFromString(jsonValue.value("saveContext", std::string("campaign")));
    jsonValue.at("entryIndex").get_to(saveGame.entryIndex);
    jsonValue.at("settings").get_to(saveGame.settings);
    saveGame.progression = battle::PlayerProgression{};
    saveGame.hasProgressionData = false;

    const auto progressionIt = jsonValue.find("progression");
    if (progressionIt != jsonValue.end() && progressionIt->is_object()) {
        saveGame.progression.unlockedCharacterKeys =
            progressionIt->value("unlockedCharacterKeys", std::vector<std::string>{});
        saveGame.progression.currentPartyLineup =
            progressionIt->value("currentPartyLineup", std::vector<std::string>{});
        saveGame.progression.clearedBattleKeys =
            progressionIt->value("clearedBattleKeys", std::vector<std::string>{});
        saveGame.hasProgressionData = true;
    }
}

} // namespace save
