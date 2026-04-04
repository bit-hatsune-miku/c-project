#ifndef BATTLE_SESSION_DOCUMENT_UPDATES_H
#define BATTLE_SESSION_DOCUMENT_UPDATES_H

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
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

#include "../../platform/text_fallback.h"
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

inline std::string escapeRmlText(const std::string& text) {
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

inline std::string escapeRmlAttribute(const std::string& text) {
    return escapeRmlText(text);
}

inline void setElementText(Rml::ElementDocument* document, const std::string& id, const std::string& text) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetInnerRML(text);
    }
}

inline void setElementEscapedText(Rml::ElementDocument* document, const std::string& id, const std::string& text) {
    if (document == nullptr) {
        return;
    }
    if (Rml::Element* element = document->GetElementById(id)) {
        element->SetInnerRML(escapeRmlText(text));
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

inline void setCombatDecorator(Rml::ElementDocument* document,
                               const std::string& id,
                               const std::string& folder,
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
        const std::string path = dependencies.findCombatImagePath(folder, assetName);
        if (!path.empty()) {
            element->SetProperty("decorator", "image(" + path + " cover center center)");
        } else {
            element->RemoveProperty("decorator");
        }
    }
}

inline std::size_t utf8CodepointBytes(const std::string& text, std::size_t byteOffset) {
    if (byteOffset >= text.size()) {
        return 0;
    }
    return static_cast<std::size_t>(std::max(1, platform::text::utf8CodepointLength(
        static_cast<unsigned char>(text[byteOffset]))));
}

inline bool containsCjkText(const std::string& text) {
    std::size_t index = 0;
    while (index < text.size()) {
        const std::size_t codeBytes = utf8CodepointBytes(text, index);
        if (codeBytes == 0 || index + codeBytes > text.size()) {
            break;
        }

        uint32_t codepoint = 0;
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        if (codeBytes == 1) {
            codepoint = lead;
        } else if (codeBytes == 2) {
            codepoint = ((lead & 0x1Fu) << 6u) |
                        (static_cast<unsigned char>(text[index + 1]) & 0x3Fu);
        } else if (codeBytes == 3) {
            codepoint = ((lead & 0x0Fu) << 12u) |
                        ((static_cast<unsigned char>(text[index + 1]) & 0x3Fu) << 6u) |
                        (static_cast<unsigned char>(text[index + 2]) & 0x3Fu);
        } else {
            codepoint = ((lead & 0x07u) << 18u) |
                        ((static_cast<unsigned char>(text[index + 1]) & 0x3Fu) << 12u) |
                        ((static_cast<unsigned char>(text[index + 2]) & 0x3Fu) << 6u) |
                        (static_cast<unsigned char>(text[index + 3]) & 0x3Fu);
        }

        if (platform::text::isCjkCodepoint(codepoint)) {
            return true;
        }

        index += codeBytes;
    }

    return false;
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
            << "<div class=\"unit-status-host\">"
            << "<div class=\"status-badge-lane status-badge-lane--unit\" id=\"unit-status-badges-" << displayIndex << "\"></div>"
            << "</div>"
            << "</div>"
            << "<div class=\"unit-hit-flash\" id=\"unit-hit-flash-" << displayIndex << "\"></div>"
            << "</div>";
    }
    return markup.str();
}

inline const char* statusBadgeCategoryClass(battle::BattleStatusBadgeCategory category) {
    switch (category) {
        case battle::BattleStatusBadgeCategory::Shield:
            return "status-badge--shield";
        case battle::BattleStatusBadgeCategory::Buff:
            return "status-badge--buff";
        case battle::BattleStatusBadgeCategory::Debuff:
            return "status-badge--debuff";
    }
    return "status-badge--buff";
}

inline std::string formatStatusBadgeText(const battle::BattleStatusBadge& badge) {
    const auto withSign = [](int value) {
        return value > 0 ? "+" + std::to_string(value) : std::to_string(value);
    };

    switch (badge.valueKind) {
        case battle::BattleStatusBadgeValueKind::Flat:
            if (!badge.statLabel.empty()) {
                return withSign(badge.value) + " " + badge.statLabel;
            }
            return withSign(badge.value);
        case battle::BattleStatusBadgeValueKind::Percent:
            if (!badge.statLabel.empty()) {
                return withSign(badge.value) + "% " + badge.statLabel;
            }
            return withSign(badge.value) + "%";
        case battle::BattleStatusBadgeValueKind::Charges:
            if (!badge.statusName.empty()) {
                return badge.statusName + " x" + std::to_string(std::max(0, badge.value));
            }
            return "x" + std::to_string(std::max(0, badge.value));
        case battle::BattleStatusBadgeValueKind::None:
            break;
    }

    if (badge.category == battle::BattleStatusBadgeCategory::Shield) {
        return std::to_string(std::max(0, badge.value));
    }
    if (!badge.statusName.empty()) {
        return badge.statusName;
    }
    return badge.abilityId;
}

inline std::string buildStatusBadgeMarkup(const battle::BattleStatusBadge& badge,
                                          const BattleHudDocumentDependencies& dependencies) {
    std::ostringstream markup;
    markup << "<div class=\"status-badge " << statusBadgeCategoryClass(badge.category) << "\">";

    if (badge.category == battle::BattleStatusBadgeCategory::Shield) {
        markup << "<div class=\"status-badge-icon status-badge-icon--shield\"></div>";
    } else {
        markup << "<div class=\"status-badge-icon status-badge-icon--portrait\"";
        if (dependencies.findCombatImagePath && !badge.sourceAssetId.empty()) {
            const std::string iconPath = dependencies.findCombatImagePath("icons", badge.sourceAssetId);
            if (!iconPath.empty()) {
                markup << " style=\"decorator:image(" << escapeRmlAttribute(iconPath)
                       << " cover center center);\"";
            }
        }
        markup << "></div>";
    }

    markup << "<div class=\"status-badge-text\">"
           << escapeRmlText(formatStatusBadgeText(badge))
           << "</div></div>";
    return markup.str();
}

