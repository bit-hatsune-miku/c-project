#pragma once

#include <SDL2/SDL_ttf.h>

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

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

} // namespace platform::text
