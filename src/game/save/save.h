#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../vn/vn_script.h"

#include <nlohmann/json.hpp>

namespace save {

constexpr int SAVE_VERSION = 1;

struct SaveGame {
    struct Settings {
        bool fullscreen = false;
        int voiceVolume = 82;
        int textSpeed = 42;
    };

    int version = SAVE_VERSION;
    std::string timestamp;
    std::string label;
    std::string chapter;
    int entryIndex = 0;
    Settings settings;
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
        {"voiceVolume", settings.voiceVolume},
        {"textSpeed", settings.textSpeed}
    };
}

inline void from_json(const nlohmann::json& jsonValue, SaveGame::Settings& settings) {
    jsonValue.at("fullscreen").get_to(settings.fullscreen);
    jsonValue.at("voiceVolume").get_to(settings.voiceVolume);
    jsonValue.at("textSpeed").get_to(settings.textSpeed);
}

inline void to_json(nlohmann::json& jsonValue, const SaveGame& saveGame) {
    jsonValue = nlohmann::json::object();
    jsonValue["version"] = saveGame.version;
    jsonValue["timestamp"] = saveGame.timestamp;
    jsonValue["label"] = saveGame.label;
    jsonValue["chapter"] = saveGame.chapter;
    jsonValue["entryIndex"] = saveGame.entryIndex;
    jsonValue["settings"] = saveGame.settings;
}

inline void from_json(const nlohmann::json& jsonValue, SaveGame& saveGame) {
    jsonValue.at("version").get_to(saveGame.version);
    jsonValue.at("timestamp").get_to(saveGame.timestamp);
    jsonValue.at("label").get_to(saveGame.label);
    jsonValue.at("chapter").get_to(saveGame.chapter);
    jsonValue.at("entryIndex").get_to(saveGame.entryIndex);
    jsonValue.at("settings").get_to(saveGame.settings);
}

} // namespace save
