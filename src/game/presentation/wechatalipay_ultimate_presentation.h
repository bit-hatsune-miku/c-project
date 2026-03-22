
#pragma once
#include "ability_presentation.h"
#include "../core/battle_manager.h" // for PresentationContext
#include "../render/camera_3d.h"    // for Camera3D

namespace battle {

class WechatalipayUltimatePresentation : public AbilityPresentation {
public:
    WechatalipayUltimatePresentation(const battle::PresentationContext& context);
    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
private:
    enum class Phase { PhoneUI, LockOn, Zoom, Done };
    Phase phase_ = Phase::PhoneUI;
    float timer_ = 0.0f;
    bool soundPlayed_ = false;
    float zoomProgress_ = 0.0f;
};

} // namespace battle
