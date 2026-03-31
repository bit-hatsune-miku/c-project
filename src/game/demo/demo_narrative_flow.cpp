#include "demo_narrative_flow.h"

#include <iostream>
#include <string>
#include <vector>

#include "../core/battle_turn_flow.h"
#include "../vn/vn_system.h"
#include "../../platform/path_resolution.h"

namespace battle::demo {

bool DemoNarrativeFlow::initialize() {
    shutdown();

    if (!loadDialogueLinesFromScript("assets/vn/json/demo.json", introDialogueLines_) ||
        !loadDialogueLinesFromScript("assets/vn/json/demo_after_miku_first_skill.json", postMikuSkillDialogueLines_) ||
        !loadDialogueLinesFromScript("assets/vn/json/demo_after_miku_first_ultimate.json", postMikuUltimateDialogueLines_) ||
        !loadDialogueLinesFromScript("assets/vn/json/demo_after_lyoo_attack_post_miku_ultimate.json", postLyooAttackAfterMikuUltimateDialogueLines_) ||
        !loadDialogueLinesFromScript("assets/vn/json/demo_boss_defeated.json", bossDefeatedDialogueLines_)) {
        shutdown();
        return false;
    }

    dialogueInProgress_ = true;
    spaceEnabledForBattle_ = false;
    currentDialogueLine_ = 0;
    hasShownMikuFirstSkillTutorial_ = false;
    hasShownMikuFirstUltimateTutorial_ = false;
    hasShownPostLyooAttackAfterMikuUltimateTutorial_ = false;
    pendingPostLyooAttackAfterMikuUltimateTutorial_ = false;
    hasShownBossDefeatedDialogue_ = false;

    startDialogueSequence(introDialogueLines_);
    return true;
}

void DemoNarrativeFlow::shutdown() {
    introDialogueLines_.clear();
    postMikuSkillDialogueLines_.clear();
    postMikuUltimateDialogueLines_.clear();
    postLyooAttackAfterMikuUltimateDialogueLines_.clear();
    bossDefeatedDialogueLines_.clear();

    activeDialogueLines_ = nullptr;
    dialogueInProgress_ = false;
    spaceEnabledForBattle_ = false;
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

bool DemoNarrativeFlow::isSpaceEnabledForBattle() const {
    return spaceEnabledForBattle_;
}

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

void DemoNarrativeFlow::handleAutomaticProgression(BattleManager& manager) {
    if (dialogueInProgress_) {
        return;
    }

    if (pendingPostLyooAttackAfterMikuUltimateTutorial_ && !hasShownPostLyooAttackAfterMikuUltimateTutorial_) {
        const std::size_t eventCountBefore = manager.getRecentActionEvents().size();
        if (manager.processAutomaticTurns()) {
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

    manager.processAutomaticTurns();
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
