#ifndef BATTLE_STORY_COMPETITION_BRACKET_H
#define BATTLE_STORY_COMPETITION_BRACKET_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "battle_manager.h"
#include "player_progression.h"

namespace battle::competition {

enum class RoundNodeState {
    Cleared,
    Current,
    Future,
    Locked
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct Segment {
    Rect rect;
};

struct RoundEntry {
    BattleDefinition battle;
    BossDefinition boss;
    std::string spritePath;
    std::string instructionHint;
    bool cleared = false;
};

struct BracketModel {
    CharacterDefinition player;
    std::string playerSpritePath;
    std::vector<RoundEntry> rounds;
    std::size_t nextUnclearedIndex = 0;
    bool finaleUnlocked = false;
};

struct LayoutSpec {
    float width = 0.0f;
    float height = 0.0f;
    float leftInset = 0.0f;
    float rightInset = 0.0f;
    float topInset = 0.0f;
    float bottomInset = 0.0f;
    float playerWidth = 0.0f;
    float playerHeight = 0.0f;
    float rivalWidth = 0.0f;
    float rivalHeight = 0.0f;
    float ghostWidth = 0.0f;
    float ghostHeight = 0.0f;
    float finaleWidth = 0.0f;
    float finaleHeight = 0.0f;
};

struct RoundLayout {
    Rect playerRect;
    Rect rivalRect;
    Rect rivalIntroRect;
    Rect ghostTopRect;
    Rect ghostBottomRect;
    Segment playerToJoin;
    Segment joinToRival;
    Segment rivalToBranch;
    Segment branchVertical;
    Segment branchToGhostTop;
    Segment branchToGhostBottom;
    std::vector<Segment> advanceSegments;
};

struct BracketLayout {
    Rect playerStartRect;
    Rect finaleRect;
    std::vector<RoundLayout> rounds;
};

bool buildBracketModel(const PlayerProgression& progression,
                       BracketModel& outModel,
                       std::string* outError = nullptr);
std::optional<std::size_t> findRoundIndex(const BracketModel& model, const std::string& battleKey);
std::size_t resolveTargetRoundIndex(const BracketModel& model, const std::string& battleKey);
RoundNodeState resolveRoundState(const BracketModel& model,
                                 std::size_t roundIndex,
                                 std::size_t selectedIndex,
                                 bool practiceMode);
BracketLayout buildBracketLayout(const BracketModel& model, const LayoutSpec& spec);

}  // namespace battle::competition

#endif  // BATTLE_STORY_COMPETITION_BRACKET_H