inline void updateStatusBadgeLaneDocument(Rml::ElementDocument* document,
                                          const std::string& id,
                                          const std::vector<battle::BattleStatusBadge>& badges,
                                          battle::BattleStatusBadgeTarget target,
                                          int targetPartyIndex,
                                          const BattleHudDocumentDependencies& dependencies) {
    if (document == nullptr) {
        return;
    }

    if (Rml::Element* element = document->GetElementById(id)) {
        std::vector<const battle::BattleStatusBadge*> matchingBadges;
        matchingBadges.reserve(badges.size());
        for (const battle::BattleStatusBadge& badge : badges) {
            if (badge.target != target || badge.targetPartyIndex != targetPartyIndex) {
                continue;
            }
            matchingBadges.push_back(&badge);
        }

        std::ostringstream markup;
        bool hasBadges = false;
        for (auto it = matchingBadges.rbegin(); it != matchingBadges.rend(); ++it) {
            hasBadges = true;
            markup << buildStatusBadgeMarkup(**it, dependencies);
        }

        element->SetInnerRML(markup.str());
        element->SetProperty("display", hasBadges ? "flex" : "none");
    }
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

inline void updateHintDocument(Rml::ElementDocument* document, HudFeedbackState& feedback);

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

inline std::string battleHintBadgeText(const BattleHintRequest& request) {
    if (!request.badgeText.empty()) {
        return request.badgeText;
    }

    switch (request.family) {
        case BattleHintFamily::Tutorial:
            return "GUIDE";
        case BattleHintFamily::Warning:
            return "ALERT";
        case BattleHintFamily::Major:
            return "PHASE";
        case BattleHintFamily::Info:
        default:
            return "INFO";
    }
}

inline std::string buildBattleHintSlotMarkup(const std::string& baseId) {
    std::ostringstream markup;
    markup
        << "<div class=\"battle-hint-slot\" id=\"" << baseId << "\">"
        << "<div class=\"battle-hint-shell\" id=\"" << baseId << "-shell\">"
        << "<div class=\"battle-hint-inner\" id=\"" << baseId << "-inner\">"
        << "<div class=\"battle-hint-accent\" id=\"" << baseId << "-accent\">"
        << "<div class=\"battle-hint-accent-shine\"></div>"
        << "</div>"
        << "<div class=\"battle-hint-copy\" id=\"" << baseId << "-copy\">"
        << "<div class=\"battle-hint-topline\">"
        << "<div class=\"battle-hint-kicker\" id=\"" << baseId << "-kicker\"></div>"
        << "<div class=\"battle-hint-source\" id=\"" << baseId << "-source\"></div>"
        << "</div>"
        << "<div class=\"battle-hint-message\" id=\"" << baseId << "-message\"></div>"
        << "</div>"
        << "<div class=\"battle-hint-side\" id=\"" << baseId << "-side\">"
        << "<div class=\"battle-hint-badge\" id=\"" << baseId << "-badge\">"
        << "<div class=\"battle-hint-badge-text\" id=\"" << baseId << "-badge-text\"></div>"
        << "</div>"
        << "<div class=\"battle-hint-dismiss\" id=\"" << baseId << "-dismiss\"></div>"
        << "</div>"
        << "</div>"
        << "<div class=\"battle-hint-timer\" id=\"" << baseId << "-timer\">"
        << "<div class=\"battle-hint-timer-fill\" id=\"" << baseId << "-timer-fill\"></div>"
        << "</div>"
        << "</div>"
        << "</div>";
    return markup.str();
}

inline std::string buildBattleHintStackMarkup(std::size_t slotCount) {
    std::ostringstream markup;
    for (std::size_t i = 0; i < slotCount; ++i) {
        markup << buildBattleHintSlotMarkup("battle-hint-slot-" + std::to_string(i + 1));
    }
    return markup.str();
}

inline void ensureBattleHintDocument(Rml::ElementDocument* document) {
    if (document == nullptr) {
        return;
    }

    if (Rml::Element* stack = document->GetElementById("battle-hint-stack")) {
        const std::string slotCount = std::to_string(kBattleHintMaxVisible);
        if (stack->GetAttribute<std::string>("data-slot-count", "") != slotCount) {
            stack->SetInnerRML(buildBattleHintStackMarkup(kBattleHintMaxVisible));
            stack->SetAttribute("data-slot-count", slotCount);
        }
    }

    if (Rml::Element* measureHost = document->GetElementById("battle-hint-measure-host")) {
        if (measureHost->GetAttribute<std::string>("data-ready", "") != "1") {
            measureHost->SetInnerRML(buildBattleHintSlotMarkup("battle-hint-measure"));
            measureHost->SetAttribute("data-ready", "1");
        }
    }
}

inline void applyBattleHintFamilyClasses(Rml::Element* slot, BattleHintFamily family) {
    if (slot == nullptr) {
        return;
    }

    slot->SetClass("family-info", family == BattleHintFamily::Info);
    slot->SetClass("family-tutorial", family == BattleHintFamily::Tutorial);
    slot->SetClass("family-warning", family == BattleHintFamily::Warning);
    slot->SetClass("family-major", family == BattleHintFamily::Major);
}

inline void populateBattleHintSlot(Rml::ElementDocument* document,
                                   const std::string& baseId,
                                   const BattleHintInstance& hint,
                                   const std::string& messageText) {
    setElementText(document, baseId + "-kicker", hint.request.kicker);
    setElementText(document, baseId + "-source", hint.request.sourceTag);
    setElementText(document, baseId + "-message", messageText);
    setElementText(document, baseId + "-badge-text", battleHintBadgeText(hint.request));
    setElementText(document, baseId + "-dismiss", hint.request.dismissLabel);

    const bool showTimer = battleHintHasAutoTimeout(hint);

    setElementDisplay(document, baseId + "-source", false);
    setElementDisplay(document, baseId + "-badge", false);
    setElementDisplay(document, baseId + "-dismiss", false);
    setElementDisplay(document, baseId + "-timer", showTimer);
    setElementProperty(document, baseId + "-side", "display", "none");
}

inline float battleHintOpenProgress(const BattleHintInstance& hint) {
    if (hint.phase != BattleHintPhase::Opening) {
        return 1.0f;
    }

    return easeOutCubic(
        std::clamp(
            static_cast<float>(hint.phaseElapsedMs) / static_cast<float>(std::max<Uint64>(kBattleHintOpenDurationMs, 1)),
            0.0f,
            1.0f));
}

inline float battleHintCloseProgress(const BattleHintInstance& hint) {
    if (hint.phase != BattleHintPhase::Closing) {
        return 0.0f;
    }

    return easeInCubic(
        std::clamp(
            static_cast<float>(hint.phaseElapsedMs) / static_cast<float>(std::max<Uint64>(kBattleHintCloseDurationMs, 1)),
            0.0f,
            1.0f));
}

inline float battleHintShellVisibility(const BattleHintInstance& hint) {
    if (hint.phase == BattleHintPhase::Opening) {
        return battleHintOpenProgress(hint);
    }
    if (hint.phase == BattleHintPhase::Closing) {
        return 1.0f - battleHintCloseProgress(hint);
    }
    return 1.0f;
}

inline float battleHintCopyVisibility(const BattleHintInstance& hint) {
    if (hint.phase == BattleHintPhase::Closing) {
        return battleHintShellVisibility(hint);
    }

    return easeOutCubic(
        std::clamp((battleHintShellVisibility(hint) - 0.18f) / 0.82f, 0.0f, 1.0f));
}

inline float battleHintStackVisibility(const BattleHintInstance& hint) {
    if (hint.phase != BattleHintPhase::Closing) {
        return 1.0f;
    }
    return 1.0f - battleHintCloseProgress(hint);
}

inline void measureBattleHintInstance(Rml::ElementDocument* document, BattleHintInstance& hint) {
    constexpr float kBattleHintShellBorderWidthDp = 1.0f;

    ensureBattleHintDocument(document);
    if (document == nullptr) {
        return;
    }

    Rml::Element* slot = document->GetElementById("battle-hint-measure");
    Rml::Element* shell = document->GetElementById("battle-hint-measure-shell");
    if (slot == nullptr || shell == nullptr) {
        return;
    }

    applyBattleHintFamilyClasses(slot, hint.request.family);
    slot->SetClass("live", true);
    slot->SetClass("closing", false);
    populateBattleHintSlot(
        document,
        "battle-hint-measure",
        hint,
        !hint.request.message.empty() ? hint.request.message : hint.visibleMessage);

    shell->SetProperty("width", "auto");
    const float contentWidth = std::max(shell->GetClientWidth(), 0.0f);
    const float fallbackContentWidth = std::max(shell->GetOffsetWidth() - (kBattleHintShellBorderWidthDp * 2.0f), 0.0f);
    const float unclampedWidth = std::max(contentWidth > 0.0f ? contentWidth : fallbackContentWidth, kBattleHintMinWidthDp);
    const float measuredWidth = std::clamp(unclampedWidth, kBattleHintMinWidthDp, kBattleHintMaxWidthDp);
    shell->SetProperty("width", decimalText(measuredWidth) + "dp");

    hint.measuredWidthDp = measuredWidth;
    hint.measuredHeightDp = std::max(shell->GetOffsetHeight(), 0.0f);
    hint.measurementDirty = false;
}

inline void resetBattleHintSlotDocument(Rml::ElementDocument* document, const std::string& baseId) {
    if (document == nullptr) {
        return;
    }

    if (Rml::Element* slot = document->GetElementById(baseId)) {
        slot->SetClass("family-info", true);
        slot->SetClass("family-tutorial", false);
        slot->SetClass("family-warning", false);
        slot->SetClass("family-major", false);
        slot->SetClass("live", false);
        slot->SetClass("closing", false);
        slot->SetProperty("top", "0dp");
        slot->SetProperty("height", "0dp");
        slot->SetProperty("z-index", "0");
    }
    if (Rml::Element* shell = document->GetElementById(baseId + "-shell")) {
        shell->SetProperty("width", "0dp");
        shell->SetProperty("margin-left", "0dp");
        shell->SetProperty("opacity", "0");
    }
    if (Rml::Element* copy = document->GetElementById(baseId + "-copy")) {
        copy->SetProperty("opacity", "0");
        copy->SetProperty("transform", "translateY(10dp)");
    }
    if (Rml::Element* timerFill = document->GetElementById(baseId + "-timer-fill")) {
        timerFill->SetProperty("width", "0%");
    }
}

inline void updateHintDocument(Rml::ElementDocument* document, HudFeedbackState& feedback) {
    constexpr float kBattleHintShellBorderWidthDp = 1.0f;

    if (document == nullptr) {
        return;
    }

    ensureBattleHintDocument(document);
    const std::size_t visibleCount = std::min(feedback.hints.active.size(), kBattleHintMaxVisible);
    setElementDisplay(document, "battle-hint-lane", visibleCount > 0);
    Rml::Element* stack = document->GetElementById("battle-hint-stack");
    float accumulatedTopDp = 0.0f;

    for (std::size_t i = 0; i < kBattleHintMaxVisible; ++i) {
        const std::string baseId = "battle-hint-slot-" + std::to_string(i + 1);
        if (i >= visibleCount) {
            resetBattleHintSlotDocument(document, baseId);
            continue;
        }

        BattleHintInstance& hint = feedback.hints.active[i];
        if (hint.measurementDirty || hint.measuredWidthDp <= 0.0f || hint.measuredHeightDp <= 0.0f) {
            measureBattleHintInstance(document, hint);
        }

        Rml::Element* slot = document->GetElementById(baseId);
        Rml::Element* shell = document->GetElementById(baseId + "-shell");
        Rml::Element* copy = document->GetElementById(baseId + "-copy");
        Rml::Element* timerFill = document->GetElementById(baseId + "-timer-fill");
        if (slot == nullptr || shell == nullptr || copy == nullptr || timerFill == nullptr) {
            continue;
        }

        applyBattleHintFamilyClasses(slot, hint.request.family);
        const bool closing = hint.phase == BattleHintPhase::Closing;
        slot->SetClass("live", !closing);
        slot->SetClass("closing", closing);
        populateBattleHintSlot(document, baseId, hint, hint.visibleMessage);

        const float shellVisibility = battleHintShellVisibility(hint);
        const float copyVisibility = battleHintCopyVisibility(hint);
        const float stackVisibility = battleHintStackVisibility(hint);
        const float slotHeight =
            (hint.measuredHeightDp + ((i + 1 < visibleCount) ? kBattleHintStackGapDp : 0.0f)) * stackVisibility;
        const float shellWidth = hint.measuredWidthDp * shellVisibility;
        const float shellOuterWidth = shellWidth + (kBattleHintShellBorderWidthDp * 2.0f * shellVisibility);

        float copyTranslateY = 0.0f;
        if (hint.phase == BattleHintPhase::Opening) {
            copyTranslateY = lerpValue(14.0f, 0.0f, copyVisibility);
        } else if (hint.phase == BattleHintPhase::Closing) {
            copyTranslateY = lerpValue(0.0f, -10.0f, 1.0f - copyVisibility);
        }

        slot->SetProperty("height", decimalText(slotHeight) + "dp");
        slot->SetProperty("top", decimalText(accumulatedTopDp) + "dp");
        slot->SetProperty("z-index", std::to_string(static_cast<int>(visibleCount - i)));
        shell->SetProperty("width", decimalText(shellWidth) + "dp");
        shell->SetProperty("margin-left", decimalText(-shellOuterWidth * 0.5f) + "dp");
        shell->SetProperty("opacity", decimalText(shellVisibility));
        copy->SetProperty("opacity", decimalText(copyVisibility));
        copy->SetProperty("transform", "translateY(" + decimalText(copyTranslateY) + "dp)");
        timerFill->SetProperty("width", decimalText(battleHintTimeoutProgress(hint) * 100.0f, 1) + "%");

        accumulatedTopDp += slotHeight;
    }

    if (stack != nullptr) {
        stack->SetProperty("height", decimalText(accumulatedTopDp) + "dp");
    }
}

inline constexpr std::size_t kBattleInputPromptMaxFollowUpSlots = 8;

inline void applyBattleInputPromptTypeClasses(Rml::ElementDocument* document,
                                              const BattleInputPromptState& prompt) {
    setElementClass(document, "battle-input-prompt", "has-primary", battleInputPromptHasPrimary(prompt));
    setElementClass(document, "battle-input-prompt", "has-follow-up", battleInputPromptHasFollowUp(prompt));
    setElementClass(document, "battle-input-prompt", "type-wild", prompt.type == battle::InputPromptType::Wild);
    setElementClass(document, "battle-input-prompt", "type-custom", prompt.type == battle::InputPromptType::Custom);
    setElementClass(document, "battle-input-prompt", "type-arrows", prompt.type == battle::InputPromptType::Arrows);
    setElementClass(document, "battle-input-prompt", "type-left-right", prompt.type == battle::InputPromptType::LeftRight);
    setElementClass(document, "battle-input-prompt", "type-spam-space", prompt.type == battle::InputPromptType::SpamSpace);
}

inline Uint64 battleInputPromptNextRandom(Uint64& state) {
    state += 0x9e3779b97f4a7c15ULL;
    Uint64 z = state;
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

inline Uint64 battleInputPromptSeed(const BattleInputPromptState& prompt, Uint64 salt = 0) {
    const Uint64 abilityHash = static_cast<Uint64>(std::hash<std::string>{}(prompt.abilityId));
    const Uint64 base = prompt.startedMs != 0 ? prompt.startedMs : 0x243f6a8885a308d3ULL;
    return abilityHash ^ (base * 0x9e3779b97f4a7c15ULL) ^ salt;
}

inline float battleInputPromptKeyTiltDegrees(const BattleInputPromptState& prompt, std::size_t keyIndex) {
    if (prompt.type != battle::InputPromptType::Wild) {
        return 0.0f;
    }

    static constexpr float kWildTilts[] = {-13.0f, -7.0f, 0.0f, 7.0f, 13.0f};
    if (keyIndex < (sizeof(kWildTilts) / sizeof(kWildTilts[0]))) {
        return kWildTilts[keyIndex];
    }
    return 0.0f;
}

inline bool battleInputPromptUsesArrowGrid(const BattleInputPromptState& prompt, std::size_t keyCount) {
    return prompt.type == battle::InputPromptType::Arrows && keyCount >= 4;
}

inline std::string battleInputPromptTransformText(float translateYDp, float rotateDeg = 0.0f) {
    std::ostringstream transform;
    transform << "translateY(" << decimalText(translateYDp) << "dp)";
    if (std::fabs(rotateDeg) > 0.01f) {
        transform << " rotate(" << decimalText(rotateDeg) << "deg)";
    }
    return transform.str();
}

inline void setBattleInputPromptKeyState(Rml::ElementDocument* document,
                                         const std::string& keyId,
                                         const std::string& labelId,
                                         bool visible,
                                         const std::string& label,
                                         bool pressed,
                                         float rotateDeg = 0.0f) {
    setElementProperty(document, keyId, "display", visible ? "flex" : "none");
    setElementEscapedText(document, labelId, visible ? label : "");
    if (Rml::Element* key = document->GetElementById(keyId)) {
        key->SetClass("is-pressed", visible && pressed);
        key->SetClass("is-space", visible && label == "SPACE");
        key->SetProperty(
            "transform",
            battleInputPromptTransformText(visible && pressed ? 4.0f : 0.0f, visible ? rotateDeg : 0.0f));
    }
}

inline std::vector<std::string> battleInputPromptResolvedLabels(const BattleInputPromptState& prompt, Uint64 nowMs) {
    if (prompt.type != battle::InputPromptType::Wild) {
        return prompt.followUpKeys;
    }

    static constexpr const char* kWildPool[] = {
        "Q", "W", "E", "R", "A", "S", "D", "F", "J", "K", "L", "Z", "X", "C", "V"
    };
    constexpr std::size_t kWildPoolSize = sizeof(kWildPool) / sizeof(kWildPool[0]);

    const std::size_t keyCount = std::min(prompt.followUpKeys.size(), kWildPoolSize);
    std::vector<std::string> labels;
    labels.reserve(keyCount);
    if (keyCount == 0) {
        return labels;
    }

    const Uint64 elapsedMs = nowMs >= prompt.startedMs ? nowMs - prompt.startedMs : 0;
    const Uint64 refreshIndex = elapsedMs / 1200;
    std::array<const char*, kWildPoolSize> shuffledPool{};
    std::copy(std::begin(kWildPool), std::end(kWildPool), shuffledPool.begin());
    Uint64 randomState = battleInputPromptSeed(
        prompt,
        0xa0761d6478bd642fULL ^ (refreshIndex * 0xe7037ed1a0b428dbULL));
    for (std::size_t i = kWildPoolSize - 1; i > 0; --i) {
        const std::size_t swapIndex =
            static_cast<std::size_t>(battleInputPromptNextRandom(randomState) % static_cast<Uint64>(i + 1));
        std::swap(shuffledPool[i], shuffledPool[swapIndex]);
    }
    for (std::size_t i = 0; i < keyCount; ++i) {
        labels.emplace_back(shuffledPool[i]);
    }
    return labels;
}

inline bool battleInputPromptPrimaryPressed(const BattleInputPromptState& prompt, Uint64 nowMs) {
    if (!battleInputPromptHasPrimary(prompt)) {
        return false;
    }

    const Uint64 elapsedMs = nowMs >= prompt.startedMs ? nowMs - prompt.startedMs : 0;
    if (prompt.type == battle::InputPromptType::SpamSpace &&
        !battleInputPromptHasFollowUp(prompt)) {
        const Uint64 cycleMs = elapsedMs % 420;
        return cycleMs >= 100 && cycleMs < 250;
    }

    const Uint64 cycleMs = elapsedMs % 1320;
    return cycleMs >= 780 && cycleMs < 1000;
}

inline int battleInputPromptActiveFollowUpIndex(const BattleInputPromptState& prompt,
                                                Uint64 nowMs,
                                                std::size_t keyCount) {
    if (!battleInputPromptHasFollowUp(prompt) || keyCount == 0) {
        return -1;
    }

    const Uint64 elapsedMs = nowMs >= prompt.startedMs ? nowMs - prompt.startedMs : 0;
    if (prompt.type == battle::InputPromptType::SpamSpace) {
        const Uint64 cycleMs = elapsedMs % 420;
        return (cycleMs >= 110 && cycleMs < 250) ? 0 : -1;
    }

    if (elapsedMs < 140) {
        return -1;
    }

    const Uint64 pulseElapsedMs = elapsedMs - 140;
    constexpr Uint64 kPulseWindowMs = 240;
    constexpr Uint64 kPulseHoldMs = 150;
    if ((pulseElapsedMs % kPulseWindowMs) >= kPulseHoldMs) {
        return -1;
    }

    const std::size_t cycleIndex = static_cast<std::size_t>(pulseElapsedMs / kPulseWindowMs);
    Uint64 randomState = battleInputPromptSeed(
        prompt,
        0x8e80d37c4f1a9d5bULL ^ (static_cast<Uint64>(cycleIndex) * 0x9e3779b97f4a7c15ULL));
    std::size_t activeIndex = static_cast<std::size_t>(battleInputPromptNextRandom(randomState) % keyCount);
    if (keyCount > 1 && cycleIndex > 0) {
        Uint64 previousState = battleInputPromptSeed(
            prompt,
            0x8e80d37c4f1a9d5bULL ^ (static_cast<Uint64>(cycleIndex - 1) * 0x9e3779b97f4a7c15ULL));
        const std::size_t previousIndex =
            static_cast<std::size_t>(battleInputPromptNextRandom(previousState) % keyCount);
        if (activeIndex == previousIndex) {
            Uint64 rerollState = battleInputPromptSeed(
                prompt,
                0xd1b54a32d192ed03ULL ^ (static_cast<Uint64>(cycleIndex) * 0x94d049bb133111ebULL));
            activeIndex = (previousIndex + 1 +
                           (static_cast<std::size_t>(battleInputPromptNextRandom(rerollState)) % (keyCount - 1))) %
                keyCount;
        }
    }
    return static_cast<int>(activeIndex);
}

inline void updateBattleInputPromptDocument(Rml::ElementDocument* document,
                                            const BattleInputPromptState& prompt,
                                            Uint64 nowMs) {
    if (document == nullptr) {
        return;
    }

    setElementClass(document, "battle-input-prompt", "visible", prompt.visible);
    setElementDisplay(document, "battle-input-prompt", prompt.visible);
    applyBattleInputPromptTypeClasses(document, prompt);

    if (!prompt.visible) {
        setElementDisplay(document, "battle-input-prompt-row-primary", false);
        setElementDisplay(document, "battle-input-prompt-row-plus", false);
        setElementDisplay(document, "battle-input-prompt-row-follow-up", false);
        setElementProperty(document, "battle-input-prompt-plus", "display", "none");
        setElementProperty(document, "battle-input-prompt-follow-up-strip", "display", "none");
        setElementProperty(document, "battle-input-prompt-follow-up-arrows", "display", "none");
        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-primary",
            "battle-input-prompt-primary-label",
            false,
            "",
            false);
        for (std::size_t i = 0; i < kBattleInputPromptMaxFollowUpSlots; ++i) {
            const std::string index = std::to_string(i + 1);
            setBattleInputPromptKeyState(document,
                                         "battle-input-prompt-follow-up-key-" + index,
                                         "battle-input-prompt-follow-up-label-" + index,
                                         false,
                                         "",
                                         false);
        }
        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-arrow-key-up",
            "battle-input-prompt-arrow-label-up",
            false,
            "",
            false);
        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-arrow-key-left",
            "battle-input-prompt-arrow-label-left",
            false,
            "",
            false);
        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-arrow-key-down",
            "battle-input-prompt-arrow-label-down",
            false,
            "",
            false);
        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-arrow-key-right",
            "battle-input-prompt-arrow-label-right",
            false,
            "",
            false);
        return;
    }

    const bool showPrimary = battleInputPromptHasPrimary(prompt);
    const std::vector<std::string> followUpLabels = battleInputPromptResolvedLabels(prompt, nowMs);
    if (followUpLabels.size() > kBattleInputPromptMaxFollowUpSlots) {
        static std::string warnedAbilityId;
        if (warnedAbilityId != prompt.abilityId) {
            std::fprintf(stderr,
                         "[BattleHUD] Input prompt for ability '%s' has %zu follow-up keys; clamping to %zu.\n",
                         prompt.abilityId.c_str(),
                         followUpLabels.size(),
                         kBattleInputPromptMaxFollowUpSlots);
            warnedAbilityId = prompt.abilityId;
        }
    }

    const std::size_t visibleFollowUpCount = std::min(followUpLabels.size(), kBattleInputPromptMaxFollowUpSlots);
    const bool showFollowUp = visibleFollowUpCount > 0;
    setElementDisplay(document, "battle-input-prompt-row-primary", showPrimary);
    setElementDisplay(document, "battle-input-prompt-row-plus", showPrimary && showFollowUp);
    setElementDisplay(document, "battle-input-prompt-row-follow-up", showFollowUp);
    setElementEscapedText(document, "battle-input-prompt-primary-label", prompt.primaryLabel);
    setElementProperty(document,
                       "battle-input-prompt-plus",
                       "display",
                       showPrimary && showFollowUp ? "inline-block" : "none");

    setBattleInputPromptKeyState(document,
                                 "battle-input-prompt-primary",
                                 "battle-input-prompt-primary-label",
                                 showPrimary,
                                 prompt.primaryLabel,
                                 showPrimary && battleInputPromptPrimaryPressed(prompt, nowMs));

    if (showFollowUp) {
        const bool useArrowGrid = battleInputPromptUsesArrowGrid(prompt, visibleFollowUpCount);
        const int activeFollowUpIndex =
            battleInputPromptActiveFollowUpIndex(prompt, nowMs, visibleFollowUpCount);
        setElementProperty(document, "battle-input-prompt-follow-up-strip", "display", useArrowGrid ? "none" : "flex");
        setElementProperty(document, "battle-input-prompt-follow-up-arrows", "display", useArrowGrid ? "block" : "none");

        if (useArrowGrid) {
            for (std::size_t i = 0; i < kBattleInputPromptMaxFollowUpSlots; ++i) {
                const std::string index = std::to_string(i + 1);
                setBattleInputPromptKeyState(document,
                                             "battle-input-prompt-follow-up-key-" + index,
                                             "battle-input-prompt-follow-up-label-" + index,
                                             false,
                                             "",
                                             false);
            }

            setBattleInputPromptKeyState(document,
                                         "battle-input-prompt-arrow-key-up",
                                         "battle-input-prompt-arrow-label-up",
                                         true,
                                         followUpLabels[1],
                                         activeFollowUpIndex == 1);
            setBattleInputPromptKeyState(document,
                                         "battle-input-prompt-arrow-key-left",
                                         "battle-input-prompt-arrow-label-left",
                                         true,
                                         followUpLabels[0],
                                         activeFollowUpIndex == 0);
            setBattleInputPromptKeyState(document,
                                         "battle-input-prompt-arrow-key-down",
                                         "battle-input-prompt-arrow-label-down",
                                         true,
                                         followUpLabels[2],
                                         activeFollowUpIndex == 2);
            setBattleInputPromptKeyState(document,
                                         "battle-input-prompt-arrow-key-right",
                                         "battle-input-prompt-arrow-label-right",
                                         true,
                                         followUpLabels[3],
                                         activeFollowUpIndex == 3);
            return;
        }

        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-arrow-key-up",
            "battle-input-prompt-arrow-label-up",
            false,
            "",
            false);
        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-arrow-key-left",
            "battle-input-prompt-arrow-label-left",
            false,
            "",
            false);
        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-arrow-key-down",
            "battle-input-prompt-arrow-label-down",
            false,
            "",
            false);
        setBattleInputPromptKeyState(
            document,
            "battle-input-prompt-arrow-key-right",
            "battle-input-prompt-arrow-label-right",
            false,
            "",
            false);

        for (std::size_t i = 0; i < kBattleInputPromptMaxFollowUpSlots; ++i) {
            const std::string index = std::to_string(i + 1);
            const bool visibleKey = i < visibleFollowUpCount;
            setBattleInputPromptKeyState(document,
                                         "battle-input-prompt-follow-up-key-" + index,
                                         "battle-input-prompt-follow-up-label-" + index,
                                         visibleKey,
                                         visibleKey ? followUpLabels[i] : "",
                                         visibleKey && static_cast<int>(i) == activeFollowUpIndex,
                                         battleInputPromptKeyTiltDegrees(prompt, i));
        }
        return;
    }

    setElementProperty(document, "battle-input-prompt-follow-up-strip", "display", "none");
    setElementProperty(document, "battle-input-prompt-follow-up-arrows", "display", "none");
    for (std::size_t i = 0; i < kBattleInputPromptMaxFollowUpSlots; ++i) {
        const std::string index = std::to_string(i + 1);
        setBattleInputPromptKeyState(document,
                                     "battle-input-prompt-follow-up-key-" + index,
                                     "battle-input-prompt-follow-up-label-" + index,
                                     false,
                                     "",
                                     false);
    }
    setBattleInputPromptKeyState(
        document,
        "battle-input-prompt-arrow-key-up",
        "battle-input-prompt-arrow-label-up",
        false,
        "",
        false);
    setBattleInputPromptKeyState(
        document,
        "battle-input-prompt-arrow-key-left",
        "battle-input-prompt-arrow-label-left",
        false,
        "",
        false);
    setBattleInputPromptKeyState(
        document,
        "battle-input-prompt-arrow-key-down",
        "battle-input-prompt-arrow-label-down",
        false,
        "",
        false);
    setBattleInputPromptKeyState(
        document,
        "battle-input-prompt-arrow-key-right",
        "battle-input-prompt-arrow-label-right",
        false,
        "",
        false);
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

