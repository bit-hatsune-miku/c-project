#include "battle_party_setup.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <SDL2/SDL.h>

#include "../../platform/path_resolution.h"
#include "../core/battle_loader.h"

namespace battle::demo {

namespace {

using ui::kReferenceHeight;
using ui::kReferenceWidth;
using ui::kStartTransitionBlackHoldMs;
using ui::kStartTransitionDurationMs;
using ui::kStartTransitionFadeMs;
using ui::kStartTransitionFirstSweepMs;
using ui::kStartTransitionSecondSweepMs;
using ui::kVisibleRosterRows;

} // namespace

bool BattlePartySetupScreen::initialize(SDL_Renderer* renderer, const BattleDefinition& battleDefinition) {
    shutdown();

    battleDefinition_ = battleDefinition;
    result_.bossKey = battleDefinition.bossKey;
    if (!validateBattleDefinition()) {
        return false;
    }
    if (!loadRoster(renderer)) {
        return false;
    }
    seedSelection();
    if (battleDefinition_.isLineupFixed && !canStart()) {
        std::cerr << "[Battle] Fixed lineup battle requires exactly " << battleDefinition_.partySize
                  << " valid party members: " << battleDefinition_.key << "\n";
        shutdown();
        return false;
    }
    syncResult();
    resetInteractionState();

    if (battleDefinition_.isLineupFixed) {
        skipSetup_ = true;
        startRequested_ = true;
    }

    loadFontsForScale(1.0f);
    initialized_ = true;
    return true;
}

bool BattlePartySetupScreen::validateBattleDefinition() const {
    if (battleDefinition_.bossKey.empty()) {
        std::cerr << "[Battle] Battle definition missing bossKey: " << battleDefinition_.key << "\n";
        return false;
    }
    if (battleDefinition_.partySize < 1 || battleDefinition_.partySize > 4) {
        std::cerr << "[Battle] Battle definition has invalid partySize: " << battleDefinition_.key << "\n";
        return false;
    }

    std::vector<std::string> uniqueLocked;
    for (const std::string& key : battleDefinition_.lockedLineup) {
        if (std::find(uniqueLocked.begin(), uniqueLocked.end(), key) != uniqueLocked.end()) {
            std::cerr << "[Battle] Battle definition contains duplicate locked lineup entries: "
                      << battleDefinition_.key << "\n";
            return false;
        }
        uniqueLocked.push_back(key);
    }
    if (static_cast<int>(uniqueLocked.size()) > battleDefinition_.partySize) {
        std::cerr << "[Battle] Battle definition locks more characters than allowed by partySize: "
                  << battleDefinition_.key << "\n";
        return false;
    }

    if (battleDefinition_.isLineupFixed) {
        std::vector<std::string> merged = uniqueLocked;
        for (const std::string& key : battleDefinition_.lineup) {
            if (std::find(merged.begin(), merged.end(), key) == merged.end()) {
                merged.push_back(key);
            }
        }
        if (static_cast<int>(merged.size()) != battleDefinition_.partySize) {
            std::cerr << "[Battle] Fixed lineup battle must define exactly partySize unique characters: "
                      << battleDefinition_.key << "\n";
            return false;
        }
    }

    return true;
}

bool BattlePartySetupScreen::loadRoster(SDL_Renderer* renderer) {
    std::vector<CharacterDefinition> rosterCharacters;
    if (!loader::loadAllCharacterDefinitions(rosterCharacters)) {
        return false;
    }

    roster_.reserve(rosterCharacters.size());
    for (const CharacterDefinition& character : rosterCharacters) {
        Entry entry;
        entry.character = character;
        entry.iconTexture = ui::loadTexture(renderer, ui::findTexturePath("icons", character.assets));
        entry.spriteTexture = ui::loadTexture(renderer, ui::findTexturePath("sprites", character.assets));
        rosterIndexByKey_.emplace(character.key, static_cast<int>(roster_.size()));
        roster_.push_back(entry);
    }

    textures_.lock = ui::loadTexture(renderer, platform::path::resolvePath("assets/ui/party_setup/lock.png"));
    textures_.atk = ui::loadTexture(renderer, platform::path::resolvePath("assets/ui/party_setup/atk.png"));
    textures_.def = ui::loadTexture(renderer, platform::path::resolvePath("assets/ui/party_setup/def.png"));
    textures_.spd = ui::loadTexture(renderer, platform::path::resolvePath("assets/ui/party_setup/spd.png"));
    textures_.bigPlus = ui::loadTexture(renderer, platform::path::resolvePath("assets/ui/party_setup/big_plus.png"));
    textures_.remove = ui::loadTexture(renderer, platform::path::resolvePath("assets/ui/party_setup/x_remove.png"));
    return true;
}

void BattlePartySetupScreen::seedSelection() {
    selectedKeys_.clear();

    const auto appendIfAvailable = [this](const std::string& key) {
        if (findRosterIndexByKey(key) >= 0 && !isSelectedCharacter(key) &&
            static_cast<int>(selectedKeys_.size()) < battleDefinition_.partySize) {
            selectedKeys_.push_back(key);
        }
    };

    for (const std::string& key : battleDefinition_.lockedLineup) {
        appendIfAvailable(key);
    }
    for (const std::string& key : battleDefinition_.lineup) {
        appendIfAvailable(key);
    }

    if (selectedKeys_.empty() && !roster_.empty() && !battleDefinition_.isLineupFixed) {
        selectedKeys_.push_back(roster_.front().character.key);
    }
}

void BattlePartySetupScreen::resetInteractionState() {
    focusedRosterIndex_ = 0;
    scrollOffset_ = 0;
    hoveredRosterIndex_ = -1;
    hoveredSelectedSlot_ = -1;
    hoveredStartButton_ = false;
    lastMouseMoveTick_ = 0;
    draggingScrollbar_ = false;
    scrollbarDragGrabOffset_ = 0.0f;
    startTransitionActive_ = false;
    startTransitionTick_ = 0;
}

void BattlePartySetupScreen::releaseFonts() {
#ifdef BATTLE_ENABLE_TTF
    if (fonts_.title != nullptr) {
        TTF_CloseFont(fonts_.title);
        fonts_.title = nullptr;
    }
    if (fonts_.body != nullptr) {
        TTF_CloseFont(fonts_.body);
        fonts_.body = nullptr;
    }
    if (fonts_.small != nullptr) {
        TTF_CloseFont(fonts_.small);
        fonts_.small = nullptr;
    }
    if (fonts_.rosterName != nullptr) {
        TTF_CloseFont(fonts_.rosterName);
        fonts_.rosterName = nullptr;
    }
    if (fonts_.rosterMeta != nullptr) {
        TTF_CloseFont(fonts_.rosterMeta);
        fonts_.rosterMeta = nullptr;
    }
    if (fonts_.cjkTitle != nullptr) {
        TTF_CloseFont(fonts_.cjkTitle);
        fonts_.cjkTitle = nullptr;
    }
    if (fonts_.cjkBody != nullptr) {
        TTF_CloseFont(fonts_.cjkBody);
        fonts_.cjkBody = nullptr;
    }
    if (fonts_.cjkSmall != nullptr) {
        TTF_CloseFont(fonts_.cjkSmall);
        fonts_.cjkSmall = nullptr;
    }
    if (fonts_.cjkRosterName != nullptr) {
        TTF_CloseFont(fonts_.cjkRosterName);
        fonts_.cjkRosterName = nullptr;
    }
    if (fonts_.cjkRosterMeta != nullptr) {
        TTF_CloseFont(fonts_.cjkRosterMeta);
        fonts_.cjkRosterMeta = nullptr;
    }
#endif
    fontScalePercent_ = -1;
}

void BattlePartySetupScreen::loadFontsForScale(float scale) {
    const int requestedPercent = std::max(1, static_cast<int>(std::lround(scale * 100.0f)));
    if (requestedPercent == fontScalePercent_) {
        return;
    }

    releaseFonts();
    const float appliedScale = static_cast<float>(requestedPercent) / 100.0f;
    fonts_.title = ui::openFont(std::max(1, static_cast<int>(std::lround(40.0f * appliedScale))));
    fonts_.body = ui::openFont(std::max(1, static_cast<int>(std::lround(22.0f * appliedScale))));
    fonts_.small = ui::openFont(std::max(1, static_cast<int>(std::lround(18.0f * appliedScale))));
    fonts_.rosterName = ui::openFont(std::max(1, static_cast<int>(std::lround(20.0f * appliedScale))));
    fonts_.rosterMeta = ui::openFont(std::max(1, static_cast<int>(std::lround(17.0f * appliedScale))));
    fonts_.cjkTitle = ui::openCjkFont(std::max(1, static_cast<int>(std::lround(40.0f * appliedScale))));
    fonts_.cjkBody = ui::openCjkFont(std::max(1, static_cast<int>(std::lround(22.0f * appliedScale))));
    fonts_.cjkSmall = ui::openCjkFont(std::max(1, static_cast<int>(std::lround(18.0f * appliedScale))));
    fonts_.cjkRosterName = ui::openCjkFont(std::max(1, static_cast<int>(std::lround(20.0f * appliedScale))));
    fonts_.cjkRosterMeta = ui::openCjkFont(std::max(1, static_cast<int>(std::lround(17.0f * appliedScale))));
    fontScalePercent_ = requestedPercent;
}

void BattlePartySetupScreen::releaseTextures() {
    if (textures_.lock != nullptr) {
        SDL_DestroyTexture(textures_.lock);
        textures_.lock = nullptr;
    }
    if (textures_.atk != nullptr) {
        SDL_DestroyTexture(textures_.atk);
        textures_.atk = nullptr;
    }
    if (textures_.def != nullptr) {
        SDL_DestroyTexture(textures_.def);
        textures_.def = nullptr;
    }
    if (textures_.spd != nullptr) {
        SDL_DestroyTexture(textures_.spd);
        textures_.spd = nullptr;
    }
    if (textures_.bigPlus != nullptr) {
        SDL_DestroyTexture(textures_.bigPlus);
        textures_.bigPlus = nullptr;
    }
    if (textures_.remove != nullptr) {
        SDL_DestroyTexture(textures_.remove);
        textures_.remove = nullptr;
    }
}

void BattlePartySetupScreen::shutdown() {
    for (Entry& entry : roster_) {
        if (entry.iconTexture != nullptr) {
            SDL_DestroyTexture(entry.iconTexture);
            entry.iconTexture = nullptr;
        }
        if (entry.spriteTexture != nullptr) {
            SDL_DestroyTexture(entry.spriteTexture);
            entry.spriteTexture = nullptr;
        }
    }
    roster_.clear();
    rosterIndexByKey_.clear();
    releaseTextures();
    releaseFonts();

    selectedKeys_.clear();
    result_ = PartySetupResult{};
    battleDefinition_ = BattleDefinition{};
    focusedRosterIndex_ = 0;
    scrollOffset_ = 0;
    hoveredRosterIndex_ = -1;
    hoveredSelectedSlot_ = -1;
    hoveredStartButton_ = false;
    lastMouseMoveTick_ = 0;
    startTransitionActive_ = false;
    startTransitionTick_ = 0;
    initialized_ = false;
    startRequested_ = false;
    cancelRequested_ = false;
    skipSetup_ = false;
}

void BattlePartySetupScreen::handleEvent(const SDL_Event& event, int windowWidth, int windowHeight) {
    if (!initialized_ || skipSetup_) {
        return;
    }
    if (isStartTransitionActive()) {
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        handlePointerDown(static_cast<float>(event.button.x),
                          static_cast<float>(event.button.y),
                          windowWidth,
                          windowHeight);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        releasePointer();
        return;
    }

    if (event.type == SDL_MOUSEWHEEL) {
        scrollOffset_ -= event.wheel.y;
        clampScrollOffset();
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        updateHoverState(static_cast<float>(event.motion.x),
                         static_cast<float>(event.motion.y),
                         windowWidth,
                         windowHeight);
        if (draggingScrollbar_) {
            float refX = 0.0f;
            float refY = 0.0f;
            if (ui::mapWindowPointToReference(static_cast<float>(event.motion.x),
                                              static_cast<float>(event.motion.y),
                                              windowWidth,
                                              windowHeight,
                                              refX,
                                              refY)) {
                setScrollFromPointerY(refY - scrollbarDragGrabOffset_);
            }
        }
        return;
    }

    if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        hoveredRosterIndex_ = -1;
        hoveredSelectedSlot_ = -1;
        hoveredStartButton_ = false;
        lastMouseMoveTick_ = 0;
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                cancelRequested_ = true;
                break;
            case SDLK_UP:
                if (!roster_.empty()) {
                    focusedRosterIndex_ = (focusedRosterIndex_ - 1 + static_cast<int>(roster_.size())) %
                                          static_cast<int>(roster_.size());
                    ensureFocusedEntryVisible();
                }
                break;
            case SDLK_DOWN:
                if (!roster_.empty()) {
                    focusedRosterIndex_ = (focusedRosterIndex_ + 1) % static_cast<int>(roster_.size());
                    ensureFocusedEntryVisible();
                }
                break;
            case SDLK_PAGEUP:
                scrollOffset_ -= kVisibleRosterRows;
                clampScrollOffset();
                focusedRosterIndex_ = std::clamp(focusedRosterIndex_, scrollOffset_, scrollOffset_ + kVisibleRosterRows - 1);
                break;
            case SDLK_PAGEDOWN:
                scrollOffset_ += kVisibleRosterRows;
                clampScrollOffset();
                focusedRosterIndex_ = std::clamp(focusedRosterIndex_, scrollOffset_, scrollOffset_ + kVisibleRosterRows - 1);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (canStart()) {
                    beginStartTransition();
                } else {
                    toggleFocusedCharacter();
                }
                break;
            case SDLK_SPACE:
                toggleFocusedCharacter();
                break;
            case SDLK_s:
                if (canStart()) {
                    beginStartTransition();
                }
                break;
            default:
                break;
        }
        return;
    }
}

