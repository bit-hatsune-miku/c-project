#include "presentation_runtime.h"

#include <iostream>
#include <memory>
#include <utility>

#include "splash_art_animation.h"
#include "../core/ability_system.h"
#include "../../platform/runtime_flags.h"

namespace battle::presentation_runtime {
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

    // Set basic playback flags — but hold off on exposing the presentation pointer
    // until after the splash so its render() is not called before start().
    if (stateRefs.playbackActive)   *stateRefs.playbackActive   = true;
    if (stateRefs.casterIsBoss)     *stateRefs.casterIsBoss     = context.isBoss;
    if (stateRefs.casterPartyIndex) *stateRefs.casterPartyIndex = context.casterIndex;

    // ------------------------------------------------------------------
    // Splash art intro (pre-presentation)
    // ------------------------------------------------------------------
    SDL_Texture* splashSpriteTexture = callbacks.splashSpriteTexture != nullptr
        ? callbacks.splashSpriteTexture
        : callbacks.casterSpriteTexture;
    std::optional<SplashArtConfig> splashCfg = presentation->getSplashConfig(splashSpriteTexture);
    if (!splashCfg.has_value() && context.isBoss) {
        SplashArtConfig cfg;
        cfg.sprite = splashSpriteTexture;
        splashCfg = std::move(cfg);
    }

    const bool allowSplash = context.isBoss || !context.isUltimate;
    if (!platform::runtime::skipAnimationsAndWaitsEnabled() &&
        allowSplash && splashCfg && callbacks.setRenderOverlay && callbacks.clearRenderOverlay) {
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
                    if (event.key.keysym.sym == SDLK_ESCAPE) { finished = true; break; }
                    if (event.key.keysym.sym == SDLK_SPACE)  { splash.skip(); }
                }
            }
            if (finished) break;
            const Uint64 splashNow = SDL_GetPerformanceCounter();
            const float  splashDt  = static_cast<float>(splashNow - splashCounter) /
                                     static_cast<float>(SDL_GetPerformanceFrequency());
            splashCounter = splashNow;
            splash.update(splashDt);
            if (callbacks.onPostUpdate)            callbacks.onPostUpdate(splashDt);
            if (context.isBoss && callbacks.onBossPresentationFrame) {
                callbacks.onBossPresentationFrame();
            }
            if (callbacks.renderAndPresentFrame)   callbacks.renderAndPresentFrame();
        }
        callbacks.clearRenderOverlay();
    }

    // Now expose the presentation and start the main animation.
    if (stateRefs.activePresentation) {
        *stateRefs.activePresentation = presentation.get();
    }

    presentation->setExternalTextures(callbacks.casterSpriteTexture, callbacks.targetSpriteTexture);
    presentation->setOverlayTextures(callbacks.overlayCasterSpriteTexture, callbacks.overlayTargetSpriteTexture);
    presentation->setTargetPartyIndex(context.targetIndex);
    presentation->setTargetWorldPosition(targetX, targetY, targetZ);
    presentation->setPresentationValue(context.presentationValue);
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

    presentation->start();
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
                    finished = true;
                    break;
                }
                if (event.key.keysym.sym == SDLK_SPACE) {
                    presentation->onSpacePressed();
                }
                presentation->onKeyPressed(event.key.keysym.sym);
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
                    entity.visible = entity.partyIndex == focusedPartyIndex;
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
    return result;
}

} // namespace battle::presentation_runtime
