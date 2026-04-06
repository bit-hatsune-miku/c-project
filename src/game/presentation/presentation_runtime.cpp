#include "presentation_runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>

#include "splash_art_animation.h"
#include "../core/ability_system.h"

namespace battle::presentation_runtime {
namespace {

constexpr int kWarningBlinkCount = 4;
constexpr float kWarningBlinkOnSeconds = 0.16f;
constexpr float kWarningBlinkOffSeconds = 0.10f;
constexpr char kWarningBlinkSfxPath[] = "assets/ui/sfx/Multiplayer_countdown-warn-final.wav";

class BossWarningIntroOverlay {
public:
    void start() {
        blinkIndex_ = 0;
        blinkVisible_ = true;
        blinkStarted_ = true;
        complete_ = false;
        timer_ = 0.0f;
    }

    void update(float deltaSeconds) {
        if (complete_) {
            return;
        }

        timer_ += deltaSeconds;
        const float phaseDuration = blinkVisible_ ? kWarningBlinkOnSeconds : kWarningBlinkOffSeconds;
        if (timer_ < phaseDuration) {
            return;
        }

        timer_ = 0.0f;
        if (blinkVisible_) {
            blinkVisible_ = false;
            return;
        }

        ++blinkIndex_;
        if (blinkIndex_ >= kWarningBlinkCount) {
            complete_ = true;
            return;
        }

        blinkVisible_ = true;
        blinkStarted_ = true;
    }

    bool isComplete() const {
        return complete_;
    }

    bool consumeBlinkStarted() {
        if (!blinkStarted_) {
            return false;
        }
        blinkStarted_ = false;
        return true;
    }

    void renderOverlay(SDL_Renderer* renderer, int screenW, int screenH) const {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        const float alphaScale = blinkVisible_ ? 1.0f : 0.16f;
        const float signSize = std::min(static_cast<float>(screenW) * 0.15f, static_cast<float>(screenH) * 0.27f);
        const std::array<float, 2> yRatios{0.34f, 0.66f};
        for (float yRatio : yRatios) {
            drawWarningSign(
                renderer,
                static_cast<float>(screenW) * 0.13f,
                static_cast<float>(screenH) * yRatio,
                signSize,
                alphaScale);
            drawWarningSign(
                renderer,
                static_cast<float>(screenW) * 0.87f,
                static_cast<float>(screenH) * yRatio,
                signSize,
                alphaScale);
        }
    }

private:
    static Uint8 scaledAlpha(Uint8 baseAlpha, float alphaScale) {
        const float scaled = std::clamp(static_cast<float>(baseAlpha) * alphaScale, 0.0f, 255.0f);
        return static_cast<Uint8>(scaled);
    }

    static std::array<SDL_FPoint, 3> warningTrianglePoints(float centerX, float centerY, float size) {
        return std::array<SDL_FPoint, 3>{
            SDL_FPoint{centerX, centerY - (size * 0.46f)},
            SDL_FPoint{centerX + (size * 0.46f), centerY + (size * 0.38f)},
            SDL_FPoint{centerX - (size * 0.46f), centerY + (size * 0.38f)}
        };
    }

    static std::array<SDL_FPoint, 3> scaleTriangle(const std::array<SDL_FPoint, 3>& points,
                                                   float centerX,
                                                   float centerY,
                                                   float scale) {
        std::array<SDL_FPoint, 3> scaled = points;
        for (SDL_FPoint& point : scaled) {
            point.x = centerX + ((point.x - centerX) * scale);
            point.y = centerY + ((point.y - centerY) * scale);
        }
        return scaled;
    }

