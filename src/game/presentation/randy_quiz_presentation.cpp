#include "randy_quiz_presentation.h"

#include "../../platform/path_resolution.h"

#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace battle {
namespace {

using json = nlohmann::json;

constexpr float kResultHoldSeconds = 0.28f;
constexpr char kQuestionBankPath[] = "assets/combat/randy_c_questions.json";

constexpr char kHintInstructionKey[] = "randy_quiz_instruction";
constexpr char kHintQuestionKey[] = "randy_quiz_question";
constexpr char kHintAnswer1Key[] = "randy_quiz_answer_1";
constexpr char kHintAnswer2Key[] = "randy_quiz_answer_2";
constexpr char kHintAnswer3Key[] = "randy_quiz_answer_3";

std::string resolveQuestionBankPath() {
    return platform::path::resolvePath(kQuestionBankPath);
}

PresentationHintCommand makeHint(const char* stableKey,
                                 const char* badgeText,
                                 const std::string& message) {
    PresentationHintCommand command;
    command.type = PresentationHintCommandType::Upsert;
    command.family = PresentationHintFamily::Info;
    command.stableKey = stableKey;
    command.kicker = "CLASSWORK";
    command.sourceTag = "RANDY";
    command.badgeText = badgeText == nullptr ? std::string() : std::string(badgeText);
    command.message = message;
    return command;
}

} // namespace

RandyQuizPresentation::RandyQuizPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) {
    (void)casterWorldX;
    (void)casterWorldY;
    (void)casterWorldZ;
    (void)targetWorldX;
    (void)targetWorldY;
    (void)targetWorldZ;
    totalDuration_ = answerWindowSeconds_ + kResultHoldSeconds;
}

void RandyQuizPresentation::start() {
    promptElapsed_ = 0.0f;
    resultElapsed_ = 0.0f;
    responseTimeSeconds_ = 0.0f;
    successMultiplier_ = 0.0f;
    chosenIndex_ = -1;
    answeredCorrectly_ = false;
    phase_ = Phase::Prompt;
    pendingFeedbackEvents_.clear();
    pendingHintCommands_.clear();
    totalDuration_ = answerWindowSeconds_ + kResultHoldSeconds;

    const std::vector<Question> questions = loadQuestionBank();
    if (!questions.empty()) {
        std::uniform_int_distribution<std::size_t> dist(0, questions.size() - 1);
        activeQuestion_ = questions[dist(rng_)];
    } else {
        activeQuestion_.question = "Which header is used for printf in C?";
        activeQuestion_.answers = {"<stdio.h>", "<stdlib.h>", "<string.h>"};
        activeQuestion_.correctIndex = 0;
    }
    queueQuestionHints();
}

void RandyQuizPresentation::update(float deltaTime) {
    if (phase_ == Phase::Prompt) {
        promptElapsed_ += deltaTime;
        if (promptElapsed_ >= answerWindowSeconds_) {
            finalizeAnswer(-1);
        }
        return;
    }

    if (phase_ == Phase::Result) {
        resultElapsed_ += deltaTime;
        if (resultElapsed_ >= kResultHoldSeconds) {
            phase_ = Phase::Complete;
        }
    }
}

void RandyQuizPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)renderer;
    (void)screenW;
    (void)screenH;
    (void)camera;
}

bool RandyQuizPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

bool RandyQuizPresentation::onKeyPressed(SDL_Keycode key) {
    if (phase_ != Phase::Prompt) {
        return false;
    }

    int chosenIndex = -1;
    switch (key) {
        case SDLK_1:
        case SDLK_KP_1:
            chosenIndex = 0;
            break;
        case SDLK_2:
        case SDLK_KP_2:
            chosenIndex = 1;
            break;
        case SDLK_3:
        case SDLK_KP_3:
            chosenIndex = 2;
            break;
        default:
            return true;
    }

    finalizeAnswer(chosenIndex);
    return true;
}

float RandyQuizPresentation::getInputMultiplier() const {
    return successMultiplier_;
}

std::vector<PresentationFeedbackEvent> RandyQuizPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

std::vector<PresentationHintCommand> RandyQuizPresentation::consumeHintCommands() {
    std::vector<PresentationHintCommand> commands;
    commands.swap(pendingHintCommands_);
    return commands;
}

bool RandyQuizPresentation::shouldRenderAboveHud() const {
    return false;
}

std::vector<RandyQuizPresentation::Question> RandyQuizPresentation::loadQuestionBank() {
    std::vector<Question> questions;

    std::ifstream stream(resolveQuestionBankPath());
    if (!stream.is_open()) {
        return questions;
    }

    json root;
    try {
        stream >> root;
    } catch (...) {
        return questions;
    }

    const auto questionsIt = root.find("questions");
    if (questionsIt == root.end() || !questionsIt->is_array()) {
        return questions;
    }

    for (const json& entry : *questionsIt) {
        if (!entry.is_object()) {
            continue;
        }

        Question question;
        question.question = entry.value("question", "");
        question.correctIndex = entry.value("correctIndex", 0);
        if (const auto answersIt = entry.find("answers"); answersIt != entry.end() && answersIt->is_array()) {
            for (const json& answer : *answersIt) {
                if (answer.is_string()) {
                    question.answers.push_back(answer.get<std::string>());
                }
            }
        }

        if (!question.question.empty() && question.answers.size() == 3 &&
            question.correctIndex >= 0 && question.correctIndex < 3) {
            questions.push_back(std::move(question));
        }
    }

    return questions;
}

void RandyQuizPresentation::queueQuestionHints() {
    pendingHintCommands_.push_back(makeHint(
        kHintInstructionKey,
        "QUIZ",
        "Answer the C question correctly by pressing 1/2/3"
    ));
    pendingHintCommands_.push_back(makeHint(kHintQuestionKey, "Q", activeQuestion_.question));
    pendingHintCommands_.push_back(makeHint(kHintAnswer1Key, "1", activeQuestion_.answers[0]));
    pendingHintCommands_.push_back(makeHint(kHintAnswer2Key, "2", activeQuestion_.answers[1]));
    pendingHintCommands_.push_back(makeHint(kHintAnswer3Key, "3", activeQuestion_.answers[2]));
}

void RandyQuizPresentation::finalizeAnswer(int chosenIndex) {
    if (phase_ != Phase::Prompt) {
        return;
    }

    chosenIndex_ = chosenIndex;
    responseTimeSeconds_ = std::clamp(promptElapsed_, 0.0f, answerWindowSeconds_);
    answeredCorrectly_ = chosenIndex_ == activeQuestion_.correctIndex;
    successMultiplier_ = answeredCorrectly_ ? currentSuccessMultiplier() : 0.0f;
    phase_ = Phase::Result;
    resultElapsed_ = 0.0f;

    if (!answeredCorrectly_) {
        return;
    }

    PresentationFeedbackEvent event;
    event.signal = PresentationFeedbackSignal::forcedPerfect();
    event.comboEligible = true;
    event.multiplier = successMultiplier_;
    event.rewardText =
        "+" + std::to_string(static_cast<int>(std::lround(successMultiplier_ * 100.0f))) + "% DMG TAKEN";
    pendingFeedbackEvents_.push_back(std::move(event));
}

float RandyQuizPresentation::currentSuccessMultiplier() const {
    const float remainingRatio =
        1.0f - std::clamp(responseTimeSeconds_ / std::max(0.001f, answerWindowSeconds_), 0.0f, 1.0f);
    return 0.30f + (0.60f * remainingRatio);
}

} // namespace battle
