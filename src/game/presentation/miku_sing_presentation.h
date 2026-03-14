#ifndef MIKU_SING_PRESENTATION_H
#define MIKU_SING_PRESENTATION_H

#include "ability_presentation.h"
#include <vector>

namespace battle {

class MikuSingPresentation : public AbilityPresentation {
public:
    MikuSingPresentation(
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    );

    void start() override;
    void update(float deltaTime) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override;

private:
    void spawnNote();

    float casterWorldX_;
    float casterWorldY_;
    float casterWorldZ_;
    float targetWorldX_;
    float targetWorldY_;
    float targetWorldZ_;

    std::vector<ProjectileParticle> notes_;
    float nextSpawnTime_;
    float spawnInterval_;
    int notesToSpawn_;
    int notesSpawned_;
    int pendingHitEvents_ = 0;
};

} // namespace battle

#endif // MIKU_SING_PRESENTATION_H