void BattlePartySetupScreen::render(SDL_Renderer* renderer, int windowWidth, int windowHeight) {
    if (!initialized_ || renderer == nullptr || skipSetup_) {
        return;
    }

    const ui::LayoutMetrics layout = ui::computeLayout(windowWidth, windowHeight);
    loadFontsForScale(layout.scale);
    const SDL_FRect screenRect = ui::toWindowRect(SDL_FRect{0.0f, 0.0f, kReferenceWidth, kReferenceHeight}, layout);
    const ui::PartySetupGeometry geometry = ui::computeGeometry();

    ui::fillRect(renderer, screenRect, SDL_Color{8, 14, 30, 255});
    ui::fillRect(renderer,
                 ui::toWindowRect(SDL_FRect{0.0f, 0.0f, kReferenceWidth, 230.0f}, layout),
                 SDL_Color{18, 34, 78, 110});
    ui::fillRect(renderer,
                 ui::toWindowRect(SDL_FRect{0.0f, 460.0f, kReferenceWidth, 260.0f}, layout),
                 SDL_Color{76, 44, 112, 70});

    const SDL_FRect rosterPanel = ui::toWindowRect(geometry.rosterPanel, layout);
    const SDL_FRect stagePanel = ui::toWindowRect(geometry.stagePanel, layout);
    ui::fillRect(renderer, rosterPanel, SDL_Color{10, 18, 36, 220});
    ui::fillRect(renderer, stagePanel, SDL_Color{12, 18, 42, 196});
    ui::drawRect(renderer, rosterPanel, SDL_Color{107, 146, 227, 255});
    ui::drawRect(renderer, stagePanel, SDL_Color{124, 152, 234, 220});

    ui::drawText(renderer,
                 fonts_.title,
                 fonts_.cjkTitle,
                 "PARTY SETUP",
                 SDL_Color{242, 242, 248, 255},
                 ui::toWindowRect(geometry.stageTitleRect, layout));
    ui::drawText(renderer,
                 fonts_.small,
                 fonts_.cjkSmall,
                 battleDefinition_.name.empty() ? "BATTLE" : ui::uppercase(battleDefinition_.name),
                 SDL_Color{127, 211, 255, 255},
                 ui::toWindowRect(geometry.stageKickerRect, layout));
    ui::drawWrappedText(renderer,
                        fonts_.small,
                        fonts_.cjkSmall,
                        battleDefinition_.description.empty()
                            ? "Choose who enters the fight."
                            : battleDefinition_.description,
                        SDL_Color{199, 213, 236, 230},
                        ui::toWindowRect(geometry.stageDescriptionRect, layout));
    ui::drawText(renderer,
                 fonts_.small,
                 fonts_.cjkSmall,
                 "SELECT " + std::to_string(battleDefinition_.partySize) + " MEMBERS",
                 SDL_Color{243, 198, 112, 255},
                 ui::toWindowRect(SDL_FRect{
                     geometry.stagePanel.x + geometry.stagePanel.w - 220.0f,
                     42.0f,
                     220.0f,
                     24.0f
                 }, layout),
                 true);

    renderRosterPanel(renderer, geometry, layout);
    renderSelectedPartyPanel(renderer, geometry, layout);
    renderStartButton(renderer, geometry, layout);
    renderTransitionOverlay(renderer, screenRect, layout);
}

