#ifndef DEMO_NARRATIVE_FLOW_H
#define DEMO_NARRATIVE_FLOW_H

#include <string>
#include <vector>

#include "../core/battle_manager.h"
#include "../core/battle_flow_controller.h"
#include "../vn/vn_script.h"

/**
 * Initialize the demo narrative system and preload required dialogue line sets.
 * @param deferIntroDialogue If `true`, mark the intro dialogue as pending instead of starting it immediately.
 * @returns `true` if initialization and dialogue loading succeeded, `false` otherwise.
 */
/**
 * Shut down the demo narrative system and stop any active dialogue or pending narrative state.
 */

/**
 * Query whether a dialogue sequence is currently active.
 * @returns `true` if a dialogue sequence is in progress, `false` otherwise.
 */

/**
 * Query whether battle input (space) is currently enabled.
 * @returns `true` if battle space input is enabled, `false` otherwise.
 */

/**
 * Start the intro dialogue if it was previously marked as pending.
 */

/**
 * Handle the user pressing the space key during dialogue, advancing or completing the active dialogue sequence.
 */

/**
 * Process effects of a player turn execution on narrative progression and tutorial state.
 * @param turnExecution Details of the executed player turn.
 * @returns `true` if the call triggered narrative progression or a state change that the caller should act on, `false` otherwise.
 */

/**
 * Check battle state and start the boss-defeated dialogue if victory conditions are met and it has not been shown.
 * @param manager Battle manager used to inspect current battle state.
 */

/**
 * Perform automatic narrative progression checks and start any pending tutorial or post-action dialogue sequences when prerequisites are met.
 * @param manager Battle manager used to coordinate any automatic actions that affect battle flow.
 */

/**
 * Load dialogue lines from a JSON script located at the given relative path.
 * @param jsonRelativePath Path to the JSON script relative to the game's script root.
 * @param outLines Vector to be filled with the loaded dialogue lines.
 * @returns `true` if the script was successfully loaded and parsed into `outLines`, `false` otherwise.
 */

/**
 * Display a single dialogue line to the player.
 * @param line Dialogue entry to show.
 */

/**
 * Begin a dialogue sequence using the provided lines and disable battle input until the sequence finishes.
 * @param lines Sequence of dialogue lines to play.
 */

/**
 * Finish the currently active dialogue sequence and re-enable battle input.
 */
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