inline const JudgementPalette& presentationDamagePalette() {
    static const JudgementPalette totalDamage{
        {83, 18, 60},
        {255, 179, 221},
        {255, 205, 126},
        {255, 246, 232},
        {72, 15, 51},
        {255, 212, 140}
    };
    return totalDamage;
}

inline std::string presentationDamageMarkup(const HudFeedbackState& feedback, Uint64 nowMs) {
    constexpr float kDigitIntervalSeconds = 0.024f;
    const JudgementPalette& palette = presentationDamagePalette();
    const Uint64 elapsedMs =
        feedback.presentationDamageStartedMs == 0 || nowMs <= feedback.presentationDamageStartedMs
            ? 0
            : nowMs - feedback.presentationDamageStartedMs;
    const float elapsedSeconds = feedback.presentationDamageStartedMs == 0
        ? 0.0f
        : static_cast<float>(elapsedMs) / 1000.0f;

    std::string markup;
    markup.reserve(feedback.presentationDamageText.size() * 192);

    int visibleDigitIndex = 0;
    for (char glyph : feedback.presentationDamageText) {
        if (glyph == ' ') {
            markup += "<span class=\"battle-total-damage-space\"></span>";
            continue;
        }

        const float digitElapsedSeconds =
            elapsedSeconds - (static_cast<float>(visibleDigitIndex) * kDigitIntervalSeconds);
        if (digitElapsedSeconds < 0.0f) {
            ++visibleDigitIndex;
            continue;
        }

        markup += "<span class=\"battle-total-damage-digit\" style=\"";
        markup += judgementLetterStyle(palette, digitElapsedSeconds);
        markup += "\">";
        markup.push_back(glyph);
        markup += "</span>";
        ++visibleDigitIndex;
    }

    return markup;
}

