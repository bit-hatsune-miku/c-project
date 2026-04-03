#include "story_competition_bracket.h"

#include <algorithm>
#include <filesystem>
#include <set>

#include "battle_loader.h"
#include "../../platform/path_resolution.h"

namespace battle::competition {
namespace {

/**
 * @brief Constructs a Rect from the provided position and size.
 *
 * @param x The x-coordinate of the rectangle's origin (left).
 * @param y The y-coordinate of the rectangle's origin (top).
 * @param width The rectangle's width (may be zero or positive).
 * @param height The rectangle's height (may be zero or positive).
 * @return Rect A rectangle with origin (x, y) and the specified width and height.
 */
Rect makeRect(float x, float y, float width, float height) {
    return Rect{x, y, width, height};
}

/**
 * @brief Constructs a horizontal segment rectangle spanning between two x coordinates.
 *
 * The returned Segment's rect starts at the lesser of `x0` and `x1`, extends to the greater,
 * is vertically centered at `centerY`, and has the specified `thickness`. The segment width is
 * clamped to be at least 0.
 *
 * @param x0 One horizontal endpoint.
 * @param x1 The other horizontal endpoint.
 * @param centerY Vertical center position of the segment.
 * @param thickness Height of the segment rectangle (defaults to 4.0f).
 * @return Segment A horizontal segment represented as a rectangular `Segment`.
 */
Segment makeHorizontalSegment(float x0, float x1, float centerY, float thickness = 4.0f) {
    const float left = std::min(x0, x1);
    const float right = std::max(x0, x1);
    return Segment{makeRect(left, centerY - thickness * 0.5f, std::max(0.0f, right - left), thickness)};
}

/**
 * @brief Constructs a vertical rectangular segment centered horizontally at a given X and spanning two Y coordinates.
 *
 * @param centerX X coordinate of the rectangle's horizontal center.
 * @param y0 First Y coordinate of the vertical span.
 * @param y1 Second Y coordinate of the vertical span.
 * @param thickness Width of the rectangle (horizontal size) defaulting to 4.0f.
 * @return Segment A `Segment` whose rectangle is centered at `centerX` with width `thickness` and height spanning from the lesser of `y0`/`y1` to the greater. If `y0 == y1` the rectangle's height is zero.
 */
Segment makeVerticalSegment(float centerX, float y0, float y1, float thickness = 4.0f) {
    const float top = std::min(y0, y1);
    const float bottom = std::max(y0, y1);
    return Segment{makeRect(centerX - thickness * 0.5f, top, thickness, std::max(0.0f, bottom - top))};
}

/**
 * @brief Resolves a UI sprite asset name to an Rml-safe combat sprite path if the file exists.
 *
 * If `assetName` is empty or no matching file is found, returns an empty string.
 *
 * @param assetName Base asset name (without extension) to resolve.
 * @return std::string Rml-safe path for "assets/combat/sprites/{assetName}.{ext}" where `{ext}`
 * is "png" or "webp" for the first existing file, or an empty string if none is found.
 */
std::string resolveUiSpritePath(const std::string& assetName) {
    if (assetName.empty()) {
        return {};
    }

    for (const char* extension : {"png", "webp"}) {
        const std::string relativeAssetPath =
            "assets/combat/sprites/" + assetName + "." + extension;
        const std::string candidate = loader::resolveAssetPath(relativeAssetPath);
        if (std::filesystem::exists(candidate)) {
            return platform::path::resolvePathForRml(relativeAssetPath);
        }
    }

    return {};
}

/**
 * @brief Return the first non-empty instruction hint found among a boss's abilities.
 *
 * Checks the boss's `skillAbility`, `ability`, then `ultimate` (in that order) and returns
 * the first non-empty `instructionHint` found in a successfully loaded ability definition.
 *
 * @param boss Boss definition to inspect for ability instruction hints.
 * @return std::string The first non-empty instruction hint from the boss's abilities, or
 * "No special interaction hint is available for this round." if none are available.
 */
std::string resolveInstructionHint(const BossDefinition& boss) {
    AbilityDefinition ability;
    for (const std::string* abilityId : {&boss.skillAbility, &boss.ability, &boss.ultimate}) {
        if (abilityId->empty()) {
            continue;
        }
        if (loader::loadAbilityDefinition(*abilityId, ability) && !ability.instructionHint.empty()) {
            return ability.instructionHint;
        }
    }
    return "No special interaction hint is available for this round.";
}

}  /**
 * @brief Builds a bracket model for the story competition using player progression.
 *
 * Populates @p outModel with rounds, player and boss data, sprite paths, instruction
 * hints, the index of the next uncleared round, and whether the finale is unlocked.
 * @p outModel is reset to a default-initialized state on entry.
 *
 * @param progression Player progression used to determine which rounds are cleared.
 * @param outModel Destination model that will be populated with bracket data.
 * @param outError Optional output for a human-readable error message when loading fails.
 * @return true if a non-empty BracketModel was produced; `false` if loading failed or no rounds were available.
 */

bool buildBracketModel(const PlayerProgression& progression,
                       BracketModel& outModel,
                       std::string* outError) {
    outModel = BracketModel{};

    std::vector<BattleDefinition> competitionBattles;
    if (!loader::loadStoryCompetitionBattles(competitionBattles)) {
        if (outError != nullptr) {
            *outError = "Failed to load story competition battles.";
        }
        return false;
    }

    if (!loader::loadCharacterDefinition("miku", outModel.player)) {
        if (outError != nullptr) {
            *outError = "Failed to load Hatsune Miku definition.";
        }
        return false;
    }
    outModel.playerSpritePath = resolveUiSpritePath(outModel.player.assets);

    bool foundUnclearedRound = false;
    for (const BattleDefinition& battle : competitionBattles) {
        BossDefinition boss;
        if (!loader::loadBossDefinition(battle.bossKey, boss)) {
            if (outError != nullptr) {
                *outError = "Failed to load boss definition for " + battle.key + ".";
            }
            return false;
        }

        RoundEntry entry;
        entry.battle = battle;
        entry.boss = boss;
        entry.spritePath = resolveUiSpritePath(boss.assets);
        entry.instructionHint = resolveInstructionHint(boss);
        entry.cleared = ::battle::hasClearedBattle(progression, battle.key);

        if (!foundUnclearedRound && !entry.cleared) {
            outModel.nextUnclearedIndex = outModel.rounds.size();
            foundUnclearedRound = true;
        }

        outModel.rounds.push_back(std::move(entry));
    }

    if (!foundUnclearedRound) {
        outModel.nextUnclearedIndex = outModel.rounds.size();
    }

    outModel.finaleUnlocked =
        !outModel.rounds.empty() &&
        std::all_of(outModel.rounds.begin(), outModel.rounds.end(), [](const RoundEntry& round) {
            return round.cleared;
        });
    return !outModel.rounds.empty();
}

/**
 * @brief Finds the zero-based index of the round matching the given battle key.
 *
 * Searches the model's rounds for an entry whose battle key equals `battleKey`.
 *
 * @param battleKey The battle identifier to locate; if empty, the function returns `std::nullopt`.
 * @return std::optional<std::size_t> Containing the index of the matching round, or `std::nullopt` if no match is found or `battleKey` is empty.
 */
std::optional<std::size_t> findRoundIndex(const BracketModel& model, const std::string& battleKey) {
    if (battleKey.empty()) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < model.rounds.size(); ++i) {
        if (model.rounds[i].battle.key == battleKey) {
            return i;
        }
    }

