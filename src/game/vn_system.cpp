#include "vn_system.h"

#include <algorithm>
#include <iostream>
#include <vector>

#ifdef VN_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif
#ifdef VN_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace vn {
namespace {

SDL_Renderer* gRenderer = nullptr;
int gWindowW = 1280;
int gWindowH = 720;

std::string gSpeakerName;
std::string gText;
std::string gVoicePath;
std::string gFontPath;

SDL_Texture* gIconTexture = nullptr;
int gIconFrameCount = 1;
int gIconFrameWidth = 96;
int gIconFrameHeight = 96;
float gIconFps = 8.0f;
float gIconAnimTime = 0.0f;
int gIconCurrentFrame = 0;

SDL_Texture* gBackgroundTexture = nullptr;

float gCharsPerSecond = 45.0f;
float gTypeAccumulator = 0.0f;
size_t gVisibleChars = 0;
bool gAutoAdvanceOnVoiceEnd = false;
bool gAdvanceRequested = false;

// Basic WAV playback via SDL audio queue (no external mixer needed)
SDL_AudioDeviceID gAudioDevice = 0;
Uint8* gLoadedWavBuffer = nullptr;
Uint32 gLoadedWavLength = 0;
bool gVoicePlaying = false;
bool gImageInitialized = false;

#ifdef VN_ENABLE_TTF
TTF_Font* gFont = nullptr;
int gFontSize = 28;
#endif

SDL_Rect dialogueBoxRect() {
    return SDL_Rect{40, gWindowH - 240, gWindowW - 80, 200};
}

void stopAndFreeVoiceBuffer() {
    if (gAudioDevice != 0) {
        SDL_ClearQueuedAudio(gAudioDevice);
    }
    gVoicePlaying = false;

    if (gLoadedWavBuffer != nullptr) {
        SDL_FreeWAV(gLoadedWavBuffer);
        gLoadedWavBuffer = nullptr;
        gLoadedWavLength = 0;
    }
}

void playVoiceIfAny() {
    stopAndFreeVoiceBuffer();

    if (gVoicePath.empty()) {
        return;
    }

    SDL_AudioSpec wavSpec{};
    if (SDL_LoadWAV(gVoicePath.c_str(), &wavSpec, &gLoadedWavBuffer, &gLoadedWavLength) == nullptr) {
        std::cerr << "[VN] Could not load voice WAV: " << gVoicePath << " (" << SDL_GetError() << ")\n";
        return;
    }

    // Close and reopen audio device to match the WAV file's format
    if (gAudioDevice != 0) {
        SDL_CloseAudioDevice(gAudioDevice);
        gAudioDevice = 0;
    }

    gAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &wavSpec, nullptr, 0);
    if (gAudioDevice == 0) {
        std::cerr << "[VN] Could not open audio device: " << SDL_GetError() << "\n";
        stopAndFreeVoiceBuffer();
        return;
    }

    if (SDL_QueueAudio(gAudioDevice, gLoadedWavBuffer, gLoadedWavLength) != 0) {
        std::cerr << "[VN] Could not queue audio: " << SDL_GetError() << "\n";
        stopAndFreeVoiceBuffer();
        return;
    }

    SDL_PauseAudioDevice(gAudioDevice, 0);
    gVoicePlaying = true;
}

#ifdef VN_ENABLE_TTF
TTF_Font* openBestAvailableFont(const std::string& preferredPath, int ptSize) {
    std::vector<std::string> candidates;
    if (!preferredPath.empty()) {
        candidates.push_back(preferredPath);
    }

    candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");

    for (const auto& path : candidates) {
        TTF_Font* font = TTF_OpenFont(path.c_str(), ptSize);
        if (font != nullptr) {
            return font;
        }
    }

    return nullptr;
}

void ensureFontLoaded() {
    if (gFont != nullptr) {
        return;
    }

    gFont = openBestAvailableFont(gFontPath, gFontSize);
    if (gFont == nullptr) {
        std::cerr << "[VN] No usable TTF font found. Install SDL2_ttf + DejaVu fonts or set a font path.\n";
    }
}

void drawText(const std::string& text, const SDL_Color& color, const SDL_Rect& area, bool centered) {
    if (text.empty() || gFont == nullptr) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(gFont, text.c_str(), color, static_cast<Uint32>(area.w));
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(gRenderer, surface);
    if (tex == nullptr) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst = area;
    dst.w = surface->w;
    dst.h = surface->h;

    if (centered) {
        dst.x = area.x + (area.w - dst.w) / 2;
    }

    SDL_RenderCopy(gRenderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surface);
}
#endif

} // namespace