    static void fillTriangle(SDL_Renderer* renderer,
                             const std::array<SDL_FPoint, 3>& points,
                             SDL_Color color) {
        const float minY = std::min({points[0].y, points[1].y, points[2].y});
        const float maxY = std::max({points[0].y, points[1].y, points[2].y});
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

        for (int y = static_cast<int>(std::floor(minY)); y <= static_cast<int>(std::ceil(maxY)); ++y) {
            const float sampleY = static_cast<float>(y) + 0.5f;
            std::array<float, 3> intersections{};
            int count = 0;

            for (int edgeIndex = 0; edgeIndex < 3; ++edgeIndex) {
                const SDL_FPoint& a = points[static_cast<size_t>(edgeIndex)];
                const SDL_FPoint& b = points[static_cast<size_t>((edgeIndex + 1) % 3)];
                const float minEdgeY = std::min(a.y, b.y);
                const float maxEdgeY = std::max(a.y, b.y);
                if (std::fabs(b.y - a.y) <= 0.001f ||
                    sampleY < minEdgeY ||
                    sampleY > maxEdgeY) {
                    continue;
                }

                const float t = (sampleY - a.y) / (b.y - a.y);
                intersections[static_cast<size_t>(count++)] = a.x + ((b.x - a.x) * t);
            }

            if (count < 2) {
                continue;
            }

            std::sort(intersections.begin(), intersections.begin() + count);
            SDL_RenderDrawLineF(renderer, intersections[0], sampleY, intersections[count - 1], sampleY);
        }
    }

    static void drawTriangleOutline(SDL_Renderer* renderer,
                                    const std::array<SDL_FPoint, 3>& points,
                                    SDL_Color color,
                                    int thickness) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        for (int offset = 0; offset < std::max(1, thickness); ++offset) {
            const float yOffset = static_cast<float>(offset) - (static_cast<float>(thickness - 1) * 0.5f);
            SDL_RenderDrawLineF(
                renderer,
                points[0].x,
                points[0].y + yOffset,
                points[1].x,
                points[1].y + yOffset);
            SDL_RenderDrawLineF(
                renderer,
                points[1].x,
                points[1].y + yOffset,
                points[2].x,
                points[2].y + yOffset);
            SDL_RenderDrawLineF(
                renderer,
                points[2].x,
                points[2].y + yOffset,
                points[0].x,
                points[0].y + yOffset);
        }
    }

    static void drawWarningSign(SDL_Renderer* renderer,
                                float centerX,
                                float centerY,
                                float size,
                                float alphaScale) {
        const std::array<SDL_FPoint, 3> outerPoints = warningTrianglePoints(centerX, centerY, size);
        const std::array<SDL_FPoint, 3> glowPoints = scaleTriangle(outerPoints, centerX, centerY, 1.08f);
        const std::array<SDL_FPoint, 3> innerPoints = scaleTriangle(outerPoints, centerX, centerY, 0.86f);

        fillTriangle(renderer, glowPoints, SDL_Color{255, 45, 45, scaledAlpha(66, alphaScale)});
        fillTriangle(renderer, innerPoints, SDL_Color{255, 45, 45, scaledAlpha(36, alphaScale)});
        drawTriangleOutline(renderer, outerPoints, SDL_Color{255, 45, 45, scaledAlpha(255, alphaScale)}, 3);

        const SDL_FRect stem{
            centerX - (size * 0.035f),
            centerY - (size * 0.10f),
            size * 0.07f,
            size * 0.29f
        };
        const SDL_FRect dot{
            centerX - (size * 0.042f),
            centerY + (size * 0.20f),
            size * 0.084f,
            size * 0.084f
        };

        SDL_SetRenderDrawColor(renderer, 255, 45, 45, scaledAlpha(255, alphaScale));
        SDL_RenderFillRectF(renderer, &stem);
        SDL_RenderFillRectF(renderer, &dot);
    }

    int blinkIndex_ = 0;
    bool blinkVisible_ = true;
    bool blinkStarted_ = false;
    bool complete_ = false;
    float timer_ = 0.0f;
};

} // namespace