void BattlePartySetupScreen::renderRosterPanel(SDL_Renderer* renderer,
                                               const ui::PartySetupGeometry& geometry,
                                               const ui::LayoutMetrics& layout) {
    ui::drawText(renderer,
                 fonts_.body,
                 fonts_.cjkBody,
                 "ROSTER",
                 SDL_Color{244, 240, 228, 255},
                 ui::toWindowRect(geometry.rosterTitleRect, layout));
    ui::drawText(renderer,
                 fonts_.small,
                 fonts_.cjkSmall,
                 "Space to toggle. Enter to start.",
                 SDL_Color{151, 168, 202, 255},
                 ui::toWindowRect(geometry.rosterHintRect, layout));

    const int visibleEnd = std::min(static_cast<int>(roster_.size()), scrollOffset_ + kVisibleRosterRows);
    const bool mouseHoverActive = isMouseHoverActive();
    for (int index = scrollOffset_; index < visibleEnd; ++index) {
        const Entry& entry = roster_[static_cast<size_t>(index)];
        const int visibleIndex = index - scrollOffset_;
        const SDL_FRect rowRefRect = ui::rosterRowRect(geometry, visibleIndex);
        const SDL_FRect rowRect = ui::toWindowRect(rowRefRect, layout);
        const bool focused = index == focusedRosterIndex_;
        const bool hovered = mouseHoverActive && index == hoveredRosterIndex_;
        const int selectedSlot = selectedSlotForCharacter(entry.character.key);
        const bool selected = selectedSlot >= 0;
        const bool locked = isLockedCharacter(entry.character.key);

        ui::fillRect(renderer, rowRect, selected ? SDL_Color{34, 52, 102, 214} : SDL_Color{18, 28, 54, 206});
        ui::drawRect(renderer,
                     rowRect,
                     (hovered || focused) ? SDL_Color{255, 201, 97, 255}
                                          : SDL_Color{76, 99, 146, 220});

        const float portraitSize = rowRect.h - 20.0f;
        const SDL_FRect portraitRect{rowRect.x + 10.0f, rowRect.y + (rowRect.h - portraitSize) * 0.5f, portraitSize, portraitSize};
        ui::fillRect(renderer, portraitRect, SDL_Color{22, 30, 58, 255});
        if (hovered) {
            ui::fillRect(renderer, portraitRect, SDL_Color{255, 204, 96, 24});
            ui::drawRect(renderer, portraitRect, SDL_Color{255, 214, 128, 48});
        }
        if (entry.iconTexture != nullptr) {
            ui::renderTextureCover(renderer, entry.iconTexture, portraitRect);
        } else if (entry.spriteTexture != nullptr) {
            ui::renderTextureCover(renderer, entry.spriteTexture, portraitRect);
        }

        const float rowTextLeft = portraitRect.x + portraitRect.w + 16.0f;
        const float rightInset = selected ? 72.0f : 20.0f;
        const float nameHeight = std::max(20.0f, static_cast<float>(ui::fontLineHeight(fonts_.rosterName, fonts_.body)));
        const float metaHeight = std::max(16.0f, static_cast<float>(ui::fontLineHeight(fonts_.rosterMeta, fonts_.small)));
        const float textGap = 2.0f;
        const float textBlockHeight = nameHeight + metaHeight + metaHeight + (textGap * 2.0f);
        const float textTop = rowRect.y + std::round((rowRect.h - textBlockHeight) * 0.5f) - 1.0f;
        const SDL_FRect nameRect{rowTextLeft, textTop, rowRect.w - (rowTextLeft - rowRect.x) - rightInset, nameHeight};
        const SDL_FRect classRect{rowTextLeft, textTop + nameHeight + textGap, rowRect.w - (rowTextLeft - rowRect.x) - rightInset, metaHeight};
        const SDL_FRect statsRect{rowTextLeft, textTop + nameHeight + metaHeight + (textGap * 2.0f), rowRect.w - (rowTextLeft - rowRect.x) - rightInset, metaHeight};
        const SDL_Rect clipRect = ui::toClipRect(rowRect);
        SDL_RenderSetClipRect(renderer, &clipRect);
        ui::drawText(renderer,
                     fonts_.rosterName != nullptr ? fonts_.rosterName : fonts_.body,
                     fonts_.cjkRosterName != nullptr ? fonts_.cjkRosterName : fonts_.cjkBody,
                     entry.character.title,
                     SDL_Color{246, 242, 235, 255},
                     nameRect);
        ui::drawText(renderer,
                     fonts_.rosterMeta != nullptr ? fonts_.rosterMeta : fonts_.small,
                     fonts_.cjkRosterMeta != nullptr ? fonts_.cjkRosterMeta : fonts_.cjkSmall,
                     ui::uppercase(entry.character.characterClass),
                     SDL_Color{124, 222, 255, 255},
                     classRect);
        ui::drawText(renderer,
                     fonts_.rosterMeta != nullptr ? fonts_.rosterMeta : fonts_.small,
                     fonts_.cjkRosterMeta != nullptr ? fonts_.cjkRosterMeta : fonts_.cjkSmall,
                     "HP " + std::to_string(entry.character.hp) + "  ATK " + std::to_string(entry.character.atk) +
                         "  SPD " + std::to_string(entry.character.spd),
                     SDL_Color{190, 202, 224, 255},
                     statsRect);
        SDL_RenderSetClipRect(renderer, nullptr);

        if (selected) {
            const SDL_FRect badgeRect = ui::toWindowRect(SDL_FRect{
                rowRefRect.x + rowRefRect.w - 38.0f,
                rowRefRect.y + 14.0f,
                24.0f,
                24.0f
            }, layout);
            ui::fillRect(renderer, badgeRect, SDL_Color{244, 196, 76, 255});
            ui::drawText(renderer,
                         fonts_.body,
                         fonts_.cjkBody,
                         std::to_string(selectedSlot + 1),
                         SDL_Color{18, 24, 46, 255},
                         SDL_FRect{badgeRect.x + 2.0f, badgeRect.y + 1.0f, badgeRect.w - 4.0f, badgeRect.h - 2.0f},
                         true,
                         true);
        }

        if (locked) {
            ui::fillRect(renderer, rowRect, SDL_Color{126, 132, 144, 116});
        }
    }

    if (static_cast<int>(roster_.size()) > kVisibleRosterRows) {
        const SDL_FRect trackRect = ui::toWindowRect(geometry.rosterTrackRect, layout);
        ui::fillRect(renderer, trackRect, SDL_Color{38, 48, 78, 255});
        const SDL_FRect thumbRect = ui::toWindowRect(
            ui::rosterThumbRect(geometry, static_cast<int>(roster_.size()), scrollOffset_),
            layout);
        ui::fillRect(renderer, thumbRect, SDL_Color{246, 198, 82, 255});
    }
}

