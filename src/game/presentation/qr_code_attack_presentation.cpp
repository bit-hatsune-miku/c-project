#include "qr_code_attack_presentation.h"

#include "../../platform/path_resolution.h"
#include "../core/easing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr float kSurvivalDurationSeconds = 5.0f;
constexpr float kFreezeDurationSeconds = 2.0f;
constexpr float kQrRespawnIntervalSeconds = 1.5f;
constexpr float kPhoneMoveSpeedPixels = 680.0f;
constexpr float kQrChaseSpeedPixels = 320.0f;
constexpr float kPhoneTargetHeight = 330.0f;
constexpr float kPhoneFallbackWidth = 250.0f;
constexpr float kQrTargetHeight = 220.0f;
constexpr float kQrFallbackWidth = 220.0f;
constexpr float kCursorSizePixels = 6.0f;
constexpr float kQrScanWidthRatio = 0.42f;
constexpr float kQrScanHeightRatio = 0.42f;
constexpr float kBackdropAlpha = 92.0f;
constexpr float kFreezeBackdropAlpha = 148.0f;
constexpr float kProgressBarWidth = 340.0f;
constexpr float kProgressBarHeight = 10.0f;

constexpr char kPhoneTexturePath[] = "assets/combat/presentations/wechatalipay/huaweisanzhedian.png";
constexpr char kWeixinTexturePath[] = "assets/combat/presentations/wechatalipay/weixin.png";
constexpr char kZhifuTexturePath[] = "assets/combat/presentations/wechatalipay/zhifu.png";
float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float randomFloat(float minValue, float maxValue) {
    const float unit = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minValue + (maxValue - minValue) * unit;
}

SDL_FRect centeredRect(float centerX, float centerY, float width, float height) {
    return SDL_FRect{
        centerX - width * 0.5f,
        centerY - height * 0.5f,
        width,
        height
    };
}

bool rectsOverlap(const SDL_FRect& lhs, const SDL_FRect& rhs) {
    return lhs.x < rhs.x + rhs.w &&
        lhs.x + lhs.w > rhs.x &&
        lhs.y < rhs.y + rhs.h &&
        lhs.y + lhs.h > rhs.y;
}

} // namespace

QrCodeAttackPresentation::QrCodeAttackPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = kSurvivalDurationSeconds + kFreezeDurationSeconds;
}

QrCodeAttackPresentation::~QrCodeAttackPresentation() {
    destroyTextures();
}

void QrCodeAttackPresentation::start() {
    elapsedTime_ = 0.0f;
    phase_ = Phase::Active;
    spawnCount_ = 0;
    activeTime_ = 0.0f;
    freezeTime_ = 0.0f;
    nextSpawnTime_ = kQrRespawnIntervalSeconds;
    cursorX_ = static_cast<float>(screenW_) * 0.5f;
    cursorY_ = static_cast<float>(screenH_) * 0.58f;
    damageMultiplier_ = 1.0f;
    survivedRatio_ = 0.0f;
    pendingAbilityAudioCues_ = 0;
    pendingHitEvents_ = 0;
    hitQueued_ = false;
    pendingAudioCommands_.clear();
    pendingFeedbackEvents_.clear();
    activeCodes_.clear();
    // Spawn the initial QR code
    spawnQrCode();
    spawnQrCode();
}

void QrCodeAttackPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (phase_ == Phase::Complete) {
        return;
    }

    if (phase_ == Phase::Frozen) {
        freezeTime_ += deltaTime;
        if (!hitQueued_ && freezeTime_ >= kFreezeDurationSeconds) {
            pendingHitEvents_ = 1;
            hitQueued_ = true;
            phase_ = Phase::Complete;
        }
        return;
    }

    activeTime_ = std::min(kSurvivalDurationSeconds, activeTime_ + deltaTime);
    survivedRatio_ = clamp01(activeTime_ / kSurvivalDurationSeconds);

    updateCursor(deltaTime);
    updateQrCodes(deltaTime);

    while (activeTime_ >= nextSpawnTime_ && phase_ == Phase::Active) {
        spawnQrCode();
        nextSpawnTime_ += kQrRespawnIntervalSeconds;
    }

    if (cursorTouchesAnyQr()) {
        damageMultiplier_ = 1.0f - survivedRatio_;
        freezeTime_ = 0.0f;
        pendingAbilityAudioCues_ = 1;
        queueFeedbackEvent();
        phase_ = Phase::Frozen;
        return;
    }

    if (activeTime_ >= kSurvivalDurationSeconds) {
        damageMultiplier_ = 0.0f;
        queueFeedbackEvent();
        phase_ = Phase::Complete;
    }
}

void QrCodeAttackPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)camera;
    screenW_ = std::max(1, screenW);
    screenH_ = std::max(1, screenH);
    ensureTexturesLoaded(renderer);

    const float overlayAlpha = phase_ == Phase::Frozen ? kFreezeBackdropAlpha : kBackdropAlpha;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 12, 18, static_cast<Uint8>(overlayAlpha));
    SDL_Rect fullRect{0, 0, screenW_, screenH_};
    SDL_RenderFillRect(renderer, &fullRect);


    // Draw all active QR codes
    for (const auto& code : activeCodes_) {
        const SDL_FRect qrRect = this->qrRect(code);
        const LoadedTexture& tex = textureFor(code.type);
        drawTexture(renderer, tex, qrRect);
    }

    const SDL_FRect deviceRect = phoneRect();
    drawTexture(renderer, phoneTexture_, deviceRect);

    const SDL_FRect cursorMarker = cursorRect();
    const Uint8 cursorAlpha = phase_ == Phase::Frozen ? 255 : 220;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, cursorAlpha);
    SDL_RenderFillRectF(renderer, &cursorMarker);

    const SDL_FRect horizontalBar{
        cursorMarker.x - 6.0f,
        cursorMarker.y + cursorMarker.h * 0.5f - 1.0f,
        cursorMarker.w + 12.0f,
        2.0f
    };
    const SDL_FRect verticalBar{
        cursorMarker.x + cursorMarker.w * 0.5f - 1.0f,
        cursorMarker.y - 6.0f,
        2.0f,
        cursorMarker.h + 12.0f
    };
    SDL_RenderFillRectF(renderer, &horizontalBar);
    SDL_RenderFillRectF(renderer, &verticalBar);

    const float progress = phase_ == Phase::Frozen ? survivedRatio_ : clamp01(activeTime_ / kSurvivalDurationSeconds);
    const SDL_FRect progressBg{
        static_cast<float>(screenW_) * 0.5f - kProgressBarWidth * 0.5f,
        34.0f,
        kProgressBarWidth,
        kProgressBarHeight
    };
    SDL_SetRenderDrawColor(renderer, 28, 34, 42, 220);
    SDL_RenderFillRectF(renderer, &progressBg);

    const SDL_FRect progressFill{
        progressBg.x,
        progressBg.y,
        progressBg.w * progress,
        progressBg.h
    };
    if (phase_ == Phase::Frozen) {
        SDL_SetRenderDrawColor(renderer, 232, 124, 124, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 142, 232, 184, 255);
    }
    SDL_RenderFillRectF(renderer, &progressFill);
}

bool QrCodeAttackPresentation::isComplete() const {
    return phase_ == Phase::Complete;
}

bool QrCodeAttackPresentation::shouldHideNonCasterCharacters() const {
    return false;
}

bool QrCodeAttackPresentation::shouldRenderAboveHud() const {
    return true;
}

float QrCodeAttackPresentation::getInputMultiplier() const {
    return damageMultiplier_;
}

PresentationFeedbackSignal QrCodeAttackPresentation::getFeedbackSignal() const {
    return PresentationFeedbackSignal::graded(1.0f - damageMultiplier_);
}

std::vector<PresentationFeedbackEvent> QrCodeAttackPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}

float QrCodeAttackPresentation::consumeHitDamageMultiplier() {
    return getInputMultiplier();
}

int QrCodeAttackPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int QrCodeAttackPresentation::consumeHitEvents() {
    const int hitEvents = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hitEvents;
}

int QrCodeAttackPresentation::getDamageLabelHitCount() const {
    return 1;
}

