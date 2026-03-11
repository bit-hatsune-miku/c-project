#include "vn_system.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
constexpr int kBaseWindowW = 1280;
constexpr int kBaseWindowH = 720;

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
float gVoiceVolume = 0.85f;
float gTypeAccumulator = 0.0f;
size_t gVisibleChars = 0;
size_t gTotalVisibleChars = 0;
bool gAutoAdvanceOnVoiceEnd = false;
bool gAdvanceRequested = false;
bool gPaused = false;

// Basic WAV playback via SDL audio queue (no external mixer needed)
SDL_AudioDeviceID gAudioDevice = 0;
Uint8* gLoadedWavBuffer = nullptr;
Uint32 gLoadedWavLength = 0;
bool gVoicePlaying = false;
bool gImageInitialized = false;

#ifdef VN_ENABLE_TTF
TTF_Font* gFont = nullptr;
int gFontSize = 28;
int gBaseFontSize = 28;
#endif

SDL_Rect dialogueBoxRect() {
    const float scale = std::min(
        static_cast<float>(gWindowW) / static_cast<float>(kBaseWindowW),
        static_cast<float>(gWindowH) / static_cast<float>(kBaseWindowH)
    );
    const int marginX = static_cast<int>(std::lround(40.0f * scale));
    const int boxH = static_cast<int>(std::lround(200.0f * scale));
    const int bottomMargin = static_cast<int>(std::lround(40.0f * scale));
    return SDL_Rect{marginX, gWindowH - bottomMargin - boxH, gWindowW - marginX * 2, boxH};
}

void freeLoadedVoiceBuffer() {
    if (gLoadedWavBuffer != nullptr) {
        SDL_FreeWAV(gLoadedWavBuffer);
        gLoadedWavBuffer = nullptr;
        gLoadedWavLength = 0;
    }
}

void closeAudioDeviceIfOpen() {
    if (gAudioDevice != 0) {
        SDL_CloseAudioDevice(gAudioDevice);
        gAudioDevice = 0;
    }
}

void stopAndFreeVoiceBuffer() {
    if (gAudioDevice != 0) {
        SDL_ClearQueuedAudio(gAudioDevice);
    }
    gVoicePlaying = false;
    freeLoadedVoiceBuffer();
    closeAudioDeviceIfOpen();
}

void playVoiceIfAny() {
    stopAndFreeVoiceBuffer();

    if (gVoicePath.empty() || gVoiceVolume <= 0.0f) {
        return;
    }

    SDL_AudioSpec wavSpec{};
    if (SDL_LoadWAV(gVoicePath.c_str(), &wavSpec, &gLoadedWavBuffer, &gLoadedWavLength) == nullptr) {
        std::cerr << "[VN] Could not load voice WAV: " << gVoicePath << " (" << SDL_GetError() << ")\n";
        return;
    }

    gAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &wavSpec, nullptr, 0);
    if (gAudioDevice == 0) {
        std::cerr << "[VN] Could not open audio device: " << SDL_GetError() << "\n";
        stopAndFreeVoiceBuffer();
        return;
    }

    const int sdlVolume = static_cast<int>(std::clamp(gVoiceVolume, 0.0f, 1.0f) * SDL_MIX_MAXVOLUME);
    if (sdlVolume >= SDL_MIX_MAXVOLUME) {
        if (SDL_QueueAudio(gAudioDevice, gLoadedWavBuffer, gLoadedWavLength) != 0) {
            std::cerr << "[VN] Could not queue audio: " << SDL_GetError() << "\n";
            stopAndFreeVoiceBuffer();
            return;
        }
    } else {
        std::vector<Uint8> mixedBuffer(gLoadedWavLength, 0);
        SDL_MixAudioFormat(mixedBuffer.data(), gLoadedWavBuffer, wavSpec.format, gLoadedWavLength, sdlVolume);

        if (SDL_QueueAudio(gAudioDevice, mixedBuffer.data(), gLoadedWavLength) != 0) {
            std::cerr << "[VN] Could not queue audio: " << SDL_GetError() << "\n";
            stopAndFreeVoiceBuffer();
            return;
        }
    }

    freeLoadedVoiceBuffer();
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

