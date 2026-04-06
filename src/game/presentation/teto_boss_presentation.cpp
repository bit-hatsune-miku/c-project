#include "teto_boss_presentation.h"
#include "../../platform/path_resolution.h"
#include "../render/battle_asset_loading.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <random>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL_image.h>
#endif

namespace battle {

namespace {
constexpr float kIntroDurationSeconds = 1.0f;
constexpr float kCompleteDurationSeconds = 1.0f;
constexpr float kBaseDamageMultiplier = 1.0f;
constexpr float kAdditionalHitDamageMultiplier = 0.5f;
constexpr float kCollisionStartProgress = 0.45f;
constexpr float kCollisionEndProgress = 0.55f;

// Camera tuning adapted from Jiafei
constexpr float kFocusOffsetX = 0.0f;
constexpr float kFocusOffsetY = 340.0f;
constexpr float kFocusOffsetZ = -125.0f;
constexpr float kFocusYawDegrees = 180.0f;
constexpr float kFocusPitchDegrees = -1.3f;
constexpr float kFocusFocalLength = 47000.0f;
constexpr float kFocusPosLerp = 0.35f;
constexpr float kFocusRotLerp = 0.30f;
constexpr float kFocusFocalLerp = 0.25f;

constexpr float kCharacterSpriteHeight = 260.0f;
constexpr float kFocusedCharacterScale = 0.6f; // "reduce the size of the sprite target"

constexpr float kLaneSpacing = 80.0f;

constexpr int kBaguetteWidth = 140;
constexpr int kBaguetteHeight = 70;

std::mt19937 g_rng(std::random_device{}());

} // namespace

TetoBossPresentation::TetoBossPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) : casterX_(casterWorldX), casterY_(casterWorldY), casterZ_(casterWorldZ),
    targetX_(targetWorldX), targetY_(targetWorldY), targetZ_(targetWorldZ) {
    
    // Choose a random lane
    std::uniform_int_distribution<int> laneDist(0, 2);
    playerLane_ = 0; // Default placement is lowest layer
    currentLaneOffset_ = -kLaneSpacing;
}

TetoBossPresentation::~TetoBossPresentation() {
    if (baguetteTexture_) {
        SDL_DestroyTexture(baguetteTexture_);
    }
    if (targetIconTexture_) {
        SDL_DestroyTexture(targetIconTexture_);
    }
}

void TetoBossPresentation::start() {
    phase_ = Phase::Intro;
    survivalElapsed_ = 0.0f;
    spawnTimer_ = 0.0f;
    baguettes_.clear();
    damageMultiplier_ = kBaseDamageMultiplier;
    landedHitCount_ = 0;
    pendingHitDamageMultipliers_.clear();
    
    if (focusedPartyIndex_ >= 0) {
        // Find a way to randomly select an ally... Actually, BattleSession should set targetPartyIndex if it was resolved
    }
}

void TetoBossPresentation::setTuningProfile(const PresentationTuningProfile& profile) {
    if (profile.floatParams.count("survivalDurationSeconds")) {
        survivalDurationSeconds_ = profile.floatParams.at("survivalDurationSeconds");
    }
    if (profile.floatParams.count("baseBaguetteSpeed")) {
        baseBaguetteSpeed_ = profile.floatParams.at("baseBaguetteSpeed");
    }
    if (profile.floatParams.count("baseSpawnInterval")) {
        baseSpawnInterval_ = profile.floatParams.at("baseSpawnInterval");
    }
}