PlaybackResult runAbilityPresentation(SDL_Renderer* renderer,
                                      bool& finished,
                                      Camera3D& camera,
                                      std::vector<render::SceneEntity>& entities,
                                      const PresentationContext& context,
                                      PlaybackStateRefs stateRefs,
                                      PlaybackCallbacks callbacks) {
    if (renderer == nullptr || finished || context.presentationId.empty()) {
        return {};
    }

    float casterX = -600.0f;
    float casterY = 300.0f;
    float casterZ = 0.0f;
    float targetX = 0.0f;
    float targetY = 720.0f;
    float targetZ = 0.0f;

    if (context.isBoss) {
        casterX = 0.0f;
        casterY = 720.0f;
        targetX = -600.0f;
        targetY = 300.0f;
    } else {
        for (const render::SceneEntity& entity : entities) {
            if (!entity.isBoss && entity.partyIndex == context.casterIndex) {
                casterX = entity.worldX;
                casterY = entity.worldY;
                casterZ = entity.worldZ;
                break;
            }
        }
    }

    if (context.isBoss && context.targetIndex >= 0) {
        for (const render::SceneEntity& entity : entities) {
            if (!entity.isBoss && entity.partyIndex == context.targetIndex) {
                targetX = entity.worldX;
                targetY = entity.worldY;
                targetZ = entity.worldZ;
                break;
            }
        }
    }

    std::unique_ptr<AbilityPresentation> presentation = PresentationRegistry::instance().create(
        context.presentationId,
        casterX, casterY, casterZ,
        targetX, targetY, targetZ
    );
    if (!presentation) {
        std::cerr << "[Presentation] Missing presentation id: " << context.presentationId << "\n";
        return {};
    }

    presentation->preload(renderer);

    std::cout << "[Presentation] Playing: " << context.presentationId << "\n";

    if (stateRefs.playbackActive)   *stateRefs.playbackActive   = true;
    if (stateRefs.casterIsBoss)     *stateRefs.casterIsBoss     = context.isBoss;
    if (stateRefs.casterPartyIndex) *stateRefs.casterPartyIndex = context.casterIndex;

    presentation->setExternalTextures(callbacks.casterSpriteTexture, callbacks.targetSpriteTexture);
    presentation->setOverlayTextures(callbacks.overlayCasterSpriteTexture, callbacks.overlayTargetSpriteTexture);
    presentation->setTargetPartyIndex(context.targetIndex);
    presentation->setTargetWorldPosition(targetX, targetY, targetZ);
    presentation->setPresentationValue(context.presentationValue);
    presentation->setTuningProfile(context.tuningProfile);
    {
        std::vector<std::string> partyAssetNames;
        partyAssetNames.reserve(entities.size());
        for (const render::SceneEntity& entity : entities) {
            if (entity.isBoss || entity.assetName.empty()) {
                continue;
            }
            partyAssetNames.push_back(entity.assetName);
        }
        presentation->setPartyAssetNames(partyAssetNames);
    }
    presentation->setPartyTargetableStates(callbacks.partyTargetableStates);

    bool presentationStarted = false;

    // ------------------------------------------------------------------
    // Boss warning / splash art intro (pre-presentation)
    // ------------------------------------------------------------------
    if (context.isBoss && callbacks.setRenderOverlay && callbacks.clearRenderOverlay) {
        presentation->start();
        presentationStarted = true;
        if (stateRefs.activePresentation) {
            *stateRefs.activePresentation = presentation.get();
        }

        BossWarningIntroOverlay warning;
        warning.start();
        callbacks.setRenderOverlay([&warning](SDL_Renderer* r, int w, int h) {
            warning.renderOverlay(r, w, h);
        });

        Uint64 warningCounter = SDL_GetPerformanceCounter();
        while (!finished && !warning.isComplete()) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    finished = true;
                    break;
                }
                if (event.type == SDL_WINDOWEVENT &&
                    (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                     event.window.event == SDL_WINDOWEVENT_RESIZED)) {
                    if (callbacks.onWindowResized) {
                        callbacks.onWindowResized(event.window.data1, event.window.data2);
                    }
                }
                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        if (callbacks.onPauseBlocked) {
                            callbacks.onPauseBlocked();
                        }
                        continue;
                    }
                }
            }
            if (finished) {
                break;
            }

            if (warning.consumeBlinkStarted() && callbacks.onAudioCommands) {
                callbacks.onAudioCommands({PresentationAudioCommand{
                    PresentationAudioCommandType::PlayOneShot,
                    kWarningBlinkSfxPath,
                    1.0f
                }});
            }

            const Uint64 warningNow = SDL_GetPerformanceCounter();
            const float warningDt = static_cast<float>(warningNow - warningCounter) /
                static_cast<float>(SDL_GetPerformanceFrequency());
            warningCounter = warningNow;
            warning.update(warningDt);
            if (callbacks.onPostUpdate) {
                callbacks.onPostUpdate(warningDt);
            }
            if (callbacks.onBossPresentationFrame) {
                callbacks.onBossPresentationFrame();
            }
            if (presentation->overridesCamera()) {
                presentation->applyCameraState(camera);
            }
            if (callbacks.renderAndPresentFrame) {
                callbacks.renderAndPresentFrame();
            }
        }
        callbacks.clearRenderOverlay();
    } else {
        SDL_Texture* splashSpriteTexture = callbacks.splashSpriteTexture != nullptr
            ? callbacks.splashSpriteTexture
            : callbacks.casterSpriteTexture;
        std::optional<SplashArtConfig> splashCfg = presentation->getSplashConfig(splashSpriteTexture);
        const bool allowSplash = !context.isUltimate;
        if (allowSplash && splashCfg && callbacks.setRenderOverlay && callbacks.clearRenderOverlay) {
            if (!context.abilityName.empty()) {
                splashCfg->abilityName = context.abilityName;
            } else if (!context.abilityId.empty()) {
                splashCfg->abilityName = context.abilityId;
            } else {
                splashCfg->abilityName = context.presentationId;
            }
            if (callbacks.onSplashArtStart) {
                callbacks.onSplashArtStart();
            }
            SplashArtAnimation splash(*splashCfg);
            splash.start();
            callbacks.setRenderOverlay([&splash](SDL_Renderer* r, int w, int h) {
                splash.renderOverlay(r, w, h);
            });
            Uint64 splashCounter = SDL_GetPerformanceCounter();
            while (!finished && !splash.isComplete()) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) { finished = true; break; }
                    if (event.type == SDL_WINDOWEVENT &&
                        (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                         event.window.event == SDL_WINDOWEVENT_RESIZED)) {
                        if (callbacks.onWindowResized) {
                            callbacks.onWindowResized(event.window.data1, event.window.data2);
                        }
                    }
                    if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            if (callbacks.onPauseBlocked) {
                                callbacks.onPauseBlocked();
                            }
                            continue;
                        }
                        if (event.key.keysym.sym == SDLK_SPACE)  {
                            splash.skip();
                        } else if (callbacks.onUnhandledKeyDown) {
                            callbacks.onUnhandledKeyDown(event.key.keysym.sym);
                        }
                    }
                }
                if (finished) break;
                const Uint64 splashNow = SDL_GetPerformanceCounter();
                const float  splashDt  = static_cast<float>(splashNow - splashCounter) /
                                         static_cast<float>(SDL_GetPerformanceFrequency());
                splashCounter = splashNow;
                splash.update(splashDt);
                if (callbacks.onPostUpdate)            callbacks.onPostUpdate(splashDt);
                if (callbacks.renderAndPresentFrame)   callbacks.renderAndPresentFrame();
            }
            callbacks.clearRenderOverlay();
        }
    }

    if (stateRefs.activePresentation) {
        *stateRefs.activePresentation = presentation.get();
    }
    if (!presentationStarted) {
        presentation->start();
    }
    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (!finished && !presentation->isComplete()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                finished = true;
                break;
            }

            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                 event.window.event == SDL_WINDOWEVENT_RESIZED)) {
                if (callbacks.onWindowResized) {
                    callbacks.onWindowResized(event.window.data1, event.window.data2);
                }
            }

            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    if (callbacks.onPauseBlocked) {
                        callbacks.onPauseBlocked();
                    }
                    continue;
                }
                if (event.key.keysym.sym == SDLK_SPACE) {
                    presentation->onSpacePressed();
                }
                const bool consumed = presentation->onKeyPressed(event.key.keysym.sym);
                if (event.key.keysym.sym != SDLK_SPACE &&
                    !consumed &&
                    callbacks.onUnhandledKeyDown) {
                    callbacks.onUnhandledKeyDown(event.key.keysym.sym);
                }
            }
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(now - lastCounter) /
            static_cast<float>(SDL_GetPerformanceFrequency());
        lastCounter = now;

        presentation->update(deltaSeconds);

        const std::vector<PresentationAudioCommand> audioCommands = presentation->consumeAudioCommands();
        if (!audioCommands.empty() && callbacks.onAudioCommands) {
            callbacks.onAudioCommands(audioCommands);
        }

        const int abilityAudioCues = presentation->consumeAbilityAudioCues();
        if (abilityAudioCues > 0) {
            if (callbacks.onAbilityAudioCues) {
                callbacks.onAbilityAudioCues(abilityAudioCues);
            }
        }

        const int presentationHitEvents = presentation->consumeHitEvents();
        if (presentationHitEvents > 0) {
            if (callbacks.onHitEvents) {
                callbacks.onHitEvents(
                    presentationHitEvents,
                    std::max(1, presentation->getDamageLabelHitCount())
                );
            }
        }

        if (callbacks.onPostUpdate) {
            callbacks.onPostUpdate(deltaSeconds);
        }

        Camera3D previousCamera = camera;
        std::vector<render::SceneEntity> previousEntities = entities;

        if (context.isBoss && callbacks.onBossPresentationFrame) {
            callbacks.onBossPresentationFrame();
        }

        const bool hideNonCasterCharacters = presentation->shouldHideNonCasterCharacters();
        const int focusedPartyIndex = presentation->getFocusedPartyIndex();
        const bool renderCasterEntity = presentation->shouldRenderCasterEntity();
        const bool renderBossEntity = presentation->shouldRenderBossEntity();
        const bool renderFocusedTargetEntity = presentation->shouldRenderFocusedTargetEntity();

        for (size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
            render::SceneEntity& entity = entities[entityIndex];
            const bool wasVisible = entityIndex < previousEntities.size()
                ? previousEntities[entityIndex].visible
                : entity.visible;

            if (entity.isBoss) {
                if (context.isBoss) {
                    entity.visible = renderCasterEntity;
                } else {
                    entity.visible = renderBossEntity;
                }
                continue;
            }

            if (!wasVisible) {
                entity.visible = false;
                continue;
            }

            if (context.isBoss) {
                if (hideNonCasterCharacters && focusedPartyIndex >= 0) {
                    entity.visible = entity.partyIndex == focusedPartyIndex
                        ? renderFocusedTargetEntity
                        : false;
                } else {
                    entity.visible = !hideNonCasterCharacters;
                }
                continue;
            }

            if (entity.partyIndex == context.casterIndex) {
                entity.visible = renderCasterEntity;
            } else {
                entity.visible = !hideNonCasterCharacters;
            }
        }

        if (presentation->overridesCamera()) {
            presentation->applyCameraState(camera);
        }

        float overrideX = 0.0f;
        float overrideY = 0.0f;
        float overrideZ = 0.0f;
        if (presentation->getCasterWorldOverride(overrideX, overrideY, overrideZ)) {
            for (render::SceneEntity& entity : entities) {
                if (context.isBoss && entity.isBoss) {
                    entity.worldX = overrideX;
                    entity.worldY = overrideY;
                    entity.worldZ = overrideZ;
                    break;
                }
                if (!context.isBoss && !entity.isBoss && entity.partyIndex == context.casterIndex) {
                    entity.worldX = overrideX;
                    entity.worldY = overrideY;
                    entity.worldZ = overrideZ;
                    break;
                }
            }
        }

        if (focusedPartyIndex >= 0 &&
            presentation->getTargetWorldOverride(overrideX, overrideY, overrideZ)) {
            for (render::SceneEntity& entity : entities) {
                if (!entity.isBoss && entity.partyIndex == focusedPartyIndex) {
                    entity.worldX = overrideX;
                    entity.worldY = overrideY;
                    entity.worldZ = overrideZ;
                    break;
                }
            }
        }

        if (callbacks.renderAndPresentFrame) {
            callbacks.renderAndPresentFrame();
        }

        entities = std::move(previousEntities);
        // camera = previousCamera; // Removed to allow presentation camera overrides to persist
    }

    if (stateRefs.playbackActive) {
        *stateRefs.playbackActive = false;
    }
    if (stateRefs.casterIsBoss) {
        *stateRefs.casterIsBoss = false;
    }
    if (stateRefs.casterPartyIndex) {
        *stateRefs.casterPartyIndex = -1;
    }
    if (stateRefs.activePresentation) {
        *stateRefs.activePresentation = nullptr;
    }

    PlaybackResult result;
    result.multiplier = presentation->getInputMultiplier();
    result.resultText = presentation->getInputResultText();
    result.correctToneCount = presentation->getCorrectToneCount();
    result.scoreValue = presentation->getScoreValue();
    result.feedbackSignal = presentation->getFeedbackSignal();
    return result;
}

} // namespace battle::presentation_runtime
