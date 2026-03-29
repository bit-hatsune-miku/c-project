#ifndef VN_SYSTEM_H
#define VN_SYSTEM_H

#include <SDL2/SDL.h>
#include <optional>
#include <string>

/**
 * Holds the current presentation state exposed to callers.
 *
 * Contains visible and full RML text, speaker and asset paths, typewriter progress,
 * and whether the current line has finished.
 */

/**
 * Initialize the visual-novel presentation system with an SDL renderer and window size.
 * @param renderer SDL_Renderer used for rendering.
 * @param windowWidth Window width in pixels.
 * @param windowHeight Window height in pixels.
 * @returns `true` if initialization succeeded, `false` otherwise.
 */

/**
 * Shut down the presentation system and release resources.
 */

/**
 * Update the internal viewport size to match the given window dimensions.
 * @param windowWidth Window width in pixels.
 * @param windowHeight Window height in pixels.
 */

/**
 * Set the current speaker name used for subsequently shown lines.
 * @param name Speaker name string (empty to clear).
 */

/**
 * Set the current speaker icon image and optional animation parameters.
 * @param imagePath Filesystem path to the icon image.
 * @param frameCount Number of frames in the icon animation (default 1).
 * @param fps Animation frames per second (default 8.0f).
 */

/**
 * Set the current background image path used for subsequently shown lines.
 * @param imagePath Filesystem path to the background image.
 */

/**
 * Set the voice audio file that will be played for subsequent lines.
 * @param wavPath Filesystem path to a WAV file (empty to clear).
 */

/**
 * Set the background music (BGM) audio file and default playback volume.
 * @param wavPath Filesystem path to a WAV file (empty to clear).
 * @param volume01 Playback volume in range [0.0, 1.0].
 */

/**
 * Stop background music playback immediately.
 */

/**
 * Pause or resume background music playback.
 * @param paused `true` to pause BGM, `false` to resume.
 */

/**
 * Report whether a BGM track is currently loaded for playback.
 * @returns `true` if a BGM track is available, `false` otherwise.
 */

/**
 * Report whether BGM playback is currently paused.
 * @returns `true` if BGM is paused, `false` otherwise.
 */

/**
 * Set the font used for rendering text.
 * @param fontPath Filesystem path to a font file.
 * @param ptSize Point size for the font (default 28).
 */

/**
 * Enable or disable automatic line advance when voice playback ends.
 * @param enabled `true` to auto-advance on voice end, `false` to disable.
 */

/**
 * Set the typewriter speed in characters per second.
 * @param charsPerSecond Characters per second for typewriter effect.
 */

/**
 * Set the global music volume (affects music tracks).
 * @param volume01 Volume in range [0.0, 1.0].
 */

/**
 * Set the voice playback volume.
 * @param volume01 Volume in range [0.0, 1.0].
 */

/**
 * Set the BGM playback volume.
 * @param volume01 Volume in range [0.0, 1.0].
 */

/**
 * Get the configured typewriter speed in characters per second.
 * @returns The configured characters-per-second speed.
 */

/**
 * Get the configured music volume.
 * @returns The music volume in range [0.0, 1.0].
 */

/**
 * Get the configured voice volume.
 * @returns The voice volume in range [0.0, 1.0].
 */

/**
 * Get the configured BGM volume.
 * @returns The BGM volume in range [0.0, 1.0].
 */

/**
 * Set the current full text content used for subsequent lines.
 * @param text Text (RML) to display.
 */

/**
 * Begin a new line using the currently configured content and media.
 */

/**
 * Display a line with optional overrides for speaker, icon, voice, font, background, and BGM.
 *
 * Parameters allow controlling icon animation, BGM volume/stop/pause behavior, and
 * whether to auto-advance when voice ends.
 *
 * @param text Text (RML) to display.
 * @param speakerName Optional speaker name override.
 * @param iconPath Optional icon image path override.
 * @param voicePath Optional voice WAV path override.
 * @param fontPath Optional font path override.
 * @param autoAdvanceOnVoiceEnd If `true`, advance automatically when voice ends.
 * @param iconFrameCount Number of frames for icon animation (default 1).
 * @param iconFps Icon animation frames per second (default 8.0f).
 * @param backgroundPath Optional background image path override.
 * @param bgmPath Optional BGM WAV path to start for this line.
 * @param bgmVolume BGM volume for this line; negative value leaves volume unchanged.
 * @param bgmStop If `true`, stop any currently playing BGM before applying bgmPath.
 * @param bgmPause Optional pause state to apply to BGM playback.
 */

