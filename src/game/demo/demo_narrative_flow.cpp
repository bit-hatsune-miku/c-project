#include "demo_narrative_flow.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "../core/battle_turn_flow.h"
#include "../vn/vn_system.h"
#include "../../platform/path_resolution.h"

namespace battle::demo {

/**
 * @brief Initializes the demo narrative flow by loading dialogue scripts and resetting runtime state.
 *
 * Loads required VN dialogue scripts, clears and initializes runtime flags and counters, and either
 * starts the intro dialogue immediately or defers it based on the parameter.
 *
 * @param deferIntroDialogue If `true`, do not start the intro dialogue immediately but mark it pending when intro lines exist; if `false`, start the intro dialogue right away.
 * @return true on successful load and initialization; `false` if any script fails to load (in which case state is reset).
 */
bool DemoNarrativeFlow::initialize(bool deferIntroDialogue) {
    shutdown();

    if (!loadDialogueLinesFromScript("assets/vn/json/demo.json", introDialogueLines_) ||
        !loadDialogueLinesFromScript("assets/vn/json/demo_after_miku_first_skill.json", postMikuSkillDialogueLines_) ||
        !loadDialogueLinesFromScript("assets/vn/json/demo_after_miku_first_ultimate.json", postMikuUltimateDialogueLines_) ||
        !loadDialogueLinesFromScript("assets/vn/json/demo_after_lyoo_attack_post_miku_ultimate.json", postLyooAttackAfterMikuUltimateDialogueLines_) ||
        !loadDialogueLinesFromScript("assets/vn/json/demo_boss_defeated.json", bossDefeatedDialogueLines_)) {
        shutdown();
        return false;
    }

    dialogueInProgress_ = false;
    spaceEnabledForBattle_ = false;
    introDialoguePending_ = false;
    currentDialogueLine_ = 0;
    hasShownMikuFirstSkillTutorial_ = false;
    hasShownMikuFirstUltimateTutorial_ = false;
    hasShownPostLyooAttackAfterMikuUltimateTutorial_ = false;
    pendingPostLyooAttackAfterMikuUltimateTutorial_ = false;
    hasShownBossDefeatedDialogue_ = false;

    if (deferIntroDialogue) {
        introDialoguePending_ = !introDialogueLines_.empty();
    } else {
        startDialogueSequence(introDialogueLines_);
    }
    return true;
}

void DemoNarrativeFlow::setDialogueLinePresenter(DialogueLinePresenter presenter) {
    dialogueLinePresenter_ = std::move(presenter);
}

/**
 * @brief Reset narrative state and clear all loaded dialogue sequences.
 *
 * Clears all dialogue line buffers, cancels any active or pending dialogue, and
 * resets runtime flags and counters related to tutorial progression, automatic
 * triggers, and battle-space interaction.
 */
void DemoNarrativeFlow::shutdown() {
    introDialogueLines_.clear();
    postMikuSkillDialogueLines_.clear();
    postMikuUltimateDialogueLines_.clear();
    postLyooAttackAfterMikuUltimateDialogueLines_.clear();
    bossDefeatedDialogueLines_.clear();

    activeDialogueLines_ = nullptr;
    dialogueInProgress_ = false;
    spaceEnabledForBattle_ = false;
    introDialoguePending_ = false;
    currentDialogueLine_ = 0;

    hasShownMikuFirstSkillTutorial_ = false;
    hasShownMikuFirstUltimateTutorial_ = false;
    hasShownPostLyooAttackAfterMikuUltimateTutorial_ = false;
    pendingPostLyooAttackAfterMikuUltimateTutorial_ = false;
    hasShownBossDefeatedDialogue_ = false;
}

bool DemoNarrativeFlow::isDialogueInProgress() const {
    return dialogueInProgress_;
}

/**
 * @brief Indicates whether the space key is enabled for battle interaction.
 *
 * @return `true` if space input is enabled for battle, `false` otherwise.
 */
bool DemoNarrativeFlow::isSpaceEnabledForBattle() const {
    return spaceEnabledForBattle_;
}

/**
 * @brief Starts the intro dialogue if it was deferred.
 *
 * If an intro dialogue is pending, clears the pending flag and starts the
 * intro dialogue sequence when intro lines are available. Does nothing when
 * no intro is pending.
 */
void DemoNarrativeFlow::startIntroDialogueIfPending() {
    if (!introDialoguePending_) {
        return;
    }

    introDialoguePending_ = false;
    if (!introDialogueLines_.empty()) {
        startDialogueSequence(introDialogueLines_);
    }
}

/**
 * @brief Handles user "space" input while a dialogue sequence is active.
 *
 * If no dialogue is in progress this is a no-op. If the current visual novel
 * line is still animating, the input is forwarded to the VN input handler.
 * If the current line is finished, advances to the next line and displays it;
 * if the sequence is exhausted, resets the VN state and finishes the dialogue
 * sequence.
 */
void DemoNarrativeFlow::onDialogueSpacePressed() {
    if (!dialogueInProgress_) {
        return;
    }

    if (!vn::isLineFinished()) {
        vn::onSpacePressed();
        return;
    }

    ++currentDialogueLine_;
    if (activeDialogueLines_ != nullptr &&
        currentDialogueLine_ < static_cast<int>(activeDialogueLines_->size())) {
        showDialogueLine((*activeDialogueLines_)[static_cast<size_t>(currentDialogueLine_)]);
        return;
    }

    vn::reset();
    finishDialogueSequence();
}

bool DemoNarrativeFlow::onPlayerTurnExecuted(const flow::PlayerTurnExecution& turnExecution) {
    if (turnExecution.actorWasMikuUltimateExtra && !hasShownMikuFirstUltimateTutorial_) {
        hasShownMikuFirstUltimateTutorial_ = true;
        startDialogueSequence(postMikuUltimateDialogueLines_);
        return false;
    }

    if (turnExecution.actorWasMiku && !hasShownMikuFirstSkillTutorial_) {
        hasShownMikuFirstSkillTutorial_ = true;
        startDialogueSequence(postMikuSkillDialogueLines_);
        return false;
    }

    return !pendingPostLyooAttackAfterMikuUltimateTutorial_;
}

void DemoNarrativeFlow::maybeStartBossDefeatedDialogue(const BattleManager& manager) {
    if (dialogueInProgress_ || hasShownBossDefeatedDialogue_ || !manager.isBattleOver()) {
        return;
    }

    hasShownBossDefeatedDialogue_ = true;
    startDialogueSequence(bossDefeatedDialogueLines_);
}

void DemoNarrativeFlow::handleAutomaticProgression(BattleManager& manager,
                                                  const AutomaticProgressionRunner& automaticRunner) {
    if (dialogueInProgress_) {
        return;
    }

    const auto runAutomaticProgression = [&](BattleManager& targetManager) {
        if (automaticRunner) {
            return automaticRunner(targetManager);
        }
        return targetManager.processAutomaticTurns();
    };

    if (pendingPostLyooAttackAfterMikuUltimateTutorial_ && !hasShownPostLyooAttackAfterMikuUltimateTutorial_) {
        const std::size_t eventCountBefore = manager.getRecentActionEvents().size();
        if (runAutomaticProgression(manager)) {
            const auto& actionEvents = manager.getRecentActionEvents();
            for (std::size_t i = eventCountBefore; i < actionEvents.size(); ++i) {
                if (actionEvents[i].actorType != ParticipantType::Boss) {
                    continue;
                }

                hasShownPostLyooAttackAfterMikuUltimateTutorial_ = true;
                pendingPostLyooAttackAfterMikuUltimateTutorial_ = false;
                startDialogueSequence(postLyooAttackAfterMikuUltimateDialogueLines_);
                break;
            }
        }

        return;
    }

    (void)runAutomaticProgression(manager);
}

bool DemoNarrativeFlow::loadDialogueLinesFromScript(const std::string& jsonRelativePath,
                                                    std::vector<DialogueLine>& outLines) {
    vn::Script script;
    if (!vn::loadScript(platform::path::resolvePath(jsonRelativePath), script)) {
        std::cerr << "[Demo] Failed to load dialogue script: " << jsonRelativePath << "\n";
        return false;
    }

    outLines.clear();
    outLines.reserve(script.entries.size());
    outLines.insert(outLines.end(), script.entries.begin(), script.entries.end());

    return !outLines.empty();
}

void DemoNarrativeFlow::showDialogueLine(const DialogueLine& line) const {
    if (dialogueLinePresenter_) {
        dialogueLinePresenter_(line);
        return;
    }

    const std::string speakerName = vn::getDisplaySpeakerName(line);
    const std::string iconPath = line.icon.empty() ? std::string{} : platform::path::resolvePath(line.icon);
    const std::string backgroundPath = line.background.empty()
        ? std::string{}
        : (vn::isHexColorString(line.background) ? line.background : platform::path::resolvePath(line.background));
    const std::string voicePath = line.voice.empty() ? std::string{} : platform::path::resolvePath(line.voice);
    const std::string bgmPath = line.bgm.empty() ? std::string{} : platform::path::resolvePath(line.bgm);
    const std::string fontPath = line.fontPath.empty() ? std::string{} : platform::path::resolvePath(line.fontPath);

    vn::showLine(
        line.text,
        speakerName,
        iconPath,
        voicePath,
        fontPath,
        line.autoAdvanceOnVoiceEnd,
        line.iconFrameCount,
        line.iconFps,
        backgroundPath,
        bgmPath,
        line.bgmVolume,
        line.bgmStop,
        line.bgmPause
    );
}

void DemoNarrativeFlow::startDialogueSequence(const std::vector<DialogueLine>& lines) {
    activeDialogueLines_ = &lines;
    dialogueInProgress_ = true;
    spaceEnabledForBattle_ = false;
    currentDialogueLine_ = 0;

    if (!activeDialogueLines_->empty()) {
        showDialogueLine((*activeDialogueLines_)[0]);
    }
}

void DemoNarrativeFlow::finishDialogueSequence() {
    const bool finalVictoryDialogue = activeDialogueLines_ == &bossDefeatedDialogueLines_;

    dialogueInProgress_ = false;
    spaceEnabledForBattle_ = !finalVictoryDialogue;

    if (activeDialogueLines_ == &postMikuUltimateDialogueLines_ &&
        !hasShownPostLyooAttackAfterMikuUltimateTutorial_) {
        pendingPostLyooAttackAfterMikuUltimateTutorial_ = true;
    }

    activeDialogueLines_ = nullptr;
}

} // namespace battle::demo
