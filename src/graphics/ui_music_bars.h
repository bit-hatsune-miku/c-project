#pragma once

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "../game/audio/ui_music_types.h"

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
