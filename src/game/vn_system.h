#ifndef VN_SYSTEM_H
#define VN_SYSTEM_H

#include <SDL2/SDL.h>
#include <string>

namespace vn {

bool initialize(SDL_Renderer* renderer, int windowWidth, int windowHeight);
void shutdown();
void setViewportSize(int windowWidth, int windowHeight);

void setSpeakerName(const std::string& name);
void setIcon(const std::string& imagePath, int frameCount = 1, float fps = 8.0f);
void setBackground(const std::string& imagePath);
void setVoice(const std::string& wavPath);
void setFont(const std::string& fontPath, int ptSize = 28);
void setAutoAdvanceOnVoiceEnd(bool enabled = false);
void setTypewriterSpeed(float charsPerSecond);
void setVoiceVolume(float volume01);
float getTypewriterSpeed();
float getVoiceVolume();
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
    const std::string& backgroundPath = ""
);

void update(float deltaSeconds);
void setPaused(bool paused);
bool isPaused();
void stopVoicePlayback();
void render();
void onSpacePressed();

bool isLineFinished();
bool isWaitingForAdvance();
bool consumeAdvanceRequest();

} // namespace vn

#endif
