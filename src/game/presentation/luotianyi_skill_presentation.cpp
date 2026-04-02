#include "luotianyi_skill_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>

namespace battle {
namespace {

// Timing constants (same feel as boss version)
constexpr float kAnswerDurationSeconds   = 7.0f;
constexpr float kResultLeadSeconds       = 0.55f;
constexpr float kResultDurationSeconds   = 0.80f;
constexpr float kToneInitialDelaySeconds = 0.50f;
constexpr float kToneIntervalSeconds     = 0.90f;
constexpr float kToneHighlightSeconds    = 0.26f;
constexpr float kPostSequenceDelaySeconds= 0.60f;

// ATK buff/nerf multipliers returned via getInputMultiplier().
// applyPartyBuffFromAbility() scales atkBuff (from JSON) by this value.
//
// Design: atkBuff in JSON = 60 (the +60% ATK cap for 4/4 correct).
//
//   4 correct -> multiplier =  1.000  -> +60% ATK
//   3 correct -> multiplier =  0.583  -> +35% ATK
//   2 correct -> multiplier =  0.250  -> +15% ATK
//   1 correct -> multiplier =  0.083  ->  +5% ATK
//   0 correct -> multiplier = -0.333  -> -20% ATK (nerf)
//
// Derivation: scale each correct count linearly so the user targets 4/4.
// 0 correct gives a meaningful penalty to create risk/reward.
static float multiplierForCorrectCount(int correct) {
    switch (correct) {
        case 4: return  1.000f;
        case 3: return  0.583f;
        case 2: return  0.250f;
        case 1: return  0.083f;
        default: return -0.333f; // 0 correct: nerf
    }
}

constexpr char kToneVoiceDirectory[] = "assets/combat/voices/luotianyiBoss/";

SDL_Color toneColor(int tone) {
    switch (tone) {
        case 1: return SDL_Color{235, 108, 108, 255};
        case 2: return SDL_Color{245, 190,  92, 255};
        case 3: return SDL_Color{106, 211, 154, 255};
        case 4: return SDL_Color{ 96, 162, 255, 255};
        default: return SDL_Color{120, 126, 138, 255};
    }
}

SDL_FRect centeredRect(float centerX, float centerY, float width, float height) {
    return SDL_FRect{
        centerX - (width * 0.5f),
        centerY - (height * 0.5f),
        width,
        height
    };
}

std::string tonePathForValue(int tone) {
    switch (tone) {
        case 1: return std::string(kToneVoiceDirectory) + "first.wav";
        case 2: return std::string(kToneVoiceDirectory) + "second.wav";
        case 3: return std::string(kToneVoiceDirectory) + "third.wav";
        case 4: return std::string(kToneVoiceDirectory) + "fourth.wav";
        default: return {};
    }
}

} // namespace

LuotianyiSkillPresentation::LuotianyiSkillPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ)
    , rng_(std::random_device{}())
{
    totalDuration_ =
        kToneInitialDelaySeconds +
        (kToneIntervalSeconds * 3.0f) +
        kPostSequenceDelaySeconds +
        kAnswerDurationSeconds +
        kResultDurationSeconds;
}

void LuotianyiSkillPresentation::start() {
    elapsedTime_ = 0.0f;
    inputElapsed_ = 0.0f;
    responseElapsed_ = 0.0f;
    resultElapsed_ = 0.0f;
    phase_ = Phase::Input;
    userInputs_.clear();
    pendingAudioCommands_.clear();
    pendingFeedbackEvents_.clear();
    toneSequence_ = {1, 2, 3, 4};
    std::shuffle(toneSequence_.begin(), toneSequence_.end(), rng_);
    nextTonePlaybackIndex_ = 0;
    highlightedToneIndex_ = -1;
    highlightedToneElapsed_ = 0.0f;
    correctCount_ = 0;
    inputMultiplier_ = 1.0f;
    acceptingInput_ = false;
}

void LuotianyiSkillPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (phase_ == Phase::Complete) {
        return;
    }

    if (phase_ == Phase::Input) {
        inputElapsed_ += deltaTime;

        // Animate tone highlight flash
        if (highlightedToneIndex_ >= 0) {
            highlightedToneElapsed_ += deltaTime;
            if (highlightedToneElapsed_ >= kToneHighlightSeconds) {
                highlightedToneIndex_ = -1;
                highlightedToneElapsed_ = 0.0f;
            }
        }

        // Play each tone in sequence
        while (nextTonePlaybackIndex_ < toneSequence_.size()) {
            const float playbackTime =
                kToneInitialDelaySeconds + (static_cast<float>(nextTonePlaybackIndex_) * kToneIntervalSeconds);
            if (inputElapsed_ < playbackTime) {
                break;
            }
            highlightedToneIndex_ = static_cast<int>(nextTonePlaybackIndex_);
            highlightedToneElapsed_ = 0.0f;
            queueAudioCommand(
                PresentationAudioCommandType::PlayOneShot,
                tonePathForValue(toneSequence_[nextTonePlaybackIndex_]),
                1.0f
            );
            ++nextTonePlaybackIndex_;
        }

        // Open answer window after last tone + delay
        const float answerStartTime =
            kToneInitialDelaySeconds +
            (kToneIntervalSeconds * static_cast<float>(toneSequence_.size() - 1)) +
            kPostSequenceDelaySeconds;
        if (!acceptingInput_ && inputElapsed_ >= answerStartTime) {
            acceptingInput_ = true;
            responseElapsed_ = 0.0f;
        }

        if (acceptingInput_) {
            responseElapsed_ += deltaTime;
        }

        // Finalize when time runs out or all 4 slots filled
        if (acceptingInput_ &&
            (responseElapsed_ >= kAnswerDurationSeconds || userInputs_.size() >= toneSequence_.size())) {
            finalizeInput();
        }
        return;
    }

    // Phase::Result
    resultElapsed_ += deltaTime;
    if (resultElapsed_ >= kResultDurationSeconds) {
        phase_ = Phase::Complete;
    }
}

void LuotianyiSkillPresentation::render(SDL_Renderer* renderer,
                                        int screenW,
                                        int screenH,
                                        const Camera3D& camera) {
    (void)camera;
    if (renderer == nullptr) {
        return;
    }

    const float progress = (phase_ == Phase::Input)
        ? (acceptingInput_ ? easing::clamp01(responseElapsed_ / kAnswerDurationSeconds) : 0.0f)
        : 1.0f;
    const bool showResult = phase_ != Phase::Input;

    // --- Backdrop ---
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 18, 10, 120); // slightly greenish tint vs boss blue
    SDL_Rect backdrop{
        static_cast<int>(std::lround(screenW * 0.17f)),
        static_cast<int>(std::lround(screenH * 0.12f)),
        static_cast<int>(std::lround(screenW * 0.66f)),
        static_cast<int>(std::lround(screenH * 0.28f))
    };
    SDL_RenderFillRect(renderer, &backdrop);
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 86);
    SDL_RenderDrawRect(renderer, &backdrop);

    // --- Tone slots ---
    const float centerX  = static_cast<float>(screenW) * 0.5f;
    const float topRowY  = static_cast<float>(backdrop.y) + 70.0f;
    const float inputRowY= topRowY + 86.0f;
    constexpr float kSlotW = 88.0f;
    constexpr float kSlotH = 60.0f;
    constexpr float kGap   = 20.0f;
    const float totalWidth = (kSlotW * 4.0f) + (kGap * 3.0f);
    const float startX = centerX - (totalWidth * 0.5f) + (kSlotW * 0.5f);

    for (int i = 0; i < 4; ++i) {
        const SDL_FRect seqRect = centeredRect(startX + i * (kSlotW + kGap), topRowY, kSlotW, kSlotH);
        const bool   highlighted = (phase_ == Phase::Input && i == highlightedToneIndex_);
        const int    tone        = toneSequence_[static_cast<size_t>(i)];
        const bool   isCorrect   = (i < static_cast<int>(userInputs_.size()) &&
                                    userInputs_[static_cast<size_t>(i)] == tone);
        // Top row: show the actual sequence (hidden until revealed on result)
        drawToneSlot(renderer, seqRect, showResult ? tone : 0, !showResult, highlighted, showResult, isCorrect);

        const SDL_FRect inputRect = centeredRect(startX + i * (kSlotW + kGap), inputRowY, kSlotW, kSlotH);
        const int  enteredTone  = (i < static_cast<int>(userInputs_.size())) ? userInputs_[static_cast<size_t>(i)] : 0;
        const bool inputCorrect = (enteredTone > 0 && enteredTone == tone);
        drawToneSlot(renderer, inputRect, enteredTone, enteredTone <= 0, false, showResult, inputCorrect);
    }

    // --- Timer bar ---
    const SDL_FRect timerBg{
        static_cast<float>(backdrop.x) + 42.0f,
        static_cast<float>(backdrop.y + backdrop.h) - 34.0f,
        static_cast<float>(backdrop.w) - 84.0f,
        12.0f
    };
    SDL_SetRenderDrawColor(renderer, 34, 38, 46, 220);
    SDL_RenderFillRectF(renderer, &timerBg);

    const SDL_FRect timerFill{
        timerBg.x,
        timerBg.y,
        timerBg.w * (showResult ? 1.0f : progress),
        timerBg.h
    };
    if (showResult) {
        const Uint8 resultAlpha = static_cast<Uint8>(
            std::lround(255.0f * (1.0f - easing::clamp01(resultElapsed_ / kResultDurationSeconds)))
        );
        if (correctCount_ == 4) {
            SDL_SetRenderDrawColor(renderer, 124, 232, 170, resultAlpha); // green: perfect
        } else if (correctCount_ == 0) {
            SDL_SetRenderDrawColor(renderer, 236, 112, 112, resultAlpha); // red: nerf
        } else {
            SDL_SetRenderDrawColor(renderer, 235, 198, 100, resultAlpha); // yellow: partial
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 160, 255, 200, 255); // teal-green for buff vibe
    }
    SDL_RenderFillRectF(renderer, &timerFill);
}