/**
 * Advance the presentation simulation by the given elapsed time.
 * @param deltaSeconds Time elapsed since last update, in seconds.
 */

/**
 * Set or clear the paused state of the presentation.
 * @param paused `true` to pause, `false` to resume.
 */

/**
 * Query whether the presentation is currently paused.
 * @returns `true` if paused, `false` otherwise.
 */

/**
 * Stop any currently playing voice audio.
 */

/**
 * Query whether a voice audio track is currently playing.
 * @returns `true` if voice audio is playing, `false` otherwise.
 */

/**
 * Reset the presentation state to baseline defaults.
 */

/**
 * Render the current presentation to the configured SDL renderer.
 */

/**
 * Handle a Space key press (advance or skip behavior depends on current state).
 */

/**
 * Report whether the currently displayed line has finished.
 * @returns `true` if the line is finished, `false` otherwise.
 */

/**
 * Report whether the system is waiting for an external advance action.
 * @returns `true` if waiting for advance, `false` otherwise.
 */

/**
 * Consume and report whether an advance request was pending.
 * @returns `true` if an advance request was consumed, `false` otherwise.
 */

/**
 * Get a snapshot of the current presentation state.
 * @returns A PresentationState containing current speaker, text, asset paths, and progress counters.
 */
namespace vn {

struct PresentationState {
    std::string speakerName;
    std::string visibleTextRml;
    std::string fullTextRml;
    std::string iconPath;
    std::string backgroundPath;
    std::size_t visibleCharacters = 0;
    std::size_t totalVisibleCharacters = 0;
    bool lineFinished = false;
};

bool initialize(SDL_Renderer* renderer, int windowWidth, int windowHeight);
void shutdown();
void setViewportSize(int windowWidth, int windowHeight);

void setSpeakerName(const std::string& name);
void setIcon(const std::string& imagePath, int frameCount = 1, float fps = 8.0f);
void setBackground(const std::string& imagePath);
void setVoice(const std::string& wavPath);
void setBgm(const std::string& wavPath, float volume01 = 1.0f);
void stopBgmPlayback();
void setBgmPaused(bool paused);
bool hasBgmPlayback();
bool isBgmPlaybackPaused();
void setFont(const std::string& fontPath, int ptSize = 28);
void setAutoAdvanceOnVoiceEnd(bool enabled = false);
void setTypewriterSpeed(float charsPerSecond);
void setMusicVolume(float volume01);
void setVoiceVolume(float volume01);
void setBgmVolume(float volume01);
float getTypewriterSpeed();
float getMusicVolume();
float getVoiceVolume();
float getBgmVolume();
void setText(const std::string& text);

void startLine();
void showLine(
    const std::string& text,
    const std::string& speakerName = "",
    const std::string& iconPath = "",
    const std::string& voicePath = "",
    const std::string& fontPath = "",
    bool autoAdvanceOnVoiceEnd = false,
    int iconFrameCount = 1,
    float iconFps = 8.0f,
    const std::string& backgroundPath = "",
    const std::string& bgmPath = "",
    float bgmVolume = -1.0f,
    bool bgmStop = false,
    std::optional<bool> bgmPause = std::nullopt
);

void update(float deltaSeconds);
void setPaused(bool paused);
bool isPaused();
void stopVoicePlayback();
bool isVoicePlaying();
void reset();
void render();
void onSpacePressed();

bool isLineFinished();
bool isWaitingForAdvance();
bool consumeAdvanceRequest();
PresentationState getPresentationState();

} // namespace vn

#endif
