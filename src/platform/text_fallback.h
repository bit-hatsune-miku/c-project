#pragma once

#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "path_resolution.h"

namespace platform::text {

struct FontRun {
    std::string text;
    TTF_Font* font = nullptr;
};

inline int utf8CodepointLength(unsigned char c) {
    if ((c & 0x80u) == 0) return 1;
    if ((c & 0xE0u) == 0xC0u) return 2;
    if ((c & 0xF0u) == 0xE0u) return 3;
    if ((c & 0xF8u) == 0xF0u) return 4;
    return 1;
}

inline uint32_t decodeUtf8Codepoint(const std::string& glyph) {
    if (glyph.empty()) {
        return 0;
    }

    const unsigned char lead = static_cast<unsigned char>(glyph[0]);
    if ((lead & 0x80u) == 0) {
        return lead;
    }
    if (glyph.size() >= 2 && (lead & 0xE0u) == 0xC0u) {
        return ((lead & 0x1Fu) << 6) |
               (static_cast<unsigned char>(glyph[1]) & 0x3Fu);
    }
    if (glyph.size() >= 3 && (lead & 0xF0u) == 0xE0u) {
        return ((lead & 0x0Fu) << 12) |
               ((static_cast<unsigned char>(glyph[1]) & 0x3Fu) << 6) |
               (static_cast<unsigned char>(glyph[2]) & 0x3Fu);
    }
    if (glyph.size() >= 4 && (lead & 0xF8u) == 0xF0u) {
        return ((lead & 0x07u) << 18) |
               ((static_cast<unsigned char>(glyph[1]) & 0x3Fu) << 12) |
               ((static_cast<unsigned char>(glyph[2]) & 0x3Fu) << 6) |
               (static_cast<unsigned char>(glyph[3]) & 0x3Fu);
    }
    return 0;
}

inline bool isCjkCodepoint(uint32_t codepoint) {
    return (codepoint >= 0x3000u && codepoint <= 0x303Fu) ||
           (codepoint >= 0x3040u && codepoint <= 0x30FFu) ||
           (codepoint >= 0x31F0u && codepoint <= 0x31FFu) ||
           (codepoint >= 0x3400u && codepoint <= 0x4DBFu) ||
           (codepoint >= 0x4E00u && codepoint <= 0x9FFFu) ||
           (codepoint >= 0xF900u && codepoint <= 0xFAFFu) ||
           (codepoint >= 0xFF00u && codepoint <= 0xFFEFu);
}

inline bool shouldUseCjkFont(const std::string& glyph) {
    return isCjkCodepoint(decodeUtf8Codepoint(glyph));
}

inline bool isWhitespaceGlyph(const std::string& glyph) {
    return glyph.size() == 1 &&
           glyph[0] != '\n' &&
           std::isspace(static_cast<unsigned char>(glyph[0])) != 0;
}

inline TTF_Font* selectFontForGlyph(const std::string& glyph, TTF_Font* latinFont, TTF_Font* cjkFont) {
    if (shouldUseCjkFont(glyph) && cjkFont != nullptr) {
        return cjkFont;
    }
    return latinFont != nullptr ? latinFont : cjkFont;
}

inline std::vector<FontRun> buildFontRuns(const std::string& text, TTF_Font* latinFont, TTF_Font* cjkFont) {
    std::vector<FontRun> runs;
    for (size_t i = 0; i < text.size();) {
        const int codeLen = utf8CodepointLength(static_cast<unsigned char>(text[i]));
        const std::string glyph = text.substr(i, static_cast<size_t>(codeLen));
        i += static_cast<size_t>(codeLen);

        TTF_Font* font = selectFontForGlyph(glyph, latinFont, cjkFont);
        if (font == nullptr) {
            continue;
        }

        if (!runs.empty() && runs.back().font == font) {
            runs.back().text += glyph;
        } else {
            runs.push_back(FontRun{glyph, font});
        }
    }
    return runs;
}

inline std::vector<FontRun> buildWrapRuns(const std::string& text, TTF_Font* latinFont, TTF_Font* cjkFont) {
    std::vector<FontRun> runs;

    auto pushRun = [&](const std::string& runText, TTF_Font* font) {
        if (runText.empty()) {
            return;
        }
        if (!runs.empty() &&
            runs.back().font == font &&
            runs.back().text != "\n" &&
            !isWhitespaceGlyph(runs.back().text) &&
            !isWhitespaceGlyph(runText)) {
            runs.back().text += runText;
        } else {
            runs.push_back(FontRun{runText, font});
        }
    };

    for (size_t i = 0; i < text.size();) {
        const int codeLen = utf8CodepointLength(static_cast<unsigned char>(text[i]));
        const std::string glyph = text.substr(i, static_cast<size_t>(codeLen));
        i += static_cast<size_t>(codeLen);

        if (glyph == "\n") {
            runs.push_back(FontRun{"\n", nullptr});
            continue;
        }

        TTF_Font* font = selectFontForGlyph(glyph, latinFont, cjkFont);
        if (font == nullptr) {
            continue;
        }

        if (isWhitespaceGlyph(glyph) || shouldUseCjkFont(glyph)) {
            runs.push_back(FontRun{glyph, font});
            continue;
        }

        std::string token = glyph;
        while (i < text.size()) {
            const int nextLen = utf8CodepointLength(static_cast<unsigned char>(text[i]));
            const std::string nextGlyph = text.substr(i, static_cast<size_t>(nextLen));
            if (nextGlyph == "\n" || isWhitespaceGlyph(nextGlyph) || shouldUseCjkFont(nextGlyph)) {
                break;
            }

            TTF_Font* nextFont = selectFontForGlyph(nextGlyph, latinFont, cjkFont);
            if (nextFont != font) {
                break;
            }

            token += nextGlyph;
            i += static_cast<size_t>(nextLen);
        }

        pushRun(token, font);
    }

    return runs;
}

inline TTF_Font* openBestAvailableCjkFont(int ptSize, const std::vector<std::string>& preferredPaths = {}) {
    if (TTF_WasInit() == 0) {
        return nullptr;
    }

    const std::vector<std::string> candidates = platform::path::preferredCjkFontPaths(preferredPaths);
    for (const auto& path : candidates) {
        TTF_Font* font = TTF_OpenFont(path.c_str(), ptSize);
        if (font != nullptr) {
            return font;
        }
    }

    return nullptr;
}

class CjkFontCache {
public:
    ~CjkFontCache() {
        clear();
    }

