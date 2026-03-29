#pragma once

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "../game/audio/ui_music_types.h"

/**
 * Cached pointers to the RmlUi elements that comprise the UI music bars strip.
 */
 
/**
 * Format a floating-point value with three decimal places.
 * @param value Value to format.
 * @returns The value formatted as a decimal string with three digits after the decimal point.
 */

/**
 * Format a floating-point value as device-independent pixels with two decimal places.
 * @param value Value in pixels to format.
 * @returns The value formatted with two decimal places and the "dp" suffix (e.g. "12.34dp").
 */

/**
 * Locate and cache the UI elements for the music-bar strip inside the given document.
 *
 * If the track container has no children, this function will populate it with the
 * expected number of bar <div> elements before resolving and caching their pointers.
 * If the root or track element cannot be found, the returned UiMusicBarStrip will
 * contain nullptr for root and an empty bars vector.
 *
 * @param document Rml document containing the music-bars elements.
 * @returns A UiMusicBarStrip with `root` set to the container element (or nullptr)
 *          and `bars` populated with pointers to each bar element (elements may be nullptr if not found).
 */

/**
 * Update the cached music-bar UI to reflect the provided visual state.
 *
 * If the strip is invalid (null root or empty bars) this function does nothing.
 * When `state.visible` is false or `state.opacity` is approximately zero, the strip
 * is hidden and each bar's height is reset to 10.00dp. Otherwise the strip is shown,
 * its opacity is set to the clamped `state.opacity` (formatted with three decimals),
 * and each bar height is set to `10dp + normalized*340dp` where `normalized` is the
 * per-bar value clamped to [0,1].
 *
 * @param strip Cached UI elements to update.
 * @param state Visual state containing visibility, opacity, and per-bar values.
 */
namespace graphics {

struct UiMusicBarStrip {
    Rml::Element* root = nullptr;
    std::vector<Rml::Element*> bars;
};

inline std::string formatUiMusicNumber(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

inline std::string formatUiMusicDp(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << "dp";
    return stream.str();
}

inline UiMusicBarStrip cacheUiMusicBarStrip(Rml::ElementDocument& document) {
    UiMusicBarStrip strip;
    strip.root = document.GetElementById("ui-music-bars-root");
    Rml::Element* track = document.GetElementById("ui-music-bars-track");
    if (strip.root == nullptr || track == nullptr) {
        return strip;
    }

    if (track->GetNumChildren() == 0) {
        std::ostringstream markup;
        for (std::size_t i = 0; i < game::audio::kUiMusicBarCount; ++i) {
            markup << "<div class=\"ui-music-bar\" id=\"ui-music-bar-" << i << "\"></div>";
        }
        track->SetInnerRML(markup.str());
    }

    strip.bars.reserve(game::audio::kUiMusicBarCount);
    for (std::size_t i = 0; i < game::audio::kUiMusicBarCount; ++i) {
        strip.bars.push_back(document.GetElementById("ui-music-bar-" + std::to_string(i)));
    }

    return strip;
}

inline void applyUiMusicBarStrip(const UiMusicBarStrip& strip,
                                 const game::audio::UiMusicVisualState& state) {
    if (strip.root == nullptr || strip.bars.empty()) {
        return;
    }

    if (!state.visible || state.opacity <= 0.001f) {
        strip.root->SetProperty("display", "none");
        strip.root->SetProperty("opacity", "0.0");
        for (Rml::Element* bar : strip.bars) {
            if (bar != nullptr) {
                bar->SetProperty("height", formatUiMusicDp(10.0f));
            }
        }
        return;
    }

    strip.root->SetProperty("display", "block");
    strip.root->SetProperty("opacity", formatUiMusicNumber(std::clamp(state.opacity, 0.0f, 1.0f)));
    for (std::size_t i = 0; i < strip.bars.size() && i < state.bars.size(); ++i) {
        if (strip.bars[i] == nullptr) {
            continue;
        }

        const float normalized = std::clamp(state.bars[i], 0.0f, 1.0f);
        const float height = 10.0f + normalized * 340.0f;
        strip.bars[i]->SetProperty("height", formatUiMusicDp(height));
    }
}

}  // namespace graphics
