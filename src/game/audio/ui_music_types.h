#pragma once

#include <array>
#include <cstddef>

/**
 * @brief Number of visual bars used to render UI music visualization.
 */

/**
 * @brief UI surface contexts that can display music visualization.
 *
 * Enumerates the distinct UI locations where the music visualizer may appear.
 */
 
/**
 * @brief Visual state for the UI music visualizer.
 *
 * Holds whether the visualizer is shown, the per-bar amplitude values, and
 * the overall visualizer opacity.
 *
 * @var visible True when the visualizer should be rendered, false otherwise.
 * @var bars Fixed-size array of per-bar amplitude values with length kUiMusicBarCount.
 * @var opacity Opacity of the visualizer in the range [0.0, 1.0].
 */
namespace game::audio {

inline constexpr std::size_t kUiMusicBarCount = 20;

enum class UiMusicSurface {
    None,
    MainMenu,
    PauseMenu,
    BattleSelector,
    PartyLoader,
    PostBattle,
};

struct UiMusicVisualState {
    bool visible = false;
    std::array<float, kUiMusicBarCount> bars{};
    float opacity = 0.0f;
};

}  // namespace game::audio