    return std::nullopt;
}

/**
 * @brief Determines which round index should be targeted based on an explicit battle key or progression state.
 *
 * If `battleKey` corresponds to a round in `model`, that round's index is returned. Otherwise, when the model has
 * no rounds returns `0`; if `model.nextUnclearedIndex` points to an existing round returns that index; otherwise
 * returns the last round index.
 *
 * @param model Bracket model containing rounds and progression metadata.
 * @param battleKey Optional battle key used to select a specific round; an empty or non-matching key is ignored.
 * @return std::size_t Index of the targeted round (clamped to a valid index when necessary).
 */
std::size_t resolveTargetRoundIndex(const BracketModel& model, const std::string& battleKey) {
    if (const std::optional<std::size_t> roundIndex = findRoundIndex(model, battleKey); roundIndex.has_value()) {
        return *roundIndex;
    }

    if (model.rounds.empty()) {
        return 0;
    }
    if (model.nextUnclearedIndex < model.rounds.size()) {
        return model.nextUnclearedIndex;
    }
    return model.rounds.size() - 1;
}

/**
 * @brief Determine the visual state for a bracket round node.
 *
 * Evaluates the given round's cleared status, selection, and practice mode to
 * choose the appropriate UI state.
 *
 * - If `roundIndex` is out of range, the state is `RoundNodeState::Future`.
 * - In practice mode:
 *   - Uncleared rounds are `RoundNodeState::Locked`.
 *   - Cleared rounds are `RoundNodeState::Current` when `roundIndex == selectedIndex`, otherwise `RoundNodeState::Cleared`.
 * - In normal mode:
 *   - Cleared rounds are `RoundNodeState::Cleared`.
 *   - Unlocked (uncleared) rounds are `RoundNodeState::Current` when `roundIndex == selectedIndex`, otherwise `RoundNodeState::Future`.
 *
 * @param model Bracket model containing rounds and their cleared states.
 * @param roundIndex Index of the round to evaluate.
 * @param selectedIndex Index of the currently selected round.
 * @param practiceMode If true, use practice-mode visibility rules (locks uncleared rounds).
 * @return RoundNodeState The node state: `Locked`, `Current`, `Cleared`, or `Future`.
 */