void BattlePartySetupScreen::renderSelectedPartyPanel(SDL_Renderer* renderer,
                                                      const ui::PartySetupGeometry& geometry,
                                                      const ui::LayoutMetrics& layout) {
    const bool mouseHoverActive = isMouseHoverActive();
    int rosterMaxHp = 1;
    int rosterMaxAtk = 1;
    int rosterMaxSpd = 1;
    for (const Entry& rosterEntry : roster_) {
        rosterMaxHp = std::max(rosterMaxHp, rosterEntry.character.hp);
        rosterMaxAtk = std::max(rosterMaxAtk, rosterEntry.character.atk);
        rosterMaxSpd = std::max(rosterMaxSpd, rosterEntry.character.spd);
    }

    for (int slot = 0; slot < battleDefinition_.partySize; ++slot) {
        const SDL_FRect slotRefRect = ui::selectedCardRect(geometry, slot);
        const SDL_FRect slotRect = ui::toWindowRect(slotRefRect, layout);
        const bool slotHovered = mouseHoverActive && hoveredSelectedSlot_ == slot;
        ui::fillRect(renderer, slotRect, SDL_Color{24, 34, 70, 196});
        ui::drawRect(renderer,
                     slotRect,
                     slotHovered ? SDL_Color{255, 201, 97, 255} : SDL_Color{86, 119, 186, 230});

        if (slot >= static_cast<int>(selectedKeys_.size())) {
            if (textures_.bigPlus != nullptr) {
                SDL_SetTextureAlphaMod(textures_.bigPlus, 92);
                ui::renderTextureContain(renderer,
                                         textures_.bigPlus,
                                         ui::toWindowRect(SDL_FRect{
                                             slotRefRect.x + 84.0f,
                                             slotRefRect.y + 24.0f,
                                             slotRefRect.w - 168.0f,
                                             slotRefRect.h - 48.0f
                                         }, layout));
                SDL_SetTextureAlphaMod(textures_.bigPlus, 255);
            } else {
                ui::drawText(renderer,
                             fonts_.body,
                             fonts_.cjkBody,
                             "EMPTY SLOT",
                             SDL_Color{140, 154, 186, 255},
                             ui::toWindowRect(SDL_FRect{slotRefRect.x, slotRefRect.y + 72.0f, slotRefRect.w, 28.0f}, layout),
                             true);
            }
            continue;
        }

        const int rosterIndex = findRosterIndexByKey(selectedKeys_[static_cast<size_t>(slot)]);
        if (rosterIndex < 0) {
            continue;
        }

        const Entry& entry = roster_[static_cast<size_t>(rosterIndex)];
        const bool slotLocked = shouldShowSelectedLockIcon(slot);
        const SDL_FRect artRect = ui::toWindowRect(SDL_FRect{
            slotRefRect.x + 10.0f,
            slotRefRect.y + 10.0f,
            120.0f,
            slotRefRect.h - 20.0f
        }, layout);
        ui::fillRect(renderer, artRect, SDL_Color{16, 24, 48, 255});
        if (slotHovered) {
            ui::fillRect(renderer, artRect, SDL_Color{255, 204, 96, 20});
            ui::drawRect(renderer, artRect, SDL_Color{255, 214, 128, 40});
        }
        if (entry.spriteTexture != nullptr) {
            ui::renderTextureContain(renderer, entry.spriteTexture, artRect);
        } else if (entry.iconTexture != nullptr) {
            ui::renderTextureContain(renderer, entry.iconTexture, artRect);
        }

        ui::drawText(renderer,
                     fonts_.body,
                     fonts_.cjkBody,
                     ui::uppercase(entry.character.title),
                     SDL_Color{246, 243, 236, 255},
                     ui::toWindowRect(SDL_FRect{slotRefRect.x + 144.0f, slotRefRect.y + 18.0f, slotRefRect.w - 188.0f, 24.0f}, layout));
        ui::drawText(renderer,
                     fonts_.small,
                     fonts_.cjkSmall,
                     ui::uppercase(entry.character.characterClass),
                     SDL_Color{136, 214, 255, 255},
                     ui::toWindowRect(SDL_FRect{slotRefRect.x + 144.0f, slotRefRect.y + 46.0f, slotRefRect.w - 188.0f, 18.0f}, layout));

        const float statColumnLeft = slotRefRect.x + 138.0f;
        const float statColumnRight = slotRefRect.x + slotRefRect.w - 28.0f;
        const float statIconSize = 22.0f;
        const float statValueWidth = 28.0f;
        const float statGap = 6.0f;
        const float statRowHeight = 22.0f;
        const float statBarHeight = 10.0f;
        const float statRowsTop = slotRefRect.y + 76.0f;
        const float statRowSpacing = 10.0f;
        const float statValueLeft = statColumnRight - statValueWidth;
        const float statBarLeft = statColumnLeft + statIconSize + statGap;
        const float statBarWidth = std::max(36.0f, (statValueLeft - statGap) - statBarLeft);

        const SDL_FRect statIconRect1 = ui::toWindowRect(SDL_FRect{statColumnLeft, statRowsTop, statIconSize, statIconSize}, layout);
        const SDL_FRect statIconRect2 = ui::toWindowRect(SDL_FRect{statColumnLeft, statRowsTop + statRowHeight + statRowSpacing, statIconSize, statIconSize}, layout);
        const SDL_FRect statIconRect3 = ui::toWindowRect(SDL_FRect{statColumnLeft, statRowsTop + (statRowHeight + statRowSpacing) * 2.0f, statIconSize, statIconSize}, layout);
        const SDL_FRect statBarRect1 = ui::toWindowRect(SDL_FRect{statBarLeft, statRowsTop + ((statRowHeight - statBarHeight) * 0.5f), statBarWidth, statBarHeight}, layout);
        const SDL_FRect statBarRect2 = ui::toWindowRect(SDL_FRect{statBarLeft, statRowsTop + statRowHeight + statRowSpacing + ((statRowHeight - statBarHeight) * 0.5f), statBarWidth, statBarHeight}, layout);
        const SDL_FRect statBarRect3 = ui::toWindowRect(SDL_FRect{statBarLeft, statRowsTop + (statRowHeight + statRowSpacing) * 2.0f + ((statRowHeight - statBarHeight) * 0.5f), statBarWidth, statBarHeight}, layout);
        const SDL_FRect statValueRect1 = ui::toWindowRect(SDL_FRect{statValueLeft, statRowsTop - 1.0f, statValueWidth, 22.0f}, layout);
        const SDL_FRect statValueRect2 = ui::toWindowRect(SDL_FRect{statValueLeft, statRowsTop + statRowHeight + statRowSpacing - 1.0f, statValueWidth, 22.0f}, layout);
        const SDL_FRect statValueRect3 = ui::toWindowRect(SDL_FRect{statValueLeft, statRowsTop + (statRowHeight + statRowSpacing) * 2.0f - 1.0f, statValueWidth, 22.0f}, layout);

        if (textures_.def != nullptr) {
            ui::renderTextureContain(renderer, textures_.def, statIconRect1);
        }
        if (textures_.atk != nullptr) {
            ui::renderTextureContain(renderer, textures_.atk, statIconRect2);
        }
        if (textures_.spd != nullptr) {
            ui::renderTextureContain(renderer, textures_.spd, statIconRect3);
        }
        ui::drawProgressBar(renderer, statBarRect1,
                            static_cast<float>(entry.character.hp) / static_cast<float>(rosterMaxHp),
                            SDL_Color{83, 201, 121, 208});
        ui::drawProgressBar(renderer, statBarRect2,
                            static_cast<float>(entry.character.atk) / static_cast<float>(rosterMaxAtk),
                            SDL_Color{255, 162, 86, 208});
        ui::drawProgressBar(renderer, statBarRect3,
                            static_cast<float>(entry.character.spd) / static_cast<float>(rosterMaxSpd),
                            SDL_Color{69, 188, 255, 208});
        ui::drawText(renderer, fonts_.small, fonts_.cjkSmall, std::to_string(entry.character.hp),
                     SDL_Color{233, 235, 241, 176}, statValueRect1);
        ui::drawText(renderer, fonts_.small, fonts_.cjkSmall, std::to_string(entry.character.atk),
                     SDL_Color{233, 235, 241, 176}, statValueRect2);
        ui::drawText(renderer, fonts_.small, fonts_.cjkSmall, std::to_string(entry.character.spd),
                     SDL_Color{233, 235, 241, 176}, statValueRect3);

        const SDL_FRect slotBadge = ui::toWindowRect(
            SDL_FRect{slotRefRect.x + slotRefRect.w - 44.0f, slotRefRect.y + 14.0f, 30.0f, 30.0f},
            layout);
        ui::fillRect(renderer, slotBadge, SDL_Color{246, 198, 82, 255});
        if (slotLocked && textures_.lock != nullptr) {
            ui::renderTexturePixelPerfect(renderer, textures_.lock, slotBadge);
        } else if (slotHovered && textures_.remove != nullptr) {
            ui::renderTexturePixelPerfect(renderer, textures_.remove,
                                          SDL_FRect{slotBadge.x + 1.0f, slotBadge.y + 1.0f, slotBadge.w - 2.0f, slotBadge.h - 2.0f});
        } else {
            ui::drawText(renderer,
                         fonts_.body,
                         fonts_.cjkBody,
                         std::to_string(slot + 1),
                         SDL_Color{18, 24, 46, 255},
                         SDL_FRect{slotBadge.x + 2.0f, slotBadge.y + 1.0f, slotBadge.w - 4.0f, slotBadge.h - 2.0f},
                         true,
                         true);
        }

        if (slotLocked) {
            ui::fillRect(renderer, slotRect, SDL_Color{164, 170, 178, 28});
        }
    }
}

