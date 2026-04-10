#include "luotianyi_boss_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>

namespace battle {
namespace {

constexpr float kResultLeadSeconds = 0.55f;
constexpr float kResultDurationSeconds = 0.80f;
constexpr float kToneInitialDelaySeconds = 0.50f;
constexpr float kToneHighlightSeconds = 0.26f;
constexpr float kPerfectDamageMultiplier = 0.10f;

constexpr float kCameraPosYOffset = -675.0f;
constexpr float kCameraPosZ = -175.0f;
constexpr float kCameraPitchDegrees = -2.5f;
constexpr float kCameraYawDegrees = 0.0f;
constexpr float kCameraFocalLength = 32000.0f;

constexpr char kToneVoiceDirectory[] = "assets/combat/voices/luotianyiBoss/";

SDL_Color toneColor(int tone) {
    switch (tone) {
        case 1: return SDL_Color{235, 108, 108, 255};
        case 2: return SDL_Color{245, 190, 92, 255};
        case 3: return SDL_Color{106, 211, 154, 255};
        case 4: return SDL_Color{96, 162, 255, 255};
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
        case 1: return std::string(kToneVoiceDirectory) + "first.opus";
        case 2: return std::string(kToneVoiceDirectory) + "second.opus";
        case 3: return std::string(kToneVoiceDirectory) + "third.opus";
        case 4: return std::string(kToneVoiceDirectory) + "fourth.opus";
        default: return {};
    }
}

} // namespace

LuotianyiBossPresentation::LuotianyiBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ)
    , rng_(std::random_device{}()) {
    rebuildTotalDuration();
}

void LuotianyiBossPresentation::start() {
    elapsedTime_ = 0.0f;
    inputElapsed_ = 0.0f;
    responseElapsed_ = 0.0f;
    resultElapsed_ = 0.0f;
    phase_ = Phase::Input;
    userInputs_.clear();
    pendingAudioCommands_.clear();
    pendingFeedbackEvents_.clear();
    generateToneSequence();
    nextTonePlaybackIndex_ = 0;
    highlightedToneIndex_ = -1;
    highlightedToneElapsed_ = 0.0f;
    correctCount_ = 0;
    damageMultiplier_ = 1.0f;
    hitQueued_ = false;
    resultHitQueued_ = false;
    acceptingInput_ = false;
}

void LuotianyiBossPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (phase_ == Phase::Complete) {
        return;
    }

    if (phase_ == Phase::Input) {
        inputElapsed_ += deltaTime;

        if (highlightedToneIndex_ >= 0) {
            highlightedToneElapsed_ += deltaTime;
            if (highlightedToneElapsed_ >= kToneHighlightSeconds) {
                highlightedToneIndex_ = -1;
                highlightedToneElapsed_ = 0.0f;
            }
        }

        while (nextTonePlaybackIndex_ < toneSequence_.size()) {
            const float playbackTime =
                kToneInitialDelaySeconds + (static_cast<float>(nextTonePlaybackIndex_) * toneIntervalSeconds_);
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

        const float answerStartTime =
            kToneInitialDelaySeconds +
            (toneIntervalSeconds_ * static_cast<float>(toneSequence_.size() - 1)) +
            postSequenceDelaySeconds_;
        if (!acceptingInput_ && inputElapsed_ >= answerStartTime) {
            acceptingInput_ = true;
            responseElapsed_ = 0.0f;
        }

        if (acceptingInput_) {
            responseElapsed_ += deltaTime;
        }

        if (acceptingInput_ &&
            (responseElapsed_ >= answerDurationSeconds_ || userInputs_.size() >= toneSequence_.size())) {
            finalizeInput();
        }
        return;
    }

    resultElapsed_ += deltaTime;
    if (!resultHitQueued_ && resultElapsed_ >= kResultLeadSeconds) {
        hitQueued_ = true;
        resultHitQueued_ = true;
    }
    if (resultElapsed_ >= kResultDurationSeconds) {
        phase_ = Phase::Complete;
    }
}