bool initialize(SDL_Renderer* renderer, int windowWidth, int windowHeight) {
    gRenderer = renderer;
    gWindowW = windowWidth;
    gWindowH = windowHeight;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "[VN] Audio subsystem init failed: " << SDL_GetError() << "\n";
    }

#ifdef VN_ENABLE_TTF
    if (TTF_Init() == -1) {
        std::cerr << "[VN] TTF init failed: " << TTF_GetError() << "\n";
    }
    ensureFontLoaded();
#else
    std::cout << "[VN] Built without SDL2_ttf. Text rendering is disabled until SDL2_ttf is installed.\n";
#endif

#ifdef VN_ENABLE_IMAGE
    const int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    const int inited = IMG_Init(imgFlags);
    if ((inited & imgFlags) != imgFlags) {
        std::cerr << "[VN] SDL2_image init incomplete: " << IMG_GetError() << "\n";
    } else {
        gImageInitialized = true;
    }
#else
    std::cout << "[VN] Built without SDL2_image. Icons support BMP only.\n";
#endif

    return gRenderer != nullptr;
}

void shutdown() {
    if (gIconTexture != nullptr) {
        SDL_DestroyTexture(gIconTexture);
        gIconTexture = nullptr;
    }

    if (gBackgroundTexture != nullptr) {
        SDL_DestroyTexture(gBackgroundTexture);
        gBackgroundTexture = nullptr;
    }

    stopAndFreeVoiceBuffer();

    if (gAudioDevice != 0) {
        SDL_CloseAudioDevice(gAudioDevice);
        gAudioDevice = 0;
    }

#ifdef VN_ENABLE_TTF
    if (gFont != nullptr) {
        TTF_CloseFont(gFont);
        gFont = nullptr;
    }
    TTF_Quit();
#endif

#ifdef VN_ENABLE_IMAGE
    if (gImageInitialized) {
        IMG_Quit();
        gImageInitialized = false;
    }
#endif
}

void setSpeakerName(const std::string& name) {
    gSpeakerName = name;
}

void setIcon(const std::string& imagePath, int frameCount, float fps) {
    if (gIconTexture != nullptr) {
        SDL_DestroyTexture(gIconTexture);
        gIconTexture = nullptr;
    }

    gIconFrameCount = std::max(1, frameCount);
    gIconFps = std::max(0.1f, fps);
    gIconAnimTime = 0.0f;
    gIconCurrentFrame = 0;

    if (imagePath.empty()) {
        return;
    }

    SDL_Surface* surface = nullptr;
#ifdef VN_ENABLE_IMAGE
    surface = IMG_Load(imagePath.c_str());
#else
    surface = SDL_LoadBMP(imagePath.c_str());
#endif
    if (surface == nullptr) {
        std::cerr << "[VN] Could not load icon image: " << imagePath << "\n";
        return;
    }

    gIconFrameWidth = surface->w / gIconFrameCount;
    gIconFrameHeight = surface->h;

    gIconTexture = SDL_CreateTextureFromSurface(gRenderer, surface);
    SDL_FreeSurface(surface);
}

void setBackground(const std::string& imagePath) {
    if (imagePath.empty()) {
        return;
    }

    if (gBackgroundTexture != nullptr) {
        SDL_DestroyTexture(gBackgroundTexture);
        gBackgroundTexture = nullptr;
    }

    SDL_Surface* surface = nullptr;
#ifdef VN_ENABLE_IMAGE
    surface = IMG_Load(imagePath.c_str());
#else
    surface = SDL_LoadBMP(imagePath.c_str());
#endif
    if (surface == nullptr) {
        std::cerr << "[VN] Could not load background image: " << imagePath << "\n";
        return;
    }

    gBackgroundTexture = SDL_CreateTextureFromSurface(gRenderer, surface);
    SDL_FreeSurface(surface);
}

void setVoice(const std::string& wavPath) {
    gVoicePath = wavPath;
}

void setFont(const std::string& fontPath, int ptSize) {
    gFontPath = fontPath;
#ifdef VN_ENABLE_TTF
    gFontSize = std::max(8, ptSize);

    if (gFont != nullptr) {
        TTF_CloseFont(gFont);
        gFont = nullptr;
    }

    ensureFontLoaded();
#else
    (void)ptSize;
#endif
}

void setAutoAdvanceOnVoiceEnd(bool enabled) {
    gAutoAdvanceOnVoiceEnd = enabled;
}

