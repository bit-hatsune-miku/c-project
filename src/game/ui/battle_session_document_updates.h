#ifndef BATTLE_SESSION_DOCUMENT_UPDATES_H
#define BATTLE_SESSION_DOCUMENT_UPDATES_H

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#include "../core/turn_system.h"
#include "battle_session_ui_state.h"

namespace battle::app::ui {

struct BattleHudDocumentDependencies {
    std::function<std::string(std::string)> uppercaseText;
    std::function<std::string(const std::string&, const std::string&)> findCombatImagePath;
    std::function<std::string(const std::string&, Uint64, Uint64, float)> revealNarrationText;
};

namespace detail {

struct PauseOverlayCopy {
    const char* title = "";
    const char* hint = "";
};

inline void setElementText(Rml::ElementDocument* document, const std::string& id, const std::string& text) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetInnerRML(text);
    }
}

inline void setElementDisplay(Rml::ElementDocument* document, const std::string& id, bool visible) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetProperty("display", visible ? "block" : "none");
    }
}

inline void setElementProperty(Rml::ElementDocument* document,
                               const std::string& id,
                               const std::string& property,
                               const std::string& value) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetProperty(property, value);
    }
}

inline void setElementClass(Rml::ElementDocument* document,
                            const std::string& id,
                            const std::string& className,
                            bool enabled) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetClass(className, enabled);
    }
}

inline void setRangeValue(Rml::ElementDocument* document, const std::string& id, const std::string& value) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        if (auto* control = dynamic_cast<Rml::ElementFormControl*>(element)) {
            control->SetValue(value);
        }
    }
}

inline void setPortraitDecorator(Rml::ElementDocument* document,
                                 const std::string& id,
                                 const std::string& assetName,
                                 const BattleHudDocumentDependencies& dependencies) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        if (!dependencies.findCombatImagePath) {
            element->RemoveProperty("decorator");
            return;
        }
        const std::string path = dependencies.findCombatImagePath("icons", assetName);
        if (!path.empty()) {
            element->SetProperty("decorator", "image(" + path + " cover center center)");
        } else {
            element->RemoveProperty("decorator");
        }
    }
}

inline std::vector<int> getSortedTurnActorIndices(const battle::TurnState& turnState) {
    std::vector<int> sorted(turnState.actors.size());
    std::iota(sorted.begin(), sorted.end(), 0);
    std::sort(sorted.begin(), sorted.end(), [&](int lhs, int rhs) {
        constexpr float kEpsilon = 0.0001f;
        const battle::TurnActor& a = turnState.actors[static_cast<size_t>(lhs)];
        const battle::TurnActor& b = turnState.actors[static_cast<size_t>(rhs)];
        if (std::fabs(a.currentActionValue - b.currentActionValue) > kEpsilon) {
            return a.currentActionValue < b.currentActionValue;
        }
        return battle::turn::turnPriorityLess(a, b);
    });
    return sorted;
}

inline std::size_t utf8CodepointCount(const std::string& text) {
    std::size_t count = 0;
    for (unsigned char byte : text) {
        if ((byte & 0xC0u) != 0x80u) {
            ++count;
        }
    }
    return count;
}

inline const char* bossFontSizeForName(const std::string& name) {
    const std::size_t codepoints = utf8CodepointCount(name);
    if (codepoints >= 38) {
        return "16dp";
    }
    if (codepoints >= 30) {
        return "18dp";
    }
    if (codepoints >= 24) {
        return "21dp";
    }
    if (codepoints >= 18) {
        return "24dp";
    }
    return "28dp";
}

inline std::string formatPercent(float ratio) {
    const int percent = static_cast<int>(std::lround(std::clamp(ratio, 0.0f, 1.0f) * 100.0f));
    return std::to_string(percent) + "%";
}

inline std::string formatHexColor(unsigned char red, unsigned char green, unsigned char blue) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", red, green, blue);
    return buffer;
}

inline std::string buildPartyRackMarkup(std::size_t partyCount) {
    std::ostringstream markup;
    constexpr int kCardLeftStepDp = 198;
    for (std::size_t i = 0; i < partyCount; ++i) {
        const std::size_t displayIndex = i + 1;
        const int leftDp = static_cast<int>(i) * kCardLeftStepDp;
        markup
            << "<div class=\"unit-card\" id=\"unit-card-" << displayIndex << "\" style=\"left: " << leftDp << "dp;\">"
            << "<div class=\"unit-ult\">"
            << "<div class=\"unit-ult-fill\" id=\"unit-ult-fill-" << displayIndex << "\"></div>"
            << "<div class=\"unit-ult-sheen\" id=\"unit-ult-sheen-" << displayIndex << "\"></div>"
            << "<div class=\"unit-ult-rim\"></div>"
            << "<div class=\"unit-ult-core\" id=\"unit-ult-text-" << displayIndex << "\"></div>"
            << "</div>"
            << "<div class=\"unit-icon\" id=\"unit-icon-" << displayIndex << "\"></div>"
            << "<div class=\"unit-hp-shell\">"
            << "<div class=\"unit-hp-lane\">"
            << "<div class=\"unit-hp-trail\" id=\"unit-hp-trail-" << displayIndex << "\"></div>"
            << "<div class=\"unit-hp-fill\" id=\"unit-hp-fill-" << displayIndex << "\"></div>"
            << "<div class=\"unit-shield-stroke\">"
            << "<div class=\"unit-shield-line unit-shield-line-top\" id=\"unit-shield-top-" << displayIndex << "\"></div>"
            << "<div class=\"unit-shield-line unit-shield-line-bottom\" id=\"unit-shield-bottom-" << displayIndex << "\"></div>"
            << "<div class=\"unit-shield-line unit-shield-line-left\" id=\"unit-shield-left-" << displayIndex << "\"></div>"
            << "<div class=\"unit-shield-line unit-shield-line-right\" id=\"unit-shield-right-" << displayIndex << "\"></div>"
            << "</div>"
            << "<div class=\"unit-hp-text\" id=\"unit-hp-text-" << displayIndex << "\"></div>"
            << "</div>"
            << "</div>"
            << "<div class=\"unit-shield-badge\" id=\"unit-shield-badge-" << displayIndex << "\">"
            << "<div class=\"unit-shield-icon\"></div>"
            << "<div class=\"unit-shield-value\" id=\"unit-shield-value-" << displayIndex << "\"></div>"
            << "</div>"
            << "<div class=\"unit-hit-flash\" id=\"unit-hit-flash-" << displayIndex << "\"></div>"
            << "</div>";
    }
    return markup.str();
}