void BattlePartySetupScreen::renderStartButton(SDL_Renderer* renderer,
                                               const ui::PartySetupGeometry& geometry,
                                               const ui::LayoutMetrics& layout) {
    const SDL_FRect startButton = ui::toWindowRect(geometry.startButton, layout);
    const bool startHovered = isMouseHoverActive() && hoveredStartButton_ && canStart();
    ui::fillRect(renderer,
                 startButton,
                 !canStart() ? SDL_Color{104, 110, 124, 210}
                             : (startHovered ? SDL_Color{247, 205, 88, 255} : SDL_Color{237, 230, 215, 255}));
    ui::drawRect(renderer,
                 startButton,
                 !canStart() ? SDL_Color{142, 146, 156, 220}
                             : (startHovered ? SDL_Color{255, 228, 144, 255} : SDL_Color{255, 240, 202, 255}));
    ui::drawText(renderer,
                 fonts_.title,
                 fonts_.cjkTitle,
                 "START BATTLE",
                 !canStart() ? SDL_Color{58, 62, 76, 255}
                             : (startHovered ? SDL_Color{56, 40, 12, 255} : SDL_Color{42, 48, 72, 255}),
                 startButton,
                 true,
                 true);
    ui::drawText(renderer,
                 fonts_.small,
                 fonts_.cjkSmall,
                 std::to_string(static_cast<int>(selectedKeys_.size())) + " / " + std::to_string(battleDefinition_.partySize) + " selected",
                 SDL_Color{204, 216, 236, 255},
                 ui::toWindowRect(geometry.selectedCountRect, layout),
                 true);
}

