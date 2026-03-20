#include "miku_sing_presentation.h"
#include "../core/easing.h"
#include <algorithm>
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

// ---------------------------------------------------------------------------
// start — reset both phases
// ---------------------------------------------------------------------------
void MikuSingPresentation::start() {
    phase_                   = Phase::RhythmGame;
    pendingAbilityAudioCue_  = false;
    rhythmGame_.start();

    // Pre-clear animation state (properly reset on phase transition)
    elapsedTime_      = 0.0f;
    nextSpawnTime_    = 0.1f;
    notesSpawned_     = 0;
    pendingHitEvents_ = 0;
    notes_.clear();
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------
void MikuSingPresentation::update(float deltaTime) {
    // ── Phase 1: rhythm mini-game ────────────────────────────────────────────
    if (phase_ == Phase::RhythmGame) {
        rhythmGame_.update(deltaTime);
        if (rhythmGame_.isComplete()) {
            phase_            = Phase::Animation;
            elapsedTime_      = 0.0f;
            nextSpawnTime_    = 0.1f;
            notesSpawned_     = 0;
            pendingHitEvents_ = 0;
            notes_.clear();
            pendingAbilityAudioCue_ = true;
        }
        return;
    }

    // ── Phase 2: original note-fly animation ─────────────────────────────────
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

    // Keep container bounded over time by dropping finished notes.
    notes_.erase(
        std::remove_if(notes_.begin(), notes_.end(), [](const ProjectileParticle& note) {
            return !note.active;
        }),
        notes_.end()
    );
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void MikuSingPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    // ── Phase 1: delegate to rhythm game overlay ─────────────────────────────
    if (phase_ == Phase::RhythmGame) {
        rhythmGame_.render(renderer, screenW, screenH);
        return;
    }

    // ── Phase 2: original projectile render ──────────────────────────────────
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

// ---------------------------------------------------------------------------
// isComplete
// ---------------------------------------------------------------------------
bool MikuSingPresentation::isComplete() const {
    // Complete when all notes have been spawned and finished
    if (phase_ == Phase::RhythmGame) return false;

    if (notesSpawned_ < notesToSpawn_) return false;
    
    for (const auto& note : notes_) {
        if (note.active) return false;
    }
    
    return true;
}

bool MikuSingPresentation::overridesCamera() const {
    return false;
}

void MikuSingPresentation::applyCameraState(Camera3D& camera) const {
    const float t = std::clamp(elapsedTime_ / std::max(0.001f, totalDuration_), 0.0f, 1.0f);
    const float ease = easing::easeOutCubic(t);

    const float anchorX = easing::lerp(casterWorldX_ - 460.0f, casterWorldX_ - 390.0f, ease);
    const float anchorY = easing::lerp(casterWorldY_ - 90.0f, casterWorldY_ - 25.0f, ease);
    const float anchorZ = casterWorldZ_ - 185.0f + std::sin(elapsedTime_ * 3.2f) * 8.0f;

    camera.posX = anchorX;
    camera.posY = anchorY;
    camera.posZ = anchorZ;
    camera.pitchDegrees = -6.0f + std::sin(elapsedTime_ * 2.1f) * 1.8f;
    camera.yawDegrees = 21.0f + std::sin(elapsedTime_ * 2.7f) * 4.5f;
    camera.focalLength = 36000.0f;
}

bool MikuSingPresentation::getCasterWorldOverride(float& outX, float& outY, float& outZ) const {
    (void)outX;
    (void)outY;
    (void)outZ;
    return false;
}

int MikuSingPresentation::consumeHitEvents() {
    const int hits    = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

int MikuSingPresentation::getDamageLabelHitCount() const {
    return std::max(1, notesToSpawn_);
}

// ---------------------------------------------------------------------------
// Input forwarding
// ---------------------------------------------------------------------------
void MikuSingPresentation::onKeyPressed(SDL_Keycode key) {
    if (phase_ == Phase::RhythmGame) {
        rhythmGame_.onKeyPressed(key);
    }
}

int MikuSingPresentation::consumeAbilityAudioCues() {
    if (pendingAbilityAudioCue_) {
        pendingAbilityAudioCue_ = false;
        return 1;
    }
    return 0;
}

float MikuSingPresentation::getInputMultiplier() const {
    return rhythmGame_.getInputMultiplier();
}

std::string MikuSingPresentation::getInputResultText() const {
    return rhythmGame_.getResultText();
}

bool MikuSingPresentation::shouldRenderAboveHud() const {
    return phase_ != Phase::RhythmGame;
}

// ---------------------------------------------------------------------------
// spawnNote (animation phase helper)
// ---------------------------------------------------------------------------
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
