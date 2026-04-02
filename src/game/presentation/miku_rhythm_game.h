#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

namespace battle {

// ---------------------------------------------------------------------------
// MikuRhythmGame
//
// Self-contained 4-lane falling-note mini-game used as the pre-phase of
// Miku's ability presentation.  One note per lane falls from the top; the
// player must press the corresponding key (D / F / J / K, left → right) when
// the note reaches the hit-zone at the bottom.
//
// Usage pattern:
//   game.start();
//   while (!game.isComplete()) {
//       game.onKeyPressed(key);   // from SDL event loop
//       game.update(dt);
//       game.render(renderer, w, h);
//   }
//   float multiplier  = game.getInputMultiplier();   // applied to ability dmg
//   std::string result = game.getResultText();        // shown as post-hint
// ---------------------------------------------------------------------------
struct MikuRhythmGame {

    // -----------------------------------------------------------------------
    // Tuning
    // -----------------------------------------------------------------------
    static constexpr int   kLaneCount        = 4;
    static constexpr float kFallDuration     = 1.40f;   // seconds to fall to hit-line
    static constexpr float kPerfectWindowSec = 0.055f;
    static constexpr float kGoodWindowSec    = 0.110f;
    static constexpr float kOkWindowSec      = 0.165f;
    static constexpr float kMaxDamageBonus   = 1.20f;   // at 100 % avg accuracy: ×2.20
    static constexpr float kFlashDuration    = 0.22f;   // receptor flash duration

    // Spawn times (seconds after start) for note slots 0-3
    static constexpr float kNoteSpawnOffsets[kLaneCount] = { 0.10f, 0.55f, 0.85f, 1.35f };

    // Key bindings: column 0 → D, 1 → F, 2 → J, 3 → K
    static constexpr SDL_Keycode kLaneKeys[kLaneCount] = {
        SDLK_d, SDLK_f, SDLK_j, SDLK_k
    };
    static constexpr const char* kLaneKeyLabels[kLaneCount] = { "D", "F", "J", "K" };

    // -----------------------------------------------------------------------
    // Per-note state
    // -----------------------------------------------------------------------
    enum class NoteResult { Pending, Perfect, Good, Ok, Miss };

    struct Lane {
        int        column     = 0;
        float      spawnTime  = 0.0f;
        float      hitTime    = 0.0f;    // = spawnTime + kFallDuration
        bool       spawned    = false;
        bool       resolved   = false;
        NoteResult result     = NoteResult::Pending;
        float      flashTimer = 0.0f;   // counts down from kFlashDuration
    };

    struct ResolvedNoteFeedback {
        NoteResult result = NoteResult::Pending;
        float projectedAverageAccuracy = 0.0f;
    };

    // -----------------------------------------------------------------------
    // Public interface
    // -----------------------------------------------------------------------
    void        start();
    void        update(float dt);
    bool        onKeyPressed(SDL_Keycode key);
    void        render(SDL_Renderer* renderer, int screenW, int screenH);

    bool        isComplete()         const;
    float       getAverageAccuracy() const; // 0.0 – 1.0
    float       getInputMultiplier() const; // 1.0 + kMaxDamageBonus * accuracy
    std::string getResultText()      const;
    std::vector<ResolvedNoteFeedback> consumeResolvedFeedback();

    // Hint shown by the HUD banner before the game starts
    static const char* hintText() {
        return "D / F / J / K  \xe2\x80\x94  hit the notes as they reach the line!";
    }

private:
    float elapsed_       = 0.0f;
    int   resolvedCount_ = 0;
    std::array<Lane, kLaneCount> lanes_{};
    std::vector<ResolvedNoteFeedback> pendingResolvedFeedback_;

    static float     accuracyForResult(NoteResult result);
    static SDL_Color resultFlashColor(NoteResult result);
    float averageResolvedAccuracy() const;
    void resolveLane(Lane& lane, NoteResult result);
};

} // namespace battle
