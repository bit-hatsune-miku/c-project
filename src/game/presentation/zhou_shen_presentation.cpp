#include <cstdio>
#include <iostream>
#include "zhou_shen_presentation.h"
#include <SDL2/SDL_image.h>
#include <cmath>

namespace battle {

ZhouShenPresentation::ZhouShenPresentation(float cwx, float cwy, float cwz, float twx, float twy, float twz) 
    : casterX_(cwx), casterY_(cwy), casterZ_(cwz), targetX_(twx), targetY_(twy), targetZ_(twz) {}

ZhouShenPresentation::~ZhouShenPresentation() {
    if (noteTex_) SDL_DestroyTexture(noteTex_);
    if (fishTex_) SDL_DestroyTexture(fishTex_);
}

void ZhouShenPresentation::start() {
    printf("ZHOU SHEN START CALLED!\n"); fflush(stdout);
    std::cout << "Zhou Shen started! Phase: " << phase_ << " NumNotes: " << notes_.size() << std::endl;
    // Generate notes based on phase
    float freq = 2.5f; // More frequent notes for phase 1
    float spd = 2.5f; // Slower speed for phase 1 (takes 2.5 seconds to cross)
    if (phase_ == 2) { freq = 3.5f; spd = 1.5f; duration_ = 8.0f; } // Medium speed
    else if (phase_ >= 3) { freq = 5.0f; spd = 1.0f; duration_ = 12.0f; } // Fast speed

    int numNotes = duration_ * freq;
    for (int i=0; i<numNotes; i++) {
        Note n;
        n.row = rand() % 4;
        n.spawnTime = 0.5f + static_cast<float>(i) / freq;
        n.hitTime = n.spawnTime + spd;
        n.speed = spd;
        n.resolved = false;
        n.result = MikuRhythmGame::NoteResult::Pending;
        notes_.push_back(n);

        if (phase_ >= 3 && (rand()%3 == 0)) {
            Note n2 = n; n2.row = (n.row + 1) % 4; notes_.push_back(n2);
        }
    }
}




float ZhouShenPresentation::consumeHitDamageMultiplier() {
    float dmg = 1.0f - damageReduction_; // damage reduction ratio
    if (dmg < 0.0f) dmg = 0.0f;
    return dmg; 
}

int ZhouShenPresentation::consumeHitEvents() {
    if (rhythmDone_) {
        float fhits = elapsed_ / 0.15f;
        int expected = (int)std::floor(fhits);
        if (expected > hitsEmitted_ && hitsEmitted_ < 5) {
            int d = expected - hitsEmitted_;
            if (hitsEmitted_ + d > 5) d = 5 - hitsEmitted_;
            hitsEmitted_ += d;
            return d;
        }
    }
    return 0;
}

std::string ZhouShenPresentation::getInputResultText() const {
    if (damageReduction_ > 0.95f) return "PERFECT!";
    if (damageReduction_ > 0.5f) return "GOOD!";
    if (damageReduction_ > 0.0f) return "OKAY!";
    return "FLOP!";
}

void ZhouShenPresentation::applyCameraState(Camera3D& camera) const {
    // Keep camera identical to Luo Tianyi / Ari Boss presentations for perfect framing
    camera.posX = casterX_;
    camera.posY = casterY_ - 675.0f;
    camera.posZ = -175.0f;
    camera.pitchDegrees = -2.5f;
    camera.yawDegrees = 0.0f;
    camera.focalLength = 32000.0f;
}

bool ZhouShenPresentation::isComplete() const {
    return rhythmDone_ && hitsEmitted_ >= 5 && elapsed_ > 1.2f;
}


std::vector<PresentationFeedbackEvent> ZhouShenPresentation::consumeFeedbackEvents() {
    auto ev = pendingFeedbackEvents_;
    pendingFeedbackEvents_.clear();
    return ev;
}

bool ZhouShenPresentation::onKeyPressed(SDL_Keycode key) {
    if (rhythmDone_) return false;
    int col = -1;
    if (key == SDLK_d) col = 0;
    else if (key == SDLK_f) col = 1;
    else if (key == SDLK_j) col = 2;
    else if (key == SDLK_k) col = 3;
    if (col == -1) return false;

    for (auto& n : notes_) {
        if (!n.resolved && n.row == col) {
            float diff = std::abs(n.hitTime - elapsed_);
            PresentationFeedbackEvent ev;
            ev.comboEligible = true;
            bool hit = false;
            MikuRhythmGame::NoteResult nr = MikuRhythmGame::NoteResult::Pending;
            if (diff < 0.15f) {
                nr = MikuRhythmGame::NoteResult::Perfect;
                ev.signal = PresentationFeedbackSignal::graded(1.0f); hit = true;
            } else if (diff < 0.25f) {
                nr = MikuRhythmGame::NoteResult::Good;
                ev.signal = PresentationFeedbackSignal::graded(0.66f); hit = true;
            } else if (diff < 0.40f) {
                nr = MikuRhythmGame::NoteResult::Ok;
                ev.signal = PresentationFeedbackSignal::graded(0.33f); hit = true;
            }
            if (hit) {
                n.result = nr;
                n.resolved = true;
                pendingFeedbackEvents_.push_back(ev);
                return true;
            }
        }
    }
    return false;
}

void ZhouShenPresentation::update(float dt) {
    elapsed_ += dt;
    if (!rhythmDone_) {
        bool allDone = true;
        float score = 0.0f;
        int total = 0;
        for (auto& n : notes_) {
            if (!n.resolved && elapsed_ > n.hitTime + 0.45f) {
                n.resolved = true;
                n.result = MikuRhythmGame::NoteResult::Miss;
                
                PresentationFeedbackEvent ev;
                ev.comboEligible = true;
                ev.signal = PresentationFeedbackSignal::graded(0.0f);
                pendingFeedbackEvents_.push_back(ev);
            }
            if (!n.resolved) allDone = false;
            
            if (n.resolved) {
                total++;
                if (n.result == MikuRhythmGame::NoteResult::Perfect) score += 1.0f;
                else if (n.result == MikuRhythmGame::NoteResult::Good) score += 0.7f;
                else if (n.result == MikuRhythmGame::NoteResult::Ok) score += 0.4f;
            }
        }
        if (allDone || elapsed_ > duration_ + 2.0f) {
            rhythmDone_ = true;
            if (total > 0) damageReduction_ = score / total;
            else damageReduction_ = 0.0f;
            elapsed_ = 0.0f;
        }
    }
}

void ZhouShenPresentation::render(SDL_Renderer* renderer, int w, int h, const Camera3D& camera) {
    if (!noteTex_) {
        SDL_Surface* surf = IMG_Load("assets/combat/presentations/zhouShenBoss/note.png");
        if (surf) { noteTex_ = SDL_CreateTextureFromSurface(renderer, surf); SDL_FreeSurface(surf); }
    }
    if (!fishTex_) {
        SDL_Surface* surf = IMG_Load("assets/combat/presentations/zhouShenBoss/fish.png");
        if (surf) { fishTex_ = SDL_CreateTextureFromSurface(renderer, surf); SDL_FreeSurface(surf); }
    }

    if (!rhythmDone_ && noteTex_) {
        int startY = (h - (4 * 80)) / 2;
        
        // Draw white background
        SDL_Rect bg = { 0, startY - 20, w, (4*80) + 40 };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 245);
        SDL_RenderFillRect(renderer, &bg);

        // Draw 4 rows
        for (int i=0; i<4; i++) {
            SDL_Rect lr = {50, startY + 20 + i*80, w-100, 2};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
            SDL_RenderFillRect(renderer, &lr);
            
            SDL_Rect htz = {80, startY + i*80, 40, 40};
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 150);
            SDL_RenderDrawRect(renderer, &htz);
        }

        for (auto& n : notes_) {
            if (!n.resolved) {
                float prog = (n.hitTime - elapsed_); // time until hit
                float fraction = prog / n.speed; // 1.0 at spawn, 0.0 at hit time
                int x = 80 + fraction * (w + 40); // moving from right edge of screen to perfectly align with hit zone
                SDL_Rect dst = { x, startY + n.row * 80, 40, 40 };
                SDL_RenderCopy(renderer, noteTex_, nullptr, &dst);
            }
        }
    } else if (rhythmDone_ && fishTex_) {
        // fish moving diagonal up right, massively fast
        float progress = elapsed_ / 0.8f; 
        int cx = progress * w;
        int cy = h - (progress * h);
        SDL_Rect dst = {cx - 800, cy - 800, 1600, 1600};
        SDL_RenderCopyEx(renderer, fishTex_, nullptr, &dst, -45.0, nullptr, SDL_FLIP_NONE);
    }
}
}
