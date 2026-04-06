#include "ari_boss_presentation.h"
#include "ability_presentation.h"
#include "ari_skill_presentation.h"
#include "ari_ultimate_presentation.h"
#include "cupcakke_drum_presentation.h"
#include "cupcakke_ultimate_presentation.h"
#include "disciple_boss_presentation.h"
#include "disciple_debuff_presentation.h"
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
#include "minako_strike_presentation.h"
#include "huafei_boss_presentation.h"
#include "huafei_support_presentation.h"
#include "pompom_boss_presentation.h"
#include "pompom_gacha_presentation.h"
#include "randy_boss_presentation.h"
#include "randy_logo_strike_presentation.h"
#include "randy_quiz_presentation.h"

#include "qr_code_attack_presentation.h"
#include "qr_code_shield_presentation.h"
#include "wechatalipay_ultimate_presentation.h"
#include "teto_boss_presentation.h"
#include "teto_skill_presentation.h"
#include "teto_ultimate_presentation.h"
#include "sailor_venus_boss_presentation.h"
#include "zhou_shen_skill_presentation.h"
#include "zhou_shen_ultimate_presentation.h"
#include "zhou_shen_presentation.h"

namespace battle {

void registerAllPresentations() {
    auto& registry = PresentationRegistry::instance();
    registry.registerPresentation("boss_attack_zhoushen", [](float cwx, float cwy, float cwz, float twx, float twy, float twz) -> std::unique_ptr<AbilityPresentation> { return std::make_unique<ZhouShenPresentation>(cwx, cwy, cwz, twx, twy, twz); });
    registry.registerPresentation("zhou_shen_singer_buff", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<ZhouShenSkillPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("zhou_shen_big_fish", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<ZhouShenUltimatePresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("sailor_venus_love_and_beauty_shock", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<SailorVenusBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            SailorVenusBossPresentation::Variant::BossParry
        );
    });
    registry.registerPresentation("minako_strike", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<MinakoStrikePresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("sailor_venus_love_and_beauty_shock_playable", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<SailorVenusBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            SailorVenusBossPresentation::Variant::LoveAndBeautyShock
        );
    });
    registry.registerPresentation("sailor_venus_transformation", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<SailorVenusBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            SailorVenusBossPresentation::Variant::Transformation
        );
    });
    registry.registerPresentation("sailor_venus_crescent_beam", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<SailorVenusBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            SailorVenusBossPresentation::Variant::CrescentBeam
        );
    });
    registry.registerPresentation("sailor_venus_love_me_chain", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<SailorVenusBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            SailorVenusBossPresentation::Variant::LoveMeChain
        );
    });
    registry.registerPresentation("randy_assignment", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<RandyBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("randy_c_quiz", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<RandyQuizPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("randy_logo_strike", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<RandyLogoStrikePresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("disciple_blackout_barrage", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<DiscipleBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("disciple_mark", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<DiscipleDebuffPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            DiscipleDebuffPresentation::Variant::Skill
        );
    });
    registry.registerPresentation("disciple_sermon", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<DiscipleDebuffPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            DiscipleDebuffPresentation::Variant::Ultimate
        );
    });
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

    registry.registerPresentation("pompom_dino_attack", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<PomPomBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("pompom_gacha_skill", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<PomPomGachaPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            PomPomGachaPresentation::Variant::Skill
        );
    });
    registry.registerPresentation("pompom_gacha_ultimate", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<PomPomGachaPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ,
            PomPomGachaPresentation::Variant::Ultimate
        );
    });

    registry.registerPresentation("huafei_hostage_grab", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<HuafeiBossPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });
    registry.registerPresentation("huafei_support", [](
        float casterWorldX, float casterWorldY, float casterWorldZ,
        float targetWorldX, float targetWorldY, float targetWorldZ
    ) -> std::unique_ptr<AbilityPresentation> {
        return std::make_unique<HuafeiSupportPresentation>(
            casterWorldX, casterWorldY, casterWorldZ,
            targetWorldX, targetWorldY, targetWorldZ
        );
    });

    // Future presentations can be registered here
    // registry.registerPresentation("drum_attack", ...);
    // registry.registerPresentation("miku_ultimate", ...);
}

} // namespace battle
