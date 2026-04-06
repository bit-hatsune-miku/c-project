#include "ari_boss_presentation.h"
#include "ability_presentation.h"
#include "ari_skill_presentation.h"
#include "ari_ultimate_presentation.h"
#include "cupcakke_drum_presentation.h"
#include "cupcakke_ultimate_presentation.h"
#include "jiafei_boss_presentation.h"
#include "jiafei_scream_presentation.h"
#include "jiafei_ultimate_presentation.h"
#include "lyoo_heal_presentation.h"
#include "lyoo_boss_presentation.h"
#include "lyoo_plot_twist_presentation.h"
#include "luotianyi_boss_presentation.h"
#include "luotianyi_skill_presentation.h"
#include "luotianyi_ultimate_presentation.h"
#include "miku_sing_presentation.h"
#include "miku_diandong_presentation.h"
#include "miku_self_corruption_presentation.h"

#include "qr_code_attack_presentation.h"
#include "qr_code_shield_presentation.h"
#include "wechatalipay_ultimate_presentation.h"
#include "teto_boss_presentation.h"
#include "teto_skill_presentation.h"
#include "teto_ultimate_presentation.h"
#include "zhou_shen_presentation.h"

namespace battle {

void registerAllPresentations() {
    auto& registry = PresentationRegistry::instance();
    registry.registerPresentation("boss_attack_zhoushen", [](float cwx, float cwy, float cwz, float twx, float twy, float twz) -> std::unique_ptr<AbilityPresentation> { return std::make_unique<ZhouShenPresentation>(cwx, cwy, cwz, twx, twy, twz); });
    registry.registerPresentation("wechatalipay_ultimate", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<WechatalipayUltimatePresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    // Register Miku's singing ability (musical_notes)
        // Register Wechatalipay's shield minigame (sao_ma_zhi_fu)
        registry.registerPresentation("sao_ma_zhi_fu", [](
            float casterWorldX, float casterWorldY, float casterWorldZ,
            float targetWorldX, float targetWorldY, float targetWorldZ
        ) -> std::unique_ptr<AbilityPresentation> {
            return std::make_unique<QrCodeShieldPresentation>(
                casterWorldX, casterWorldY, casterWorldZ,
                targetWorldX, targetWorldY, targetWorldZ
            );
        });
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

    registry.registerPresentation("self_corruption", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<MikuSelfCorruptionPresentation>(
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

    registry.registerPresentation("yin_long", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<LuotianyiBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("rang_wo_men_shuo_zhong_wen", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<LuotianyiSkillPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("quan_yu_tian_xia", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<LuotianyiUltimatePresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("whistle_note", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<AriBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("i_want_it_i_got_it", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<AriSkillPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("high_note", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<AriUltimatePresentation>(
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

    registry.registerPresentation("teto_healing_touch", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<TetoSkillPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("teto_recovery_encore", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<TetoUltimatePresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    registry.registerPresentation("teto_baguette_attack", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<TetoBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    // Future presentations can be registered here
    // registry.registerPresentation("drum_attack", ...);
    // registry.registerPresentation("miku_ultimate", ...);
}

} // namespace battle