inline void ensurePartyRackDocument(Rml::ElementDocument* document, std::size_t partyCount) {
    if (document == nullptr) {
        return;
    }

    Rml::Element* partyRack = document->GetElementById("party-rack");
    if (partyRack == nullptr) {
        return;
    }

    const std::string countValue = std::to_string(partyCount);
    if (partyRack->GetAttribute<std::string>("data-party-count", "") != countValue) {
        partyRack->SetInnerRML(buildPartyRackMarkup(partyCount));
        partyRack->SetAttribute("data-party-count", countValue);
    }

    constexpr int kCardWidthDp = 186;
    constexpr int kCardLeftStepDp = 198;
    const int rackWidthDp = partyCount == 0
        ? kCardWidthDp
        : kCardWidthDp + static_cast<int>(partyCount - 1) * kCardLeftStepDp;
    partyRack->SetProperty("width", std::to_string(rackWidthDp) + "dp");
}

inline float hitReactionProgress(const HudHitReactionState& state, Uint64 nowMs) {
    if (!state.active || state.untilMs <= state.startedMs) {
        return 0.0f;
    }

    const double elapsed = static_cast<double>(nowMs - state.startedMs);
    const double duration = static_cast<double>(state.untilMs - state.startedMs);
    return std::clamp(static_cast<float>(elapsed / duration), 0.0f, 1.0f);
}

inline float ultimateSheenProgress(const HudUnitAnimationState& state, Uint64 nowMs) {
    if (state.ultSheenUntilMs <= state.ultSheenStartedMs || nowMs >= state.ultSheenUntilMs) {
        return -1.0f;
    }

    const double elapsed = static_cast<double>(nowMs - state.ultSheenStartedMs);
    const double duration = static_cast<double>(state.ultSheenUntilMs - state.ultSheenStartedMs);
    return std::clamp(static_cast<float>(elapsed / duration), 0.0f, 1.0f);
}

inline std::string hpColorForRatio(float ratio) {
    if (ratio > 0.66f) {
        return "#48bf7b";
    }
    if (ratio > 0.33f) {
        return "#e4a943";
    }
    return "#d96b56";
}

inline PauseOverlayCopy getPauseOverlayCopy(PauseOverlayMode mode) {
    switch (mode) {
        case PauseOverlayMode::Settings:
            return PauseOverlayCopy{
                "SETTINGS",
                "ADJUST BATTLE SETTINGS. CHANGES CARRY BACK TO THE MAIN APP."
            };
        case PauseOverlayMode::Menu:
        default:
            return PauseOverlayCopy{
                "PAUSED",
                "ESC TO RESUME. SETTINGS MATCH THE MAIN APP."
            };
    }
}

inline void updateToastDocument(Rml::ElementDocument* document, const HudFeedbackState& feedback) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* toast = document->GetElementById("battle-toast")) {
        toast->SetInnerRML(feedback.toastText);
        toast->SetClass("visible", !feedback.toastText.empty());
    }
}

inline void updateHintDocument(Rml::ElementDocument* document, const HudFeedbackState& feedback) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* hint = document->GetElementById("battle-hint")) {
        hint->SetInnerRML(feedback.hintText);
        hint->SetClass("visible", !feedback.hintText.empty());
    }
}

inline std::string comboBonusText(float comboBonusFraction) {
    const float percent = std::max(0.0f, comboBonusFraction) * 100.0f;
    const float roundedTenth = std::round(percent * 10.0f) / 10.0f;
    const int wholePercent = static_cast<int>(std::lround(roundedTenth));
    if (std::fabs(roundedTenth - static_cast<float>(wholePercent)) <= 0.05f) {
        return "+" + std::to_string(wholePercent) + "% DMG";
    }

    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(1);
    stream << "+" << roundedTenth << "% DMG";
    return stream.str();
}

struct JudgementColor {
    unsigned char red = 255;
    unsigned char green = 255;
    unsigned char blue = 255;
};

struct JudgementPalette {
    JudgementColor dark;
    JudgementColor mid;
    JudgementColor bright;
    JudgementColor final;
    JudgementColor shadow;
    JudgementColor reward;
};

inline float lerpValue(float from, float to, float t) {
    return from + (to - from) * t;
}

inline float easeOutCubic(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    const float inverse = 1.0f - t;
    return 1.0f - (inverse * inverse * inverse);
}

inline float easeInCubic(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * t;
}

inline JudgementColor lerpColor(const JudgementColor& from, const JudgementColor& to, float t) {
    return JudgementColor{
        static_cast<unsigned char>(std::lround(lerpValue(static_cast<float>(from.red), static_cast<float>(to.red), t))),
        static_cast<unsigned char>(std::lround(lerpValue(static_cast<float>(from.green), static_cast<float>(to.green), t))),
        static_cast<unsigned char>(std::lround(lerpValue(static_cast<float>(from.blue), static_cast<float>(to.blue), t)))
    };
}

inline std::string rgbaText(const JudgementColor& color, float alpha) {
    char buffer[48];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "rgba(%u, %u, %u, %u)",
                  static_cast<unsigned int>(color.red),
                  static_cast<unsigned int>(color.green),
                  static_cast<unsigned int>(color.blue),
                  static_cast<unsigned int>(std::clamp(std::lround(alpha * 255.0f), 0l, 255l)));
    return buffer;
}

inline std::string decimalText(float value, int precision = 3) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, static_cast<double>(value));
    return buffer;
}

