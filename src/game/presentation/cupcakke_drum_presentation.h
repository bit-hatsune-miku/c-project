#ifndef CUPCAKKE_DRUM_PRESENTATION_H
#define CUPCAKKE_DRUM_PRESENTATION_H

#include "ability_presentation.h"

namespace battle {

class CupcakkeDrumPresentation : public AbilityPresentation {
public:
    CupcakkeDrumPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;

    bool overridesCamera() const override;
    void applyCameraState(Camera3D& camera) const override;
    bool getCasterWorldOverride(float& outX, float& outY, float& outZ) const override;

private:
    float casterStartX_ = 0.0f;
    float casterStartY_ = 0.0f;
    float casterStartZ_ = 0.0f;
    float targetX_ = 0.0f;
    float targetY_ = 0.0f;
    float targetZ_ = 0.0f;

    float casterCurrentX_ = 0.0f;
    float casterCurrentY_ = 0.0f;
    float casterCurrentZ_ = 0.0f;

    float orbitDuration_ = 1.1f;
    float holdDuration_ = 0.45f;
    float dashDuration_ = 0.35f;
};

} // namespace battle

#endif // CUPCAKKE_DRUM_PRESENTATION_H