bool LuotianyiSkillPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

void LuotianyiSkillPresentation::onKeyPressed(SDL_Keycode key) {
    if (phase_ != Phase::Input || !acceptingInput_ || userInputs_.size() >= toneSequence_.size()) {
        return;
    }
    const int tone = mapToneFromKey(key);
    if (tone <= 0) {
        return;
    }
    const size_t inputIndex = userInputs_.size();
    const bool enteredCorrectTone = tone == toneSequence_[inputIndex];
    userInputs_.push_back(tone);
    pendingFeedbackEvents_.push_back(buildImmediateFeedbackEvent(
        enteredCorrectTone,
        static_cast<int>(userInputs_.size())
    ));
}

bool LuotianyiSkillPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool LuotianyiSkillPresentation::overridesCamera() const {
    return false; // playable: don't hijack camera
}

bool LuotianyiSkillPresentation::shouldRenderCasterEntity() const {
    return true;
}

bool LuotianyiSkillPresentation::shouldRenderAboveHud() const {
    return true;
}

float LuotianyiSkillPresentation::getInputMultiplier() const {
    return inputMultiplier_;
}

PresentationFeedbackSignal LuotianyiSkillPresentation::getFeedbackSignal() const {
    return {};
}

std::vector<PresentationFeedbackEvent> LuotianyiSkillPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

std::string LuotianyiSkillPresentation::getInputResultText() const {
    const int wrongCount = 4 - correctCount_;
    const int buffPct = static_cast<int>(
        std::lround(static_cast<float>(60) * inputMultiplier_) // 60 = atkBuff max in JSON
    );
    if (buffPct < 0) {
        return "Correct tones: " + std::to_string(correctCount_) +
               "/4. Wrong tones: " + std::to_string(wrongCount) +
               ". ATK NERF " + std::to_string(-buffPct) + "% applied!";
    }
    return "Correct tones: " + std::to_string(correctCount_) +
           "/4. Wrong tones: " + std::to_string(wrongCount) +
           ". ATK +" + std::to_string(buffPct) + "% applied!";
}

int LuotianyiSkillPresentation::getCorrectToneCount() const {
    return correctCount_;
}

std::vector<PresentationAudioCommand> LuotianyiSkillPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

std::string LuotianyiSkillPresentation::resolvePath(const std::string& relativePath) {
    return platform::path::resolvePath(relativePath);
}

void LuotianyiSkillPresentation::queueAudioCommand(PresentationAudioCommandType type,
                                                    const std::string& id,
                                                    float volume) {
    pendingAudioCommands_.push_back(PresentationAudioCommand{type, resolvePath(id), volume});
}

