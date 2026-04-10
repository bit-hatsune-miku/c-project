#include "vn_script.h"
#include "vn_script_catalog.h"

#include "../../platform/path_resolution.h"

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vn {
namespace {

struct SpeakerCombatIconAlias {
    const char* speakerKey = "";
    const char* assetId = "";
};

struct SpeakerDirectIconAlias {
    const char* speakerKey = "";
    const char* relativePath = "";
};

std::string trimAsciiCopy(std::string value) {
    auto isAsciiSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    while (!value.empty() && isAsciiSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isAsciiSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string normalizedSpeakerKey(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());

    bool pendingSpace = false;
    for (unsigned char ch : value) {
        if (std::isspace(ch) != 0) {
            pendingSpace = !normalized.empty();
            continue;
        }

        if (pendingSpace) {
            normalized.push_back(' ');
            pendingSpace = false;
        }
        normalized.push_back(static_cast<char>(std::tolower(ch)));
    }

    return normalized;
}

std::string existingRelativePath(const std::string& relativePath) {
    if (relativePath.empty()) {
        return {};
    }

    const std::string resolved = platform::path::resolvePath(relativePath);
    if (std::filesystem::exists(resolved)) {
        return relativePath;
    }

    return {};
}

std::string combatIconPathForAsset(const std::string& assetId) {
    const std::string trimmed = trimAsciiCopy(assetId);
    if (trimmed.empty()) {
        return {};
    }

    const std::array<const char*, 2> extensions = {".png", ".webp"};
    for (const char* extension : extensions) {
        const std::string relativePath = "assets/combat/icons/" + trimmed + extension;
        if (const std::string existingPath = existingRelativePath(relativePath); !existingPath.empty()) {
            return existingPath;
        }
    }

    return {};
}

std::string defaultIconForDialogueEntry(const ScriptEntry& entry) {
    if (entry.type != EntryType::Dialogue) {
        return {};
    }

    const std::string speakerKey = normalizedSpeakerKey(entry.speaker);

    constexpr std::array<SpeakerDirectIconAlias, 5> kDirectAliases = {{
        {"lyoo", "assets/vn/icons/lyoo/0.png"},
        {"adult lyoo", "assets/vn/icons/lyoo/0.png"},
        {"adult", "assets/vn/icons/lyoo/0.png"},
        {"baby lyoo", "assets/vn/icons/lyoo/baby.png"},
        {"baby", "assets/vn/icons/lyoo/baby.png"},
    }};
    for (const SpeakerDirectIconAlias& alias : kDirectAliases) {
        if (speakerKey == alias.speakerKey) {
            return existingRelativePath(alias.relativePath);
        }
    }

    constexpr std::array<SpeakerCombatIconAlias, 12> kCombatAliases = {{
        {"miku", "miku"},
        {"hatsune miku", "miku"},
        {"cupcakke", "cupcakke"},
        {"jiafei", "jiafei"},
        {"luo tianyi", "luotianyi"},
        {"teto", "teto"},
        {"ariana grande", "ari"},
        {"wechat-jie", "wechatalipay"},
        {"alipay-jie", "wechatalipay"},
        {"zhou shen", "zhouShen"},
        {"zhao laoshi", "randy"},
        {"sailor venus", "sailorVenus"},
    }};
    for (const SpeakerCombatIconAlias& alias : kCombatAliases) {
        if (speakerKey == alias.speakerKey) {
            return combatIconPathForAsset(alias.assetId);
        }
    }

    if (speakerKey == "pompom") {
        return combatIconPathForAsset("pompom");
    }
    if (speakerKey == "huafei") {
        return combatIconPathForAsset("huafei");
    }
    if (speakerKey == "disciple") {
        return combatIconPathForAsset("disciple");
    }

    return combatIconPathForAsset(entry.voiceSpeakerId);
}

} // namespace

bool loadScript(const std::string& jsonPath, Script& outScript) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[VN Script] Failed to open: " << jsonPath << std::endl;
        return false;
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        std::cerr << "[VN Script] JSON parse error: " << e.what() << std::endl;
        return false;
    }

    try {
        outScript.chapter = j.value("chapter", 0);
        outScript.title = j.value("title", "");
        outScript.scriptId = canonicalScriptId(j.value("scriptId", ""));
        if (outScript.scriptId.empty()) {
            outScript.scriptId = canonicalScriptId(std::filesystem::path(jsonPath).filename().string());
        }
        outScript.endReturnScreen = j.value("endReturnScreen", "");
        outScript.credits = j.value("credits", false);
        outScript.entries.clear();

        if (!j.contains("entries") || !j["entries"].is_array()) {
            std::cerr << "[VN Script] No 'entries' array found in JSON" << std::endl;
            return false;
        }

        for (const auto& entryJson : j["entries"]) {
            ScriptEntry entry;

            entry.speaker = entryJson.value("speaker", "");
            entry.color = entryJson.value("color", "");
            entry.text = entryJson.value("text", "");
            
            // Parse entry type
            std::string typeStr = entryJson.value("type", "dialogue");
            if (typeStr == "thought") {
                entry.type = EntryType::Thought;
            } else if (typeStr == "narration") {
                entry.type = EntryType::Narration;
            } else {
                entry.type = EntryType::Dialogue;
            }

            entry.background = entryJson.value("background", "");
            entry.clearBackground = entryJson.value("clearBackground", false);
            entry.voice = entryJson.value("voice", "");
            entry.voiceSpeakerId = entryJson.value("voiceSpeakerId", "");
            entry.bgm = entryJson.value("bgm", "");
            if (const auto bgmVolumeIt = entryJson.find("bgmVolume");
                bgmVolumeIt != entryJson.end() && bgmVolumeIt->is_number()) {
                entry.bgmVolume = bgmVolumeIt->get<float>();
            }
            entry.bgmStop = entryJson.value("bgmStop", false);
            if (const auto bgmPauseIt = entryJson.find("bgmPause");
                bgmPauseIt != entryJson.end() && bgmPauseIt->is_boolean()) {
                entry.bgmPause = bgmPauseIt->get<bool>();
            }
            entry.fontPath = entryJson.value("fontPath", "");
            entry.icon = entryJson.value("icon", "");
            entry.iconFrameCount = entryJson.value("iconFrameCount", 1);
            entry.iconFps = entryJson.value("iconFps", 8.0f);
            entry.autoAdvanceOnVoiceEnd = entryJson.value("autoAdvanceOnVoiceEnd", false);
            entry.battleFocus = entryJson.value("battleFocus", "");
            entry.scriptedAction = entryJson.value("scriptedAction", "");
            entry.bossTitleOverride = entryJson.value("bossTitleOverride", "");
            entry.battleKey = entryJson.value("battleKey", "");
            entry.battleId = entryJson.value("battleId", -1);
            entry.battleWinScript = canonicalScriptId(entryJson.value("battleWinScript", ""));
            entry.battleLoseScript = canonicalScriptId(entryJson.value("battleLoseScript", ""));

            if (entry.icon.empty()) {
                const std::string fallbackIcon = defaultIconForDialogueEntry(entry);
                if (!fallbackIcon.empty()) {
                    entry.icon = fallbackIcon;
                }
            }

            outScript.entries.push_back(entry);
        }

        std::cout << "[VN Script] Loaded chapter " << outScript.chapter 
                  << " (" << outScript.title << ") with " 
                  << outScript.entries.size() << " entries" << std::endl;

        return true;

    } catch (const json::exception& e) {
        std::cerr << "[VN Script] Error parsing JSON structure: " << e.what() << std::endl;
        return false;
    }
}

std::string getDisplaySpeakerName(const ScriptEntry& entry) {
    switch (entry.type) {
        case EntryType::Thought:
            return entry.speaker + ", thoughts:";
        case EntryType::Narration:
            return ""; // Narration typically doesn't show a speaker name
        case EntryType::Dialogue:
        default:
            return entry.speaker;
    }
}

} // namespace vn
