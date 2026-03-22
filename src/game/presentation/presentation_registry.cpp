#include "ability_presentation.h"
#include "cupcakke_drum_presentation.h"
#include "cupcakke_ultimate_presentation.h"
#include "jiafei_boss_presentation.h"
#include "jiafei_scream_presentation.h"
#include "jiafei_ultimate_presentation.h"
#include "lyoo_heal_presentation.h"
#include "lyoo_boss_presentation.h"
#include "lyoo_plot_twist_presentation.h"
#include "miku_sing_presentation.h"
#include "miku_diandong_presentation.h"
#include "qr_code_attack_presentation.h"

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

    registry.registerPresentation("niagara_falls_ultimate", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<CupcakkeUltimatePresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    // Register Lyoo's healing animation (heal_hearts).
    registry.registerPresentation("heal_hearts", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<LyooHealPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            LyooHealPresentation::Variant::Skill
        );
    });

    registry.registerPresentation("lyoo_revive_heal", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<LyooHealPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            LyooHealPresentation::Variant::Ultimate
        );
    });

    // Register Lyoo boss attack presentations.
    registry.registerPresentation("boss_attack_lyoo", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<LyooBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("boss_attack_lyoo_ultimate", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<LyooBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("boss_attack_lyoo_plot_twist", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<LyooPlotTwistPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("aesthetic_warning", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<JiafeiBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("aesthetic_scream", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<JiafeiScreamPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("mei_ci_du_xiang_zhuang", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<JiafeiUltimatePresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("qr_code_attack", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<QrCodeAttackPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    // Future presentations can be registered here
    // registry.registerPresentation("drum_attack", ...);
    // registry.registerPresentation("miku_ultimate", ...);
}

} // namespace battle
