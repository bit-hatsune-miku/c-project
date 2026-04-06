#pragma once

#include "ability_presentation.h"

#include <random>
#include <string>
#include <vector>

namespace battle {

class RandyQuizPresentation : public AbilityPresentation {
public:
    RandyQuizPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    bool onKeyPressed(SDL_Keycode key) override;

    float getInputMultiplier() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    std::vector<PresentationHintCommand> consumeHintCommands() override;
    bool shouldRenderAboveHud() const override;

private:
    struct Question {
        std::string question;
        std::vector<std::string> answers;
        int correctIndex = 0;
    };

    enum class Phase {
        Prompt,
        Result,
        Complete
    };

    static std::vector<Question> loadQuestionBank();
    void queueQuestionHints() ;
    void finalizeAnswer(int chosenIndex);
    float currentSuccessMultiplier() const;

    float answerWindowSeconds_ = 3.0f;
    float promptElapsed_ = 0.0f;
    float resultElapsed_ = 0.0f;
    float responseTimeSeconds_ = 0.0f;
    float successMultiplier_ = 0.0f;
    int chosenIndex_ = -1;
    bool answeredCorrectly_ = false;
    Phase phase_ = Phase::Prompt;
    Question activeQuestion_{};
    std::mt19937 rng_{std::random_device{}()};
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
    std::vector<PresentationHintCommand> pendingHintCommands_;
};

} // namespace battle