inline const JudgementPalette& judgementPalette(const std::string& judgementClassName) {
    static const JudgementPalette perfect{
        {92, 23, 63},
        {255, 233, 244},
        {255, 243, 249},
        {255, 249, 252},
        {84, 18, 55},
        {255, 194, 228}
    };
    static const JudgementPalette good{
        {21, 77, 57},
        {234, 255, 246},
        {243, 255, 250},
        {248, 255, 252},
        {13, 66, 48},
        {184, 255, 226}
    };
    static const JudgementPalette okay{
        {106, 66, 16},
        {255, 243, 222},
        {255, 249, 239},
        {255, 252, 247},
        {101, 61, 13},
        {255, 230, 184}
    };
    static const JudgementPalette flop{
        {111, 26, 36},
        {255, 232, 236},
        {255, 242, 245},
        {255, 248, 249},
        {106, 22, 33},
        {255, 193, 197}
    };

    if (judgementClassName == "perfect") {
        return perfect;
    }
    if (judgementClassName == "good") {
        return good;
    }
    if (judgementClassName == "okay") {
        return okay;
    }
    return flop;
}

inline std::string judgementLetterStyle(const JudgementPalette& palette, float elapsedSeconds) {
    constexpr float kLetterDurationSeconds = 0.44f;
    const float progress = std::clamp(elapsedSeconds / kLetterDurationSeconds, 0.0f, 1.0f);

    float opacity = 1.0f;
    float translateYDp = 0.0f;
    float scale = 1.0f;
    JudgementColor color = palette.final;

    if (progress < 0.54f) {
        const float t = easeOutCubic(progress / 0.54f);
        opacity = t;
        translateYDp = lerpValue(-54.0f, 8.0f, t);
        scale = lerpValue(1.62f, 0.94f, t);
        color = lerpColor(palette.dark, palette.mid, t);
    } else if (progress < 0.78f) {
        const float t = easeOutCubic((progress - 0.54f) / 0.24f);
        opacity = 1.0f;
        translateYDp = lerpValue(8.0f, -2.0f, t);
        scale = lerpValue(0.94f, 1.06f, t);
        color = lerpColor(palette.mid, palette.bright, t);
    } else {
        const float t = easeOutCubic((progress - 0.78f) / 0.22f);
        opacity = 1.0f;
        translateYDp = lerpValue(-2.0f, 0.0f, t);
        scale = lerpValue(1.06f, 1.0f, t);
        color = lerpColor(palette.bright, palette.final, t);
    }

    std::string style = "opacity: ";
    style += decimalText(opacity);
    style += "; transform: translateY(";
    style += decimalText(translateYDp);
    style += "dp) scale(";
    style += decimalText(scale);
    style += "); color: ";
    style += rgbaText(color, 1.0f);
    style += ";";
    return style;
}

inline std::string judgementMarkup(const HudFeedbackState& feedback, Uint64 nowMs) {
    constexpr float kLetterIntervalSeconds = 0.024f;
    const JudgementPalette& palette = judgementPalette(feedback.judgementClassName);
    const float elapsedSeconds = feedback.judgementStartedMs == 0
        ? 0.0f
        : static_cast<float>(nowMs - feedback.judgementStartedMs) / 1000.0f;

    std::string markup;
    markup.reserve(feedback.judgementText.size() * 200);

    int visibleLetterIndex = 0;
    for (char glyph : feedback.judgementText) {
        if (glyph == ' ') {
            markup += "<span class=\"battle-judgement-space\"></span>";
            continue;
        }

        const float letterElapsedSeconds = elapsedSeconds - (static_cast<float>(visibleLetterIndex) * kLetterIntervalSeconds);
        if (letterElapsedSeconds < 0.0f) {
            ++visibleLetterIndex;
            continue;
        }

        markup += "<span class=\"battle-judgement-letter\" style=\"";
        markup += judgementLetterStyle(palette, letterElapsedSeconds);
        markup += "\">";
        markup.push_back(glyph);
        markup += "</span>";
        ++visibleLetterIndex;
    }

    return markup;
}

inline std::string judgementContainerTransform(const HudFeedbackState& feedback, Uint64 nowMs) {
    const Uint64 durationMs = feedback.judgementUntilMs > feedback.judgementStartedMs
        ? (feedback.judgementUntilMs - feedback.judgementStartedMs)
        : 1;
    const float progress = std::clamp(
        static_cast<float>(nowMs - feedback.judgementStartedMs) / static_cast<float>(durationMs),
        0.0f,
        1.0f
    );

    float translateXDp = 0.0f;
    float scale = 1.0f;
    if (progress < 0.14f) {
        const float t = easeOutCubic(progress / 0.14f);
        translateXDp = lerpValue(28.0f, 0.0f, t);
        scale = lerpValue(0.96f, 1.02f, t);
    } else if (progress < 0.70f) {
        const float t = (progress - 0.14f) / 0.56f;
        translateXDp = 0.0f;
        scale = lerpValue(1.02f, 1.0f, t);
    } else {
        const float t = easeInCubic((progress - 0.70f) / 0.30f);
        translateXDp = lerpValue(0.0f, 10.0f, t);
        scale = lerpValue(1.0f, 0.99f, t);
    }

    return "translateX(" + decimalText(translateXDp) + "dp) scale(" + decimalText(scale) + ")";
}

inline float judgementContainerOpacity(const HudFeedbackState& feedback, Uint64 nowMs) {
    const Uint64 durationMs = feedback.judgementUntilMs > feedback.judgementStartedMs
        ? (feedback.judgementUntilMs - feedback.judgementStartedMs)
        : 1;
    const float progress = std::clamp(
        static_cast<float>(nowMs - feedback.judgementStartedMs) / static_cast<float>(durationMs),
        0.0f,
        1.0f
    );

    if (progress < 0.14f) {
        return easeOutCubic(progress / 0.14f);
    }
    if (progress < 0.70f) {
        return 1.0f;
    }
    return 1.0f - easeInCubic((progress - 0.70f) / 0.30f);
}

inline float judgementRewardOpacity(const HudFeedbackState& feedback, Uint64 nowMs) {
    constexpr float kLetterIntervalSeconds = 0.024f;
    constexpr float kRewardDelaySeconds = 0.08f;
    constexpr float kRewardDurationSeconds = 0.26f;
    const float visibleLetters = static_cast<float>(std::max<std::size_t>(feedback.judgementText.size(), 1));
    const float rewardStartSeconds =
        ((visibleLetters - 1.0f) * kLetterIntervalSeconds) + kRewardDelaySeconds;
    const float elapsedSeconds = feedback.judgementStartedMs == 0
        ? 0.0f
        : static_cast<float>(nowMs - feedback.judgementStartedMs) / 1000.0f;
    const float rewardElapsedSeconds = elapsedSeconds - rewardStartSeconds;
    if (rewardElapsedSeconds <= 0.0f) {
        return 0.0f;
    }
    return easeOutCubic(std::clamp(rewardElapsedSeconds / kRewardDurationSeconds, 0.0f, 1.0f));
}

