#include "qr_code_shield_presentation.h"
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
constexpr float kMaxDurationSeconds = 3.0f;
constexpr float kPhoneMoveSpeedPixels = 680.0f;
constexpr float kQrTargetHeight = 220.0f;
constexpr float kQrFallbackWidth = 220.0f;
constexpr float kPhoneTargetHeight = 330.0f;
constexpr float kPhoneFallbackWidth = 250.0f;
constexpr float kCursorSizePixels = 6.0f;
constexpr float kQrScanWidthRatio = 0.42f;
constexpr float kQrScanHeightRatio = 0.42f;
constexpr float kBackdropAlpha = 92.0f;
constexpr float kProgressBarWidth = 340.0f;
constexpr float kProgressBarHeight = 10.0f;
constexpr char kPhoneTexturePath[] = "assets/combat/presentations/wechatalipay/huaweisanzhedian.png";
constexpr char kQrTexturePath[] = "assets/combat/presentations/wechatalipay/weixin.png";
float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
float randomFloat(float minValue, float maxValue) {
    const float unit = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minValue + (maxValue - minValue) * unit;
}
SDL_FRect centeredRect(float centerX, float centerY, float width, float height) {
    return SDL_FRect{ centerX - width * 0.5f, centerY - height * 0.5f, width, height };
}
bool rectsOverlap(const SDL_FRect& lhs, const SDL_FRect& rhs) {
    return lhs.x < rhs.x + rhs.w && lhs.x + lhs.w > rhs.x && lhs.y < rhs.y + rhs.h && lhs.y + lhs.h > rhs.y;
}
} // namespace

QrCodeShieldPresentation::QrCodeShieldPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ)
    : casterX_(casterWorldX), casterY_(casterWorldY), casterZ_(casterWorldZ),
      targetX_(targetWorldX), targetY_(targetWorldY), targetZ_(targetWorldZ) {}

QrCodeShieldPresentation::~QrCodeShieldPresentation() { destroyTextures(); }

void QrCodeShieldPresentation::start() {
    phase_ = Phase::Active;
    activeTime_ = 0.0f;
    shieldMultiplier_ = 0.0f;
    cursorX_ = static_cast<float>(screenW_) * 0.5f;
    cursorY_ = static_cast<float>(screenH_) * 0.58f;
    pendingAudioCommands_.clear();
    pendingFeedbackEvents_.clear();
    spawnQrCode();
}

void QrCodeShieldPresentation::update(float deltaTime) {
    if (phase_ == Phase::Complete) return;
    activeTime_ += deltaTime;
    updateCursor(deltaTime);
    if (cursorTouchesQr()) {
        // The faster, the higher the multiplier
        shieldMultiplier_ = 1.0f - clamp01(activeTime_ / kMaxDurationSeconds);
        queueFeedbackEvent();
        phase_ = Phase::Complete;
        return;
    }
    if (activeTime_ >= kMaxDurationSeconds) {
        shieldMultiplier_ = 0.0f;
        queueFeedbackEvent();
        phase_ = Phase::Complete;
    }
}

void QrCodeShieldPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D&) {
    screenW_ = std::max(1, screenW);
    screenH_ = std::max(1, screenH);
    ensureTexturesLoaded(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 12, 18, static_cast<Uint8>(kBackdropAlpha));
    SDL_Rect fullRect{0, 0, screenW_, screenH_};
    SDL_RenderFillRect(renderer, &fullRect);
    drawTexture(renderer, qrTexture_, qrRect());
    drawTexture(renderer, phoneTexture_, phoneRect());
    const SDL_FRect cursorMarker = cursorRect();
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
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
    const float progress = clamp01(activeTime_ / kMaxDurationSeconds);
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
        progressBg.w * (1.0f - progress),
        progressBg.h
    };
    SDL_SetRenderDrawColor(renderer, 142, 232, 184, 255);
    SDL_RenderFillRectF(renderer, &progressFill);
}

bool QrCodeShieldPresentation::isComplete() const { return phase_ == Phase::Complete; }

float QrCodeShieldPresentation::getInputMultiplier() const { return shieldMultiplier_; }
PresentationFeedbackSignal QrCodeShieldPresentation::getFeedbackSignal() const {
    return PresentationFeedbackSignal::graded(shieldMultiplier_);
}
std::vector<PresentationFeedbackEvent> QrCodeShieldPresentation::consumeFeedbackEvents() {
    std::vector<PresentationFeedbackEvent> events;
    events.swap(pendingFeedbackEvents_);
    return events;
}
float QrCodeShieldPresentation::consumeHitDamageMultiplier() { return getInputMultiplier(); }
int QrCodeShieldPresentation::consumeAbilityAudioCues() { return 0; }
int QrCodeShieldPresentation::consumeHitEvents() { return 0; }
std::string QrCodeShieldPresentation::getInputResultText() const {
    int percent = static_cast<int>(std::lround(shieldMultiplier_ * 100.0f));
    if (shieldMultiplier_ <= 0.0f) return "Missed the QR code. No shield granted.";
    return "Scanned QR code! Shield granted: " + std::to_string(percent) + "%";
}
std::vector<PresentationAudioCommand> QrCodeShieldPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}
std::string QrCodeShieldPresentation::resolvePath(const std::string& relativePath) {
    return platform::path::resolvePath(relativePath);
}
void QrCodeShieldPresentation::queueFeedbackEvent() {
    PresentationFeedbackEvent event;
    event.signal = PresentationFeedbackSignal::graded(shieldMultiplier_);
    event.multiplier = shieldMultiplier_;
    event.comboEligible = true;
    pendingFeedbackEvents_.push_back(event);
}
void QrCodeShieldPresentation::ensureTexturesLoaded(SDL_Renderer* renderer) {
    if (attemptedTextureLoad_ || renderer == nullptr) return;
    attemptedTextureLoad_ = true;
#ifdef BATTLE_ENABLE_IMAGE
    auto loadTexture = [renderer](const std::string& path, LoadedTexture& outTexture) {
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface == nullptr) return;
        outTexture.texture = SDL_CreateTextureFromSurface(renderer, surface);
        outTexture.width = surface->w;
        outTexture.height = surface->h;
        SDL_FreeSurface(surface);
    };
    loadTexture(resolvePath(kPhoneTexturePath), phoneTexture_);
    loadTexture(resolvePath(kQrTexturePath), qrTexture_);