void LuotianyiBossPresentation::render(SDL_Renderer* renderer,
                                       int screenW,
                                       int screenH,
                                       const Camera3D& camera) {
    (void)camera;
    if (renderer == nullptr) {
        return;
    }

    const float progress = (phase_ == Phase::Input)
        ? (acceptingInput_ ? easing::clamp01(responseElapsed_ / std::max(0.001f, answerDurationSeconds_)) : 0.0f)
        : 1.0f;
    const bool showResult = phase_ != Phase::Input;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 10, 18, 112);
    SDL_Rect backdrop{
        static_cast<int>(std::lround(screenW * 0.17f)),
        static_cast<int>(std::lround(screenH * 0.12f)),
        static_cast<int>(std::lround(screenW * 0.66f)),
        static_cast<int>(std::lround(screenH * 0.28f))
    };
    SDL_RenderFillRect(renderer, &backdrop);

    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 86);
    SDL_RenderDrawRect(renderer, &backdrop);

    const float centerX = static_cast<float>(screenW) * 0.5f;
    const float topRowY = static_cast<float>(backdrop.y) + 70.0f;
    const float inputRowY = topRowY + 86.0f;
    const float slotWidth = 88.0f;
    const float slotHeight = 60.0f;
    const float gap = 20.0f;
    const float totalWidth = (slotWidth * 4.0f) + (gap * 3.0f);
    const float startX = centerX - (totalWidth * 0.5f) + (slotWidth * 0.5f);

    for (int i = 0; i < 4; ++i) {
        const SDL_FRect seqRect = centeredRect(startX + i * (slotWidth + gap), topRowY, slotWidth, slotHeight);
        const bool highlighted = phase_ == Phase::Input && i == highlightedToneIndex_;
        const int tone = toneSequence_[static_cast<size_t>(i)];
        const bool isCorrect = i < static_cast<int>(userInputs_.size()) && userInputs_[static_cast<size_t>(i)] == tone;
        drawToneSlot(renderer, seqRect, 0, false, highlighted, showResult, isCorrect);

        const SDL_FRect inputRect = centeredRect(startX + i * (slotWidth + gap), inputRowY, slotWidth, slotHeight);
        const int enteredTone = i < static_cast<int>(userInputs_.size()) ? userInputs_[static_cast<size_t>(i)] : 0;
        const bool inputCorrect = enteredTone > 0 && enteredTone == tone;
        drawToneSlot(renderer, inputRect, enteredTone, enteredTone <= 0, false, showResult, inputCorrect);
    }

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
        const Uint8 resultAlpha = static_cast<Uint8>(std::lround(255.0f * (1.0f - easing::clamp01(resultElapsed_ / kResultDurationSeconds))));
        if (correctCount_ == static_cast<int>(toneSequence_.size())) {
            SDL_SetRenderDrawColor(renderer, 124, 232, 170, resultAlpha);
        } else {
            SDL_SetRenderDrawColor(renderer, 236, 112, 112, resultAlpha);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 120, 188, 255, 255);
    }
    SDL_RenderFillRectF(renderer, &timerFill);
}

bool LuotianyiBossPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

bool LuotianyiBossPresentation::onKeyPressed(SDL_Keycode key) {
    if (phase_ != Phase::Input || !acceptingInput_ || userInputs_.size() >= toneSequence_.size()) {
        return false;
    }

    const int tone = mapToneFromKey(key);
    if (tone <= 0) {
        return false;
    }

    const size_t inputIndex = userInputs_.size();
    const bool enteredCorrectTone = tone == toneSequence_[inputIndex];
    userInputs_.push_back(tone);
    pendingFeedbackEvents_.push_back(buildImmediateFeedbackEvent(enteredCorrectTone));
    return true;
}

void LuotianyiBossPresentation::setTuningProfile(const PresentationTuningProfile& profile) {
    if (const auto it = profile.floatParams.find("answerDurationSeconds"); it != profile.floatParams.end()) {
        answerDurationSeconds_ = std::max(0.5f, it->second);
    }
    if (const auto it = profile.floatParams.find("toneIntervalSeconds"); it != profile.floatParams.end()) {
        toneIntervalSeconds_ = std::max(0.05f, it->second);
    }
    if (const auto it = profile.floatParams.find("postSequenceDelaySeconds"); it != profile.floatParams.end()) {
        postSequenceDelaySeconds_ = std::max(0.0f, it->second);
    }
    exactDuplicatePair_ = false;
    if (const auto it = profile.intParams.find("exactDuplicatePair"); it != profile.intParams.end()) {
        exactDuplicatePair_ = it->second != 0;
    }
    rebuildTotalDuration();
}

bool LuotianyiBossPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool LuotianyiBossPresentation::overridesCamera() const {
    return true;
}

void LuotianyiBossPresentation::applyCameraState(Camera3D& camera) const {
    camera.posX = casterX_;
    camera.posY = casterY_ + kCameraPosYOffset;
    camera.posZ = kCameraPosZ;
    camera.pitchDegrees = kCameraPitchDegrees;
    camera.yawDegrees = kCameraYawDegrees;
    camera.focalLength = kCameraFocalLength;
}

bool LuotianyiBossPresentation::shouldRenderCasterEntity() const {
    return true;
}

bool LuotianyiBossPresentation::shouldRenderAboveHud() const {
    return true;
}

float LuotianyiBossPresentation::getInputMultiplier() const {
    return damageMultiplier_;
}

PresentationFeedbackSignal LuotianyiBossPresentation::getFeedbackSignal() const {
    return {};
}

std::vector<PresentationFeedbackEvent> LuotianyiBossPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

float LuotianyiBossPresentation::consumeHitDamageMultiplier() {
    return damageMultiplier_;
}

int LuotianyiBossPresentation::consumeHitEvents() {
    if (!hitQueued_) {
        return 0;
    }
    hitQueued_ = false;
    return 1;
}

int LuotianyiBossPresentation::getDamageLabelHitCount() const {
    return 1;
}

std::string LuotianyiBossPresentation::getInputResultText() const {
    if (correctCount_ == static_cast<int>(toneSequence_.size())) {
        return "All tones matched. Damage reduced 90%.";
    }
    return "Sequence broken. Damage reduced 0%.";
}

