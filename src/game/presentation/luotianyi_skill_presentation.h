#pragma once

#include "ability_presentation.h"

#include <array>
#include <random>
#include <string>
#include <vector>

namespace battle {

// Playable Luotianyi's skill presentation.
// Same tone-echo minigame as the boss version, but instead of scaling damage
// it returns an ATK buff multiplier that applyPartyBuffFromAbility() uses:
//   4/4 correct  -> getInputMultiplier() == 1.0  (full buff, atkBuff * 1.0)
//   3/4 correct  -> getInputMultiplier() == 0.583 (scaled)
//   2/4 correct  -> getInputMultiplier() == 0.25
//   1/4 correct  -> getInputMultiplier() == 0.083
//   0/4 correct  -> getInputMultiplier() == -0.333 (nerf)
class LuotianyiSkillPresentation : public AbilityPresentation {
public:
    LuotianyiSkillPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void onKeyPressed(SDL_Keycode key) override;

    bool shouldHideNonCasterCharacters() const override;
    bool overridesCamera() const override;
    bool shouldRenderCasterEntity() const override;
    bool shouldRenderAboveHud() const override;

    float getInputMultiplier() const override;
    std::string getInputResultText() const override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;

private:
    enum class Phase {
        Input,
        Result,
        Complete
    };

    static std::string resolvePath(const std::string& relativePath);
    void queueAudioCommand(PresentationAudioCommandType type, const std::string& id, float volume = 1.0f);
    void finalizeInput();
    void drawToneSlot(SDL_Renderer* renderer,
                      const SDL_FRect& rect,
                      int tone,
                      bool hidden,
                      bool highlighted,
                      bool showResult,
                      bool isCorrect) const;
    void drawToneGlyph(SDL_Renderer* renderer, const SDL_FRect& rect, int tone, Uint8 alpha) const;
    int mapToneFromKey(SDL_Keycode key) const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    std::array<int, 4> toneSequence_{{1, 2, 3, 4}};
    std::vector<int> userInputs_;
    std::vector<PresentationAudioCommand> pendingAudioCommands_;
    std::mt19937 rng_;

    Phase phase_ = Phase::Input;
    float inputElapsed_ = 0.0f;
    float responseElapsed_ = 0.0f;
    float resultElapsed_ = 0.0f;
    size_t nextTonePlaybackIndex_ = 0;
    int highlightedToneIndex_ = -1;
    float highlightedToneElapsed_ = 0.0f;
    int correctCount_ = 0;
    float inputMultiplier_ = 1.0f; // returned as getInputMultiplier()
    bool acceptingInput_ = false;
};

} // namespace battle
