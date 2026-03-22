
#include "wechatalipay_ultimate_presentation.h"
#include "presentation_runtime.h"
#include <SDL.h>
#include <cmath>

namespace battle {

battle::WechatalipayUltimatePresentation::WechatalipayUltimatePresentation(const battle::PresentationContext& context)
{
    // If you need to store context, do so here
}
#include <SDL_image.h>

void WechatalipayUltimatePresentation::start() {
    timer_ = 0.0f;
    phase_ = Phase::PhoneUI;
    soundPlayed_ = false;
    zoomProgress_ = 0.0f;
}

void WechatalipayUltimatePresentation::update(float deltaTime) {
    timer_ += deltaTime;
    switch (phase_) {
        case Phase::PhoneUI:
            if (timer_ > 0.8f) { // Show phone UI for a short time
                phase_ = Phase::LockOn;
                timer_ = 0.0f;
            }
            break;
        case Phase::LockOn:
            if (timer_ > 1.0f) { // Simulate locking cursor on boss
                phase_ = Phase::Zoom;
                timer_ = 0.0f;
            }
            break;
        case Phase::Zoom:
                if (!soundPlayed_) {
                    // playVoice("wechatalipay/ultimate.wav"); // Uncomment if playVoice is available
                    soundPlayed_ = true;
                }
            zoomProgress_ += deltaTime * 3.5f; // Fast zoom
            if (zoomProgress_ >= 1.0f) {
                phase_ = Phase::Done;
                timer_ = 0.0f;
            }
            break;
        case Phase::Done:
            break;
    }
}

void WechatalipayUltimatePresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    // Placeholder: draw phone UI and zoom effect
    if (phase_ == Phase::PhoneUI || phase_ == Phase::LockOn) {
        // Draw phone UI overlay (reuse phone texture from QR code presentations)
        SDL_Surface* phoneSurf = IMG_Load("assets/combat/presentations/wechatalipay/huaweisanzhedian.png");
        if (phoneSurf) {
            SDL_Texture* phoneTex = SDL_CreateTextureFromSurface(renderer, phoneSurf);
            SDL_Rect dst = {screenW/2-128, screenH/2-256, 256, 512};
            SDL_RenderCopy(renderer, phoneTex, nullptr, &dst);
            SDL_DestroyTexture(phoneTex);
            SDL_FreeSurface(phoneSurf);
        }
    }
    // Optionally: draw lock-on reticle, zoom lines, etc.
}

bool WechatalipayUltimatePresentation::isComplete() const {
    return phase_ == Phase::Done && timer_ > 0.5f;
}


} // namespace battle