void BattlePartySetupScreen::renderTransitionOverlay(SDL_Renderer* renderer,
                                                     const SDL_FRect& screenRect,
                                                     const ui::LayoutMetrics& layout) {
    if (!isStartTransitionActive()) {
        return;
    }

    const Uint32 elapsedMs = SDL_GetTicks() - startTransitionTick_;
    const float elapsed = static_cast<float>(elapsedMs);
    const float fadeEnd = static_cast<float>(kStartTransitionFadeMs);
    const float firstSweepEnd = fadeEnd + static_cast<float>(kStartTransitionFirstSweepMs);
    const float secondSweepEnd = firstSweepEnd + static_cast<float>(kStartTransitionSecondSweepMs);
    const float totalEnd = secondSweepEnd + static_cast<float>(kStartTransitionBlackHoldMs);

    const float fadeProgress = ui::easeOutCubic(ui::remap01(elapsed, 0.0f, fadeEnd));
    ui::fillRect(renderer,
                 screenRect,
                 SDL_Color{0, 0, 0, static_cast<Uint8>(std::lround(255.0f * fadeProgress))});

    const float sweepWidth = std::max(110.0f, kReferenceWidth * 0.16f) * layout.scale;
    const float sweepTravel = screenRect.w + sweepWidth * 2.0f;
    const auto drawSweep = [&](float phaseStartMs, float phaseEndMs, float peakAlpha) {
        const float local = ui::remap01(elapsed, phaseStartMs, phaseEndMs);
        if (local <= 0.0f || local >= 1.0f) {
            return;
        }
        const float motion = ui::easeInOutSine(local);
        const float alphaCurve = std::sin(local * ui::kPi);
        const float sweepCenterX = screenRect.x - sweepWidth + sweepTravel * motion;
        ui::fillRect(renderer,
                     SDL_FRect{sweepCenterX - sweepWidth * 0.5f, screenRect.y, sweepWidth, screenRect.h},
                     SDL_Color{255, 206, 108, static_cast<Uint8>(std::lround(peakAlpha * alphaCurve))});
    };

    drawSweep(fadeEnd, firstSweepEnd, 92.0f);
    drawSweep(firstSweepEnd, secondSweepEnd, 76.0f);

    const float finalHold = ui::remap01(elapsed, secondSweepEnd, totalEnd);
    ui::fillRect(renderer,
                 screenRect,
                 SDL_Color{0, 0, 0, static_cast<Uint8>(std::lround(255.0f * finalHold))});
}