PresentationFeedbackEvent LuotianyiSkillPresentation::buildImmediateFeedbackEvent(bool enteredCorrectTone,
                                                                                  int enteredCount) const {
    const int safeEnteredCount = std::clamp(enteredCount, 1, 4);
    int currentCorrect = 0;
    for (int i = 0; i < safeEnteredCount; ++i) {
        if (userInputs_[static_cast<size_t>(i)] == toneSequence_[static_cast<size_t>(i)]) {
            ++currentCorrect;
        }
    }

    const int effectiveCorrect = safeEnteredCount >= 4
        ? currentCorrect
        : std::clamp(static_cast<int>(std::lround((static_cast<float>(currentCorrect) / safeEnteredCount) * 4.0f)), 0, 4);
    const float projectedMultiplier = multiplierForCorrectCount(effectiveCorrect);
    const int atkPercent = static_cast<int>(std::lround(60.0f * projectedMultiplier));

    PresentationFeedbackEvent event;
    event.signal = PresentationFeedbackSignal::binary(enteredCorrectTone);
    event.multiplier = projectedMultiplier;
    event.comboEligible = true;
    event.rewardText = atkPercent >= 0
        ? ("+" + std::to_string(atkPercent) + "% ATK")
        : (std::to_string(atkPercent) + "% ATK");
    return event;
}

void LuotianyiSkillPresentation::finalizeInput() {
    correctCount_ = 0;
    for (size_t i = 0; i < toneSequence_.size(); ++i) {
        const int entered = (i < userInputs_.size()) ? userInputs_[i] : -1;
        if (entered == toneSequence_[i]) {
            ++correctCount_;
        }
    }

    inputMultiplier_ = multiplierForCorrectCount(correctCount_);

    const bool allCorrect = (correctCount_ == 4);
    queueAudioCommand(
        PresentationAudioCommandType::PlayOneShot,
        std::string(kToneVoiceDirectory) + (allCorrect ? "correct.wav" : "false.wav"),
        1.0f
    );

    phase_ = Phase::Result;
    resultElapsed_ = 0.0f;
    highlightedToneIndex_ = -1;
    highlightedToneElapsed_ = 0.0f;
}

void LuotianyiSkillPresentation::drawToneSlot(SDL_Renderer* renderer,
                                               const SDL_FRect& rect,
                                               int tone,
                                               bool hidden,
                                               bool highlighted,
                                               bool showResult,
                                               bool isCorrect) const {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Uint8 fillAlpha = hidden ? 36 : 180;
    SDL_Color fill  = hidden ? SDL_Color{42, 46, 58, 255} : toneColor(tone);
    if (highlighted) { fillAlpha = 255; }
    if (showResult && !hidden) { fillAlpha = 220; }

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fillAlpha);
    SDL_RenderFillRectF(renderer, &rect);

    SDL_Color outline{240, 240, 240, 180};
    if (showResult && !hidden) {
        outline = isCorrect ? SDL_Color{124, 232, 170, 255} : SDL_Color{236, 112, 112, 255};
    } else if (highlighted) {
        outline = SDL_Color{255, 255, 255, 255};
    }
    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, outline.a);
    SDL_RenderDrawRectF(renderer, &rect);

    if (!hidden && tone > 0) {
        drawToneGlyph(renderer, rect, tone, 255);
    }
}

void LuotianyiSkillPresentation::drawToneGlyph(SDL_Renderer* renderer,
                                                const SDL_FRect& rect,
                                                int tone,
                                                Uint8 alpha) const {
    if (tone <= 0) {
        return;
    }
    const float barWidth  = rect.w * 0.54f;
    const float barHeight = 5.0f;
    const float gap       = 6.0f;
    const float totalH    = (barHeight * tone) + (gap * std::max(0, tone - 1));
    const float startY    = rect.y + (rect.h - totalH) * 0.5f;
    const float startX    = rect.x + (rect.w - barWidth) * 0.5f;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
    for (int i = 0; i < tone; ++i) {
        const SDL_FRect bar{startX, startY + i * (barHeight + gap), barWidth, barHeight};
        SDL_RenderFillRectF(renderer, &bar);
    }
}

int LuotianyiSkillPresentation::mapToneFromKey(SDL_Keycode key) const {
    switch (key) {
        case SDLK_1: case SDLK_KP_1: return 1;
        case SDLK_2: case SDLK_KP_2: return 2;
        case SDLK_3: case SDLK_KP_3: return 3;
        case SDLK_4: case SDLK_KP_4: return 4;
        default: return 0;
    }
}

} // namespace battle
