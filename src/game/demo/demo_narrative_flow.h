#ifndef DEMO_NARRATIVE_FLOW_H
#define DEMO_NARRATIVE_FLOW_H

#include <string>
#include <vector>

#include "../core/battle_manager.h"
#include "../core/battle_flow_controller.h"
#include "../vn/vn_script.h"

namespace battle::demo {

class DemoNarrativeFlow {
public:
    bool initialize(bool deferIntroDialogue = false);
    void shutdown();

    bool isDialogueInProgress() const;
    bool isSpaceEnabledForBattle() const;

    void startIntroDialogueIfPending();
    void onDialogueSpacePressed();
    bool onPlayerTurnExecuted(const flow::PlayerTurnExecution& turnExecution);

    void maybeStartBossDefeatedDialogue(const BattleManager& manager);
    void handleAutomaticProgression(BattleManager& manager);

private:
    using DialogueLine = vn::ScriptEntry;

    bool loadDialogueLinesFromScript(const std::string& jsonRelativePath, std::vector<DialogueLine>& outLines);
    void showDialogueLine(const DialogueLine& line) const;

    void startDialogueSequence(const std::vector<DialogueLine>& lines);
    void finishDialogueSequence();

    std::vector<DialogueLine> introDialogueLines_;
    std::vector<DialogueLine> postMikuSkillDialogueLines_;
    std::vector<DialogueLine> postMikuUltimateDialogueLines_;
    std::vector<DialogueLine> postLyooAttackAfterMikuUltimateDialogueLines_;
    std::vector<DialogueLine> bossDefeatedDialogueLines_;

    const std::vector<DialogueLine>* activeDialogueLines_ = nullptr;
    bool dialogueInProgress_ = false;
    bool spaceEnabledForBattle_ = false;
    bool introDialoguePending_ = false;
    int currentDialogueLine_ = 0;

    bool hasShownMikuFirstSkillTutorial_ = false;
    bool hasShownMikuFirstUltimateTutorial_ = false;
    bool hasShownPostLyooAttackAfterMikuUltimateTutorial_ = false;
    bool pendingPostLyooAttackAfterMikuUltimateTutorial_ = false;
    bool hasShownBossDefeatedDialogue_ = false;
};

} // namespace battle::demo

#endif