bool BattlePartySetupScreen::isActive() const {
    return initialized_ && !skipSetup_;
}

bool BattlePartySetupScreen::shouldSkipSetup() const {
    return initialized_ && skipSetup_;
}

bool BattlePartySetupScreen::consumeStartRequest(PartySetupResult& outResult) {
    if (!initialized_ || !startRequested_) {
        return false;
    }
    if (!skipSetup_ && !canStart()) {
        return false;
    }
    if (startTransitionActive_ && (SDL_GetTicks() - startTransitionTick_) < kStartTransitionDurationMs) {
        return false;
    }

    outResult = result_;
    startRequested_ = false;
    startTransitionActive_ = false;
    startTransitionTick_ = 0;
    return true;
}

bool BattlePartySetupScreen::consumeCancelRequest() {
    if (!cancelRequested_) {
        return false;
    }
    cancelRequested_ = false;
    return true;
}

void BattlePartySetupScreen::complete() {
    if (!initialized_) {
        return;
    }
    skipSetup_ = true;
    startTransitionActive_ = false;
    startTransitionTick_ = 0;
}

const PartySetupResult& BattlePartySetupScreen::currentSelection() const {
    return result_;
}

bool BattlePartySetupScreen::canStart() const {
    return static_cast<int>(selectedKeys_.size()) == battleDefinition_.partySize;
}

bool BattlePartySetupScreen::isMouseHoverActive() const {
    return lastMouseMoveTick_ != 0 && (SDL_GetTicks() - lastMouseMoveTick_) <= ui::kMouseHoverTimeoutMs;
}

int BattlePartySetupScreen::findRosterIndexByKey(const std::string& characterKey) const {
    const auto it = rosterIndexByKey_.find(characterKey);
    if (it != rosterIndexByKey_.end()) {
        return it->second;
    }
    return -1;
}

bool BattlePartySetupScreen::isLockedCharacter(const std::string& characterKey) const {
    return std::find(battleDefinition_.lockedLineup.begin(),
                     battleDefinition_.lockedLineup.end(),
                     characterKey) != battleDefinition_.lockedLineup.end();
}

bool BattlePartySetupScreen::isSelectedCharacter(const std::string& characterKey) const {
    return std::find(selectedKeys_.begin(), selectedKeys_.end(), characterKey) != selectedKeys_.end();
}

int BattlePartySetupScreen::selectedSlotForCharacter(const std::string& characterKey) const {
    for (int index = 0; index < static_cast<int>(selectedKeys_.size()); ++index) {
        if (selectedKeys_[static_cast<size_t>(index)] == characterKey) {
            return index;
        }
    }
    return -1;
}

bool BattlePartySetupScreen::shouldShowSelectedLockIcon(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(selectedKeys_.size())) {
        return false;
    }
    return isLockedCharacter(selectedKeys_[static_cast<size_t>(slotIndex)]);
}

void BattlePartySetupScreen::clampScrollOffset() {
    const int maxOffset = std::max(0, static_cast<int>(roster_.size()) - kVisibleRosterRows);
    scrollOffset_ = std::clamp(scrollOffset_, 0, maxOffset);
}

void BattlePartySetupScreen::ensureFocusedEntryVisible() {
    clampScrollOffset();
    if (focusedRosterIndex_ < scrollOffset_) {
        scrollOffset_ = focusedRosterIndex_;
    } else if (focusedRosterIndex_ >= scrollOffset_ + kVisibleRosterRows) {
        scrollOffset_ = focusedRosterIndex_ - kVisibleRosterRows + 1;
    }
    clampScrollOffset();
}