inline std::string presentationDamageContainerTransform(const HudFeedbackState& feedback, Uint64 nowMs) {
    const Uint64 elapsedMs = feedback.presentationDamageStartedMs == 0
        ? 0
        : (nowMs > feedback.presentationDamageStartedMs ? nowMs - feedback.presentationDamageStartedMs : 0);
    const float introProgress = std::clamp(static_cast<float>(elapsedMs) / 260.0f, 0.0f, 1.0f);

    float translateXDp = 0.0f;
    float scale = 1.0f;
    if (introProgress < 0.62f) {
        const float t = easeOutCubic(introProgress / 0.62f);
        translateXDp = lerpValue(28.0f, 0.0f, t);
        scale = lerpValue(0.92f, 1.04f, t);
    } else {
        const float t = easeOutCubic((introProgress - 0.62f) / 0.38f);
        scale = lerpValue(1.04f, 1.0f, t);
    }

    if (!feedback.presentationDamageActive &&
        feedback.presentationDamageHoldUntilMs != 0 &&
        feedback.presentationDamageFadeUntilMs > feedback.presentationDamageHoldUntilMs &&
        nowMs > feedback.presentationDamageHoldUntilMs) {
        const float fadeProgress = easeInCubic(std::clamp(
            static_cast<float>(nowMs - feedback.presentationDamageHoldUntilMs) /
                static_cast<float>(feedback.presentationDamageFadeUntilMs - feedback.presentationDamageHoldUntilMs),
            0.0f,
            1.0f));
        translateXDp += lerpValue(0.0f, 12.0f, fadeProgress);
        scale *= lerpValue(1.0f, 0.99f, fadeProgress);
    }

    return "translateX(" + decimalText(translateXDp) + "dp) scale(" + decimalText(scale) + ")";
}

