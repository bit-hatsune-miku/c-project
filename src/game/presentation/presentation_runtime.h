#ifndef PRESENTATION_RUNTIME_H
#define PRESENTATION_RUNTIME_H

#include <functional>
#include <vector>

#include <SDL2/SDL.h>

#include "ability_presentation.h"
#include "../core/battle_manager.h"
#include "../render/battle_scene_types.h"

namespace battle::presentation_runtime {

struct PlaybackResult {
    float multiplier = 1.0f;
    std::string resultText;
    int correctToneCount = 0;
    PresentationFeedbackSignal feedbackSignal{};
};

struct PlaybackStateRefs {
    bool* playbackActive = nullptr;
    bool* casterIsBoss = nullptr;
    int* casterPartyIndex = nullptr;
    AbilityPresentation** activePresentation = nullptr;
};

struct PlaybackCallbacks {
    std::function<void(int width, int height)> onWindowResized;
    std::function<void(int cueCount)> onAbilityAudioCues;
    std::function<void(int hitEvents, int damageLabelHitCount)> onHitEvents;
    std::function<void(const std::vector<PresentationAudioCommand>& commands)> onAudioCommands;
    std::function<void(SDL_Keycode key)> onUnhandledKeyDown;
    std::function<void(float deltaSeconds)> onPostUpdate;
    std::function<void()> onBossPresentationFrame;
    std::function<void()> renderAndPresentFrame;
    std::function<void()> onPauseBlocked;

    // Splash art support — populated by BattleSessionCore before calling runAbilityPresentation.
    SDL_Texture* splashSpriteTexture = nullptr;
    SDL_Texture* casterSpriteTexture = nullptr;
    SDL_Texture* targetSpriteTexture = nullptr;
    SDL_Texture* overlayCasterSpriteTexture = nullptr;
    SDL_Texture* overlayTargetSpriteTexture = nullptr;
    // Set a render overlay function that is called after world render but before SDL_RenderPresent.
    std::function<void(std::function<void(SDL_Renderer*, int, int)>)> setRenderOverlay;
    // Clear the render overlay set above.
    std::function<void()> clearRenderOverlay;
    // Called immediately before the splash art overlay starts rendering.
    std::function<void()> onSplashArtStart;
};

PlaybackResult runAbilityPresentation(SDL_Renderer* renderer,
                                      bool& finished,
                                      Camera3D& camera,
                                      std::vector<render::SceneEntity>& entities,
                                      const PresentationContext& context,
                                      PlaybackStateRefs stateRefs,
                                      PlaybackCallbacks callbacks);

} // namespace battle::presentation_runtime

#endif
