#include "randy_boss_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"
#include "../render/battle_asset_loading.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace battle {
namespace {

constexpr float kCameraPosYOffset = -675.0f;
constexpr float kCameraPosZ = -175.0f;
constexpr float kCameraPitchDegrees = -2.5f;
constexpr float kCameraYawDegrees = 0.0f;
constexpr float kCameraFocalLength = 32000.0f;

constexpr float kResultDurationSeconds = 0.42f;
constexpr float kHitLeadSeconds = 0.14f;
constexpr float kPaperWidthRatio = 0.64f;
constexpr float kPaperHeightRatio = 0.76f;
constexpr float kHeaderHeight = 84.0f;
constexpr float kMinDamageMultiplier = 0.40f;
constexpr float kMaxDamageMultiplier = 2.00f;
constexpr float kOkaySignalScore = 0.30f;
constexpr float kGoodSignalScore = 0.70f;
constexpr int kRecentKeyWindow = 5;
constexpr int kWrapColumnCount = 34;
constexpr int kMaxVisibleCharacters = kWrapColumnCount * 13;

char printableCharForKey(SDL_Keycode key) {
    if (key >= 32 && key <= 126) {
        return static_cast<char>(key);
    }
    if (key >= SDLK_KP_1 && key <= SDLK_KP_9) {
        return static_cast<char>('1' + (key - SDLK_KP_1));
    }
    if (key == SDLK_KP_0) {
        return '0';
    }
    return '\0';
}

#ifdef BATTLE_ENABLE_TTF
TTF_Font* openBestPaperFont(int ptSize) {
    if (TTF_WasInit() == 0) {
        return nullptr;
    }

    for (const std::string& path : platform::path::preferredLatinFontPaths()) {
        TTF_Font* font = TTF_OpenFont(path.c_str(), ptSize);
        if (font != nullptr) {
            return font;
        }
    }
    return nullptr;
}

void drawTextLine(TTF_Font* font,
                  SDL_Renderer* renderer,
                  const std::string& text,
                  int x,
                  int y,
                  SDL_Color color) {
    if (font == nullptr || renderer == nullptr || text.empty()) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture != nullptr) {
        SDL_Rect dst{x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}
#endif

} // namespace

RandyBossPresentation::RandyBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = inputDurationSeconds_ + kResultDurationSeconds;
}

RandyBossPresentation::~RandyBossPresentation() {
    releaseAssets();
}

void RandyBossPresentation::start() {
    elapsedTime_ = 0.0f;
    inputElapsed_ = 0.0f;
    resultElapsed_ = 0.0f;
    phase_ = Phase::Input;
    success_ = false;
    hitReady_ = false;
    hitDispatched_ = false;
    validPressCount_ = 0;
    typedText_.clear();
    recentKeys_.clear();
    pendingFeedbackEvents_.clear();
    totalDuration_ = inputDurationSeconds_ + kResultDurationSeconds;
}

void RandyBossPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (phase_ == Phase::Input) {
        inputElapsed_ += deltaTime;
        if (inputElapsed_ >= inputDurationSeconds_) {
            finalizeInput();
        }
        return;
    }

    if (phase_ != Phase::Result) {
        return;
    }

    resultElapsed_ += deltaTime;
    if (!success_ && !hitReady_ && resultElapsed_ >= kHitLeadSeconds) {
        hitReady_ = true;
    }
    if (resultElapsed_ >= kResultDurationSeconds && (success_ || hitDispatched_)) {
        phase_ = Phase::Complete;
    }
}

