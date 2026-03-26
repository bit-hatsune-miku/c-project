#include "vn_script.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vn {
namespace {

std::string defaultIconForCh0Speaker(const std::string& speaker) {
    if (speaker == "Miku") {
        return "assets/combat/icons/miku.png";
    }
    if (speaker == "Cupcakke") {
        return "assets/combat/icons/cupcakke.png";
    }
    if (speaker == "Lyoo") {
        return "assets/vn/icons/lyoo/0.png";
    }
    return std::string();
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
        outScript.entries.clear();

        if (!j.contains("entries") || !j["entries"].is_array()) {
            std::cerr << "[VN Script] No 'entries' array found in JSON" << std::endl;
            return false;
        }

        for (const auto& entryJson : j["entries"]) {
            ScriptEntry entry;

            entry.speaker = entryJson.value("speaker", "");
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
            entry.voice = entryJson.value("voice", "");
            entry.fontPath = entryJson.value("fontPath", "");
            entry.icon = entryJson.value("icon", "");
            entry.iconFrameCount = entryJson.value("iconFrameCount", 1);
            entry.iconFps = entryJson.value("iconFps", 8.0f);
            entry.autoAdvanceOnVoiceEnd = entryJson.value("autoAdvanceOnVoiceEnd", false);
            entry.battleKey = entryJson.value("battleKey", "");
            entry.battleId = entryJson.value("battleId", -1);
            entry.battleWinScript = entryJson.value("battleWinScript", "");
            entry.battleLoseScript = entryJson.value("battleLoseScript", "");

            // Chapter 0 data defaults requested by design:
            // - Miku/Cupcakke use combat icons
            // - Lyoo uses VN icon frame 0
            if (entry.icon.empty() && jsonPath.find("ch0.json") != std::string::npos) {
                const std::string fallbackIcon = defaultIconForCh0Speaker(entry.speaker);
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