std::string QrCodeAttackPresentation::getInputResultText() const {
    const int survivedPercent = static_cast<int>(std::lround(survivedRatio_ * 100.0f));
    const int reductionPercent = static_cast<int>(std::lround((1.0f - damageMultiplier_) * 100.0f));
    if (damageMultiplier_ <= 0.0f) {
        return "Avoided every QR code for 5.0s. Damage reduced 100%.";
    }
    return "Avoided the scan for " + std::to_string(survivedPercent) +
        "% of the timer. Damage reduced " + std::to_string(reductionPercent) + "%.";
}

std::vector<PresentationAudioCommand> QrCodeAttackPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

std::string QrCodeAttackPresentation::resolvePath(const std::string& relativePath) {
    return platform::path::resolvePath(relativePath);
}

void QrCodeAttackPresentation::queueFeedbackEvent() {
    PresentationFeedbackEvent event;
    event.signal = PresentationFeedbackSignal::graded(1.0f - damageMultiplier_);
    event.multiplier = damageMultiplier_;
    event.comboEligible = true;
    pendingFeedbackEvents_.push_back(event);
}

void QrCodeAttackPresentation::ensureTexturesLoaded(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_ || renderer == nullptr) {
        return;
    }
    attemptedTextureLoad_ = true;

#ifdef BATTLE_ENABLE_IMAGE
    auto loadTexture = [renderer](const std::string& path, LoadedTexture& outTexture) {
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface == nullptr) {
            return;
        }

        outTexture.texture = SDL_CreateTextureFromSurface(renderer, surface);
        outTexture.width = surface->w;
        outTexture.height = surface->h;
        SDL_FreeSurface(surface);
    };

    loadTexture(resolvePath(kPhoneTexturePath), phoneTexture_);
    loadTexture(resolvePath(kWeixinTexturePath), weixinTexture_);
    loadTexture(resolvePath(kZhifuTexturePath), zhifuTexture_);
#else
    (void)renderer;
#endif
}

void QrCodeAttackPresentation::destroyTextures() {
    auto destroyTexture = [](LoadedTexture& texture) {
        if (texture.texture != nullptr) {
            SDL_DestroyTexture(texture.texture);
            texture.texture = nullptr;
        }
        texture.width = 0;
        texture.height = 0;
    };

    destroyTexture(phoneTexture_);
    destroyTexture(weixinTexture_);
    destroyTexture(zhifuTexture_);
}

void QrCodeAttackPresentation::spawnQrCode() {
    QrCodeInstance code;
    code.type = (spawnCount_ % 2 == 0) ? CodeType::Weixin : CodeType::Zhifu;
    ++spawnCount_;

    // Use fallback size for spawn area
    const float halfWidth = kQrFallbackWidth * 0.5f;
    const float halfHeight = kQrTargetHeight * 0.5f;
    constexpr float kMinSpawnDistance = 120.0f; // Minimum distance from cursor
    int maxTries = 16;
    float dist = 0.0f;
    do {
        code.x = randomFloat(halfWidth, std::max(halfWidth, static_cast<float>(screenW_) - halfWidth));
        code.y = randomFloat(halfHeight, std::max(halfHeight, static_cast<float>(screenH_) - halfHeight));
        float dx = code.x - cursorX_;
        float dy = code.y - cursorY_;
        dist = std::sqrt(dx * dx + dy * dy);
    } while (dist < kMinSpawnDistance && --maxTries > 0);
    activeCodes_.push_back(code);
}

void QrCodeAttackPresentation::updateCursor(float deltaTime) {
    const Uint8* keyState = SDL_GetKeyboardState(nullptr);
    float moveX = 0.0f;
    float moveY = 0.0f;
    if (keyState[SDL_SCANCODE_LEFT]) {
        moveX -= 1.0f;
    }
    if (keyState[SDL_SCANCODE_RIGHT]) {
        moveX += 1.0f;
    }
    if (keyState[SDL_SCANCODE_UP]) {
        moveY -= 1.0f;
    }
    if (keyState[SDL_SCANCODE_DOWN]) {
        moveY += 1.0f;
    }

    const float length = std::sqrt(moveX * moveX + moveY * moveY);
    if (length > 0.001f) {
        moveX /= length;
        moveY /= length;
    }

    cursorX_ += moveX * kPhoneMoveSpeedPixels * deltaTime;
    cursorY_ += moveY * kPhoneMoveSpeedPixels * deltaTime;
    cursorX_ = std::clamp(cursorX_, 0.0f, static_cast<float>(screenW_));
    cursorY_ = std::clamp(cursorY_, 0.0f, static_cast<float>(screenH_));
}

