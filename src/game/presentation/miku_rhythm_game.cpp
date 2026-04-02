#include "miku_rhythm_game.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace battle {

// ---------------------------------------------------------------------------
// start
// ---------------------------------------------------------------------------
void MikuRhythmGame::start() {
    elapsed_       = 0.0f;
    resolvedCount_ = 0;
    pendingResolvedFeedback_.clear();

    // Fisher-Yates shuffle: randomly assign the 4 columns to the 4 spawn slots
    // so notes appear in a different order each cast.
    int cols[kLaneCount] = {0, 1, 2, 3};
    uint64_t rng = SDL_GetTicks64() ^ UINT64_C(0xDEADBEEF12345678);
    for (int i = kLaneCount - 1; i > 0; --i) {
        rng = rng * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
        const int j   = static_cast<int>((rng >> 33) % static_cast<uint64_t>(i + 1));
        const int tmp = cols[i]; cols[i] = cols[j]; cols[j] = tmp;
    }

    for (int i = 0; i < kLaneCount; ++i) {
        Lane& lane      = lanes_[i];
        lane.column     = cols[i];
        lane.spawnTime  = kNoteSpawnOffsets[i];
        lane.hitTime    = kNoteSpawnOffsets[i] + kFallDuration;
        lane.spawned    = false;
        lane.resolved   = false;
        lane.result     = NoteResult::Pending;
        lane.flashTimer = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------
void MikuRhythmGame::update(float dt) {
    elapsed_ += dt;

    for (Lane& lane : lanes_) {
        // Tick down flash timer regardless of resolved state
        if (lane.flashTimer > 0.0f) {
            lane.flashTimer = std::max(0.0f, lane.flashTimer - dt);
        }

        if (lane.resolved) continue;

        // Mark spawned when elapsed passes spawn time
        if (!lane.spawned && elapsed_ >= lane.spawnTime) {
            lane.spawned = true;
        }

        // Auto-miss: note passed the entire OK window without being pressed
        if (lane.spawned
            && lane.result == NoteResult::Pending
            && elapsed_ > lane.hitTime + kOkWindowSec)
        {
            resolveLane(lane, NoteResult::Miss);
        }
    }
}

// ---------------------------------------------------------------------------
// onKeyPressed
// ---------------------------------------------------------------------------
void MikuRhythmGame::onKeyPressed(SDL_Keycode key) {
    // Map key to column index
    int pressedColumn = -1;
    for (int i = 0; i < kLaneCount; ++i) {
        if (kLaneKeys[i] == key) { pressedColumn = i; break; }
    }
    if (pressedColumn < 0) return;

    // Find the unresolved note in this column
    for (Lane& lane : lanes_) {
        if (lane.column != pressedColumn) continue;
        if (!lane.spawned || lane.resolved)  continue;
        if (lane.result != NoteResult::Pending) continue;

        const float delta = std::fabs(elapsed_ - lane.hitTime);
        NoteResult result = NoteResult::Pending;
        if      (delta <= kPerfectWindowSec) result = NoteResult::Perfect;
        else if (delta <= kGoodWindowSec)    result = NoteResult::Good;
        else if (delta <= kOkWindowSec)      result = NoteResult::Ok;
        else return; // outside window — ignore, note will auto-miss later

        resolveLane(lane, result);
        return;
    }
}

// ---------------------------------------------------------------------------
// Query helpers
// ---------------------------------------------------------------------------
bool MikuRhythmGame::isComplete() const {
    return resolvedCount_ >= kLaneCount;
}

float MikuRhythmGame::accuracyForResult(NoteResult result) {
    switch (result) {
        case NoteResult::Perfect: return 1.00f;
        case NoteResult::Good:    return 0.66f;
        case NoteResult::Ok:      return 0.33f;
        default:                  return 0.00f;
    }
}

float MikuRhythmGame::getAverageAccuracy() const {
    float sum = 0.0f;
    for (const Lane& lane : lanes_) {
        sum += accuracyForResult(lane.result);
    }
    return sum / static_cast<float>(kLaneCount);
}

float MikuRhythmGame::getInputMultiplier() const {
    return 1.0f + kMaxDamageBonus * getAverageAccuracy();
}

std::vector<MikuRhythmGame::ResolvedNoteFeedback> MikuRhythmGame::consumeResolvedFeedback() {
    std::vector<ResolvedNoteFeedback> feedback;
    feedback.swap(pendingResolvedFeedback_);
    return feedback;
}

std::string MikuRhythmGame::getResultText() const {
    const int accPct   = static_cast<int>(std::round(getAverageAccuracy() * 100.0f));
    const int bonusPct = static_cast<int>(std::round((getInputMultiplier() - 1.0f) * 100.0f));
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Accuracy %d%% \xc2\xb7 Damage +%d%% (\xc3\x97%.2f)",
                  accPct, bonusPct, static_cast<double>(getInputMultiplier()));
    return buf;
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
SDL_Color MikuRhythmGame::resultFlashColor(NoteResult result) {
    switch (result) {
        case NoteResult::Perfect: return {255, 240,  80, 255}; // gold
        case NoteResult::Good:    return { 80, 240, 120, 255}; // green
        case NoteResult::Ok:      return {240, 190,  50, 255}; // amber
        case NoteResult::Miss:    return {240,  60,  60, 255}; // red
        default:                  return {180, 180, 180, 200};
    }
}

float MikuRhythmGame::averageResolvedAccuracy() const {
    if (resolvedCount_ <= 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (const Lane& lane : lanes_) {
        if (!lane.resolved) {
            continue;
        }
        sum += accuracyForResult(lane.result);
    }
    return sum / static_cast<float>(resolvedCount_);
}

void MikuRhythmGame::resolveLane(Lane& lane, NoteResult result) {
    if (lane.resolved) {
        return;
    }

    lane.result = result;
    lane.flashTimer = kFlashDuration;
    lane.resolved = true;
    ++resolvedCount_;

    pendingResolvedFeedback_.push_back(ResolvedNoteFeedback{
        result,
        std::clamp(averageResolvedAccuracy(), 0.0f, 1.0f)
    });
}

void MikuRhythmGame::render(SDL_Renderer* renderer, int screenW, int screenH) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // ── Full-screen dark overlay ─────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 8, 8, 18, 195);
    SDL_RenderFillRect(renderer, nullptr); // nullptr fills entire render target

    // ── Layout constants ─────────────────────────────────────────────────────
    const int laneW    = std::max(60, std::min(100, screenW / 13));
    const int laneGap  = std::max(6, laneW / 9);
    const int totalW   = kLaneCount * laneW + (kLaneCount - 1) * laneGap;
    const int areaX    = (screenW - totalW) / 2;
    const int areaTop  = screenH *  8 / 100;
    const int areaH    = screenH * 85 / 100;            // visible lane height
    const int hitLineY = areaTop + areaH * 84 / 100;   // 84 % down the lane
    const int receptH  = std::max(38, screenH / 16);
    const int noteH    = std::max(18, screenH / 36);

    // Per-column theme colours (teal / purple / pink / sky-blue — Miku palette)
    static constexpr SDL_Color kColour[kLaneCount] = {
        {  0, 210, 200, 255},  // D — teal
        {170,  60, 230, 255},  // F — purple
        {240,  80, 160, 255},  // J — pink
        { 50, 160, 255, 255},  // K — sky-blue
    };

    // ── Lane backgrounds ─────────────────────────────────────────────────────
    for (int col = 0; col < kLaneCount; ++col) {
        const int lx = areaX + col * (laneW + laneGap);

        SDL_SetRenderDrawColor(renderer, 18, 18, 35, 210);
        SDL_Rect bg{lx, areaTop, laneW, areaH};
        SDL_RenderFillRect(renderer, &bg);

        // Tinted border using 1/4 of the lane colour
        const SDL_Color& c = kColour[col];
        SDL_SetRenderDrawColor(renderer, c.r / 4, c.g / 4, c.b / 4, 220);
        SDL_RenderDrawRect(renderer, &bg);
    }

    // ── Hit-zone line ─────────────────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 200, 240, 255, 230);
    SDL_Rect hitLine{areaX - 4, hitLineY - 2, totalW + 8, 4};
    SDL_RenderFillRect(renderer, &hitLine);

    // ── Per-column: receptor boxes and falling notes ──────────────────────────
    for (int col = 0; col < kLaneCount; ++col) {
        const int lx = areaX + col * (laneW + laneGap);
        const SDL_Color& tc = kColour[col];

        // Locate the lane assigned to this visual column
        const Lane* lane = nullptr;
        for (const Lane& l : lanes_) {
            if (l.column == col) { lane = &l; break; }
        }

        // ── Receptor box ───────────────────────────────────────────────────
        const bool  flashing = lane && (lane->flashTimer > 0.0f);
        SDL_Color   rcCol;
        if (flashing) {
            // Blend from flash colour toward lane colour as timer fades
            const float t  = lane->flashTimer / kFlashDuration;
            const SDL_Color fc = resultFlashColor(lane->result);
            rcCol = {
                static_cast<Uint8>(fc.r * t + tc.r * (1.0f - t)),
                static_cast<Uint8>(fc.g * t + tc.g * (1.0f - t)),
                static_cast<Uint8>(fc.b * t + tc.b * (1.0f - t)),
                230
            };
        } else {
            rcCol = {
                static_cast<Uint8>(tc.r / 4),
                static_cast<Uint8>(tc.g / 4),
                static_cast<Uint8>(tc.b / 4),
                200
            };
        }
        SDL_SetRenderDrawColor(renderer, rcCol.r, rcCol.g, rcCol.b, rcCol.a);
        SDL_Rect receptor{lx + 2, hitLineY + 4, laneW - 4, receptH};
        SDL_RenderFillRect(renderer, &receptor);

        SDL_SetRenderDrawColor(renderer, tc.r, tc.g, tc.b, flashing ? 255 : 130);
        SDL_RenderDrawRect(renderer, &receptor);

        // ── Falling note ──────────────────────────────────────────────────
        if (!lane || !lane->spawned || lane->resolved) continue;
        if (lane->result != NoteResult::Pending)       continue;

        const float progress = std::min(1.0f,
            (elapsed_ - lane->spawnTime) / kFallDuration);
        const int noteTopY = areaTop
            + static_cast<int>(progress * (hitLineY - areaTop))
            - noteH / 2;

        // Fade-in over the first 0.10 s after spawn
        const float fadeIn = std::min(1.0f, (elapsed_ - lane->spawnTime) / 0.10f);
        // Pulse scale when the note is close to the hit line (last 12 % of fall)
        const float pulseT  = std::max(0.0f, (progress - 0.88f) / 0.12f);
        const float pulse   = 1.0f + 0.15f * std::sin(pulseT * 6.2832f * 2.0f);
        const int   nW      = static_cast<int>((laneW - 8) * pulse);
        const int   nX      = lx + (laneW - nW) / 2;
        const Uint8 noteA   = static_cast<Uint8>(
            std::max(0.0f, std::min(1.0f, fadeIn)) * 230.0f);

        SDL_SetRenderDrawColor(renderer, tc.r, tc.g, tc.b, noteA);
        SDL_Rect note{nX, noteTopY, nW, noteH};
        SDL_RenderFillRect(renderer, &note);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(noteA * 3 / 4));
        SDL_RenderDrawRect(renderer, &note);
    }
}

} // namespace battle