    TTF_Font* get(int ptSize) {
        auto it = fonts_.find(ptSize);
        if (it != fonts_.end()) {
            touch(ptSize);
            return it->second;
        }

        TTF_Font* loaded = openBestAvailableCjkFont(ptSize);
        fonts_[ptSize] = loaded;
        touch(ptSize);
        trim();
        return loaded;
    }

    void clear() {
        const bool ttfReady = TTF_WasInit() != 0;
        for (auto& [_, font] : fonts_) {
            if (ttfReady && font != nullptr) {
                TTF_CloseFont(font);
            }
        }
        fonts_.clear();
        usageOrder_.clear();
    }

private:
    static constexpr std::size_t kMaxCachedFonts = 6;

    void touch(int ptSize) {
        usageOrder_.erase(std::remove(usageOrder_.begin(), usageOrder_.end(), ptSize), usageOrder_.end());
        usageOrder_.push_back(ptSize);
    }

    void trim() {
        while (usageOrder_.size() > kMaxCachedFonts) {
            const int ptSize = usageOrder_.front();
            usageOrder_.erase(usageOrder_.begin());

            auto it = fonts_.find(ptSize);
            if (it == fonts_.end()) {
                continue;
            }

            if (it->second != nullptr && TTF_WasInit() != 0) {
                TTF_CloseFont(it->second);
            }
            fonts_.erase(it);
        }
    }

    std::map<int, TTF_Font*> fonts_;
    std::vector<int> usageOrder_;
};

inline CjkFontCache& fallbackCjkFontCache() {
    static CjkFontCache cache;
    return cache;
}

inline TTF_Font* fallbackCjkFontFor(TTF_Font* font) {
    if (font == nullptr || TTF_WasInit() == 0) {
        return nullptr;
    }

    return fallbackCjkFontCache().get(std::max(8, TTF_FontHeight(font)));
}

inline void releaseFallbackCjkFonts() {
    fallbackCjkFontCache().clear();
}

} // namespace platform::text