void QrCodeAttackPresentation::updateQrCodes(float deltaTime) {
    for (auto& code : activeCodes_) {
        const float deltaX = cursorX_ - code.x;
        const float deltaY = cursorY_ - code.y;
        const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        if (distance > 0.001f) {
            const float step = std::min(distance, kQrChaseSpeedPixels * deltaTime);
            code.x += (deltaX / distance) * step;
            code.y += (deltaY / distance) * step;
        }
    }
}


bool QrCodeAttackPresentation::cursorTouchesAnyQr() const {
    for (const auto& code : activeCodes_) {
        if (rectsOverlap(cursorRect(), qrScanRect(code))) {
            return true;
        }
    }
    return false;
}

SDL_FRect QrCodeAttackPresentation::phoneRect() const {
    const float textureWidth = phoneTexture_.width > 0 ? static_cast<float>(phoneTexture_.width) : kPhoneFallbackWidth;
    const float textureHeight = phoneTexture_.height > 0 ? static_cast<float>(phoneTexture_.height) : kPhoneTargetHeight;
    const float scale = kPhoneTargetHeight / std::max(1.0f, textureHeight);
    const float width = textureWidth * scale;
    return centeredRect(cursorX_, cursorY_, width, kPhoneTargetHeight);
}


SDL_FRect QrCodeAttackPresentation::qrRect(const QrCodeInstance& code) const {
    const LoadedTexture& texture = code.type == CodeType::Weixin ? weixinTexture_ : zhifuTexture_;
    const float textureWidth = texture.width > 0 ? static_cast<float>(texture.width) : kQrFallbackWidth;
    const float textureHeight = texture.height > 0 ? static_cast<float>(texture.height) : kQrTargetHeight;
    const float scale = kQrTargetHeight / std::max(1.0f, textureHeight);
    const float width = textureWidth * scale;
    return centeredRect(code.x, code.y, width, kQrTargetHeight);
}

SDL_FRect QrCodeAttackPresentation::qrScanRect(const QrCodeInstance& code) const {
    const SDL_FRect codeRect = qrRect(code);
    return SDL_FRect{
        codeRect.x + codeRect.w * (1.0f - kQrScanWidthRatio) * 0.5f,
        codeRect.y + codeRect.h * (1.0f - kQrScanHeightRatio) * 0.5f,
        codeRect.w * kQrScanWidthRatio,
        codeRect.h * kQrScanHeightRatio
    };
}
const QrCodeAttackPresentation::LoadedTexture& QrCodeAttackPresentation::textureFor(CodeType type) const {
    return (type == CodeType::Weixin) ? weixinTexture_ : zhifuTexture_;
}

SDL_FRect QrCodeAttackPresentation::cursorRect() const {
    return centeredRect(cursorX_, cursorY_, kCursorSizePixels, kCursorSizePixels);
}

void QrCodeAttackPresentation::drawTexture(SDL_Renderer* renderer, const LoadedTexture& texture, const SDL_FRect& rect) const {
    if (texture.texture != nullptr) {
        SDL_SetTextureBlendMode(texture.texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(texture.texture, 255);
        SDL_RenderCopyExF(renderer, texture.texture, nullptr, &rect, 0.0, nullptr, SDL_FLIP_NONE);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderFillRectF(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderDrawRectF(renderer, &rect);
}

void QrCodeAttackPresentation::queueAudioCommand(PresentationAudioCommandType type,
                                                 const std::string& id,
                                                 float volume) {
    pendingAudioCommands_.push_back(PresentationAudioCommand{type, id, volume});
}

} // namespace battle