int utf8CodepointLength(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

std::string toLowerCopy(const std::string& s) {
    std::string out = s;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool parseHexColorTag(const std::string& tagLower, SDL_Color& outColor) {
    const std::string prefix = "color=#";
    if (tagLower.rfind(prefix, 0) != 0) {
        return false;
    }

    const std::string hex = tagLower.substr(prefix.size());
    if (hex.size() != 6 && hex.size() != 8) {
        return false;
    }

    auto hexByte = [](const std::string& text, size_t offset) -> int {
        return std::stoi(text.substr(offset, 2), nullptr, 16);
    };

    try {
        outColor.r = static_cast<Uint8>(hexByte(hex, 0));
        outColor.g = static_cast<Uint8>(hexByte(hex, 2));
        outColor.b = static_cast<Uint8>(hexByte(hex, 4));
        outColor.a = static_cast<Uint8>(hex.size() == 8 ? hexByte(hex, 6) : 255);
    } catch (...) {
        return false;
    }

    return true;
}

size_t countVisibleCharsIgnoringTags(const std::string& text) {
    size_t count = 0;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '<') {
            const size_t close = text.find('>', i + 1);
            if (close != std::string::npos) {
                const std::string tagLower = toLowerCopy(text.substr(i + 1, close - i - 1));
                if (tagLower == "b" || tagLower == "/b" ||
                    tagLower == "i" || tagLower == "/i" ||
                    tagLower == "/color" || tagLower == "br" || tagLower == "br/" ||
                    tagLower.rfind("color=#", 0) == 0) {
                    if (tagLower == "br" || tagLower == "br/") {
                        ++count;
                    }
                    i = close + 1;
                    continue;
                }
            }
        }

        const int len = std::max(1, utf8CodepointLength(static_cast<unsigned char>(text[i])));
        i += static_cast<size_t>(len);
        ++count;
    }

    return count;
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

void drawRichText(const std::string& text, const SDL_Color& defaultColor, const SDL_Rect& area, size_t maxVisibleChars) {
    if (text.empty() || gFont == nullptr || maxVisibleChars == 0) {
        return;
    }

    int boldDepth = 0;
    int italicDepth = 0;
    std::vector<SDL_Color> colorStack;
    colorStack.push_back(defaultColor);

    int x = area.x;
    int y = area.y;
    const int maxX = area.x + area.w;
    const int maxY = area.y + area.h;
    const int lineSkip = TTF_FontLineSkip(gFont);

    size_t visibleCount = 0;
    size_t i = 0;

    auto newline = [&]() {
        x = area.x;
        y += lineSkip;
    };

    auto isOutOfArea = [&]() {
        return y + lineSkip > maxY;
    };

    while (i < text.size() && visibleCount < maxVisibleChars && !isOutOfArea()) {
        if (text[i] == '<') {
            const size_t close = text.find('>', i + 1);
            if (close != std::string::npos) {
                const std::string tagLower = toLowerCopy(text.substr(i + 1, close - i - 1));

                if (tagLower == "b") {
                    ++boldDepth;
                    i = close + 1;
                    continue;
                }
                if (tagLower == "/b") {
                    boldDepth = std::max(0, boldDepth - 1);
                    i = close + 1;
                    continue;
                }
                if (tagLower == "i") {
                    ++italicDepth;
                    i = close + 1;
                    continue;
                }
                if (tagLower == "/i") {
                    italicDepth = std::max(0, italicDepth - 1);
                    i = close + 1;
                    continue;
                }
                if (tagLower == "br" || tagLower == "br/") {
                    newline();
                    ++visibleCount;
                    i = close + 1;
                    continue;
                }
                if (tagLower == "/color") {
                    if (colorStack.size() > 1) {
                        colorStack.pop_back();
                    }
                    i = close + 1;
                    continue;
                }

                SDL_Color parsedColor{};
                if (parseHexColorTag(tagLower, parsedColor)) {
                    colorStack.push_back(parsedColor);
                    i = close + 1;
                    continue;
                }
            }
        }

        const int codeLen = std::max(1, utf8CodepointLength(static_cast<unsigned char>(text[i])));
        const std::string glyph = text.substr(i, static_cast<size_t>(codeLen));
        i += static_cast<size_t>(codeLen);

        if (glyph == "\n") {
            newline();
            ++visibleCount;
            continue;
        }

        int styleFlags = TTF_STYLE_NORMAL;
        if (boldDepth > 0) styleFlags |= TTF_STYLE_BOLD;
        if (italicDepth > 0) styleFlags |= TTF_STYLE_ITALIC;
        TTF_SetFontStyle(gFont, styleFlags);

        int glyphW = 0;
        int glyphH = 0;
        if (TTF_SizeUTF8(gFont, glyph.c_str(), &glyphW, &glyphH) != 0) {
            ++visibleCount;
            continue;
        }

        if (x + glyphW > maxX && x > area.x) {
            newline();
            if (isOutOfArea()) {
                break;
            }
        }

        SDL_Surface* surface = TTF_RenderUTF8_Blended(gFont, glyph.c_str(), colorStack.back());
        if (surface != nullptr) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(gRenderer, surface);
            if (tex != nullptr) {
                SDL_Rect dst{x, y, surface->w, surface->h};
                SDL_RenderCopy(gRenderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surface);
        }

        x += glyphW;
        ++visibleCount;
    }

    TTF_SetFontStyle(gFont, TTF_STYLE_NORMAL);
}

void reloadScaledFont() {
    if (gRenderer == nullptr) {
        return;
    }

    const float scale = std::min(
        static_cast<float>(gWindowW) / static_cast<float>(kBaseWindowW),
        static_cast<float>(gWindowH) / static_cast<float>(kBaseWindowH)
    );
    const int scaledSize = std::max(8, static_cast<int>(std::lround(static_cast<float>(gBaseFontSize) * scale)));
    if (scaledSize == gFontSize && gFont != nullptr) {
        return;
    }

    gFontSize = scaledSize;
    if (gFont != nullptr) {
        TTF_CloseFont(gFont);
        gFont = nullptr;
    }
    ensureFontLoaded();
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
    reloadScaledFont();
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

void setViewportSize(int windowWidth, int windowHeight) {
    gWindowW = std::max(1, windowWidth);
    gWindowH = std::max(1, windowHeight);
#ifdef VN_ENABLE_TTF
    reloadScaledFont();
#endif
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
    gBaseFontSize = std::max(8, ptSize);

    if (gFont != nullptr) {
        TTF_CloseFont(gFont);
        gFont = nullptr;
    }

    reloadScaledFont();
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

void setVoiceVolume(float volume01) {
    gVoiceVolume = std::clamp(volume01, 0.0f, 1.0f);
}

float getTypewriterSpeed() {
    return gCharsPerSecond;
}

float getVoiceVolume() {
    return gVoiceVolume;
}

void setText(const std::string& text) {
    gText = text;
#ifdef VN_ENABLE_TTF
    gTotalVisibleChars = countVisibleCharsIgnoringTags(gText);
#else
    gTotalVisibleChars = gText.size();
#endif
}

void startLine() {
    gPaused = false;
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
    if (gPaused) {
        return;
    }

    if (gVisibleChars < gTotalVisibleChars) {
        gTypeAccumulator += deltaSeconds * gCharsPerSecond;
        const size_t add = static_cast<size_t>(gTypeAccumulator);
        if (add > 0) {
            gVisibleChars = std::min(gTotalVisibleChars, gVisibleChars + add);
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

void setPaused(bool paused) {
    gPaused = paused;
    if (gAudioDevice != 0 && gVoicePlaying) {
        SDL_PauseAudioDevice(gAudioDevice, paused ? 1 : 0);
    }
}

bool isPaused() {
    return gPaused;
}

void stopVoicePlayback() {
    stopAndFreeVoiceBuffer();
}

void render() {
    if (gRenderer == nullptr) {
        return;
    }

    const float uiScale = std::min(
        static_cast<float>(gWindowW) / static_cast<float>(kBaseWindowW),
        static_cast<float>(gWindowH) / static_cast<float>(kBaseWindowH)
    );

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

    const int textInsetX = static_cast<int>(std::lround(24.0f * uiScale));
    const int textInsetY = static_cast<int>(std::lround(18.0f * uiScale));
    const int iconInset = static_cast<int>(std::lround(16.0f * uiScale));
    const int textBottomInset = static_cast<int>(std::lround(24.0f * uiScale));
    int textStartX = box.x + textInsetX;

    if (gIconTexture != nullptr) {
        const int iconSize = box.h - iconInset * 2;
        SDL_Rect iconSrc{gIconCurrentFrame * gIconFrameWidth, 0, gIconFrameWidth, gIconFrameHeight};
        SDL_Rect iconDst{box.x + iconInset, box.y + iconInset, iconSize, iconSize};
        SDL_RenderCopy(gRenderer, gIconTexture, &iconSrc, &iconDst);
        textStartX = iconDst.x + iconDst.w + iconInset;
    }

#ifdef VN_ENABLE_TTF
    ensureFontLoaded();

    const SDL_Color textColor{235, 235, 240, 255};
    const SDL_Color nameColor{255, 255, 255, 255};  // Bright white for better readability

    if (!gSpeakerName.empty()) {
        const int nameInsetX = static_cast<int>(std::lround(12.0f * uiScale));
        const int nameHeight = static_cast<int>(std::lround(28.0f * uiScale));
        const int nameGap = static_cast<int>(std::lround(10.0f * uiScale));
        SDL_Rect nameArea{box.x + nameInsetX, box.y - nameHeight - nameGap, box.w - nameInsetX * 2, nameHeight};
        drawText(gSpeakerName, nameColor, nameArea, true);
    }

    SDL_Rect textArea{
        textStartX,
        box.y + textInsetY,
        box.w - (textStartX - box.x) - textInsetY,
        box.h - textInsetY - textBottomInset
    };
    drawRichText(gText, textColor, textArea, gVisibleChars);
#else
    (void)textStartX;
#endif
}

void onSpacePressed() {
    if (!isLineFinished()) {
        gVisibleChars = gTotalVisibleChars;
        return;
    }

    gAdvanceRequested = true;
}

bool isLineFinished() {
    return gVisibleChars >= gTotalVisibleChars;
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