inline const JudgementPalette& battleResultPalette(BattleResultOverlayOutcome outcome) {
    static const JudgementPalette victory{
        {92, 23, 63},
        {255, 211, 235},
        {255, 141, 203},
        {255, 103, 182},
        {84, 18, 55},
        {255, 224, 240}
    };
    static const JudgementPalette defeat{
        {111, 26, 36},
        {255, 187, 194},
        {249, 125, 132},
        {236, 95, 102},
        {106, 22, 33},
        {255, 218, 221}
    };

    return outcome == BattleResultOverlayOutcome::Victory ? victory : defeat;
}

inline std::string battleResultLetterStyle(const JudgementPalette& palette, float elapsedSeconds) {
    const float progress = std::clamp(elapsedSeconds / kBattleResultLetterDurationSeconds, 0.0f, 1.0f);

    float opacity = 1.0f;
    float translateYDp = 0.0f;
    float scale = 1.0f;
    JudgementColor color = palette.final;

    if (progress < 0.52f) {
        const float t = easeOutCubic(progress / 0.52f);
        opacity = t;
        translateYDp = lerpValue(-154.0f, 18.0f, t);
        scale = lerpValue(1.88f, 0.84f, t);
        color = lerpColor(palette.dark, palette.mid, t);
    } else if (progress < 0.78f) {
        const float t = easeOutCubic((progress - 0.52f) / 0.26f);
        opacity = 1.0f;
        translateYDp = lerpValue(18.0f, -6.0f, t);
        scale = lerpValue(0.84f, 1.08f, t);
        color = lerpColor(palette.mid, palette.bright, t);
    } else {
        const float t = easeOutCubic((progress - 0.78f) / 0.22f);
        opacity = 1.0f;
        translateYDp = lerpValue(-6.0f, 0.0f, t);
        scale = lerpValue(1.08f, 1.0f, t);
        color = lerpColor(palette.bright, palette.final, t);
    }

    std::string style = "opacity: ";
    style += decimalText(opacity);
    style += "; transform: translateY(";
    style += decimalText(translateYDp);
    style += "dp) scale(";
    style += decimalText(scale);
    style += "); color: ";
    style += rgbaText(color, 1.0f);
    style += ";";
    return style;
}

inline std::string battleResultHiddenLetterStyle(const JudgementPalette& palette) {
    std::string style = "opacity: 0; transform: translateY(-154.000dp) scale(1.880); color: ";
    style += rgbaText(palette.dark, 1.0f);
    style += ";";
    return style;
}

inline std::string buildBattleResultWordMarkup(const BattleResultOverlayState& overlay) {
    std::string markup;
    markup.reserve(overlay.word.size() * 96);

    int visibleLetterIndex = 0;
    for (char glyph : overlay.word) {
        if (glyph == ' ') {
            markup += "<span class=\"battle-result-space\"></span>";
            continue;
        }

        markup += "<span class=\"battle-result-letter\" id=\"battle-result-letter-";
        markup += std::to_string(visibleLetterIndex);
        markup += "\">";
        markup.push_back(glyph);
        markup += "</span>";
        ++visibleLetterIndex;
    }

    return markup;
}

inline void ensureBattleResultWordDocument(Rml::ElementDocument* document, const BattleResultOverlayState& overlay) {
    if (document == nullptr) {
        return;
    }

    Rml::Element* word = document->GetElementById("battle-result-word");
    if (word == nullptr) {
        return;
    }

    if (word->GetAttribute<std::string>("data-result-word", "") != overlay.word) {
        word->SetInnerRML(buildBattleResultWordMarkup(overlay));
        word->SetAttribute("data-result-word", overlay.word);
    }
}

inline void updateBattleResultLetterElements(Rml::ElementDocument* document,
                                             const BattleResultOverlayState& overlay) {
    if (document == nullptr) {
        return;
    }

    const JudgementPalette& palette = battleResultPalette(overlay.outcome);
    const std::string hiddenStyle = battleResultHiddenLetterStyle(palette);
    const float elapsedSeconds = battleResultElapsedSeconds(overlay);
    const int totalLetters = static_cast<int>(battleResultVisibleGlyphCount(overlay.word));
    for (int letterIndex = 0; letterIndex < totalLetters; ++letterIndex) {
        Rml::Element* letter = document->GetElementById("battle-result-letter-" + std::to_string(letterIndex));
        if (letter == nullptr) {
            continue;
        }

        const std::string style = [&]() {
            if (!overlay.active) {
                return hiddenStyle;
            }

            const float letterElapsedSeconds =
                elapsedSeconds - kBattleResultIntroDelaySeconds -
                (static_cast<float>(letterIndex) * kBattleResultLetterIntervalSeconds);
            if (letterElapsedSeconds < 0.0f) {
                return hiddenStyle;
            }
            return battleResultLetterStyle(palette, letterElapsedSeconds);
        }();

        letter->SetAttribute("style", style);
    }
}

inline float battleResultDimOpacity(const BattleResultOverlayState& overlay, Uint64 nowMs) {
    (void)nowMs;
    const float elapsedSeconds = battleResultElapsedSeconds(overlay);
    return easeOutCubic(std::clamp(elapsedSeconds / kBattleResultDimFadeDurationSeconds, 0.0f, 1.0f));
}

inline float battleResultKickerOpacity(const BattleResultOverlayState& overlay, Uint64 nowMs) {
    (void)nowMs;
    const float elapsedSeconds = battleResultElapsedSeconds(overlay);
    const float delayedElapsed = elapsedSeconds - kBattleResultKickerDelaySeconds;
    if (delayedElapsed <= 0.0f) {
        return 0.0f;
    }
    return easeOutCubic(std::clamp(delayedElapsed / kBattleResultKickerDurationSeconds, 0.0f, 1.0f));
}

