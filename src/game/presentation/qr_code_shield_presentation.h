#pragma once

#include "ability_presentation.h"
#include <string>
#include <vector>

namespace battle {

class QrCodeShieldPresentation : public AbilityPresentation {
public:
    QrCodeShieldPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );
    ~QrCodeShieldPresentation() override;

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    bool shouldHideNonCasterCharacters() const override { return false; }
    bool shouldRenderAboveHud() const override { return true; }

    float getInputMultiplier() const override;
    float consumeHitDamageMultiplier() override;
    int consumeAbilityAudioCues() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override { return 1; }
    std::string getInputResultText() const override;
    std::vector<PresentationAudioCommand> consumeAudioCommands() override;

private:
    enum class Phase {
        Active,
        Complete
    };

    struct QrCodeInstance {
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
    bool cursorTouchesQr() const;
    SDL_FRect phoneRect() const;
    SDL_FRect qrRect() const;
    SDL_FRect qrScanRect() const;
    SDL_FRect cursorRect() const;
    void drawTexture(SDL_Renderer* renderer, const LoadedTexture& texture, const SDL_FRect& rect) const;

    float casterX_ = 0.0f;
    float casterY_ = 0.0f;
    float casterZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    LoadedTexture phoneTexture_{};
    LoadedTexture qrTexture_{};
    bool attemptedTextureLoad_ = false;

    Phase phase_ = Phase::Active;
    QrCodeInstance qr_{};
    float activeTime_ = 0.0f;
    float cursorX_ = 0.0f;
    float cursorY_ = 0.0f;
    float shieldMultiplier_ = 0.0f;
    int screenW_ = 1280;
    int screenH_ = 720;
    std::vector<PresentationAudioCommand> pendingAudioCommands_;
};

} // namespace battle
