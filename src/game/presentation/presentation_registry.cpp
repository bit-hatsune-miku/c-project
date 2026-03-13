#include "ability_presentation.h"
#include "cupcakke_drum_presentation.h"
#include "miku_sing_presentation.h"
#include "miku_diandong_presentation.h"

namespace battle {

void registerAllPresentations() {
    auto& registry = PresentationRegistry::instance();

    // Register Miku's singing ability (musical_notes)
    registry.registerPresentation("musical_notes", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<MikuSingPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    // Register Miku's ultimate (diandong_power_cutscene)
    registry.registerPresentation("diandong_power_cutscene", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<MikuDiandongPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    // Register Cupcakke normal ability presentation (drum_attack)
    registry.registerPresentation("drum_attack", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<CupcakkeDrumPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    // Future presentations can be registered here
    // registry.registerPresentation("drum_attack", ...);
    // registry.registerPresentation("miku_ultimate", ...);
}

} // namespace battle
