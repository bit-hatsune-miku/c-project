#include "vn_system.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>

#ifdef VN_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif
#ifdef VN_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "../audio/bgm_player.h"
#include "../../platform/path_resolution.h"
#include "../../platform/text_fallback.h"

namespace vn {
namespace {

SDL_Renderer* gRenderer = nullptr;
int gWindowW = 1280;
int gWindowH = 720;
constexpr int kBaseWindowW = 1280;
constexpr int kBaseWindowH = 720;
bool gInitialized = false;

std::string gSpeakerName;
std::string gText;
std::string gVoicePath;
std::string gBgmPath;
std::string gFontPath;
std::string gIconPath;
std::string gBackgroundPath;

SDL_Texture* gIconTexture = nullptr;
int gIconFrameCount = 1;
int gIconFrameWidth = 96;
int gIconFrameHeight = 96;
float gIconFps = 8.0f;
float gIconAnimTime = 0.0f;
int gIconCurrentFrame = 0;

SDL_Texture* gBackgroundTexture = nullptr;
SDL_Texture* gPreviousBackgroundTexture = nullptr;
float gBackgroundFadeElapsed = 0.0f;
bool gBackgroundFadeActive = false;
constexpr float kBackgroundFadeDuration = 0.30f;

float gCharsPerSecond = 45.0f;
float gBgmMasterVolume = 1.0f;
float gVoiceVolume = 0.85f;
float gBgmVolume = 1.0f;
float gBgmCurrentVolume = 0.0f;
float gTypeAccumulator = 0.0f;
size_t gVisibleChars = 0;
size_t gTotalVisibleChars = 0;
bool gAutoAdvanceOnVoiceEnd = false;
bool gAdvanceRequested = false;
bool gPaused = false;

// Basic WAV playback via SDL audio queue (no external mixer needed)
SDL_AudioDeviceID gAudioDevice = 0;
SDL_AudioSpec gLoadedWavSpec{};
Uint8* gLoadedWavBuffer = nullptr;
Uint32 gLoadedWavLength = 0;
bool gVoicePlaying = false;
bool gImageInitialized = false;
bool gTtfInitialized = false;
game::audio::BgmPlayer gBgmPlayer;
std::string gPendingBgmPath;
float gPendingBgmVolume = 1.0f;
bool gBgmFadeOutRequested = false;
bool gBgmPauseAfterFadeOut = false;
constexpr float kBgmFadeDuration = 0.35f;

#ifdef VN_ENABLE_TTF
TTF_Font* gFont = nullptr;
TTF_Font* gCjkFont = nullptr;
int gFontSize = 28;
int gBaseFontSize = 28;
#endif

int utf8CodepointLength(unsigned char c);

std::string toLowerAsciiCopy(const std::string& s) {
    std::string out = s;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

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

void destroyIconTexture() {
    if (gIconTexture != nullptr) {
        SDL_DestroyTexture(gIconTexture);
        gIconTexture = nullptr;
    }
}

void destroyBackgroundTexture() {
    if (gBackgroundTexture != nullptr) {
        SDL_DestroyTexture(gBackgroundTexture);
        gBackgroundTexture = nullptr;
    }
}

std::string escapeRmlText(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (char ch : text) {
        switch (ch) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
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

bool queueLoadedVoiceBuffer() {
    if (gAudioDevice == 0 || gLoadedWavBuffer == nullptr || gLoadedWavLength == 0) {
        return false;
    }

    SDL_ClearQueuedAudio(gAudioDevice);

    const int sdlVolume = static_cast<int>(std::clamp(gVoiceVolume, 0.0f, 1.0f) * SDL_MIX_MAXVOLUME);
    if (sdlVolume <= 0) {
        return true;
    }

    if (sdlVolume >= SDL_MIX_MAXVOLUME) {
        return SDL_QueueAudio(gAudioDevice, gLoadedWavBuffer, gLoadedWavLength) == 0;
    }

    std::vector<Uint8> mixedBuffer(gLoadedWavLength, 0);
    SDL_MixAudioFormat(mixedBuffer.data(), gLoadedWavBuffer, gLoadedWavSpec.format, gLoadedWavLength, sdlVolume);
    return SDL_QueueAudio(gAudioDevice, mixedBuffer.data(), gLoadedWavLength) == 0;
}

void playVoiceIfAny() {
    stopAndFreeVoiceBuffer();

    if (gVoicePath.empty()) {
        return;
    }

    if (SDL_LoadWAV(gVoicePath.c_str(), &gLoadedWavSpec, &gLoadedWavBuffer, &gLoadedWavLength) == nullptr) {
        std::cerr << "[VN] Could not load voice WAV: " << gVoicePath << " (" << SDL_GetError() << ")\n";
        return;
    }

    gAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &gLoadedWavSpec, nullptr, 0);
    if (gAudioDevice == 0) {
        std::cerr << "[VN] Could not open audio device: " << SDL_GetError() << "\n";
        stopAndFreeVoiceBuffer();
        return;
    }

    if (!queueLoadedVoiceBuffer()) {
        std::cerr << "[VN] Could not queue audio: " << SDL_GetError() << "\n";
        stopAndFreeVoiceBuffer();
        return;
    }

    SDL_PauseAudioDevice(gAudioDevice, 0);
    gVoicePlaying = gVoiceVolume > 0.0f;
}

void clearPendingBgmTransition() {
    gPendingBgmPath.clear();
    gPendingBgmVolume = 1.0f;
    gBgmFadeOutRequested = false;
    gBgmPauseAfterFadeOut = false;
}

void applyCurrentBgmVolume(float volume01) {
    gBgmCurrentVolume = std::clamp(volume01, 0.0f, 1.0f);
    if (gBgmPlayer.isPlaying()) {
        gBgmPlayer.setVolume(gBgmCurrentVolume * gBgmMasterVolume);
    }
}

bool startImmediateBgmPlayback(const std::string& wavPath, float volume01) {
    const float clampedVolume = std::clamp(volume01, 0.0f, 1.0f);
    if (!gBgmPlayer.play(wavPath, clampedVolume * gBgmMasterVolume)) {
        std::cerr << "[VN] Could not load BGM WAV: " << wavPath << " (" << SDL_GetError() << ")\n";
        return false;
    }

    gBgmPath = wavPath;
    gBgmVolume = clampedVolume;
    gBgmCurrentVolume = clampedVolume;
    return true;
}

void requestBgmTransition(const std::string& wavPath,
                          bool hasRequestedVolume,
                          float requestedVolume,
                          bool stopRequested,
                          std::optional<bool> pauseRequested) {
    const float targetVolume = hasRequestedVolume
        ? std::clamp(requestedVolume, 0.0f, 1.0f)
        : gBgmVolume;

    if (stopRequested) {
        if (!gBgmPlayer.isPlaying()) {
            stopBgmPlayback();
            return;
        }

        gPendingBgmPath.clear();
        gPendingBgmVolume = 0.0f;
        gBgmFadeOutRequested = true;
        return;
    }

    if (pauseRequested.has_value() && wavPath.empty()) {
        if (hasRequestedVolume) {
            gBgmVolume = targetVolume;
        }

        if (*pauseRequested) {
            if (!gBgmPlayer.isPlaying() || gBgmPlayer.isPaused()) {
                return;
            }

            gPendingBgmPath.clear();
            gPendingBgmVolume = 0.0f;
            gBgmFadeOutRequested = true;
            gBgmPauseAfterFadeOut = true;
            return;
        }

        if (!gBgmPlayer.isPlaying() || !gBgmPlayer.isPaused()) {
            return;
        }

        gBgmPlayer.resume();
        return;
    }

    if (wavPath.empty()) {
        if (hasRequestedVolume) {
            gBgmVolume = targetVolume;
        }
        return;
    }

    if (!gBgmPlayer.isPlaying()) {
        if (startImmediateBgmPlayback(wavPath, 0.0f)) {
            gBgmVolume = targetVolume;
        }
        clearPendingBgmTransition();
        return;
    }

    if (wavPath == gBgmPath) {
        if (hasRequestedVolume) {
            gBgmVolume = targetVolume;
        }
        gPendingBgmPath.clear();
        gPendingBgmVolume = 1.0f;
        return;
    }

    gPendingBgmPath = wavPath;
    gPendingBgmVolume = targetVolume;
    gBgmFadeOutRequested = true;
}

void updateBgmTransition(float deltaSeconds) {
    const float fadeStep = kBgmFadeDuration <= 0.0f ? 1.0f : deltaSeconds / kBgmFadeDuration;

    if (gBgmFadeOutRequested && gBgmPlayer.isPlaying()) {
        applyCurrentBgmVolume(std::max(0.0f, gBgmCurrentVolume - fadeStep));
        if (gBgmCurrentVolume <= 0.001f) {
            if (gBgmPauseAfterFadeOut) {
                gBgmPlayer.pause();
                gBgmFadeOutRequested = false;
                gBgmPauseAfterFadeOut = false;
                return;
            }

            gBgmPlayer.stop();
            gBgmPath.clear();
            gBgmCurrentVolume = 0.0f;
            gBgmFadeOutRequested = false;

            if (!gPendingBgmPath.empty()) {
                const std::string nextPath = gPendingBgmPath;
                const float nextVolume = gPendingBgmVolume;
                gPendingBgmPath.clear();
                gPendingBgmVolume = 1.0f;
                if (startImmediateBgmPlayback(nextPath, 0.0f)) {
                    gBgmVolume = nextVolume;
                }
            } else {
                gBgmVolume = 0.0f;
            }
        }
        return;
    }

    if (!gBgmPlayer.isPlaying()) {
        return;
    }

    if (std::fabs(gBgmCurrentVolume - gBgmVolume) <= 0.001f) {
        return;
    }

    if (gBgmCurrentVolume < gBgmVolume) {
        applyCurrentBgmVolume(std::min(gBgmVolume, gBgmCurrentVolume + fadeStep));
    } else {
        applyCurrentBgmVolume(std::max(gBgmVolume, gBgmCurrentVolume - fadeStep));
    }
}

#ifdef VN_ENABLE_TTF
TTF_Font* openBestAvailableFont(const std::string& preferredPath, int ptSize) {
    const std::vector<std::string> preferredPaths =
        preferredPath.empty() ? std::vector<std::string>{} : std::vector<std::string>{preferredPath};
    const std::vector<std::string> candidates = platform::path::preferredLatinFontPaths(preferredPaths);

    for (const auto& path : candidates) {
        TTF_Font* font = TTF_OpenFont(path.c_str(), ptSize);
        if (font != nullptr) {
            return font;
        }
    }

    return nullptr;
}

TTF_Font* openBestAvailableCjkFont(int ptSize) {
    const std::vector<std::string> candidates = platform::path::preferredCjkFontPaths();

    for (const auto& path : candidates) {
        TTF_Font* font = TTF_OpenFont(path.c_str(), ptSize);
        if (font != nullptr) {
            return font;
        }
    }

    return nullptr;
}

TTF_Font* activeFontForGlyph(const std::string& glyph) {
    return platform::text::selectFontForGlyph(glyph, gFont, gCjkFont);
}

void ensureFontLoaded() {
    if (gFont == nullptr) {
        gFont = openBestAvailableFont(gFontPath, gFontSize);
    }
    if (gCjkFont == nullptr) {
        gCjkFont = openBestAvailableCjkFont(gFontSize);
    }
    if (gFont == nullptr && gCjkFont == nullptr) {
        std::cerr << "[VN] No usable TTF font found. Install SDL2_ttf + DejaVu fonts or set a font path.\n";
    }
}

int utf8CodepointLength(unsigned char c) {
    return platform::text::utf8CodepointLength(c);
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
                const std::string tagLower = toLowerAsciiCopy(text.substr(i + 1, close - i - 1));
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

std::string buildVisibleRichTextRml(const std::string& text, size_t maxVisibleChars) {
    std::string out;
    std::vector<std::string> closingTags;
    size_t visibleCount = 0;
    size_t i = 0;

    while (i < text.size() && visibleCount < maxVisibleChars) {
        if (text[i] == '<') {
            const size_t close = text.find('>', i + 1);
            if (close != std::string::npos) {
                const std::string rawTag = text.substr(i + 1, close - i - 1);
                const std::string tagLower = toLowerAsciiCopy(rawTag);

                if (tagLower == "b") {
                    out += "<b>";
                    closingTags.push_back("b");
                    i = close + 1;
                    continue;
                }
                if (tagLower == "/b") {
                    out += "</b>";
                    if (!closingTags.empty() && closingTags.back() == "b") {
                        closingTags.pop_back();
                    }
                    i = close + 1;
                    continue;
                }
                if (tagLower == "i") {
                    out += "<i>";
                    closingTags.push_back("i");
                    i = close + 1;
                    continue;
                }
                if (tagLower == "/i") {
                    out += "</i>";
                    if (!closingTags.empty() && closingTags.back() == "i") {
                        closingTags.pop_back();
                    }
                    i = close + 1;
                    continue;
                }
                if (tagLower == "br" || tagLower == "br/") {
                    out += "<br/>";
                    ++visibleCount;
                    i = close + 1;
                    continue;
                }
                if (tagLower.rfind("color=#", 0) == 0) {
                    out += "<span style=\"color:";
                    out += escapeRmlText(rawTag.substr(6));
                    out += ";\">";
                    closingTags.push_back("span");
                    i = close + 1;
                    continue;
                }
                if (tagLower == "/color") {
                    out += "</span>";
                    if (!closingTags.empty() && closingTags.back() == "span") {
                        closingTags.pop_back();
                    }
                    i = close + 1;
                    continue;
                }
            }
        }

        const int codeLen = std::max(1, utf8CodepointLength(static_cast<unsigned char>(text[i])));
        const std::string glyph = text.substr(i, static_cast<size_t>(codeLen));
        i += static_cast<size_t>(codeLen);

        if (glyph == "\n") {
            out += "<br/>";
        } else {
            out += escapeRmlText(glyph);
        }
        ++visibleCount;
    }

    for (auto it = closingTags.rbegin(); it != closingTags.rend(); ++it) {
        out += "</";
        out += *it;
        out += ">";
    }

    return out;
}

std::string buildFullRichTextRml(const std::string& text) {
    const size_t visibleCount = countVisibleCharsIgnoringTags(text);
    return buildVisibleRichTextRml(text, visibleCount);
}

void drawText(const std::string& text, const SDL_Color& color, const SDL_Rect& area, bool centered) {
    ensureFontLoaded();
    TTF_Font* referenceFont = gFont != nullptr ? gFont : gCjkFont;
    if (text.empty() || referenceFont == nullptr) {
        return;
    }

    const std::vector<platform::text::FontRun> runs = platform::text::buildFontRuns(text, gFont, gCjkFont);
    if (runs.empty()) {
        return;
    }

    int totalWidth = 0;
    for (const platform::text::FontRun& run : runs) {
        int runW = 0;
        int runH = 0;
        if (TTF_SizeUTF8(run.font, run.text.c_str(), &runW, &runH) == 0) {
            totalWidth += runW;
        }
    }

    int cursorX = centered ? area.x + (area.w - totalWidth) / 2 : area.x;
    const int baselineY = area.y + TTF_FontAscent(referenceFont);

    for (const platform::text::FontRun& run : runs) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(run.font, run.text.c_str(), color);
        if (surface == nullptr) {
            continue;
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(gRenderer, surface);
        if (tex != nullptr) {
            SDL_Rect dst{
                cursorX,
                baselineY - TTF_FontAscent(run.font),
                surface->w,
                surface->h
            };
            SDL_RenderCopy(gRenderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        cursorX += surface->w;
        SDL_FreeSurface(surface);
    }
}

void drawRichText(const std::string& text, const SDL_Color& defaultColor, const SDL_Rect& area, size_t maxVisibleChars) {
    ensureFontLoaded();
    TTF_Font* referenceFont = gFont != nullptr ? gFont : gCjkFont;
    if (text.empty() || referenceFont == nullptr || maxVisibleChars == 0) {
        return;
    }

    struct StyledToken {
        std::string text;
        TTF_Font* font = nullptr;
        int styleFlags = TTF_STYLE_NORMAL;
        SDL_Color color{255, 255, 255, 255};
        bool isWhitespace = false;
        bool isNewline = false;
        size_t visibleChars = 0;
    };

    auto sameColor = [](const SDL_Color& lhs, const SDL_Color& rhs) {
        return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
    };

    auto countGlyphs = [](const std::string& tokenText) -> size_t {
        size_t count = 0;
        for (size_t idx = 0; idx < tokenText.size();) {
            const int len = std::max(1, utf8CodepointLength(static_cast<unsigned char>(tokenText[idx])));
            idx += static_cast<size_t>(len);
            ++count;
        }
        return count;
    };

    auto prefixGlyphs = [](const std::string& tokenText, size_t glyphCount) -> std::string {
        std::string out;
        size_t count = 0;
        for (size_t idx = 0; idx < tokenText.size() && count < glyphCount;) {
            const int len = std::max(1, utf8CodepointLength(static_cast<unsigned char>(tokenText[idx])));
            out.append(tokenText, idx, static_cast<size_t>(len));
            idx += static_cast<size_t>(len);
            ++count;
        }
        return out;
    };

    auto measureText = [](TTF_Font* font, int styleFlags, const std::string& tokenText, int& outW, int& outH) {
        outW = 0;
        outH = 0;
        if (font == nullptr || tokenText.empty()) {
            return;
        }
        TTF_SetFontStyle(font, styleFlags);
        (void)TTF_SizeUTF8(font, tokenText.c_str(), &outW, &outH);
    };

    int boldDepth = 0;
    int italicDepth = 0;
    std::vector<SDL_Color> colorStack;
    colorStack.push_back(defaultColor);
    std::vector<StyledToken> tokens;
    size_t i = 0;

    auto pushToken = [&](const std::string& tokenText,
                         TTF_Font* font,
                         int styleFlags,
                         const SDL_Color& color,
                         bool isWhitespace,
                         bool isNewline) {
        if (isNewline) {
            tokens.push_back(StyledToken{"", nullptr, TTF_STYLE_NORMAL, color, false, true, 1});
            return;
        }
        if (tokenText.empty() || font == nullptr) {
            return;
        }
        if (!tokens.empty() &&
            !tokens.back().isNewline &&
            !tokens.back().isWhitespace &&
            !isWhitespace &&
            tokens.back().font == font &&
            tokens.back().styleFlags == styleFlags &&
            sameColor(tokens.back().color, color)) {
            tokens.back().text += tokenText;
            tokens.back().visibleChars += countGlyphs(tokenText);
            return;
        }
        tokens.push_back(StyledToken{
            tokenText,
            font,
            styleFlags,
            color,
            isWhitespace,
            false,
            countGlyphs(tokenText)
        });
    };

    while (i < text.size()) {
        if (text[i] == '<') {
            const size_t close = text.find('>', i + 1);
            if (close != std::string::npos) {
                const std::string tagLower = toLowerAsciiCopy(text.substr(i + 1, close - i - 1));

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
                    pushToken("", nullptr, TTF_STYLE_NORMAL, colorStack.back(), false, true);
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
            pushToken("", nullptr, TTF_STYLE_NORMAL, colorStack.back(), false, true);
            continue;
        }

        int styleFlags = TTF_STYLE_NORMAL;
        if (boldDepth > 0) styleFlags |= TTF_STYLE_BOLD;
        if (italicDepth > 0) styleFlags |= TTF_STYLE_ITALIC;
        TTF_Font* activeFont = activeFontForGlyph(glyph);
        if (activeFont == nullptr) {
            continue;
        }

        if (platform::text::isWhitespaceGlyph(glyph)) {
            std::string whitespace = glyph;
            while (i < text.size()) {
                if (text[i] == '<') {
                    break;
                }
                const int nextLen = std::max(1, utf8CodepointLength(static_cast<unsigned char>(text[i])));
                const std::string nextGlyph = text.substr(i, static_cast<size_t>(nextLen));
                if (nextGlyph == "\n" || !platform::text::isWhitespaceGlyph(nextGlyph)) {
                    break;
                }
                whitespace += nextGlyph;
                i += static_cast<size_t>(nextLen);
            }
            pushToken(whitespace, activeFont, styleFlags, colorStack.back(), true, false);
            continue;
        }

        if (platform::text::shouldUseCjkFont(glyph)) {
            pushToken(glyph, activeFont, styleFlags, colorStack.back(), false, false);
            continue;
        }

        std::string word = glyph;
        while (i < text.size()) {
            if (text[i] == '<') {
                break;
            }
            const int nextLen = std::max(1, utf8CodepointLength(static_cast<unsigned char>(text[i])));
            const std::string nextGlyph = text.substr(i, static_cast<size_t>(nextLen));
            if (nextGlyph == "\n" ||
                platform::text::isWhitespaceGlyph(nextGlyph) ||
                platform::text::shouldUseCjkFont(nextGlyph)) {
                break;
            }
            word += nextGlyph;
            i += static_cast<size_t>(nextLen);
        }
        pushToken(word, activeFont, styleFlags, colorStack.back(), false, false);
    }

    int x = area.x;
    int y = area.y;
    const int maxX = area.x + area.w;
    const int maxY = area.y + area.h;
    const int lineSkip = TTF_FontLineSkip(referenceFont);
    size_t visibleCount = 0;

    auto newline = [&]() {
        x = area.x;
        y += lineSkip;
    };

    auto isOutOfArea = [&]() {
        return y + lineSkip > maxY;
    };

    auto renderTokenText = [&](TTF_Font* font, int styleFlags, const SDL_Color& color, const std::string& tokenText) {
        if (font == nullptr || tokenText.empty()) {
            return;
        }
        TTF_SetFontStyle(font, styleFlags);
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, tokenText.c_str(), color);
        if (surface == nullptr) {
            return;
        }
        SDL_Texture* tex = SDL_CreateTextureFromSurface(gRenderer, surface);
        if (tex != nullptr) {
            SDL_Rect dst{
                x,
                y + (TTF_FontAscent(referenceFont) - TTF_FontAscent(font)),
                surface->w,
                surface->h
            };
            SDL_RenderCopy(gRenderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        x += surface->w;
        SDL_FreeSurface(surface);
    };

    auto renderGlyphWrapped = [&](const StyledToken& token, const std::string& tokenText) {
        for (size_t idx = 0; idx < tokenText.size() && !isOutOfArea();) {
            const int len = std::max(1, utf8CodepointLength(static_cast<unsigned char>(tokenText[idx])));
            const std::string glyph = tokenText.substr(idx, static_cast<size_t>(len));
            idx += static_cast<size_t>(len);

            int glyphW = 0;
            int glyphH = 0;
            measureText(token.font, token.styleFlags, glyph, glyphW, glyphH);
            if (x + glyphW > maxX && x > area.x) {
                newline();
                if (isOutOfArea()) {
                    break;
                }
            }
            renderTokenText(token.font, token.styleFlags, token.color, glyph);
        }
    };

    for (const StyledToken& token : tokens) {
        if (visibleCount >= maxVisibleChars || isOutOfArea()) {
            break;
        }

        if (token.isNewline) {
            ++visibleCount;
            newline();
            continue;
        }

        const size_t drawVisibleChars = std::min(token.visibleChars, maxVisibleChars - visibleCount);
        const std::string tokenText =
            drawVisibleChars >= token.visibleChars ? token.text : prefixGlyphs(token.text, drawVisibleChars);

        int tokenW = 0;
        int tokenH = 0;
        measureText(token.font, token.styleFlags, tokenText, tokenW, tokenH);

        if (token.isWhitespace) {
            visibleCount += drawVisibleChars;
            if (x == area.x) {
                continue;
            }
            if (x + tokenW > maxX) {
                newline();
                continue;
            }
            renderTokenText(token.font, token.styleFlags, token.color, tokenText);
            continue;
        }

        if (x + tokenW > maxX && x > area.x) {
            newline();
            if (isOutOfArea()) {
                break;
            }
        }

        if (tokenW > area.w) {
            renderGlyphWrapped(token, tokenText);
        } else {
            renderTokenText(token.font, token.styleFlags, token.color, tokenText);
        }

        visibleCount += drawVisibleChars;
    }

    if (gFont != nullptr) {
        TTF_SetFontStyle(gFont, TTF_STYLE_NORMAL);
    }
    if (gCjkFont != nullptr) {
        TTF_SetFontStyle(gCjkFont, TTF_STYLE_NORMAL);
    }
}

void reloadScaledFont() {
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
    if (gCjkFont != nullptr) {
        TTF_CloseFont(gCjkFont);
        gCjkFont = nullptr;
    }
    ensureFontLoaded();
}
#endif

void reloadIconTexture() {
    destroyIconTexture();

    if (gRenderer == nullptr || gIconPath.empty()) {
        return;
    }

    SDL_Surface* surface = nullptr;
#ifdef VN_ENABLE_IMAGE
    surface = IMG_Load(gIconPath.c_str());
#else
    surface = SDL_LoadBMP(gIconPath.c_str());
#endif
    if (surface == nullptr) {
        std::cerr << "[VN] Could not load icon image: " << gIconPath << "\n";
        return;
    }

    gIconFrameWidth = std::max(1, surface->w / std::max(1, gIconFrameCount));
    gIconFrameHeight = surface->h;
    gIconTexture = SDL_CreateTextureFromSurface(gRenderer, surface);
    SDL_FreeSurface(surface);
}

void reloadBackgroundTexture() {
    destroyBackgroundTexture();

    if (gRenderer == nullptr || gBackgroundPath.empty()) {
        return;
    }

    SDL_Surface* surface = nullptr;
#ifdef VN_ENABLE_IMAGE
    surface = IMG_Load(gBackgroundPath.c_str());
#else
    surface = SDL_LoadBMP(gBackgroundPath.c_str());
#endif
    if (surface == nullptr) {
        std::cerr << "[VN] Could not load background image: " << gBackgroundPath << "\n";
        return;
    }

    gBackgroundTexture = SDL_CreateTextureFromSurface(gRenderer, surface);
    SDL_FreeSurface(surface);
}

} // namespace

bool initialize(SDL_Renderer* renderer, int windowWidth, int windowHeight) {
    const bool rendererChanged = gRenderer != renderer;
    gRenderer = renderer;
    gWindowW = windowWidth;
    gWindowH = windowHeight;

    if (!gInitialized) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "[VN] Audio subsystem init failed: " << SDL_GetError() << "\n";
        }

#ifdef VN_ENABLE_TTF
        if (TTF_Init() == -1) {
            std::cerr << "[VN] TTF init failed: " << TTF_GetError() << "\n";
        } else {
            gTtfInitialized = true;
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
        gInitialized = true;
    }

#ifdef VN_ENABLE_TTF
    reloadScaledFont();
    ensureFontLoaded();
#endif

    if (rendererChanged) {
        reloadBackgroundTexture();
        reloadIconTexture();
    }

    return gInitialized;
}

void setViewportSize(int windowWidth, int windowHeight) {
    gWindowW = std::max(1, windowWidth);
    gWindowH = std::max(1, windowHeight);
#ifdef VN_ENABLE_TTF
    reloadScaledFont();
#endif
}

void shutdown() {
    if (!gInitialized) {
        return;
    }

    stopBgmPlayback();
    destroyIconTexture();
    destroyBackgroundTexture();
    if (gPreviousBackgroundTexture != nullptr) {
        SDL_DestroyTexture(gPreviousBackgroundTexture);
        gPreviousBackgroundTexture = nullptr;
    }

    stopAndFreeVoiceBuffer();

#ifdef VN_ENABLE_TTF
    if (gFont != nullptr) {
        TTF_CloseFont(gFont);
        gFont = nullptr;
    }
    if (gCjkFont != nullptr) {
        TTF_CloseFont(gCjkFont);
        gCjkFont = nullptr;
    }
    if (gTtfInitialized) {
        TTF_Quit();
        gTtfInitialized = false;
    }
#endif

#ifdef VN_ENABLE_IMAGE
    if (gImageInitialized) {
        IMG_Quit();
        gImageInitialized = false;
    }
#endif

    gRenderer = nullptr;
    gInitialized = false;
}

void setSpeakerName(const std::string& name) {
    gSpeakerName = name;
}

void setIcon(const std::string& imagePath, int frameCount, float fps) {
    gIconPath = imagePath;
    gIconFrameCount = std::max(1, frameCount);
    gIconFps = std::max(0.1f, fps);
    gIconAnimTime = 0.0f;
    gIconCurrentFrame = 0;
    reloadIconTexture();
}

void setBackground(const std::string& imagePath) {
    gBackgroundPath = imagePath;

    if (imagePath.empty()) {
        if (gPreviousBackgroundTexture != nullptr) {
            SDL_DestroyTexture(gPreviousBackgroundTexture);
            gPreviousBackgroundTexture = nullptr;
        }
        gPreviousBackgroundTexture = gBackgroundTexture;
        gBackgroundTexture = nullptr;
        gBackgroundFadeElapsed = 0.0f;
        gBackgroundFadeActive = gPreviousBackgroundTexture != nullptr;
        return;
    }

    if (gRenderer == nullptr) {
        return;
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

    SDL_Texture* newTexture = SDL_CreateTextureFromSurface(gRenderer, surface);
    SDL_FreeSurface(surface);
    if (newTexture == nullptr) {
        return;
    }

    SDL_SetTextureBlendMode(newTexture, SDL_BLENDMODE_BLEND);

    if (gPreviousBackgroundTexture != nullptr) {
        SDL_DestroyTexture(gPreviousBackgroundTexture);
        gPreviousBackgroundTexture = nullptr;
    }

    gPreviousBackgroundTexture = gBackgroundTexture;
    gBackgroundTexture = newTexture;
    gBackgroundFadeElapsed = 0.0f;
    gBackgroundFadeActive = true;
}

void setVoice(const std::string& wavPath) {
    gVoicePath = wavPath;
}

void setBgm(const std::string& wavPath, float volume01) {
    if (wavPath.empty()) {
        return;
    }

    clearPendingBgmTransition();
    (void)startImmediateBgmPlayback(wavPath, volume01);
}

void stopBgmPlayback() {
    gBgmPlayer.stop();
    gBgmPath.clear();
    gBgmCurrentVolume = 0.0f;
    gBgmVolume = 0.0f;
    clearPendingBgmTransition();
}

void setBgmPaused(bool paused) {
    if (!gBgmPlayer.isPlaying()) {
        return;
    }
    if (paused) {
        gBgmPlayer.pause();
        gBgmCurrentVolume = 0.0f;
    } else {
        gBgmPlayer.resume();
    }
}

bool hasBgmPlayback() {
    return gBgmPlayer.isPlaying();
}

bool isBgmPlaybackPaused() {
    return gBgmPlayer.isPaused();
}

void setFont(const std::string& fontPath, int ptSize) {
    gFontPath = fontPath;
#ifdef VN_ENABLE_TTF
    gBaseFontSize = std::max(8, ptSize);

    if (gFont != nullptr) {
        TTF_CloseFont(gFont);
        gFont = nullptr;
    }
    if (gCjkFont != nullptr) {
        TTF_CloseFont(gCjkFont);
        gCjkFont = nullptr;
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
    const float clamped = std::max(1.0f, charsPerSecond);
    if (std::fabs(gCharsPerSecond - clamped) <= 0.001f) {
        return;
    }
    gCharsPerSecond = clamped;
}

void setMusicVolume(float volume01) {
    const float clamped = std::clamp(volume01, 0.0f, 1.0f);
    if (std::fabs(gBgmMasterVolume - clamped) <= 0.001f) {
        return;
    }

    gBgmMasterVolume = clamped;
    applyCurrentBgmVolume(gBgmCurrentVolume);
}

void setVoiceVolume(float volume01) {
    const float clamped = std::clamp(volume01, 0.0f, 1.0f);
    if (std::fabs(gVoiceVolume - clamped) <= 0.001f) {
        return;
    }
    gVoiceVolume = clamped;

    if (gAudioDevice == 0 || gLoadedWavBuffer == nullptr || gLoadedWavLength == 0) {
        return;
    }

    if (!queueLoadedVoiceBuffer()) {
        std::cerr << "[VN] Could not re-queue audio after volume change: " << SDL_GetError() << "\n";
        stopAndFreeVoiceBuffer();
        return;
    }

    SDL_PauseAudioDevice(gAudioDevice, gPaused ? 1 : 0);
    gVoicePlaying = gVoiceVolume > 0.0f;
}

void setBgmVolume(float volume01) {
    const float clamped = std::clamp(volume01, 0.0f, 1.0f);
    if (std::fabs(gBgmVolume - clamped) <= 0.001f) {
        return;
    }
    gBgmVolume = clamped;
    if (gBgmPlayer.isPlaying()) {
        applyCurrentBgmVolume(gBgmVolume);
    } else {
        gBgmCurrentVolume = gBgmVolume;
    }
}

float getTypewriterSpeed() {
    return gCharsPerSecond;
}

float getMusicVolume() {
    return gBgmMasterVolume;
}

float getVoiceVolume() {
    return gVoiceVolume;
}

float getBgmVolume() {
    return gBgmVolume;
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
    const std::string& backgroundPath,
    const std::string& bgmPath,
    float bgmVolume,
    bool bgmStop,
    std::optional<bool> bgmPause
) {
    const bool hasBgmVolume = bgmVolume >= 0.0f;

    setText(text);
    setSpeakerName(speakerName);
    setIcon(iconPath, iconFrameCount, iconFps);
    setVoice(voicePath);
    requestBgmTransition(bgmPath, hasBgmVolume, bgmVolume, bgmStop, bgmPause);
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

    updateBgmTransition(deltaSeconds);

    if (gBackgroundFadeActive) {
        gBackgroundFadeElapsed = std::min(kBackgroundFadeDuration, gBackgroundFadeElapsed + deltaSeconds);
        if (gBackgroundFadeElapsed >= kBackgroundFadeDuration) {
            gBackgroundFadeActive = false;
            if (gPreviousBackgroundTexture != nullptr) {
                SDL_DestroyTexture(gPreviousBackgroundTexture);
                gPreviousBackgroundTexture = nullptr;
            }
        }
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
        freeLoadedVoiceBuffer();
        closeAudioDeviceIfOpen();
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

bool isVoicePlaying() {
    return gVoicePlaying;
}

void reset() {
    stopAndFreeVoiceBuffer();
    stopBgmPlayback();

    destroyIconTexture();
    destroyBackgroundTexture();
    if (gPreviousBackgroundTexture != nullptr) {
        SDL_DestroyTexture(gPreviousBackgroundTexture);
        gPreviousBackgroundTexture = nullptr;
    }

    gSpeakerName.clear();
    gText.clear();
    gVoicePath.clear();
    gBgmPath.clear();
    gFontPath.clear();
    gIconPath.clear();
    gBackgroundPath.clear();

    gIconFrameCount = 1;
    gIconFrameWidth = 96;
    gIconFrameHeight = 96;
    gIconFps = 8.0f;
    gIconAnimTime = 0.0f;
    gIconCurrentFrame = 0;

    gCharsPerSecond = std::max(1.0f, gCharsPerSecond);
    gTypeAccumulator = 0.0f;
    gVisibleChars = 0;
    gTotalVisibleChars = 0;
    gBackgroundFadeElapsed = 0.0f;
    gBackgroundFadeActive = false;
    gAutoAdvanceOnVoiceEnd = false;
    gAdvanceRequested = false;
    gPaused = false;
    gVoicePlaying = false;
    gBgmCurrentVolume = 0.0f;
    gBgmVolume = 0.0f;
    clearPendingBgmTransition();

#ifdef VN_ENABLE_TTF
    if (gFont != nullptr) {
        TTF_CloseFont(gFont);
        gFont = nullptr;
    }
    if (gCjkFont != nullptr) {
        TTF_CloseFont(gCjkFont);
        gCjkFont = nullptr;
    }
    gFontSize = 28;
    gBaseFontSize = 28;
#endif
}

void render() {
    if (gRenderer == nullptr) {
        return;
    }

    const float uiScale = std::min(
        static_cast<float>(gWindowW) / static_cast<float>(kBaseWindowW),
        static_cast<float>(gWindowH) / static_cast<float>(kBaseWindowH)
    );

    SDL_Rect bgRect{0, 0, gWindowW, gWindowH};
    if (gBackgroundFadeActive) {
        const float t = std::clamp(gBackgroundFadeElapsed / kBackgroundFadeDuration, 0.0f, 1.0f);
        if (gPreviousBackgroundTexture != nullptr && gBackgroundTexture != nullptr) {
            // Draw the outgoing background fully, then fade the incoming one over it.
            // This avoids the crossfade dimming toward black mid-transition.
            SDL_SetTextureBlendMode(gPreviousBackgroundTexture, SDL_BLENDMODE_NONE);
            SDL_RenderCopy(gRenderer, gPreviousBackgroundTexture, nullptr, &bgRect);

            SDL_SetTextureBlendMode(gBackgroundTexture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(
                gBackgroundTexture,
                static_cast<Uint8>(std::lround(t * 255.0f))
            );
            SDL_RenderCopy(gRenderer, gBackgroundTexture, nullptr, &bgRect);
            SDL_SetTextureAlphaMod(gBackgroundTexture, 255);
        } else if (gPreviousBackgroundTexture != nullptr) {
            SDL_SetTextureBlendMode(gPreviousBackgroundTexture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(
                gPreviousBackgroundTexture,
                static_cast<Uint8>(std::lround((1.0f - t) * 255.0f))
            );
            SDL_RenderCopy(gRenderer, gPreviousBackgroundTexture, nullptr, &bgRect);
            SDL_SetTextureAlphaMod(gPreviousBackgroundTexture, 255);
        } else if (gBackgroundTexture != nullptr) {
            SDL_SetTextureBlendMode(gBackgroundTexture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(
                gBackgroundTexture,
                static_cast<Uint8>(std::lround(t * 255.0f))
            );
            SDL_RenderCopy(gRenderer, gBackgroundTexture, nullptr, &bgRect);
            SDL_SetTextureAlphaMod(gBackgroundTexture, 255);
        }
    } else if (gBackgroundTexture != nullptr) {
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

PresentationState getPresentationState() {
    PresentationState state;
    state.speakerName = gSpeakerName;
    state.iconPath = gIconPath;
    state.backgroundPath = gBackgroundPath;
    state.visibleCharacters = gVisibleChars;
    state.totalVisibleCharacters = gTotalVisibleChars;
    state.lineFinished = isLineFinished();
#ifdef VN_ENABLE_TTF
    state.visibleTextRml = buildVisibleRichTextRml(gText, gVisibleChars);
    state.fullTextRml = buildFullRichTextRml(gText);
#else
    state.visibleTextRml = escapeRmlText(gText.substr(0, std::min(gText.size(), gVisibleChars)));
    state.fullTextRml = escapeRmlText(gText);
#endif
    return state;
}

} // namespace vn
