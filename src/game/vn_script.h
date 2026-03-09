#ifndef VN_SCRIPT_H
#define VN_SCRIPT_H

#include <string>
#include <vector>

namespace vn {

enum class EntryType {
    Dialogue,
    Thought,
    Narration
};

struct ScriptEntry {
    std::string speaker;
    std::string text;
    EntryType type;
    std::string background;
    std::string voice;
    std::string fontPath;
    std::string icon;
    int iconFrameCount;
    float iconFps;
    bool autoAdvanceOnVoiceEnd;

    ScriptEntry()
        : type(EntryType::Dialogue)
        , iconFrameCount(1)
        , iconFps(8.0f)
        , autoAdvanceOnVoiceEnd(false)
    {}
};

struct Script {
    int chapter;
    std::string title;
    std::vector<ScriptEntry> entries;
};

// Load a VN script from a JSON file
// Returns true on success, false on error
bool loadScript(const std::string& jsonPath, Script& outScript);

// Get a display name for the speaker based on entry type
// For thoughts, returns "Speaker, thoughts:" format
// For narration, returns empty string (narrator doesn't show name box)
std::string getDisplaySpeakerName(const ScriptEntry& entry);

} // namespace vn

#endif // VN_SCRIPT_H