inline std::string battleResultKickerTransform(const BattleResultOverlayState& overlay, Uint64 nowMs) {
    const float progress = battleResultKickerOpacity(overlay, nowMs);
    const float translateYDp = lerpValue(22.0f, 0.0f, progress);
    const float scale = lerpValue(0.96f, 1.0f, progress);
    return "translateY(" + decimalText(translateYDp) + "dp) scale(" + decimalText(scale) + ")";
}

inline std::string battleResultWordTransform(const BattleResultOverlayState& overlay, Uint64 nowMs) {
    (void)nowMs;
    const float elapsedSeconds = battleResultElapsedSeconds(overlay);
    const float settleStartSeconds = battleResultSettleStartSeconds(overlay);
    if (elapsedSeconds <= settleStartSeconds) {
        return "translateY(0dp) scale(1)";
    }

    const float settleProgress = easeOutCubic(
        std::clamp((elapsedSeconds - settleStartSeconds) / kBattleResultSettleDurationSeconds, 0.0f, 1.0f));
    const float translateYDp = lerpValue(0.0f, -54.0f, settleProgress);
    const float scale = lerpValue(1.0f, 0.76f, settleProgress);
    return "translateY(" + decimalText(translateYDp) + "dp) scale(" + decimalText(scale) + ")";
}

inline float battleResultButtonRevealSeconds(const BattleResultOverlayState& overlay) {
    return battleResultButtonRevealStartSeconds(overlay);
}

inline float battleResultButtonProgress(const BattleResultOverlayState& overlay) {
    const float buttonElapsedSeconds = battleResultElapsedSeconds(overlay) - battleResultButtonRevealSeconds(overlay);
    if (buttonElapsedSeconds <= 0.0f) {
        return 0.0f;
    }
    return easeOutCubic(std::clamp(buttonElapsedSeconds / kBattleResultButtonDurationSeconds, 0.0f, 1.0f));
}

inline void applyBattleResultOverlayDocumentState(Rml::ElementDocument* document,
                                                  const BattleResultOverlayState& overlay,
                                                  Uint64 nowMs) {
    if (document == nullptr) {
        return;
    }

    const bool active = overlay.active;
    setElementClass(document, "battle-result", "active", active);
    setElementClass(document, "battle-result", "victory", overlay.outcome == BattleResultOverlayOutcome::Victory);
    setElementClass(document, "battle-result", "defeat", overlay.outcome == BattleResultOverlayOutcome::Defeat);
    setElementDisplay(document, "battle-result", active);
    setElementText(document, "battle-result-kicker", active ? "BATTLE RESOLVED" : "");
    setElementText(document, "battle-result-button-label", active ? overlay.buttonLabel : "");
    setElementText(document, "battle-result-button-sub", active ? overlay.buttonSubcopy : "");
    ensureBattleResultWordDocument(document, overlay);
    updateBattleResultLetterElements(document, overlay);

    if (Rml::Element* wordShell = document->GetElementById("battle-result-word-shell")) {
        wordShell->SetProperty("transform", active ? battleResultWordTransform(overlay, nowMs) : "translateY(0dp) scale(1)");
    }
    if (Rml::Element* dim = document->GetElementById("battle-result-dim")) {
        dim->SetProperty("opacity", active ? decimalText(battleResultDimOpacity(overlay, nowMs)) : "0");
    }
    if (Rml::Element* kicker = document->GetElementById("battle-result-kicker")) {
        if (!active) {
            kicker->SetProperty("opacity", "0");
            kicker->SetProperty("transform", "translateY(22dp) scale(0.96)");
        } else {
            kicker->SetProperty("opacity", decimalText(battleResultKickerOpacity(overlay, nowMs)));
            kicker->SetProperty("transform", battleResultKickerTransform(overlay, nowMs));
        }
    }
    if (Rml::Element* buttonStage = document->GetElementById("battle-result-button-stage")) {
        if (!active) {
            buttonStage->SetProperty("opacity", "0");
            buttonStage->SetProperty("transform", "translateY(42dp) scale(0.88)");
        } else {
            const float buttonProgress = battleResultButtonProgress(overlay);
            buttonStage->SetProperty("opacity", decimalText(buttonProgress));
            buttonStage->SetProperty(
                "transform",
                "translateY(" + decimalText(lerpValue(42.0f, 0.0f, buttonProgress)) + "dp) scale(" +
                decimalText(lerpValue(0.88f, 1.0f, buttonProgress)) + ")");
        }
    }

    setElementDisplay(document, "battle-combo", !active);
    if (!active) {
        return;
    }

    setElementDisplay(document, "battle-judgement", false);
    setElementDisplay(document, "battle-tutorial", false);
    setElementClass(document, "battle-rhythm", "visible", false);
    setElementDisplay(document, "battle-hint", false);
    setElementClass(document, "battle-hint", "visible", false);
    setElementDisplay(document, "battle-toast", false);
    setElementClass(document, "battle-toast", "visible", false);
    setElementClass(document, "battle-pause", "visible", false);
}

inline void updateComboDocument(Rml::ElementDocument* document, const HudFeedbackState& feedback) {
    if (document == nullptr) {
        return;
    }

    setElementText(document, "battle-combo-count", std::to_string(std::max(0, feedback.comboCount)));
    setElementText(document, "battle-combo-bonus", comboBonusText(feedback.comboBonusFraction));

    if (Rml::Element* fill = document->GetElementById("battle-combo-fill")) {
        const float fillRatio = comboMeterFillRatio(feedback.comboCount);
        fill->SetProperty("width", decimalText(fillRatio * 100.0f, 1) + "%");
    }
}