inline float presentationDamageContainerOpacity(const HudFeedbackState& feedback, Uint64 nowMs) {
    if (feedback.presentationDamageStartedMs == 0) {
        return feedback.presentationDamageActive ? 1.0f : 0.0f;
    }

    const Uint64 introElapsedMs =
        nowMs > feedback.presentationDamageStartedMs ? nowMs - feedback.presentationDamageStartedMs : 0;
    const float introProgress = std::clamp(
        static_cast<float>(introElapsedMs) / 140.0f,
        0.0f,
        1.0f);
    if (feedback.presentationDamageActive) {
        return easeOutCubic(introProgress);
    }
    if (feedback.presentationDamageHoldUntilMs == 0 ||
        feedback.presentationDamageFadeUntilMs <= feedback.presentationDamageHoldUntilMs ||
        nowMs <= feedback.presentationDamageHoldUntilMs) {
        return 1.0f;
    }

    const float fadeProgress = std::clamp(
        static_cast<float>(nowMs - feedback.presentationDamageHoldUntilMs) /
            static_cast<float>(feedback.presentationDamageFadeUntilMs - feedback.presentationDamageHoldUntilMs),
        0.0f,
        1.0f);
    return 1.0f - easeInCubic(fadeProgress);
}

inline float presentationDamageLabelProgress(const HudFeedbackState& feedback, Uint64 nowMs) {
    constexpr float kLabelDelaySeconds = 0.04f;
    constexpr float kLabelDurationSeconds = 0.22f;
    const Uint64 elapsedMs =
        feedback.presentationDamageStartedMs == 0 || nowMs <= feedback.presentationDamageStartedMs
            ? 0
            : nowMs - feedback.presentationDamageStartedMs;
    const float elapsedSeconds = feedback.presentationDamageStartedMs == 0
        ? 0.0f
        : static_cast<float>(elapsedMs) / 1000.0f;
    const float labelElapsedSeconds = elapsedSeconds - kLabelDelaySeconds;
    if (labelElapsedSeconds <= 0.0f) {
        return 0.0f;
    }
    return easeOutCubic(std::clamp(labelElapsedSeconds / kLabelDurationSeconds, 0.0f, 1.0f));
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

    setElementDisplay(document, "battle-total-damage", false);
    setElementDisplay(document, "battle-judgement", false);
    setElementClass(document, "battle-rhythm", "visible", false);
    setElementDisplay(document, "battle-hint-lane", false);
    setElementDisplay(document, "battle-input-prompt", false);
    setElementClass(document, "battle-input-prompt", "visible", false);
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

inline void updatePresentationDamageDocument(Rml::ElementDocument* document,
                                             const HudFeedbackState& feedback,
                                             Uint64 nowMs) {
    if (document == nullptr) {
        return;
    }

    const bool visible = feedback.presentationDamageVisible && !feedback.presentationDamageText.empty();
    setElementDisplay(document, "battle-total-damage", visible);

    if (Rml::Element* container = document->GetElementById("battle-total-damage")) {
        if (!visible) {
            container->SetProperty("opacity", "0");
            container->SetProperty("transform", "translateX(0dp) scale(1)");
        } else {
            container->SetProperty("opacity", decimalText(presentationDamageContainerOpacity(feedback, nowMs)));
            container->SetProperty("transform", presentationDamageContainerTransform(feedback, nowMs));
        }
    }

    setElementText(document, "battle-total-damage-label", visible ? "TOTAL DMG" : "");
    if (Rml::Element* label = document->GetElementById("battle-total-damage-label")) {
        if (!visible) {
            label->SetProperty("opacity", "0");
            label->SetProperty("transform", "translateY(10dp) scale(0.94)");
        } else {
            const float labelProgress = presentationDamageLabelProgress(feedback, nowMs);
            const float labelOpacity = labelProgress * presentationDamageContainerOpacity(feedback, nowMs);
            label->SetProperty("opacity", decimalText(labelOpacity));
            label->SetProperty(
                "transform",
                "translateY(" + decimalText(lerpValue(10.0f, 0.0f, labelProgress)) +
                    "dp) scale(" + decimalText(lerpValue(0.94f, 1.0f, labelProgress)) + ")");
            label->SetProperty("color", rgbaText(presentationDamagePalette().reward, 1.0f));
        }
    }

    if (Rml::Element* value = document->GetElementById("battle-total-damage-value")) {
        if (!visible) {
            value->SetInnerRML("");
        } else {
            value->SetInnerRML(presentationDamageMarkup(feedback, nowMs));
        }
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

inline const char* battleVsIntroNameModeClass(std::size_t glyphCount) {
    if (glyphCount >= 11) {
        return "ultra";
    }
    if (glyphCount >= 8) {
        return "compact";
    }
    return "";
}

inline std::string battleVsIntroNameLetterStyle(float elapsedSeconds) {
    const float progress =
        std::clamp(elapsedSeconds / kBattleVsIntroNameLetterDurationSeconds, 0.0f, 1.0f);

    float opacity = 1.0f;
    float translateYDp = 0.0f;
    float scale = 1.0f;

    if (progress < 0.54f) {
        const float t = easeOutCubic(progress / 0.54f);
        opacity = t;
        translateYDp = lerpValue(-104.0f, 12.0f, t);
        scale = lerpValue(1.58f, 0.92f, t);
    } else if (progress < 0.76f) {
        const float t = easeOutCubic((progress - 0.54f) / 0.22f);
        opacity = 1.0f;
        translateYDp = lerpValue(12.0f, -3.0f, t);
        scale = lerpValue(0.92f, 1.05f, t);
    } else {
        const float t = easeOutCubic((progress - 0.76f) / 0.24f);
        opacity = 1.0f;
        translateYDp = lerpValue(-3.0f, 0.0f, t);
        scale = lerpValue(1.05f, 1.0f, t);
    }

    return "opacity: " + decimalText(opacity) +
        "; transform: translateY(" + decimalText(translateYDp) + "dp) scale(" + decimalText(scale) + ");";
}

inline std::string battleVsIntroHiddenVsLetterStyle() {
    return "opacity: 0; transform: translateY(-190.000dp) scale(1.860) rotate(8.000deg);";
}

inline std::string battleVsIntroVsLetterStyle(float elapsedSeconds) {
    const float progress =
        std::clamp(elapsedSeconds / kBattleVsIntroVsLetterDurationSeconds, 0.0f, 1.0f);

    float opacity = 1.0f;
    float translateYDp = 0.0f;
    float scale = 1.0f;
    float rotateDeg = 0.0f;

    if (progress < 0.52f) {
        const float t = easeOutCubic(progress / 0.52f);
        opacity = t;
        translateYDp = lerpValue(-190.0f, 24.0f, t);
        scale = lerpValue(1.86f, 0.82f, t);
        rotateDeg = lerpValue(8.0f, 0.0f, t);
    } else if (progress < 0.76f) {
        const float t = easeOutCubic((progress - 0.52f) / 0.24f);
        opacity = 1.0f;
        translateYDp = lerpValue(24.0f, -8.0f, t);
        scale = lerpValue(0.82f, 1.09f, t);
    } else {
        const float t = easeOutCubic((progress - 0.76f) / 0.24f);
        opacity = 1.0f;
        translateYDp = lerpValue(-8.0f, 0.0f, t);
        scale = lerpValue(1.09f, 1.0f, t);
    }

    return "opacity: " + decimalText(opacity) +
        "; transform: translateY(" + decimalText(translateYDp) + "dp) scale(" + decimalText(scale) +
        ") rotate(" + decimalText(rotateDeg) + "deg);";
}

inline std::string buildBattleVsIntroNameMarkup(const std::string& text,
                                                float elapsedSeconds,
                                                float revealStartSeconds) {
    const int visibleLetters = battleVsIntroVisibleLetterCount(text, elapsedSeconds, revealStartSeconds);
    if (visibleLetters <= 0) {
        return std::string();
    }

    std::string markup;
    markup.reserve(text.size() * 96);

    int visibleLetterIndex = 0;
    std::size_t byteOffset = 0;
    while (byteOffset < text.size()) {
        const std::size_t codeBytes = utf8CodepointBytes(text, byteOffset);
        if (codeBytes == 0 || byteOffset + codeBytes > text.size()) {
            break;
        }

        const std::string glyph = text.substr(byteOffset, codeBytes);
        byteOffset += codeBytes;

        if (glyph == " ") {
            if (visibleLetters > visibleLetterIndex) {
                markup += "<span class=\"battle-vs-name-space\"></span>";
            }
            continue;
        }

        if (visibleLetterIndex >= visibleLetters) {
            break;
        }

        const float letterElapsedSeconds =
            elapsedSeconds - revealStartSeconds -
            (static_cast<float>(visibleLetterIndex) * kBattleVsIntroNameLetterIntervalSeconds);
        markup += "<span class=\"battle-vs-name-letter\" style=\"";
        markup += battleVsIntroNameLetterStyle(letterElapsedSeconds);
        markup += "\">";
        markup += glyph;
        markup += "</span>";
        ++visibleLetterIndex;
    }

    return markup;
}

inline void applyBattleVsIntroNameWord(Rml::ElementDocument* document,
                                       const std::string& id,
                                       const std::string& text,
                                       float elapsedSeconds,
                                       float revealStartSeconds,
                                       std::size_t sharedGlyphCount) {
    if (document == nullptr) {
        return;
    }

    if (Rml::Element* word = document->GetElementById(id)) {
        std::string className = "battle-vs-name-word ";
        className += containsCjkText(text) ? "cjk" : "latin";
        const char* modeClass = battleVsIntroNameModeClass(sharedGlyphCount);
        if (modeClass[0] != '\0') {
            className += " ";
            className += modeClass;
        }
        word->SetAttribute("class", className);
        word->SetInnerRML(buildBattleVsIntroNameMarkup(text, elapsedSeconds, revealStartSeconds));
    }
}

inline void applyBattleVsIntroOverlayDocumentState(Rml::ElementDocument* document,
                                                   const BattleVsIntroOverlayState& overlay,
                                                   const BattleHudDocumentDependencies& dependencies) {
    if (document == nullptr) {
        return;
    }

    const bool blocking = overlay.pendingStart || overlay.active;
    setElementClass(document, "battle-vs-intro", "active", blocking);
    setElementClass(document, "battle-vs-intro", "split-dropping",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroLineDropSeconds);
    setElementClass(document, "battle-vs-intro", "split-angled",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroLineTiltSeconds);
    setElementClass(document, "battle-vs-intro", "void-cleared",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroVoidFadeSeconds);
    setElementClass(document, "battle-vs-intro", "split-fading",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroExitSeconds);
    setElementClass(document, "battle-vs-intro", "left-landed",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroLeftCardSeconds);
    setElementClass(document, "battle-vs-intro", "right-landed",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroRightCardSeconds);
    setElementClass(document, "battle-vs-intro", "exiting",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroExitSeconds);
    setElementClass(document, "battle-vs-intro", "revealing",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroRevealSeconds);
    setElementClass(document, "battle-vs-intro", "vs-hidden",
                    overlay.active && battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroExitSeconds);
    setElementClass(document, "battle-vs-intro", "vs-shaking",
                    overlay.active &&
                    battleVsIntroElapsedSeconds(overlay) >= kBattleVsIntroVsSSeconds &&
                    battleVsIntroElapsedSeconds(overlay) < kBattleVsIntroVsSSeconds + 0.30f);
    setElementClass(document, "hud-root", "vs-intro-blocking", blocking);
    if (!blocking) {
        if (Rml::Element* leftWord = document->GetElementById("battle-vs-name-word-left")) {
            leftWord->SetInnerRML("");
        }
        if (Rml::Element* rightWord = document->GetElementById("battle-vs-name-word-right")) {
            rightWord->SetInnerRML("");
        }
        setElementProperty(document, "battle-vs-letter-v", "opacity", "0");
        setElementProperty(document, "battle-vs-letter-v", "transform",
                           "translateY(-190dp) scale(1.86) rotate(8deg)");
        setElementProperty(document, "battle-vs-letter-s", "opacity", "0");
        setElementProperty(document, "battle-vs-letter-s", "transform",
                           "translateY(-190dp) scale(1.86) rotate(8deg)");
        return;
    }

    setCombatDecorator(document, "battle-vs-portrait-left", "sprites", overlay.leftAsset, dependencies);
    setCombatDecorator(document, "battle-vs-portrait-right", "sprites", overlay.rightAsset, dependencies);

    if (!overlay.active) {
        if (Rml::Element* leftWord = document->GetElementById("battle-vs-name-word-left")) {
            leftWord->SetAttribute("class", "battle-vs-name-word latin");
            leftWord->SetInnerRML("");
        }
        if (Rml::Element* rightWord = document->GetElementById("battle-vs-name-word-right")) {
            rightWord->SetAttribute("class", "battle-vs-name-word latin");
            rightWord->SetInnerRML("");
        }
        setElementProperty(document, "battle-vs-letter-v", "opacity", "0");
        setElementProperty(document, "battle-vs-letter-v", "transform",
                           "translateY(-190dp) scale(1.86) rotate(8deg)");
        setElementProperty(document, "battle-vs-letter-s", "opacity", "0");
        setElementProperty(document, "battle-vs-letter-s", "transform",
                           "translateY(-190dp) scale(1.86) rotate(8deg)");
        return;
    }

    const float elapsedSeconds = battleVsIntroElapsedSeconds(overlay);
    const std::size_t sharedGlyphCount =
        std::max(battleVsIntroVisibleGlyphCount(overlay.leftName), battleVsIntroVisibleGlyphCount(overlay.rightName));
    applyBattleVsIntroNameWord(document,
                               "battle-vs-name-word-left",
                               overlay.leftName,
                               elapsedSeconds,
                               kBattleVsIntroLeftNameSeconds,
                               sharedGlyphCount);
    applyBattleVsIntroNameWord(document,
                               "battle-vs-name-word-right",
                               overlay.rightName,
                               elapsedSeconds,
                               kBattleVsIntroRightNameSeconds,
                               sharedGlyphCount);

    if (Rml::Element* vsV = document->GetElementById("battle-vs-letter-v")) {
        vsV->SetAttribute(
            "style",
            elapsedSeconds < kBattleVsIntroVsVSeconds
                ? battleVsIntroHiddenVsLetterStyle()
                : battleVsIntroVsLetterStyle(elapsedSeconds - kBattleVsIntroVsVSeconds));
    }
    if (Rml::Element* vsS = document->GetElementById("battle-vs-letter-s")) {
        vsS->SetAttribute(
            "style",
            elapsedSeconds < kBattleVsIntroVsSSeconds
                ? battleVsIntroHiddenVsLetterStyle()
                : battleVsIntroVsLetterStyle(elapsedSeconds - kBattleVsIntroVsSSeconds));
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
                                    HudFeedbackState& feedback,
                                    const BattleResultOverlayState& resultOverlay,
                                    const BattleVsIntroOverlayState& vsIntroOverlay,
                                    const TutorialOverlayState& tutorial,
                                    const BattleInputPromptState& inputPrompt,
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

    (void)tutorial;
    (void)narrationCharsPerSecond;

    const battle::BattleState& battleState = manager.getBattleState();
    const battle::TurnState& turnState = manager.getTurnState();
    const int activeActorIndex = manager.getPreviewNextActorIndex();
    const std::vector<battle::BattleStatusBadge> statusBadges = manager.getActiveStatusBadges();

    detail::ensurePartyRackDocument(document, battleState.party.size());
    detail::updateComboDocument(document, feedback);
    detail::updatePresentationDamageDocument(document, feedback, nowMs);
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
    detail::updateStatusBadgeLaneDocument(document,
                                          "boss-status-badges",
                                          statusBadges,
                                          battle::BattleStatusBadgeTarget::Boss,
                                          -1,
                                          dependencies);

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
        detail::updateStatusBadgeLaneDocument(document,
                                              "unit-status-badges-" + index,
                                              statusBadges,
                                              battle::BattleStatusBadgeTarget::PartyMember,
                                              i,
                                              dependencies);
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
    detail::updateBattleInputPromptDocument(document, inputPrompt, nowMs);
    detail::updateToastDocument(document, feedback);
    detail::updateRhythmDocument(document, rhythm, nowMs);
    if (paused || resultOverlay.active || vsIntroOverlay.pendingStart || vsIntroOverlay.active) {
        detail::setElementDisplay(document, "battle-hint-lane", false);
        detail::setElementDisplay(document, "battle-input-prompt", false);
        detail::setElementClass(document, "battle-input-prompt", "visible", false);
    }

    detail::setElementClass(document, "battle-pause", "visible", paused);
    if (paused) {
        applyPauseOverlayDocumentState(document, pauseOverlayMode, pauseSelection, settingsSelection, settings);
    }
    detail::applyBattleResultOverlayDocumentState(document, resultOverlay, nowMs);
    detail::applyBattleVsIntroOverlayDocumentState(document, vsIntroOverlay, dependencies);
}

} // namespace battle::app::ui

#endif
