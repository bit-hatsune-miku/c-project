#pragma once

#include "ability_presentation.h"

#include <string>
#include <vector>

namespace battle {

class QrCodeAttackPresentation : public AbilityPresentation {
public:
    QrCodeAttackPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~QrCodeAttackPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    void setTuningProfile(const PresentationTuningProfile& profile) override;

    bool shouldHideNonCasterCharacters() const override;
    bool shouldRenderAboveHud() const override;

    float getInputMultiplier() const override;
    PresentationFeedbackSignal getFeedbackSignal() const override;
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    float consumeHitDamageMultiplier() override;
    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;
    std::string getInputResultText() const override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;

private:
    enum class Phase {
        Active,
        Frozen,
        Complete
    };

    enum class CodeType {
        Weixin,
        Zhifu
    };

    struct QrCodeInstance {
        CodeType type = CodeType::Weixin;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct LoadedTexture {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

    static std::string resolvePath(const std::string& relativePath);

    void ensureTexturesLoaded(SDL_Renderer* renderer);
    void destroyTextures();
    void spawnQrCode();
    void updateCursor(float deltaTime);
    void updateQrCodes(float deltaTime);
    bool cursorTouchesAnyQr() const;
    SDL_FRect phoneRect() const;
    SDL_FRect qrRect(const QrCodeInstance& code) const;
    SDL_FRect qrScanRect(const QrCodeInstance& code) const;
    SDL_FRect cursorRect() const;
    void drawTexture(SDL_Renderer* renderer, const LoadedTexture& texture, const SDL_FRect& rect) const;
    void queueAudioCommand(PresentationAudioCommandType type, const std::string& id, float volume = 1.0f);
    void queueFeedbackEvent();
    const LoadedTexture& textureFor(CodeType type) const;
    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    LoadedTexture phoneTexture_{};
    LoadedTexture weixinTexture_{};
    LoadedTexture zhifuTexture_{};
    bool attemptedTextureLoad_ = false;

    Phase phase_ = Phase::Active;
    std::vector<QrCodeInstance> activeCodes_;
    int spawnCount_ = 0;
    float activeTime_ = 0.0f;
    float freezeTime_ = 0.0f;
    float nextSpawnTime_ = 0.0f;
    float cursorX_ = 0.0f;
    float cursorY_ = 0.0f;
    float damageMultiplier_ = 1.0f;
    float survivedRatio_ = 0.0f;
    int pendingAbilityAudioCues_ = 0;
    int pendingHitEvents_ = 0;
    bool hitQueued_ = false;
    int screenW_ = 1280;
    int screenH_ = 720;
    int initialSpawnCount_ = 2;
    float survivalDurationSeconds_ = 5.0f;
    float freezeDurationSeconds_ = 2.0f;
    float qrRespawnIntervalSeconds_ = 1.5f;
    float qrChaseSpeedPixels_ = 320.0f;
    float qrScanWidthRatio_ = 0.42f;
    float qrScanHeightRatio_ = 0.42f;

    std::vector<PresentationAudioCommand> pendingAudioCommands_;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
};

} // namespace battle