RoundNodeState resolveRoundState(const BracketModel& model,
                                 std::size_t roundIndex,
                                 std::size_t selectedIndex,
                                 bool practiceMode) {
    if (roundIndex >= model.rounds.size()) {
        return RoundNodeState::Future;
    }

    const RoundEntry& round = model.rounds[roundIndex];
    if (practiceMode) {
        if (!round.cleared) {
            return RoundNodeState::Locked;
        }
        return roundIndex == selectedIndex ? RoundNodeState::Current : RoundNodeState::Cleared;
    }

    if (round.cleared) {
        return RoundNodeState::Cleared;
    }
    return roundIndex == selectedIndex ? RoundNodeState::Current : RoundNodeState::Future;
}

/**
 * @brief Compute the geometric layout and connecting segments for the story competition bracket UI.
 *
 * Given a populated BracketModel and a LayoutSpec describing sizes and insets, produces a BracketLayout
 * containing the finale rectangle, per-round player/rival/ghost rectangles, routing segments (joins,
 * branches, ghost links) and advance path segments between rounds. If the model has no rounds, an
 * empty layout is returned.
 *
 * @param model Source BracketModel with rounds and their state.
 * @param spec LayoutSpec specifying dimensions, insets, and spacing used to compute positions.
 * @return BracketLayout Geometry and connecting segments for rendering the bracket UI.
 */