void BattlePartySetupScreen::setScrollFromPointerY(float refY) {
    const ui::PartySetupGeometry geometry = ui::computeGeometry();
    const SDL_FRect thumbRect = ui::rosterThumbRect(geometry, static_cast<int>(roster_.size()), scrollOffset_);
    const float usableTop = geometry.rosterTrackRect.y;
    const float usableTravel = std::max(1.0f, geometry.rosterTrackRect.h - thumbRect.h);
    const float clampedY = std::clamp(refY, usableTop, usableTop + usableTravel);
    const float ratio = (clampedY - usableTop) / usableTravel;
    const int maxOffset = std::max(0, static_cast<int>(roster_.size()) - kVisibleRosterRows);
    scrollOffset_ = static_cast<int>(std::lround(ratio * maxOffset));
    clampScrollOffset();
}

void BattlePartySetupScreen::updateHoverState(float windowX, float windowY, int windowWidth, int windowHeight) {
    hoveredRosterIndex_ = -1;
    hoveredSelectedSlot_ = -1;
    hoveredStartButton_ = false;

    float refX = 0.0f;
    float refY = 0.0f;
    if (!ui::mapWindowPointToReference(windowX, windowY, windowWidth, windowHeight, refX, refY)) {
        lastMouseMoveTick_ = 0;
        return;
    }

    const ui::PartySetupGeometry geometry = ui::computeGeometry();
    const int visibleEnd = std::min(static_cast<int>(roster_.size()), scrollOffset_ + kVisibleRosterRows);
    for (int index = scrollOffset_; index < visibleEnd; ++index) {
        if (ui::pointInRect(refX, refY, ui::rosterRowRect(geometry, index - scrollOffset_))) {
            hoveredRosterIndex_ = index;
            break;
        }
    }

    for (int slot = 0; slot < battleDefinition_.partySize; ++slot) {
        if (ui::pointInRect(refX, refY, ui::selectedCardRect(geometry, slot))) {
            hoveredSelectedSlot_ = slot;
            break;
        }
    }

    hoveredStartButton_ = ui::pointInRect(refX, refY, geometry.startButton);
    lastMouseMoveTick_ = SDL_GetTicks();
}

void BattlePartySetupScreen::toggleFocusedCharacter() {
    if (focusedRosterIndex_ < 0 || focusedRosterIndex_ >= static_cast<int>(roster_.size())) {
        return;
    }
    setSelectedCharacter(roster_[static_cast<size_t>(focusedRosterIndex_)].character.key,
                         !isSelectedCharacter(roster_[static_cast<size_t>(focusedRosterIndex_)].character.key));
}

void BattlePartySetupScreen::setSelectedCharacter(const std::string& characterKey, bool selected) {
    const auto existing = std::find(selectedKeys_.begin(), selectedKeys_.end(), characterKey);
    if (selected) {
        if (existing != selectedKeys_.end()) {
            return;
        }
        if (static_cast<int>(selectedKeys_.size()) >= battleDefinition_.partySize) {
            return;
        }
        selectedKeys_.push_back(characterKey);
    } else {
        if (existing == selectedKeys_.end() || isLockedCharacter(characterKey)) {
            return;
        }
        selectedKeys_.erase(existing);
    }

    syncResult();
}

void BattlePartySetupScreen::handlePointerDown(float windowX, float windowY, int windowWidth, int windowHeight) {
    float refX = 0.0f;
    float refY = 0.0f;
    if (!ui::mapWindowPointToReference(windowX, windowY, windowWidth, windowHeight, refX, refY)) {
        return;
    }

    const ui::PartySetupGeometry geometry = ui::computeGeometry();
    if (static_cast<int>(roster_.size()) > kVisibleRosterRows) {
        const SDL_FRect thumbRect = ui::rosterThumbRect(geometry, static_cast<int>(roster_.size()), scrollOffset_);
        if (ui::pointInRect(refX, refY, thumbRect)) {
            draggingScrollbar_ = true;
            scrollbarDragGrabOffset_ = refY - thumbRect.y;
            return;
        }
        if (ui::pointInRect(refX, refY, geometry.rosterTrackRect)) {
            draggingScrollbar_ = true;
            scrollbarDragGrabOffset_ = thumbRect.h * 0.5f;
            setScrollFromPointerY(refY - scrollbarDragGrabOffset_);
            return;
        }
    }

    const int visibleEnd = std::min(static_cast<int>(roster_.size()), scrollOffset_ + kVisibleRosterRows);
    for (int index = scrollOffset_; index < visibleEnd; ++index) {
        const int visibleIndex = index - scrollOffset_;
        const SDL_FRect rowRect = ui::rosterRowRect(geometry, visibleIndex);
        if (!ui::pointInRect(refX, refY, rowRect)) {
            continue;
        }

        focusedRosterIndex_ = index;
        ensureFocusedEntryVisible();
        toggleFocusedCharacter();
        return;
    }

    for (int slot = 0; slot < battleDefinition_.partySize; ++slot) {
        if (!ui::pointInRect(refX, refY, ui::selectedCardRect(geometry, slot))) {
            continue;
        }
        if (slot < static_cast<int>(selectedKeys_.size()) && !shouldShowSelectedLockIcon(slot)) {
            setSelectedCharacter(selectedKeys_[static_cast<size_t>(slot)], false);
        }
        return;
    }

    if (ui::pointInRect(refX, refY, geometry.startButton) && canStart()) {
        beginStartTransition();
    }
}

void BattlePartySetupScreen::releasePointer() {
    draggingScrollbar_ = false;
    scrollbarDragGrabOffset_ = 0.0f;
}

void BattlePartySetupScreen::syncResult() {
    result_.bossKey = battleDefinition_.bossKey;
    result_.partyKeys = selectedKeys_;
}

void BattlePartySetupScreen::beginStartTransition() {
    if (!canStart()) {
        return;
    }
    startRequested_ = true;
    startTransitionActive_ = true;
    startTransitionTick_ = SDL_GetTicks();
    draggingScrollbar_ = false;
    hoveredRosterIndex_ = -1;
    hoveredSelectedSlot_ = -1;
    hoveredStartButton_ = false;
    lastMouseMoveTick_ = 0;
}

bool BattlePartySetupScreen::isStartTransitionActive() const {
    return startTransitionActive_;
}

} // namespace battle::demo
