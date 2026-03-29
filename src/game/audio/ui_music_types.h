#pragma once

#include <array>
#include <cstddef>

namespace game::audio {

inline constexpr std::size_t kUiMusicBarCount = 20;

enum class UiMusicSurface {
    None,
    MainMenu,
    PauseMenu,
    BattleSelector,
};

struct UiMusicVisualState {
    bool visible = false;
    std::array<float, kUiMusicBarCount> bars{};
    float opacity = 0.0f;
};

}  // namespace game::audio