void TetoBossPresentation::update(float deltaTime) {
    float targetLaneOffset = (playerLane_ - 1) * kLaneSpacing;
    currentLaneOffset_ += (targetLaneOffset - currentLaneOffset_) * 15.0f * deltaTime;

    if (phase_ == Phase::Intro) {
        survivalElapsed_ += deltaTime;
        if (survivalElapsed_ >= kIntroDurationSeconds) {
            phase_ = Phase::Attack;
            survivalElapsed_ = 0.0f;
        }
        return;
    }

    if (phase_ == Phase::Attack) {
        survivalElapsed_ += deltaTime;
        
        // Math to make phase 3 difficult:
        // Spawn interval gets smaller (faster spawns)
        // Baguette speed gets larger
        
        spawnTimer_ += deltaTime;
        if (spawnTimer_ >= baseSpawnInterval_) {
            spawnTimer_ = 0.0f;
            spawnBaguette();
        }
        
        for (auto it = baguettes_.begin(); it != baguettes_.end();) {
            // Speed could increase over time during survival
            float speedMultiplier = 1.0f + (survivalElapsed_ / survivalDurationSeconds_) * 0.5f;
            float speed = baseBaguetteSpeed_ * speedMultiplier * deltaTime;
            
            // Abstract progress assuming screen width mapping
            float progressDelta = speed / 1280.0f; // Roughly 1 screen width
            it->progress += progressDelta;
            
            // Hit detection
            if (it->progress > kCollisionStartProgress && it->progress < kCollisionEndProgress) {
                if (it->lane == playerLane_) {
                    ++landedHitCount_;
                    damageMultiplier_ =
                        kBaseDamageMultiplier +
                        (static_cast<float>(landedHitCount_) * kAdditionalHitDamageMultiplier);
                    queueDamageHit(landedHitCount_ == 1 ? damageMultiplier_ : kAdditionalHitDamageMultiplier);
                    PresentationFeedbackEvent event;
                    event.signal = PresentationFeedbackSignal::binary(false);
                    pendingFeedbackEvents_.push_back(event);
                    it = baguettes_.erase(it); // Destroy on hit
                    continue;
                }
            }
            
            if (it->progress > 1.0f) {
                PresentationFeedbackEvent event;
                event.signal = PresentationFeedbackSignal::binary(true);
                pendingFeedbackEvents_.push_back(event);
                it = baguettes_.erase(it);
            } else {
                ++it;
            }
        }
        
        if (survivalElapsed_ >= survivalDurationSeconds_ && baguettes_.empty()) {
            phase_ = Phase::Complete;
            survivalElapsed_ = 0.0f;
            if (landedHitCount_ == 0) {
                damageMultiplier_ = kBaseDamageMultiplier;
                queueDamageHit(kBaseDamageMultiplier);
            }
        }
        return;
    }

    if (phase_ == Phase::Complete) {
        survivalElapsed_ += deltaTime;
    }
}

void TetoBossPresentation::spawnBaguette() {
    std::uniform_int_distribution<int> laneDist(0, 2);
    std::uniform_int_distribution<int> dirDist(0, 1);
    
    Baguette b;
    b.lane = laneDist(g_rng);
    b.fromRight = dirDist(g_rng) == 1;
    b.progress = 0.0f;
    
    baguettes_.push_back(b);
}

void TetoBossPresentation::queueDamageHit(float multiplier) {
    pendingHitDamageMultipliers_.push_back(std::max(0.0f, multiplier));
}

void TetoBossPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    ensureTexturesLoaded(renderer);
    
    if (phase_ != Phase::Attack) return;
    
    const SDL_FPoint targetFeet = camera.worldToScreen(targetX_, targetY_, targetZ_);
    const float targetScale = std::max(0.0001f, camera.getPerspectiveScale(targetX_, targetY_, targetZ_));
    const float chestY = targetFeet.y - (kCharacterSpriteHeight * targetScale * kFocusedCharacterScale) * 0.5f;

    // Draw the target sprite manually since we want to overwrite its scale.
    if (targetIconTexture_ != nullptr) {
        int texW = 0;
        int texH = 0;
        SDL_QueryTexture(targetIconTexture_, nullptr, nullptr, &texW, &texH);
        if (texW > 0 && texH > 0) {
            // Give icon a fixed screen size matched to the baguette logic
            const float targetDrawH = kBaguetteHeight * 1.5f;
            const float targetDrawW = targetDrawH * (static_cast<float>(texW) / static_cast<float>(texH));
            
            float targetDrawY = chestY - currentLaneOffset_;

            SDL_Rect targetRect = {
                static_cast<int>(targetFeet.x - targetDrawW * 0.5f),
                static_cast<int>(targetDrawY - targetDrawH * 0.5f),
                static_cast<int>(targetDrawW),
                static_cast<int>(targetDrawH)
            };
            SDL_SetTextureBlendMode(targetIconTexture_, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(targetIconTexture_, 255, 255, 255);
            SDL_SetTextureAlphaMod(targetIconTexture_, 255);
            SDL_RenderCopy(renderer, targetIconTexture_, nullptr, &targetRect);
        }
    }
    
    for (const auto& b : baguettes_) {
        float x = 0.0f;
        if (b.fromRight) {
            x = screenW - (b.progress * screenW);
        } else {
            x = b.progress * screenW;
        }
        
        float y = chestY - (b.lane - 1) * kLaneSpacing;
        
        SDL_Rect destRender = {
            static_cast<int>(x - kBaguetteWidth/2),
            static_cast<int>(y - kBaguetteHeight/2),
            kBaguetteWidth,
            kBaguetteHeight
        };
        
        if (baguetteTexture_) {
            SDL_RendererFlip flip = b.fromRight ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_RenderCopyEx(renderer, baguetteTexture_, nullptr, &destRender, 0.0, nullptr, flip);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
            SDL_RenderFillRect(renderer, &destRender);
        }
    }
}

void TetoBossPresentation::renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    (void)renderer; (void)screenW; (void)screenH; (void)camera;
}

bool TetoBossPresentation::isComplete() const {
    return phase_ == Phase::Complete &&
        survivalElapsed_ >= kCompleteDurationSeconds &&
        pendingHitDamageMultipliers_.empty();
}

bool TetoBossPresentation::onKeyPressed(SDL_Keycode key) {
    if (phase_ != Phase::Attack) return false;
    
    if (key == SDLK_UP) {
        playerLane_ = std::min(2, playerLane_ + 1);
        return true;
    } else if (key == SDLK_DOWN) {
        playerLane_ = std::max(0, playerLane_ - 1);
        return true;
    }
    return false;
}

bool TetoBossPresentation::shouldHideNonCasterCharacters() const {
    return true; // Match jiafei
}

bool TetoBossPresentation::shouldRenderCasterEntity() const {
    return true;
}

bool TetoBossPresentation::shouldRenderBossEntity() const {
    return true; // Match jiafei
}

bool TetoBossPresentation::shouldRenderFocusedTargetEntity() const {
    return phase_ != Phase::Attack;
}

bool TetoBossPresentation::shouldRenderAboveHud() const {
    return false;
}

bool TetoBossPresentation::overridesCamera() const {
    return true;
}

void TetoBossPresentation::applyCameraState(Camera3D& camera) const {
    // Match Jiafei camera
    const bool hasFocus = focusedPartyIndex_ >= 0;
    const float focusX = hasFocus ? targetX_ : (casterX_ + targetX_) * 0.5f;
    const float focusY = hasFocus ? targetY_ : (casterY_ + targetY_) * 0.5f;
    const float focusZ = hasFocus ? targetZ_ : (casterZ_ + targetZ_) * 0.5f;

    const float desiredX = focusX + kFocusOffsetX;
    const float desiredY = focusY + kFocusOffsetY;
    const float desiredZ = focusZ + kFocusOffsetZ;

    const float posLerp = hasFocus ? kFocusPosLerp : 0.25f;
    camera.posX += (desiredX - camera.posX) * posLerp;
    camera.posY += (desiredY - camera.posY) * posLerp;
    camera.posZ += (desiredZ - camera.posZ) * posLerp;

    camera.yawDegrees += (kFocusYawDegrees - camera.yawDegrees) * kFocusRotLerp;
    camera.pitchDegrees += (kFocusPitchDegrees - camera.pitchDegrees) * kFocusRotLerp;
    camera.focalLength += (kFocusFocalLength - camera.focalLength) * kFocusFocalLerp;
}