BracketLayout buildBracketLayout(const BracketModel& model, const LayoutSpec& spec) {
    BracketLayout layout;
    if (model.rounds.empty()) {
        return layout;
    }

    const std::size_t roundCount = model.rounds.size();
    layout.finaleRect = makeRect((spec.width - spec.finaleWidth) * 0.5f,
                                 spec.topInset,
                                 spec.finaleWidth,
                                 spec.finaleHeight);

    const float routeTopY = layout.finaleRect.y + layout.finaleRect.height + 28.0f;
    const float routeBottomY = spec.height - spec.bottomInset - spec.rivalHeight * 0.5f;
    const float routeSpanY = std::max(0.0f, routeBottomY - routeTopY);
    const float stepY = roundCount > 1
        ? (routeSpanY / static_cast<float>(roundCount - 1))
        : 0.0f;
    const float leftDrift = std::max(56.0f, spec.width * 0.12f);
    const float rightDrift = std::max(44.0f, spec.width * 0.1f);
    const float joinDrift = std::max(32.0f, spec.width * 0.06f);
    const float routeBaseX = spec.leftInset;
    const float rivalBaseX = spec.width - spec.rightInset - spec.rivalWidth;
    const float branchGap = std::max(18.0f, spec.width * 0.025f);
    const float ghostGapX = std::max(30.0f, spec.width * 0.055f);
    const float ghostGapY = std::max(26.0f, stepY * 0.28f);

    layout.rounds.reserve(roundCount);
    for (std::size_t i = 0; i < roundCount; ++i) {
        const float progress = roundCount > 1
            ? static_cast<float>(i) / static_cast<float>(roundCount - 1)
            : 0.0f;
        const float centerY = routeBottomY - progress * routeSpanY;
        const float playerX = routeBaseX + progress * leftDrift;
        const float rivalX = rivalBaseX - progress * rightDrift;
        const float joinX = std::clamp(playerX + spec.playerWidth + 44.0f + progress * joinDrift,
                                       playerX + spec.playerWidth + 24.0f,
                                       rivalX - 28.0f);
        const float branchX = rivalX + spec.rivalWidth + branchGap;
        const float ghostX = std::min(spec.width - spec.rightInset - spec.ghostWidth,
                                      branchX + ghostGapX);

        RoundLayout round;
        round.playerRect = makeRect(playerX,
                                    centerY - spec.playerHeight * 0.5f,
                                    spec.playerWidth,
                                    spec.playerHeight);
        round.rivalRect = makeRect(rivalX,
                                   centerY - spec.rivalHeight * 0.5f,
                                   spec.rivalWidth,
                                   spec.rivalHeight);
        round.rivalIntroRect = round.rivalRect;
        round.rivalIntroRect.x = std::min(spec.width - spec.rightInset - spec.rivalWidth * 0.7f,
                                          round.rivalRect.x + std::max(18.0f, spec.rivalWidth * 0.2f));
        round.ghostTopRect = makeRect(ghostX,
                                      centerY - ghostGapY - spec.ghostHeight * 0.5f,
                                      spec.ghostWidth,
                                      spec.ghostHeight);
        round.ghostBottomRect = makeRect(ghostX,
                                         centerY + ghostGapY - spec.ghostHeight * 0.5f,
                                         spec.ghostWidth,
                                         spec.ghostHeight);

        const float playerJoinX = round.playerRect.x + round.playerRect.width;
        const float rivalJoinX = round.rivalRect.x;
        round.playerToJoin = makeHorizontalSegment(playerJoinX, joinX, centerY);
        round.joinToRival = makeHorizontalSegment(joinX, rivalJoinX, centerY);
        round.rivalToBranch =
            makeHorizontalSegment(round.rivalRect.x + round.rivalRect.width, branchX, centerY);
        round.branchVertical =
            makeVerticalSegment(branchX,
                                round.ghostTopRect.y + round.ghostTopRect.height * 0.5f,
                                round.ghostBottomRect.y + round.ghostBottomRect.height * 0.5f);
        round.branchToGhostTop =
            makeHorizontalSegment(branchX,
                                  round.ghostTopRect.x,
                                  round.ghostTopRect.y + round.ghostTopRect.height * 0.5f);
        round.branchToGhostBottom =
            makeHorizontalSegment(branchX,
                                  round.ghostBottomRect.x,
                                  round.ghostBottomRect.y + round.ghostBottomRect.height * 0.5f);

        layout.rounds.push_back(std::move(round));
    }

    layout.playerStartRect = layout.rounds.front().playerRect;
    for (std::size_t i = 0; i < layout.rounds.size(); ++i) {
        const float centerY = layout.rounds[i].playerRect.y + layout.rounds[i].playerRect.height * 0.5f;
        const float joinX = layout.rounds[i].playerToJoin.rect.x + layout.rounds[i].playerToJoin.rect.width;
        if (i + 1 < layout.rounds.size()) {
            const float nextCenterY =
                layout.rounds[i + 1].playerRect.y + layout.rounds[i + 1].playerRect.height * 0.5f;
            const float nextJoinX =
                layout.rounds[i + 1].playerToJoin.rect.x + layout.rounds[i + 1].playerToJoin.rect.width;
            layout.rounds[i].advanceSegments.push_back(makeVerticalSegment(joinX, centerY, nextCenterY));
            layout.rounds[i].advanceSegments.push_back(makeHorizontalSegment(joinX, nextJoinX, nextCenterY));
        } else {
            const float finaleCenterX = layout.finaleRect.x + layout.finaleRect.width * 0.5f;
            const float finaleBottomY = layout.finaleRect.y + layout.finaleRect.height;
            layout.rounds[i].advanceSegments.push_back(makeVerticalSegment(joinX, centerY, finaleBottomY + 16.0f));
            layout.rounds[i].advanceSegments.push_back(
                makeHorizontalSegment(joinX, finaleCenterX, finaleBottomY + 16.0f));
        }
    }

    return layout;
}

}  // namespace battle::competition