inline void updateJudgementDocument(Rml::ElementDocument* document, const HudFeedbackState& feedback, Uint64 nowMs) {
    if (document == nullptr) {
        return;
    }

    const bool visible = !feedback.judgementText.empty();
    setElementDisplay(document, "battle-judgement", visible);
    setElementClass(document, "battle-judgement", "perfect", feedback.judgementClassName == "perfect");
    setElementClass(document, "battle-judgement", "good", feedback.judgementClassName == "good");
    setElementClass(document, "battle-judgement", "okay", feedback.judgementClassName == "okay");
    setElementClass(document, "battle-judgement", "flop", feedback.judgementClassName == "flop");

    if (Rml::Element* judgement = document->GetElementById("battle-judgement")) {
        if (!visible) {
            judgement->SetProperty("opacity", "0");
            judgement->SetProperty("transform", "translateX(0dp) scale(1)");
        } else {
            judgement->SetProperty("opacity", decimalText(judgementContainerOpacity(feedback, nowMs)));
            judgement->SetProperty("transform", judgementContainerTransform(feedback, nowMs));
        }
    }

    if (Rml::Element* word = document->GetElementById("battle-judgement-word")) {
        if (!visible) {
            word->SetInnerRML("");
        } else {
            word->SetInnerRML(judgementMarkup(feedback, nowMs));
        }
    }

    setElementText(document, "battle-judgement-reward", feedback.judgementRewardText);
    const bool rewardVisible = visible && !feedback.judgementRewardText.empty();
    setElementDisplay(document, "battle-judgement-reward", rewardVisible);
    if (Rml::Element* reward = document->GetElementById("battle-judgement-reward")) {
        if (!rewardVisible) {
            reward->SetProperty("opacity", "0");
            reward->SetProperty("transform", "translateY(10dp)");
        } else {
            const float rewardOpacity = judgementRewardOpacity(feedback, nowMs);
            reward->SetProperty("opacity", decimalText(rewardOpacity));
            reward->SetProperty("transform",
                                "translateY(" + decimalText(lerpValue(10.0f, 0.0f, rewardOpacity)) + "dp)");

            const JudgementPalette& palette = judgementPalette(feedback.judgementClassName);
            reward->SetProperty("color", rgbaText(palette.reward, 1.0f));
        }
    }
}

inline void updateTutorialDocument(Rml::ElementDocument* document,
                                   const TutorialOverlayState& tutorial,
                                   Uint64 nowMs,
                                   float narrationCharsPerSecond,
                                   const BattleHudDocumentDependencies& dependencies) {
    setElementClass(document, "battle-tutorial", "visible", tutorial.step != TutorialStep::None);
    if (tutorial.step == TutorialStep::None) {
        return;
    }

    setElementText(document, "battle-tutorial-speaker", vn::getDisplaySpeakerName(tutorial.entry));
    if (dependencies.revealNarrationText) {
        setElementText(document,
                       "battle-tutorial-text",
                       dependencies.revealNarrationText(
                           tutorial.entry.text,
                           tutorial.startedMs,
                           nowMs,
                           narrationCharsPerSecond));
    }

    if (tutorial.entry.icon.empty() || !dependencies.findCombatImagePath) {
        return;
    }

    const std::string tutorialIconKey = std::filesystem::path(tutorial.entry.icon).stem().string();
    const std::string iconPath = dependencies.findCombatImagePath("icons", tutorialIconKey);
    if (iconPath.empty()) {
        return;
    }

    if (Rml::Element* element = document->GetElementById("battle-tutorial-portrait")) {
        element->SetProperty("decorator", "image(" + iconPath + " cover center center)");
    }
}

inline void updateRhythmDocument(Rml::ElementDocument* document, const RhythmChallengeState& rhythm, Uint64 nowMs) {
    setElementClass(document, "battle-rhythm", "visible", rhythm.active);
    if (!rhythm.active || document == nullptr) {
        return;
    }

    setElementText(document, "battle-rhythm-title", "");
    setElementText(document, "battle-rhythm-body", "PRESS E WHEN THE CYAN PULSE CROSSES THE GOLD WINDOW.");
    if (Rml::Element* pulse = document->GetElementById("battle-rhythm-pulse")) {
        const float progress = getRhythmProgress(rhythm, nowMs);
        pulse->SetProperty("left", std::to_string(static_cast<int>(std::round(progress * 278.0f))) + "px");
    }
}

} // namespace detail

inline void applyPauseOverlayDocumentState(Rml::ElementDocument* document,
                                           PauseOverlayMode pauseOverlayMode,
                                           PauseSelection pauseSelection,
                                           SettingsSelection settingsSelection,
                                           const GameSettings* settings) {
    const bool showingSettings = pauseOverlayMode == PauseOverlayMode::Settings;
    const detail::PauseOverlayCopy copy = detail::getPauseOverlayCopy(pauseOverlayMode);

    detail::setElementText(document, "battle-pause-title", copy.title);
    detail::setElementText(document, "battle-pause-hint", copy.hint);
    detail::setElementClass(document, "battle-pause-shell", "settings-open", showingSettings);
    detail::setElementClass(document, "battle-pause-menu", "visible", pauseOverlayMode == PauseOverlayMode::Menu);
    detail::setElementClass(document, "battle-pause-settings", "visible", showingSettings);

    if (document == nullptr) {
        return;
    }

    if (Rml::Element* continueButton = document->GetElementById("battle-pause-continue")) {
        continueButton->SetClass("selected", pauseSelection == PauseSelection::Continue);
    }
    if (Rml::Element* settingsButton = document->GetElementById("battle-pause-settings-button")) {
        settingsButton->SetClass("selected", pauseSelection == PauseSelection::Settings);
    }
    if (Rml::Element* exitButton = document->GetElementById("battle-pause-exit")) {
        exitButton->SetClass("selected", pauseSelection == PauseSelection::ExitToMainMenu);
    }

    if (settings != nullptr) {
        detail::setElementText(document, "battle-settings-display-value", settings->fullscreen ? "Fullscreen" : "Windowed");
        detail::setElementText(document,
                               "battle-settings-voice-value",
                               std::to_string(static_cast<int>(std::lround(settings->voiceVolume * 100.0f))) + "%");
        detail::setElementText(document,
                               "battle-settings-text-value",
                               std::to_string(static_cast<int>(std::lround(settings->textSpeed))) + " cps");
        detail::setRangeValue(document,
                              "battle-settings-voice-slider",
                              std::to_string(static_cast<int>(std::lround(settings->voiceVolume * 100.0f))));
        detail::setRangeValue(document,
                              "battle-settings-text-slider",
                              std::to_string(static_cast<int>(std::lround(settings->textSpeed))));
    }

    detail::setElementClass(document, "battle-settings-row-display", "selected", settingsSelection == SettingsSelection::DisplayMode);
    detail::setElementClass(document, "battle-settings-row-voice", "selected", settingsSelection == SettingsSelection::VoiceVolume);
    detail::setElementClass(document, "battle-settings-row-text", "selected", settingsSelection == SettingsSelection::TextSpeed);
    detail::setElementClass(document, "battle-settings-row-back", "selected", settingsSelection == SettingsSelection::Back);
}