bool TetoBossPresentation::getTargetWorldOverride(float& outX, float& outY, float& outZ) const {
    if (focusedPartyIndex_ < 0 || phase_ != Phase::Attack) return false;
    
    // Apply lane offset vertically
    outX = targetX_;
    outY = targetY_ - (playerLane_ - 1) * kLaneSpacing; // Offset Y, invert direction if needed
    outZ = targetZ_;
    return true;
}

int TetoBossPresentation::getFocusedPartyIndex() const {
    return focusedPartyIndex_;
}

void TetoBossPresentation::setTargetPartyIndex(int index) {
    focusedPartyIndex_ = index;
}

void TetoBossPresentation::setTargetWorldPosition(float x, float y, float z) {
    targetX_ = x;
    targetY_ = y;
    targetZ_ = z;
}

void TetoBossPresentation::setPartyAssetNames(const std::vector<std::string>& assetNames) {
    if (focusedPartyIndex_ >= 0 && focusedPartyIndex_ < static_cast<int>(assetNames.size())) {
        targetAssetName_ = assetNames[focusedPartyIndex_];
    }
}

float TetoBossPresentation::getInputMultiplier() const {
    return damageMultiplier_;
}

PresentationFeedbackSignal TetoBossPresentation::getFeedbackSignal() const {
    return PresentationFeedbackSignal::binary(false);
}

std::vector<PresentationFeedbackEvent> TetoBossPresentation::consumeFeedbackEvents() {
    auto events = pendingFeedbackEvents_;
    pendingFeedbackEvents_.clear();
    return events;
}

float TetoBossPresentation::consumeHitDamageMultiplier() {
    if (pendingHitDamageMultipliers_.empty()) {
        return damageMultiplier_;
    }

    const float multiplier = pendingHitDamageMultipliers_.front();
    pendingHitDamageMultipliers_.erase(pendingHitDamageMultipliers_.begin());
    return multiplier;
}

int TetoBossPresentation::consumeAbilityAudioCues() {
    int c = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return c;
}

int TetoBossPresentation::consumeHitEvents() {
    return pendingHitDamageMultipliers_.empty() ? 0 : 1;
}

int TetoBossPresentation::getDamageLabelHitCount() const {
    return 1;
}

std::string TetoBossPresentation::getInputResultText() const {
    return "";
}

std::optional<SplashArtConfig> TetoBossPresentation::getSplashConfig(SDL_Texture* sprite) const {
    (void)sprite;
    return std::nullopt;
}

void TetoBossPresentation::ensureTexturesLoaded(SDL_Renderer* renderer) {
    if (texturesLoaded_) return;

    std::string baguettePath = platform::path::resolvePath("assets/combat/presentations/tetoBoss/eat.png");

#ifdef BATTLE_ENABLE_IMAGE
    if (std::filesystem::exists(baguettePath)) {
        SDL_Surface* surf = IMG_Load(baguettePath.c_str());
        if (surf) {
            baguetteTexture_ = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
        }
    }
#endif

    if (!targetAssetName_.empty() && !targetIconTexture_) {
        if (auto tex = battle::render::tryLoadCombatIconTexture(renderer, targetAssetName_)) {
            targetIconTexture_ = *tex;
        }
    }

    texturesLoaded_ = true;
}

SDL_Texture* TetoBossPresentation::loadTextureFallback(SDL_Renderer* renderer, const std::string& path) const {
    (void)renderer;
    (void)path;
    return nullptr;
}

} // namespace battle
