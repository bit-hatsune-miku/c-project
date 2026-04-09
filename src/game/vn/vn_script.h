#ifndef VN_SCRIPT_H
#define VN_SCRIPT_H

#include <optional>
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
    std::string color;
    std::string text;
    EntryType type;
    std::string background;
    bool clearBackground;
    std::string voice;
    std::string voiceSpeakerId;
    std::string bgm;
    float bgmVolume;
    bool bgmStop;
    std::optional<bool> bgmPause;
    std::string fontPath;
    std::string icon;
    int iconFrameCount;
    float iconFps;
    bool autoAdvanceOnVoiceEnd;
    std::string battleFocus;
    std::string scriptedAction;
    std::string bossTitleOverride;
    std::string battleKey;       // Preferred way to trigger combat from VN.
    int battleId;                // Legacy fallback; kept for compatibility.
    std::string battleWinScript; // Optional JSON script to load after winning the battle.
    std::string battleLoseScript;// Optional JSON script to load after losing the battle.

    ScriptEntry()
        : type(EntryType::Dialogue)
        , clearBackground(false)
        , bgm()
        , bgmVolume(-1.0f)
        , bgmStop(false)
        , bgmPause(std::nullopt)
        , iconFrameCount(1)
        , iconFps(8.0f)
        , autoAdvanceOnVoiceEnd(false)
        , battleFocus()
        , scriptedAction()
        , bossTitleOverride()
        , battleKey()
        , battleId(-1)
        , battleWinScript()
        , battleLoseScript()
    {}
};

struct Script {
    int chapter;
    std::string title;
    std::string scriptId;
    std::string endReturnScreen;
    bool credits;
    std::vector<ScriptEntry> entries;

    Script()
        : chapter(0)
        , title()
        , scriptId()
        , endReturnScreen()
        , credits(false)
        , entries()
    {}
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