void RandyBossPresentation::render(SDL_Renderer* renderer,
                                   int screenW,
                                   int screenH,
                                   const Camera3D& camera) {
    (void)camera;
    if (renderer == nullptr) {
        return;
    }

    ensureAssetsLoaded(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 10, 14, 20, 170);
    SDL_Rect dimRect{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &dimRect);

    const float paperWidth = static_cast<float>(screenW) * kPaperWidthRatio;
    const float paperHeight = static_cast<float>(screenH) * kPaperHeightRatio;
    const SDL_FRect paperRect{
        (static_cast<float>(screenW) - paperWidth) * 0.5f,
        (static_cast<float>(screenH) - paperHeight) * 0.5f,
        paperWidth,
        paperHeight
    };
    const SDL_FRect shadowRect{
        paperRect.x + 18.0f,
        paperRect.y + 22.0f,
        paperRect.w,
        paperRect.h
    };
    SDL_SetRenderDrawColor(renderer, 6, 8, 12, 110);
    SDL_RenderFillRectF(renderer, &shadowRect);

    SDL_SetRenderDrawColor(renderer, 250, 250, 246, 255);
    SDL_RenderFillRectF(renderer, &paperRect);
    SDL_SetRenderDrawColor(renderer, 208, 212, 220, 255);
    SDL_RenderDrawRectF(renderer, &paperRect);

    const SDL_FRect headerRect{
        paperRect.x,
        paperRect.y,
        paperRect.w,
        kHeaderHeight
    };
    SDL_SetRenderDrawColor(renderer, 38, 97, 216, 255);
    SDL_RenderFillRectF(renderer, &headerRect);

    if (logoTexture_ != nullptr) {
        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(logoTexture_, nullptr, nullptr, &texW, &texH);
        const float drawHeight = 42.0f;
        const float drawWidth = texH > 0
            ? drawHeight * (static_cast<float>(texW) / static_cast<float>(texH))
            : 86.0f;
        const SDL_FRect logoRect{
            headerRect.x + 30.0f,
            headerRect.y + (headerRect.h - drawHeight) * 0.5f,
            drawWidth,
            drawHeight
        };
        SDL_RenderCopyF(renderer, logoTexture_, nullptr, &logoRect);
    } else {
        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        SDL_FRect fallbackRect{
            headerRect.x + 30.0f,
            headerRect.y + 20.0f,
            120.0f,
            44.0f
        };
        SDL_RenderFillRectF(renderer, &fallbackRect);
    }

    drawPaperText(renderer, paperRect);

    const float progress = easing::clamp01(inputElapsed_ / std::max(0.001f, inputDurationSeconds_));
    const SDL_FRect timerBg{
        paperRect.x + 42.0f,
        paperRect.y + paperRect.h - 38.0f,
        paperRect.w - 84.0f,
        12.0f
    };
    SDL_SetRenderDrawColor(renderer, 214, 218, 224, 255);
    SDL_RenderFillRectF(renderer, &timerBg);
    SDL_SetRenderDrawColor(renderer, 38, 97, 216, 255);
    SDL_FRect timerFill = timerBg;
    timerFill.w *= (phase_ == Phase::Input) ? progress : 1.0f;
    SDL_RenderFillRectF(renderer, &timerFill);
}

bool RandyBossPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

bool RandyBossPresentation::onKeyPressed(SDL_Keycode key) {
    if (phase_ != Phase::Input) {
        return false;
    }

    const char displayChar = printableCharForKey(key);
    if (displayChar == '\0') {
        return true;
    }

    if (static_cast<int>(typedText_.size()) < kMaxVisibleCharacters) {
        typedText_.push_back(displayChar);
    }

    const bool isRecent = std::find(recentKeys_.begin(), recentKeys_.end(), key) != recentKeys_.end();
    recentKeys_.push_back(key);
    while (static_cast<int>(recentKeys_.size()) > kRecentKeyWindow) {
        recentKeys_.pop_front();
    }

    if (isRecent) {
        return true;
    }

    ++validPressCount_;
    PresentationFeedbackEvent event;
    const float ratio = completionRatio();
    if (ratio >= 1.0f) {
        event.signal = PresentationFeedbackSignal::forcedPerfect();
    } else if (ratio >= 0.5f) {
        event.signal = PresentationFeedbackSignal::graded(kGoodSignalScore);
    } else {
        event.signal = PresentationFeedbackSignal::graded(kOkaySignalScore);
    }
    event.comboEligible = true;
    pendingFeedbackEvents_.push_back(event);
    return true;
}

void RandyBossPresentation::preload(SDL_Renderer* renderer) {
    ensureAssetsLoaded(renderer);
}

void RandyBossPresentation::setTuningProfile(const PresentationTuningProfile& profile) {
    if (const auto it = profile.intParams.find("spamQuota"); it != profile.intParams.end()) {
        quota_ = std::max(1, it->second);
    }
    if (const auto it = profile.floatParams.find("inputDurationSeconds"); it != profile.floatParams.end()) {
        inputDurationSeconds_ = std::max(1.0f, it->second);
    }
    totalDuration_ = inputDurationSeconds_ + kResultDurationSeconds;
}

bool RandyBossPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool RandyBossPresentation::overridesCamera() const {
    return true;
}

void RandyBossPresentation::applyCameraState(Camera3D& camera) const {
    camera.posX = casterX_;
    camera.posY = casterY_ + kCameraPosYOffset;
    camera.posZ = kCameraPosZ;
    camera.pitchDegrees = kCameraPitchDegrees;
    camera.yawDegrees = kCameraYawDegrees;
    camera.focalLength = kCameraFocalLength;
}

