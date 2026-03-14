#include "miku_sing_presentation.h"
#include "../core/easing.h"
#include <cmath>

namespace battle {

MikuSingPresentation::MikuSingPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterWorldX_(casterWorldX)
    , casterWorldY_(casterWorldY)
    , casterWorldZ_(casterWorldZ)
    , targetWorldX_(targetWorldX)
    , targetWorldY_(targetWorldY)
    , targetWorldZ_(targetWorldZ)
    , nextSpawnTime_(0.0f)
    , spawnInterval_(0.15f)
    , notesToSpawn_(6)
    , notesSpawned_(0)
{
    totalDuration_ = 1.5f;
}

void MikuSingPresentation::start() {
    elapsedTime_ = 0.0f;
    nextSpawnTime_ = 0.1f;
    notesSpawned_ = 0;
    pendingHitEvents_ = 0;
    notes_.clear();
}

void MikuSingPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    // Spawn notes at intervals
    while (notesSpawned_ < notesToSpawn_ && elapsedTime_ >= nextSpawnTime_) {
        spawnNote();
        notesSpawned_++;
        nextSpawnTime_ += spawnInterval_;
    }

    // Update existing notes
    for (size_t i = 0; i < notes_.size(); ++i) {
        auto& note = notes_[i];
        if (!note.active) continue;

        note.lifetime += deltaTime;
        
        // Calculate progress (0 to 1)
        float t = note.lifetime / note.maxLifetime;
        if (t >= 1.0f) {
            note.active = false;
            continue;
        }

        // Apply easing for smooth motion
        float easedT = easing::easeOutCubic(t);

        // Curved trajectory with alternating variants.
        const float baseX = easing::lerp(casterWorldX_, targetWorldX_, easedT);
        note.worldY = easing::lerp(casterWorldY_, targetWorldY_, easedT);

        // "Golden ratio-ish" side bend: starts to one side, then crosses inward.
        // Shape is positive early and slightly negative near the end.
        const float sideShape = std::sin(t * 3.14159f) * (1.0f - 1.35f * t);
        const float sideAmplitude = 150.0f;
        note.worldX = baseX + note.velocityX * sideAmplitude * sideShape;

        // Vertical arc must use Z (height), not Y (ground depth).
        // In this camera setup, MORE NEGATIVE Z appears higher on screen.
        const float baseZ = easing::lerp(casterWorldZ_ - 160.0f, targetWorldZ_ - 130.0f, easedT);
        const float arcHeight = 180.0f * std::sin(t * 3.14159f);
        note.worldZ = baseZ - (note.velocityY * arcHeight);

        // Register hit once when note reaches target window.
        if (!note.hitRegistered && t >= 0.88f) {
            note.hitRegistered = true;
            ++pendingHitEvents_;
        }

        // Fade out near the end
        if (t > 0.8f) {
            float fadeT = (t - 0.8f) / 0.2f;
            note.color.a = static_cast<Uint8>(255.0f * (1.0f - fadeT));
        }
    }
}

void MikuSingPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    for (const auto& note : notes_) {
        if (!note.active) continue;

        // Check if note is in front of camera
        float depth = camera.getDepth(note.worldX, note.worldY, note.worldZ);
        if (depth <= 1.0f) continue;

        // Project to screen
        SDL_FPoint screenPos = camera.worldToScreen(note.worldX, note.worldY, note.worldZ);

        // Calculate size with perspective
        float perspectiveScale = camera.getPerspectiveScale(note.worldX, note.worldY, note.worldZ);
        float screenSize = note.size * perspectiveScale;

        // Draw as filled rectangle (placeholder for texture)
        SDL_FRect rect;
        rect.x = screenPos.x - screenSize * 0.5f;
        rect.y = screenPos.y - screenSize * 0.5f;
        rect.w = screenSize;
        rect.h = screenSize;

        SDL_SetRenderDrawColor(renderer, note.color.r, note.color.g, note.color.b, note.color.a);
        SDL_RenderFillRectF(renderer, &rect);

        // Optional: draw outline
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, note.color.a);
        SDL_RenderDrawRectF(renderer, &rect);
    }
}

bool MikuSingPresentation::isComplete() const {
    // Complete when all notes have been spawned and finished
    if (notesSpawned_ < notesToSpawn_) return false;
    
    for (const auto& note : notes_) {
        if (note.active) return false;
    }
    
    return true;
}

int MikuSingPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

int MikuSingPresentation::getDamageLabelHitCount() const {
    return std::max(1, notesToSpawn_);
}

void MikuSingPresentation::spawnNote() {
    ProjectileParticle note;
    
    // Start position: upper part of Miku's sprite (chest/head area)
    note.worldX = casterWorldX_;
    note.worldY = casterWorldY_;
    note.worldZ = casterWorldZ_ - 170.0f;
    
    // Use velocity fields as trajectory variant signs:
    // velocityX: side bend direction, velocityY: vertical arc direction.
    // Even notes: up-right then inward-left/down.
    // Odd notes: down-right then inward-left/up.
    const bool evenNote = (notesSpawned_ % 2) == 0;
    note.velocityX = evenNote ? 1.0f : 1.0f;
    note.velocityY = evenNote ? 1.0f : -1.0f;
    note.velocityZ = 0.0f;
    
    note.lifetime = 0.0f;
    note.maxLifetime = 0.8f;
    
    // Cyan color (Miku's theme color)
    note.color = {0, 200, 200, 255};
    note.size = 25.0f;
    note.active = true;
    
    notes_.push_back(note);
}

} // namespace battle