inline void updateBattleHudDocument(Rml::ElementDocument* document,
                                    const battle::BattleManager& manager,
                                    const HudAnimationState& animationState,
                                    const HudFeedbackState& feedback,
                                    const BattleResultOverlayState& resultOverlay,
                                    const TutorialOverlayState& tutorial,
                                    const RhythmChallengeState& rhythm,
                                    bool paused,
                                    PauseOverlayMode pauseOverlayMode,
                                    PauseSelection pauseSelection,
                                    SettingsSelection settingsSelection,
                                    const GameSettings* settings,
                                    float narrationCharsPerSecond,
                                    Uint64 nowMs,
                                    const BattleHudDocumentDependencies& dependencies) {
    if (document == nullptr) {
        return;
    }

    const battle::BattleState& battleState = manager.getBattleState();
    const battle::TurnState& turnState = manager.getTurnState();
    const int activeActorIndex = manager.getPreviewNextActorIndex();

    detail::ensurePartyRackDocument(document, battleState.party.size());
    detail::updateComboDocument(document, feedback);
    detail::updateJudgementDocument(document, feedback, nowMs);

    const std::string bossDisplayName = !battleState.boss.title.empty() ? battleState.boss.title : battleState.boss.key;
    detail::setElementText(document,
                           "boss-name",
                           dependencies.uppercaseText ? dependencies.uppercaseText(bossDisplayName) : bossDisplayName);
    detail::setElementProperty(document, "boss-name", "font-size", detail::bossFontSizeForName(bossDisplayName));
    const int bossCurrentHp = manager.getBossCurrentHp();
    const int bossMaxHp = std::max(1, manager.getBossMaxHp());
    const int displayedBossHp = static_cast<int>(std::lround(std::clamp(
        animationState.bossHp.initialized ? animationState.bossHp.displayedValue : static_cast<float>(bossCurrentHp),
        0.0f,
        static_cast<float>(bossMaxHp))));
    const float displayedBossRatio =
        static_cast<float>(displayedBossHp) / static_cast<float>(bossMaxHp);
    const float bossTrailRatio =
        std::clamp((animationState.bossHp.initialized ? animationState.bossHp.trailValue : static_cast<float>(bossCurrentHp)) /
                       static_cast<float>(bossMaxHp),
                   0.0f,
                   1.0f);
    const int bossPercent = static_cast<int>(std::round(displayedBossRatio * 100.0f));
    detail::setElementText(document,
                           "boss-readout",
                           std::to_string(displayedBossHp) + " / " + std::to_string(bossMaxHp) + " HP");
    detail::setElementText(document, "boss-percent", std::to_string(bossPercent) + "%");
    if (Rml::Element* bossFill = document->GetElementById("boss-fill")) {
        bossFill->SetProperty("width", detail::formatPercent(displayedBossRatio));
    }
    if (Rml::Element* bossTrail = document->GetElementById("boss-trail")) {
        const bool showTrail = bossTrailRatio > displayedBossRatio + 0.001f;
        bossTrail->SetProperty("width", detail::formatPercent(bossTrailRatio));
        bossTrail->SetProperty("opacity", showTrail ? "1.0" : "0.0");
    }
    detail::setElementClass(document, "boss-band", "hit", animationState.bossHit.active);

    const std::vector<int> sortedActorIndices = detail::getSortedTurnActorIndices(turnState);
    for (int slot = 0; slot < 5; ++slot) {
        const std::string slotIndex = std::to_string(slot + 1);
        const bool hasActor = slot < static_cast<int>(sortedActorIndices.size());
        detail::setElementDisplay(document, "turn-card-" + slotIndex, hasActor);
        if (!hasActor) {
            continue;
        }

        const battle::TurnActor& actor = turnState.actors[static_cast<size_t>(sortedActorIndices[static_cast<size_t>(slot)])];
        detail::setPortraitDecorator(
            document,
            "turn-portrait-" + slotIndex,
            actor.assetId.empty() ? actor.key : actor.assetId,
            dependencies);
        detail::setElementText(document,
                               "turn-value-" + slotIndex,
                               std::to_string(static_cast<int>(std::round(actor.currentActionValue))));
        if (Rml::Element* card = document->GetElementById("turn-card-" + slotIndex)) {
            card->SetClass("active", sortedActorIndices[static_cast<size_t>(slot)] == activeActorIndex);
            card->SetClass("boss", actor.type == battle::ParticipantType::Boss);
        }
    }

    for (int i = 0; i < static_cast<int>(battleState.party.size()); ++i) {
        const std::string index = std::to_string(i + 1);

        const battle::CharacterDefinition& character = battleState.party[static_cast<size_t>(i)];
        const int currentHp = manager.getCharacterCurrentHp(i);
        const int maxHp = std::max(1, manager.getCharacterMaxHp(i));
        const int ultimateCharge = manager.getCharacterUltimateCharge(i);
        const int ultimateRequired = std::max(1, manager.getCharacterUltimateRequired(i));
        const int shield = manager.getCharacterShield(i);
        const float shieldRatio =
            currentHp > 0 ? std::min(1.0f, static_cast<float>(shield) / static_cast<float>(currentHp)) : 0.0f;
        if (static_cast<size_t>(i) >= animationState.units.size()) {
            continue;
        }
        const HudUnitAnimationState& unitAnimation = animationState.units[static_cast<size_t>(i)];
        const int displayedHp = static_cast<int>(std::lround(std::clamp(
            unitAnimation.hp.initialized ? unitAnimation.hp.displayedValue : static_cast<float>(currentHp),
            0.0f,
            static_cast<float>(maxHp))));
        const float displayedHpRatio = static_cast<float>(displayedHp) / static_cast<float>(maxHp);
        const float hpTrailRatio =
            std::clamp((unitAnimation.hp.initialized ? unitAnimation.hp.trailValue : static_cast<float>(currentHp)) /
                           static_cast<float>(maxHp),
                       0.0f,
                       1.0f);
        const int displayedUltimate = static_cast<int>(std::lround(std::clamp(
            unitAnimation.ultimate.initialized ? unitAnimation.ultimate.displayedValue : static_cast<float>(ultimateCharge),
            0.0f,
            static_cast<float>(ultimateRequired))));
        const float displayedUltimateRatio =
            static_cast<float>(std::clamp(displayedUltimate, 0, ultimateRequired)) /
            static_cast<float>(ultimateRequired);

        detail::setElementText(document,
                               "unit-hp-text-" + index,
                               std::to_string(displayedHp) + " / " + std::to_string(maxHp));
        detail::setPortraitDecorator(document, "unit-icon-" + index, character.assets, dependencies);
        detail::setElementText(document,
                               "unit-ult-text-" + index,
                               std::to_string(displayedUltimate) + " / " + std::to_string(ultimateRequired));
        detail::setElementProperty(document,
                                   "unit-ult-fill-" + index,
                                   "height",
                                   detail::formatPercent(displayedUltimateRatio));
        if (Rml::Element* ultSheen = document->GetElementById("unit-ult-sheen-" + index)) {
            const float sheenProgress = detail::ultimateSheenProgress(unitAnimation, nowMs);
            if (sheenProgress >= 0.0f) {
                const float left = -30.0f + sheenProgress * 136.0f;
                const float fade = 1.0f - std::fabs(sheenProgress - 0.5f) * 2.0f;
                ultSheen->SetProperty("left", std::to_string(static_cast<int>(std::lround(left))) + "px");
                ultSheen->SetProperty("opacity", std::to_string(std::clamp(fade * 0.72f, 0.0f, 0.72f)));
            } else {
                ultSheen->SetProperty("left", "-30px");
                ultSheen->SetProperty("opacity", "0.0");
            }
        }
        if (Rml::Element* hpTrail = document->GetElementById("unit-hp-trail-" + index)) {
            const bool showTrail = hpTrailRatio > displayedHpRatio + 0.001f;
            hpTrail->SetProperty("width", detail::formatPercent(hpTrailRatio));
            hpTrail->SetProperty("opacity", showTrail ? "1.0" : "0.0");
        }
        if (Rml::Element* hpFill = document->GetElementById("unit-hp-fill-" + index)) {
            hpFill->SetProperty("width", detail::formatPercent(displayedHpRatio));
            hpFill->SetProperty("background-color", detail::hpColorForRatio(displayedHpRatio));
        }
        detail::setElementDisplay(document, "unit-shield-badge-" + index, shield > 0);
        detail::setElementText(document, "unit-shield-value-" + index, std::to_string(shield));
        detail::setElementDisplay(document, "unit-shield-top-" + index, shield > 0);
        detail::setElementDisplay(document, "unit-shield-bottom-" + index, shield > 0);
        detail::setElementDisplay(document, "unit-shield-left-" + index, shield > 0);
        detail::setElementDisplay(document, "unit-shield-right-" + index, shield > 0 && shieldRatio >= 0.999f);
        const std::string shieldWidth =
            std::to_string(static_cast<int>(std::round(shieldRatio * 100.0f))) + "%";
        detail::setElementProperty(document, "unit-shield-top-" + index, "width", shieldWidth);
        detail::setElementProperty(document, "unit-shield-bottom-" + index, "width", shieldWidth);
        if (Rml::Element* icon = document->GetElementById("unit-icon-" + index)) {
            const float hitProgress = detail::hitReactionProgress(unitAnimation.hit, nowMs);
            const float hitEnvelope = unitAnimation.hit.active ? (1.0f - hitProgress) : 0.0f;
            const float shakePhase = hitProgress * 8.0f * 6.2831853f;
            const int shakeOffset = static_cast<int>(std::lround(std::sin(shakePhase) * 6.0f * hitEnvelope));
            const unsigned char greenBlue =
                static_cast<unsigned char>(std::lround(255.0f - (145.0f * hitEnvelope)));
            icon->SetProperty("transform", "translateX(" + std::to_string(shakeOffset) + "px)");
            icon->SetProperty("image-color", detail::formatHexColor(255, greenBlue, greenBlue));
        }
        if (Rml::Element* flash = document->GetElementById("unit-hit-flash-" + index)) {
            const float hitProgress = detail::hitReactionProgress(unitAnimation.hit, nowMs);
            const float hitEnvelope = unitAnimation.hit.active ? (1.0f - hitProgress) : 0.0f;
            flash->SetProperty("opacity", std::to_string(std::clamp(hitEnvelope * 0.6f, 0.0f, 0.6f)));
        }
        if (Rml::Element* card = document->GetElementById("unit-card-" + index)) {
            const bool isFocused =
                activeActorIndex >= 0 && activeActorIndex < static_cast<int>(turnState.actors.size()) &&
                turnState.actors[static_cast<size_t>(activeActorIndex)].type == battle::ParticipantType::Character &&
                turnState.actors[static_cast<size_t>(activeActorIndex)].partyIndex == i;
            card->SetClass("active", isFocused);
            card->SetClass("ghost", currentHp <= 0);
            card->SetClass("hit", unitAnimation.hit.active);
            card->SetClass(
                "ult-ready",
                currentHp > 0 && ultimateCharge >= ultimateRequired
            );
        }
    }

    detail::updateHintDocument(document, feedback);
    detail::updateToastDocument(document, feedback);
    detail::updateTutorialDocument(document, tutorial, nowMs, narrationCharsPerSecond, dependencies);
    detail::updateRhythmDocument(document, rhythm, nowMs);

    detail::setElementClass(document, "battle-pause", "visible", paused);
    if (paused) {
        applyPauseOverlayDocumentState(document, pauseOverlayMode, pauseSelection, settingsSelection, settings);
    }
    detail::applyBattleResultOverlayDocumentState(document, resultOverlay, nowMs);
}

} // namespace battle::app::ui

#endif