#else
    (void)renderer;
#endif
}
void QrCodeShieldPresentation::destroyTextures() {
    auto destroyTexture = [](LoadedTexture& texture) {
        if (texture.texture != nullptr) {
            SDL_DestroyTexture(texture.texture);
            texture.texture = nullptr;
        }
        texture.width = 0;
        texture.height = 0;
    };
    destroyTexture(phoneTexture_);
    destroyTexture(qrTexture_);
}
void QrCodeShieldPresentation::spawnQrCode() {
    // Spawn a single QR code at a random position
    const float halfWidth = kQrFallbackWidth * 0.5f;
    const float halfHeight = kQrTargetHeight * 0.5f;
    qr_.x = randomFloat(halfWidth, std::max(halfWidth, static_cast<float>(screenW_) - halfWidth));
    qr_.y = randomFloat(halfHeight, std::max(halfHeight, static_cast<float>(screenH_) - halfHeight));
}
void QrCodeShieldPresentation::updateCursor(float deltaTime) {
    const Uint8* keyState = SDL_GetKeyboardState(nullptr);
    float moveX = 0.0f, moveY = 0.0f;
    if (keyState[SDL_SCANCODE_LEFT]) moveX -= 1.0f;
    if (keyState[SDL_SCANCODE_RIGHT]) moveX += 1.0f;
    if (keyState[SDL_SCANCODE_UP]) moveY -= 1.0f;
    if (keyState[SDL_SCANCODE_DOWN]) moveY += 1.0f;
    const float length = std::sqrt(moveX * moveX + moveY * moveY);
    if (length > 0.001f) { moveX /= length; moveY /= length; }
    cursorX_ += moveX * kPhoneMoveSpeedPixels * deltaTime;
    cursorY_ += moveY * kPhoneMoveSpeedPixels * deltaTime;
    cursorX_ = std::clamp(cursorX_, 0.0f, static_cast<float>(screenW_));
    cursorY_ = std::clamp(cursorY_, 0.0f, static_cast<float>(screenH_));
}
bool QrCodeShieldPresentation::cursorTouchesQr() const {
    return rectsOverlap(cursorRect(), qrScanRect());
}
SDL_FRect QrCodeShieldPresentation::phoneRect() const {
    const float textureWidth = phoneTexture_.width > 0 ? static_cast<float>(phoneTexture_.width) : kPhoneFallbackWidth;
    const float textureHeight = phoneTexture_.height > 0 ? static_cast<float>(phoneTexture_.height) : kPhoneTargetHeight;
    const float scale = kPhoneTargetHeight / std::max(1.0f, textureHeight);
    const float width = textureWidth * scale;
    return centeredRect(cursorX_, cursorY_, width, kPhoneTargetHeight);
}
SDL_FRect QrCodeShieldPresentation::qrRect() const {
    const float textureWidth = qrTexture_.width > 0 ? static_cast<float>(qrTexture_.width) : kQrFallbackWidth;
    const float textureHeight = qrTexture_.height > 0 ? static_cast<float>(qrTexture_.height) : kQrTargetHeight;
    const float scale = kQrTargetHeight / std::max(1.0f, textureHeight);
    const float width = textureWidth * scale;
    return centeredRect(qr_.x, qr_.y, width, kQrTargetHeight);
}
SDL_FRect QrCodeShieldPresentation::qrScanRect() const {
    const SDL_FRect codeRect = qrRect();
    return SDL_FRect{
        codeRect.x + codeRect.w * (1.0f - kQrScanWidthRatio) * 0.5f,
        codeRect.y + codeRect.h * (1.0f - kQrScanHeightRatio) * 0.5f,
        codeRect.w * kQrScanWidthRatio,
        codeRect.h * kQrScanHeightRatio
    };
}
SDL_FRect QrCodeShieldPresentation::cursorRect() const {
    return centeredRect(cursorX_, cursorY_, kCursorSizePixels, kCursorSizePixels);
}
void QrCodeShieldPresentation::drawTexture(SDL_Renderer* renderer, const LoadedTexture& texture, const SDL_FRect& rect) const {
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

} // namespace battle