bool RandyBossPresentation::shouldRenderAboveHud() const {
    return false;
}

PresentationFeedbackSignal RandyBossPresentation::getFeedbackSignal() const {
    return PresentationFeedbackSignal::graded(completionRatio());
}

float RandyBossPresentation::consumeHitDamageMultiplier() {
    return failureDamageMultiplier();
}

std::vector<PresentationFeedbackEvent> RandyBossPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

int RandyBossPresentation::consumeHitEvents() {
    if (success_ || !hitReady_ || hitDispatched_) {
        return 0;
    }
    hitDispatched_ = true;
    return 1;
}

std::string RandyBossPresentation::getInputResultText() const {
    std::ostringstream stream;
    if (success_) {
        stream << "Assignment submitted: " << validPressCount_ << "/" << quota_;
    } else {
        stream << "Submitted " << validPressCount_ << "/" << quota_
               << ". Team damage multiplier " << failureDamageMultiplier() << "x.";
    }
    return stream.str();
}

void RandyBossPresentation::ensureAssetsLoaded(SDL_Renderer* renderer) {
    if (renderer == nullptr) {
        return;
    }

    if (logoTexture_ == nullptr) {
        const auto texture = render::tryLoadTextureFromPath(renderer, "assets/combat/presentations/randy/lexue.png");
        if (texture.has_value()) {
            logoTexture_ = *texture;
        }
    }

#ifdef BATTLE_ENABLE_TTF
    if (paperFont_ == nullptr) {
        paperFont_ = openBestPaperFont(28);
    }
#else
    (void)renderer;
#endif
}

void RandyBossPresentation::releaseAssets() {
    if (logoTexture_ != nullptr) {
        SDL_DestroyTexture(logoTexture_);
        logoTexture_ = nullptr;
    }
#ifdef BATTLE_ENABLE_TTF
    if (paperFont_ != nullptr && TTF_WasInit() != 0) {
        TTF_CloseFont(paperFont_);
        paperFont_ = nullptr;
    }
#endif
}

void RandyBossPresentation::finalizeInput() {
    if (phase_ != Phase::Input) {
        return;
    }

    success_ = validPressCount_ >= quota_;
    phase_ = Phase::Result;
    resultElapsed_ = 0.0f;
    hitReady_ = false;
}

float RandyBossPresentation::completionRatio() const {
    return std::clamp(
        static_cast<float>(validPressCount_) / static_cast<float>(std::max(1, quota_)),
        0.0f,
        1.0f
    );
}

float RandyBossPresentation::failureDamageMultiplier() const {
    if (success_) {
        return 0.0f;
    }
    return easing::lerp(kMaxDamageMultiplier, kMinDamageMultiplier, completionRatio());
}

std::string RandyBossPresentation::wrappedTypedText() const {
    std::string wrapped;
    wrapped.reserve(typedText_.size() + (typedText_.size() / kWrapColumnCount) + 4);
    for (std::size_t i = 0; i < typedText_.size(); ++i) {
        if (i > 0 && (i % static_cast<std::size_t>(kWrapColumnCount)) == 0) {
            wrapped.push_back('\n');
        }
        wrapped.push_back(typedText_[i]);
    }
    return wrapped;
}

void RandyBossPresentation::drawPaperText(SDL_Renderer* renderer, const SDL_FRect& paperRect) const {
    const SDL_FRect bodyRect{
        paperRect.x + 44.0f,
        paperRect.y + kHeaderHeight + 26.0f,
        paperRect.w - 88.0f,
        paperRect.h - kHeaderHeight - 72.0f
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 232, 235, 240, 160);
    for (int i = 0; i < 11; ++i) {
        const float y = bodyRect.y + 18.0f + static_cast<float>(i) * 42.0f;
        SDL_RenderDrawLineF(renderer, bodyRect.x, y, bodyRect.x + bodyRect.w, y);
    }

#ifdef BATTLE_ENABLE_TTF
    if (paperFont_ == nullptr) {
        return;
    }

    std::istringstream input(wrappedTypedText());
    std::string line;
    int lineIndex = 0;
    while (std::getline(input, line)) {
        drawTextLine(
            paperFont_,
            renderer,
            line,
            static_cast<int>(std::lround(bodyRect.x)),
            static_cast<int>(std::lround(bodyRect.y + 4.0f + static_cast<float>(lineIndex) * 42.0f)),
            SDL_Color{38, 42, 54, 255}
        );
        ++lineIndex;
    }
#else
    (void)renderer;
#endif
}

} // namespace battle