void setTypewriterSpeed(float charsPerSecond) {
    gCharsPerSecond = std::max(1.0f, charsPerSecond);
}

void setText(const std::string& text) {
    gText = text;
}

void startLine() {
    gVisibleChars = 0;
    gTypeAccumulator = 0.0f;
    gAdvanceRequested = false;
    playVoiceIfAny();
}

void showLine(
    const std::string& text,
    const std::string& speakerName,
    const std::string& iconPath,
    const std::string& voicePath,
    const std::string& fontPath,
    bool autoAdvanceOnVoiceEnd,
    int iconFrameCount,
    float iconFps,
    const std::string& backgroundPath
) {
    setText(text);
    setSpeakerName(speakerName);
    setIcon(iconPath, iconFrameCount, iconFps);
    setVoice(voicePath);
    if (!fontPath.empty()) {
        setFont(fontPath);
    }
    if (!backgroundPath.empty()) {
        setBackground(backgroundPath);
    }
    setAutoAdvanceOnVoiceEnd(autoAdvanceOnVoiceEnd);
    startLine();
}

void update(float deltaSeconds) {
    if (gVisibleChars < gText.size()) {
        gTypeAccumulator += deltaSeconds * gCharsPerSecond;
        const size_t add = static_cast<size_t>(gTypeAccumulator);
        if (add > 0) {
            gVisibleChars = std::min(gText.size(), gVisibleChars + add);
            gTypeAccumulator -= static_cast<float>(add);
        }
    }

    if (gIconFrameCount > 1) {
        gIconAnimTime += deltaSeconds;
        const float frameDuration = 1.0f / gIconFps;
        while (gIconAnimTime >= frameDuration) {
            gIconAnimTime -= frameDuration;
            gIconCurrentFrame = (gIconCurrentFrame + 1) % gIconFrameCount;
        }
    }

    if (gVoicePlaying && gAudioDevice != 0 && SDL_GetQueuedAudioSize(gAudioDevice) == 0) {
        gVoicePlaying = false;
    }

    if (gAutoAdvanceOnVoiceEnd && !gVoicePlaying && isLineFinished()) {
        gAdvanceRequested = true;
    }
}

void render() {
    if (gRenderer == nullptr) {
        return;
    }

    if (gBackgroundTexture != nullptr) {
        SDL_Rect bgRect{0, 0, gWindowW, gWindowH};
        SDL_RenderCopy(gRenderer, gBackgroundTexture, nullptr, &bgRect);
    }

    const SDL_Rect box = dialogueBoxRect();

    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 170);
    SDL_RenderFillRect(gRenderer, &box);

    SDL_SetRenderDrawColor(gRenderer, 225, 225, 235, 255);
    SDL_RenderDrawRect(gRenderer, &box);

    int textStartX = box.x + 24;

    if (gIconTexture != nullptr) {
        const int iconSize = box.h - 32;
        SDL_Rect iconSrc{gIconCurrentFrame * gIconFrameWidth, 0, gIconFrameWidth, gIconFrameHeight};
        SDL_Rect iconDst{box.x + 16, box.y + 16, iconSize, iconSize};
        SDL_RenderCopy(gRenderer, gIconTexture, &iconSrc, &iconDst);
        textStartX = iconDst.x + iconDst.w + 16;
    }

#ifdef VN_ENABLE_TTF
    ensureFontLoaded();

    const SDL_Color textColor{235, 235, 240, 255};
    const SDL_Color nameColor{255, 255, 255, 255};  // Bright white for better readability

    if (!gSpeakerName.empty()) {
        SDL_Rect nameArea{box.x + 12, box.y - 34, box.w - 24, 28};
        drawText(gSpeakerName, nameColor, nameArea, true);
    }

    std::string visible = gText.substr(0, gVisibleChars);
    SDL_Rect textArea{textStartX, box.y + 18, box.w - (textStartX - box.x) - 18, box.h - 24};
    drawText(visible, textColor, textArea, false);
#else
    (void)textStartX;
#endif
}

void onSpacePressed() {
    if (!isLineFinished()) {
        gVisibleChars = gText.size();
        return;
    }

    gAdvanceRequested = true;
}

bool isLineFinished() {
    return gVisibleChars >= gText.size();
}

bool isWaitingForAdvance() {
    return isLineFinished() && !gAdvanceRequested;
}

bool consumeAdvanceRequest() {
    const bool requested = gAdvanceRequested;
    gAdvanceRequested = false;
    return requested;
}

} // namespace vn