std::vector<PresentationAudioCommand> LuotianyiBossPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

std::string LuotianyiBossPresentation::resolvePath(const std::string& relativePath) {
    return platform::path::resolvePath(relativePath);
}

void LuotianyiBossPresentation::rebuildTotalDuration() {
    totalDuration_ =
        kToneInitialDelaySeconds +
        (toneIntervalSeconds_ * static_cast<float>(toneSequence_.size() - 1)) +
        postSequenceDelaySeconds_ +
        answerDurationSeconds_ +
        kResultDurationSeconds;
}

void LuotianyiBossPresentation::generateToneSequence() {
    if (!exactDuplicatePair_) {
        toneSequence_ = {1, 2, 3, 4};
        std::shuffle(toneSequence_.begin(), toneSequence_.end(), rng_);
        rebuildTotalDuration();
        return;
    }

    std::array<int, 4> availableTones{{1, 2, 3, 4}};
    std::shuffle(availableTones.begin(), availableTones.end(), rng_);
    toneSequence_ = {
        availableTones[0],
        availableTones[0],
        availableTones[1],
        availableTones[2]
    };
    std::shuffle(toneSequence_.begin(), toneSequence_.end(), rng_);
    rebuildTotalDuration();
}

void LuotianyiBossPresentation::queueAudioCommand(PresentationAudioCommandType type,
                                                  const std::string& id,
                                                  float volume) {
    pendingAudioCommands_.push_back(PresentationAudioCommand{type, resolvePath(id), volume});
}

PresentationFeedbackEvent LuotianyiBossPresentation::buildImmediateFeedbackEvent(bool enteredCorrectTone) const {
    bool perfectChainIntact = true;
    for (size_t i = 0; i < userInputs_.size(); ++i) {
        if (userInputs_[i] != toneSequence_[i]) {
            perfectChainIntact = false;
            break;
        }
    }

    PresentationFeedbackEvent event;
    event.signal = PresentationFeedbackSignal::binary(enteredCorrectTone);
    event.multiplier = perfectChainIntact ? kPerfectDamageMultiplier : 1.0f;
    event.comboEligible = true;
    event.rewardText = perfectChainIntact ? "-90% DMG TAKEN" : "-0% DMG TAKEN";
    return event;
}

void LuotianyiBossPresentation::finalizeInput() {
    correctCount_ = 0;
    for (size_t i = 0; i < toneSequence_.size(); ++i) {
        const int enteredTone = i < userInputs_.size() ? userInputs_[i] : -1;
        if (enteredTone == toneSequence_[i]) {
            ++correctCount_;
        }
    }

    damageMultiplier_ = correctCount_ == static_cast<int>(toneSequence_.size()) ? kPerfectDamageMultiplier : 1.0f;

    const bool allCorrect = correctCount_ == static_cast<int>(toneSequence_.size());
    queueAudioCommand(
        PresentationAudioCommandType::PlayOneShot,
        std::string(kToneVoiceDirectory) + (allCorrect ? "correct.opus" : "false.opus"),
        1.0f
    );

    phase_ = Phase::Result;
    resultElapsed_ = 0.0f;
    highlightedToneIndex_ = -1;
    highlightedToneElapsed_ = 0.0f;
    resultHitQueued_ = false;
}

void LuotianyiBossPresentation::drawToneSlot(SDL_Renderer* renderer,
                                             const SDL_FRect& rect,
                                             int tone,
                                             bool hidden,
                                             bool highlighted,
                                             bool showResult,
                                             bool isCorrect) const {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Uint8 fillAlpha = hidden ? 36 : 180;
    SDL_Color fill = hidden ? SDL_Color{42, 46, 58, 255} : toneColor(tone);
    if (highlighted) {
        fillAlpha = 255;
    }
    if (showResult && !hidden) {
        fillAlpha = 220;
    }

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

void LuotianyiBossPresentation::drawToneGlyph(SDL_Renderer* renderer,
                                              const SDL_FRect& rect,
                                              int tone,
                                              Uint8 alpha) const {
    if (tone <= 0) {
        return;
    }

    const float barWidth = rect.w * 0.54f;
    const float barHeight = 5.0f;
    const float gap = 6.0f;
    const float totalHeight = (barHeight * tone) + (gap * std::max(0, tone - 1));
    float startY = rect.y + (rect.h - totalHeight) * 0.5f;
    const float startX = rect.x + (rect.w - barWidth) * 0.5f;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
    for (int i = 0; i < tone; ++i) {
        const SDL_FRect bar{startX, startY + i * (barHeight + gap), barWidth, barHeight};
        SDL_RenderFillRectF(renderer, &bar);
    }
}

int LuotianyiBossPresentation::mapToneFromKey(SDL_Keycode key) const {
    switch (key) {
        case SDLK_1:
        case SDLK_KP_1:
            return 1;
        case SDLK_2:
        case SDLK_KP_2:
            return 2;
        case SDLK_3:
        case SDLK_KP_3:
            return 3;
        case SDLK_4:
        case SDLK_KP_4:
            return 4;
        default:
            return 0;
    }
}

} // namespace battle
