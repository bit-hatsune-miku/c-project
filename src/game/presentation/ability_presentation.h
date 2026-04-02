#ifndef ABILITY_PRESENTATION_H
#define ABILITY_PRESENTATION_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "splash_art_animation.h"

#include <SDL2/SDL.h>

#include "../core/combat_feedback.h"
#include "../core/presentation_tuning_profile.h"
#include "../render/camera_3d.h"

namespace battle {

struct CameraKeyframe {
    float time = 0.0f;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float pitchDegrees = 0.0f;
    float yawDegrees = 0.0f;
    float focalLength = 50000.0f;
};

struct InputWindow {
    float startTime = 0.0f;
    float endTime = 0.0f;
    bool active = false;
};

enum class PresentationAudioCommandType {
    PlayOneShot,
    PlayOneShotAllowOverlap,
    StartLoop,
    StopLoop,
    StopAllSfx,
    PauseBgm,
    ResumeBgm
};

struct PresentationAudioCommand {
    PresentationAudioCommandType type = PresentationAudioCommandType::PlayOneShot;
    std::string id;
    float volume = 1.0f;
};

struct ProjectileParticle {
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
    float lifetime = 0.0f;
    float maxLifetime = 1.0f;
    SDL_Color color{255, 255, 255, 255};
    float size = 20.0f;
    bool active = true;
    bool hitRegistered = false;
};

class AbilityPresentation {
public:
    virtual ~AbilityPresentation() = default;

    virtual void start() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) = 0;
    virtual void preload(SDL_Renderer* renderer) {
        (void)renderer;
    }
    virtual void renderBelowWorld(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
        (void)renderer;
        (void)screenW;
        (void)screenH;
        (void)camera;
    }
    virtual bool isComplete() const = 0;
    
    // Input handling
    virtual void onSpacePressed() {}
    virtual void onKeyPressed(SDL_Keycode key) {}
    virtual float getInputMultiplier() const { return 1.0f; }
    virtual float consumeHitDamageMultiplier() { return getInputMultiplier(); }
    virtual PresentationFeedbackSignal getFeedbackSignal() const { return {}; }
    virtual std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() { return {}; }
    virtual std::string getInputResultText() const { return {}; }
    virtual int getCorrectToneCount() const { return 0; }
    virtual int consumeAbilityAudioCues() { return 0; }
    virtual int consumeHitEvents() { return 0; }
    virtual int getDamageLabelHitCount() const { return 1; }
    virtual std::vector<PresentationAudioCommand> consumeAudioCommands() { return {}; }

    // Focused target (optional)
    virtual int getFocusedPartyIndex() const { return -1; }

    // Camera control
    virtual bool overridesCamera() const { return false; }
    virtual void applyCameraState(Camera3D& camera) const {}
    virtual bool getCasterWorldOverride(float& outX, float& outY, float& outZ) const {
        (void)outX;
        (void)outY;
        (void)outZ;
        return false;
    }
    virtual bool getTargetWorldOverride(float& outX, float& outY, float& outZ) const {
        (void)outX;
        (void)outY;
        (void)outZ;
        return false;
    }
    virtual void setTargetPartyIndex(int index) {
        (void)index;
    }
    virtual void setTargetWorldPosition(float x, float y, float z) {
        (void)x;
        (void)y;
        (void)z;
    }
    virtual void setPresentationValue(int value) {
        (void)value;
    }
    virtual void setTuningProfile(const PresentationTuningProfile& profile) {
        (void)profile;
    }
    virtual void setPartyAssetNames(const std::vector<std::string>& assetNames) {
        (void)assetNames;
    }
    virtual void setExternalTextures(SDL_Texture* caster, SDL_Texture* target) {
        (void)caster;
        (void)target;
    }
    virtual void setOverlayTextures(SDL_Texture* caster, SDL_Texture* target) {
        (void)caster;
        (void)target;
    }
    virtual bool shouldHideNonCasterCharacters() const { return true; }
    virtual bool shouldRenderCasterEntity() const { return true; }
    virtual bool shouldRenderBossEntity() const { return true; }
    virtual bool shouldRenderAboveHud() const { return true; }
    virtual bool shouldBlackoutWorld() const { return false; }
    virtual bool shouldRenderFloor() const { return true; }
    virtual bool shouldUseCenteredPartyLayout() const { return false; }

    // Override to play a splash art intro before the ability animation.
    // sprite: caster's loaded SDL_Texture (may be nullptr if unavailable).
    virtual std::optional<SplashArtConfig> getSplashConfig(SDL_Texture* sprite) const {
        (void)sprite;
        return std::nullopt;
    }

protected:
    float elapsedTime_ = 0.0f;
    float totalDuration_ = 2.0f;
    InputWindow inputWindow_;
};

// Factory function type
using PresentationFactory = std::function<std::unique_ptr<AbilityPresentation>(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)>;

// Registry for presentation factories
class PresentationRegistry {
public:
    static PresentationRegistry& instance();

    void registerPresentation(const std::string& id, PresentationFactory factory);
    std::unique_ptr<AbilityPresentation> create(
        const std::string& id,
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

private:
    PresentationRegistry() = default;
    std::unordered_map<std::string, PresentationFactory> factories_;
};

// Register all built-in presentations
void registerAllPresentations();

} // namespace battle

#endif // ABILITY_PRESENTATION_H
