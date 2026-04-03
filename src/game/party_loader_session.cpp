#define GL_GLEXT_PROTOTYPES

#include "party_loader_session.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "audio/wav_one_shot.h"
#include "core/battle_loader.h"
#include "../graphics/rmlui_sdl_gl_renderer.h"
#include "../graphics/ui_music_bars.h"
#include "../platform/path_resolution.h"
#include "../window.h"

namespace battle::partyloader {
namespace {

constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr int kVisibleRows = 4;
constexpr float kRowHeight = 80.0f;
constexpr float kRowGap = 10.0f;
constexpr float kRowStep = kRowHeight + kRowGap;
constexpr float kScrollbarMinThumbHeight = 42.0f;
constexpr float kConfirmDurationSeconds = 0.42f;
constexpr float kSlotSettleDurationSeconds = 0.34f;
constexpr float kToastDurationSeconds = 1.08f;

constexpr const char* kDocumentPath = "assets/rmlui/party_loader.rml";
constexpr const char* kLoadingOverlayDocumentPath = "assets/rmlui/shared/loading_overlay.rml";
constexpr const char* kScrollSfxRelativePath = "assets/ui/sfx/UI_notification-done.wav";
constexpr const char* kConfirmSfxRelativePath = "assets/ui/sfx/Selection_roulette-result.wav";
constexpr const char* kStatDefRelativePath = "../ui/party_setup/def.png";
constexpr const char* kStatAtkRelativePath = "../ui/party_setup/atk.png";
constexpr const char* kStatSpdRelativePath = "../ui/party_setup/spd.png";
constexpr const char* kPlusRelativePath = "../ui/party_setup/big_plus.png";

bool loadRmlFontIfPresent(const std::string& path, bool fallback = false) {
    if (path.empty() || !std::filesystem::exists(path)) {
        return false;
    }

    const std::string extension = std::filesystem::path(path).extension().string();
    if (extension == ".ttc" || extension == ".otc") {
        bool anyLoaded = false;
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            if (!Rml::LoadFontFace(path, fallback, Rml::Style::FontWeight::Auto, faceIndex)) {
                if (faceIndex == 0 && !anyLoaded) {
                    return false;
                }
                break;
            }
            anyLoaded = true;
        }
        return anyLoaded;
    }

    return Rml::LoadFontFace(path, fallback);
}

std::string escapeRml(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const char ch : text) {
        switch (ch) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string formatNumber(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::string formatDp(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << "dp";
    return stream.str();
}

std::string formatPercent(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value << "%";
    return stream.str();
}

std::string translateY(float y) {
    return "translate(0dp, " + formatDp(y) + ")";
}

std::string resolveRuntimeImagePath(const std::string& folder, const std::string& assetName) {
    if (assetName.empty()) {
        return {};
    }

    for (const char* extension : {"png", "webp"}) {
        const std::string candidate =
            platform::path::resolvePath("assets/combat/" + folder + "/" + assetName + "." + extension);
        if (std::filesystem::exists(candidate)) {
            return "../combat/" + folder + "/" + assetName + "." + extension;
        }
    }

    return {};
}

std::string initialsForName(const std::string& name) {
    std::stringstream words(name);
    std::string word;
    std::string initials;
    while (words >> word) {
        if (!word.empty()) {
            initials.push_back(word.front());
        }
        if (initials.size() >= 2) {
            break;
        }
    }
    if (initials.empty()) {
        initials = "?";
    }
    return initials;
}

float statRatio(int value, int maxValue) {
    return maxValue > 0 ? std::clamp(static_cast<float>(value) / static_cast<float>(maxValue), 0.0f, 1.0f) : 0.0f;
}

std::string resolveBossDescription(const BossDefinition& bossDefinition,
                                   const BattleDefinition& battleDefinition) {
    AbilityDefinition bossAbility;
    for (const std::string* abilityId :
         {&bossDefinition.skillAbility, &bossDefinition.ability, &bossDefinition.ultimate}) {
        if (abilityId->empty()) {
            continue;
        }
        if (loader::loadAbilityDefinition(*abilityId, bossAbility) && !bossAbility.instructionHint.empty()) {
            return bossAbility.instructionHint;
        }
    }

    if (!battleDefinition.description.empty()) {
        return battleDefinition.description;
    }

    return "Build the opening formation, then press ENTER to launch the battle.";
}

}  // namespace

class SessionImpl {
public:
    struct Entry {
        CharacterDefinition character;
        std::string rosterImagePath;
        std::string stageImagePath;
    };

    bool initialize(Window& window,
                    const BattleDefinition& battleDefinition,
                    const PlayerProgression& progression) {
        shutdown();

        windowHost_ = &window;
        window_ = window.getNativeWindow();
        glContext_ = window.getGlContext();
        if (window_ == nullptr || glContext_ == nullptr) {
            std::cerr << "[PartyLoader] Window is not in OpenGL mode.\n";
            return false;
        }

        initializeAudio();

        SDL_GL_MakeCurrent(window_, glContext_);
        SDL_GL_SetSwapInterval(1);
        SDL_StopTextInput();

        Rml::String glInitMessage;
        if (!RmlGL3::Initialize(&glInitMessage)) {
            std::cerr << "[PartyLoader] RmlGL3 initialization failed: " << glInitMessage << "\n";
            shutdown();
            return false;
        }
        rmlGlInitialized_ = true;

        systemInterface_.SetWindow(window_);
        renderInterface_ = std::make_unique<graphics::RmlUiSdlGlRenderInterface>();
        if (!(*renderInterface_)) {
            std::cerr << "[PartyLoader] Failed to construct GL render interface.\n";
            shutdown();
            return false;
        }

        Rml::SetSystemInterface(&systemInterface_);
        Rml::SetRenderInterface(renderInterface_.get());
        if (!Rml::Initialise()) {
            std::cerr << "[PartyLoader] RmlUi core initialization failed.\n";
            shutdown();
            return false;
        }
        rmlInitialized_ = true;

        if (!loadFonts()) {
            std::cerr << "[PartyLoader] No usable fonts were loaded.\n";
            shutdown();
            return false;
        }

        updateViewportFromWindow();
        renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
        context_ = Rml::CreateContext("party-loader", Rml::Vector2i(windowWidth_, windowHeight_));
        if (context_ == nullptr) {
            std::cerr << "[PartyLoader] Failed to create RmlUi context.\n";
            shutdown();
            return false;
        }
        applyContextScale();

        if (!loadBattleData(battleDefinition, progression)) {
            std::cerr << "[PartyLoader] Failed to load party-loader data.\n";
            shutdown();
            return false;
        }

        if (!loadDocument()) {
            shutdown();
            return false;
        }

        if (!loadingOverlay_.initialize(*context_, platform::path::resolvePath(kLoadingOverlayDocumentPath))) {
            std::cerr << "[PartyLoader] Failed to load loading overlay document.\n";
            shutdown();
            return false;
        }
        loadingOverlayState_ = graphics::RmlUiLoadingOverlayState{};

        initialized_ = true;
        return true;
    }

    void shutdown() {
        initialized_ = false;
        draggingScrollbar_ = false;
        scrollbarDragGrabOffset_ = 0.0f;
        toastTimer_ = 0.0f;
        confirmTimer_ = 0.0f;
        slotSettleTimer_ = 0.0f;
        focusAnimationDirection_ = 0;
        lastMouseX_ = 0.0f;
        lastMouseY_ = 0.0f;
        selectedKeys_.clear();
        roster_.clear();
        rosterRowIndices_.clear();
        battleDefinition_ = BattleDefinition{};
        bossDefinition_ = BossDefinition{};
        progression_ = PlayerProgression{};
        bossDescription_.clear();
        request_.reset();

        sfxPlayer_.shutdown();
        scrollSfxPath_.clear();
        confirmSfxPath_.clear();
        audioReady_ = false;

        if (document_ != nullptr) {
            document_->Close();
            document_ = nullptr;
        }

        rosterTrackElement_ = nullptr;
        rosterViewportElement_ = nullptr;
        scrollbarTrackElement_ = nullptr;
        scrollbarThumbElement_ = nullptr;
        stagePanelElement_ = nullptr;
        stageKickerElement_ = nullptr;
        stageTitleElement_ = nullptr;
        stageDescriptionElement_ = nullptr;
        statusCodeElement_ = nullptr;
        statusTitleElement_ = nullptr;
        statusCopyElement_ = nullptr;
        startButtonElement_ = nullptr;
        toastElement_ = nullptr;
        uiMusicBars_ = graphics::UiMusicBarStrip{};
        uiMusicVisualState_ = game::audio::UiMusicVisualState{};
        loadingOverlay_.shutdown();
        loadingOverlayState_ = graphics::RmlUiLoadingOverlayState{};
        rosterRowElements_.clear();
        slotElements_.clear();

        if (context_ != nullptr) {
            context_->UnloadAllDocuments();
            Rml::RemoveContext("party-loader");
            context_ = nullptr;
        }

        if (rmlInitialized_) {
            Rml::Shutdown();
            rmlInitialized_ = false;
        }

        if (rmlGlInitialized_) {
            RmlGL3::Shutdown();
            rmlGlInitialized_ = false;
        }

        renderInterface_.reset();
        glContext_ = nullptr;
        window_ = nullptr;
        windowHost_ = nullptr;
    }

    void handleEvent(const SDL_Event& event) {
        if (!initialized_ || context_ == nullptr || window_ == nullptr) {
            return;
        }

        if (event.type == SDL_WINDOWEVENT &&
            (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             event.window.event == SDL_WINDOWEVENT_RESIZED)) {
            updateViewportFromWindow();
            renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
            context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
            applyContextScale();
            refreshDocument();
        }

        SDL_Event mutableEvent = event;
        RmlSDL::InputEventHandler(context_, window_, mutableEvent);

        if (request_.has_value()) {
            return;
        }

        switch (event.type) {
            case SDL_MOUSEMOTION: {
                lastMouseX_ = static_cast<float>(event.motion.x);
                lastMouseY_ = static_cast<float>(event.motion.y);
                if (draggingScrollbar_) {
                    updateScrollFromPointer(lastMouseY_);
                    refreshDocument();
                } else {
                    const int hoveredIndex = hoveredRosterIndex(lastMouseX_, lastMouseY_);
                    if (hoveredIndex >= 0 && hoveredIndex != focusedRosterIndex_) {
                        const int previousIndex = focusedRosterIndex_;
                        focusedRosterIndex_ = hoveredIndex;
                        ensureFocusedVisible();
                        focusAnimationDirection_ = hoveredIndex > previousIndex ? 1 : -1;
                        refreshDocument();
                    }
                }
            } break;

            case SDL_MOUSEBUTTONDOWN: {
                if (event.button.button != SDL_BUTTON_LEFT) {
                    break;
                }

                lastMouseX_ = static_cast<float>(event.button.x);
                lastMouseY_ = static_cast<float>(event.button.y);

                if (pointInElement(lastMouseX_, lastMouseY_, scrollbarThumbElement_)) {
                    beginScrollbarDrag(lastMouseY_, false);
                    break;
                }

                if (pointInElement(lastMouseX_, lastMouseY_, scrollbarTrackElement_)) {
                    beginScrollbarDrag(lastMouseY_, true);
                    updateScrollFromPointer(lastMouseY_);
                    refreshDocument();
                    playScrollSfx();
                    break;
                }

                const int rowIndex = hoveredRosterIndex(lastMouseX_, lastMouseY_);
                if (rowIndex >= 0) {
                    focusedRosterIndex_ = rowIndex;
                    ensureFocusedVisible();
                    toggleFocusedCharacter();
                    break;
                }

                const int slotIndex = clickedSlotIndex(lastMouseX_, lastMouseY_);
                if (slotIndex >= 0) {
                    removeSelectedAt(slotIndex);
                    break;
                }

                if (pointInElement(lastMouseX_, lastMouseY_, startButtonElement_)) {
                    launchBattle();
                }
            } break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    stopScrollbarDrag();
                }
                break;

            case SDL_MOUSEWHEEL: {
                int mouseX = 0;
                int mouseY = 0;
                SDL_GetMouseState(&mouseX, &mouseY);
                lastMouseX_ = static_cast<float>(mouseX);
                lastMouseY_ = static_cast<float>(mouseY);
                if (!pointInElement(lastMouseX_, lastMouseY_, rosterViewportElement_) &&
                    !pointInElement(lastMouseX_, lastMouseY_, scrollbarTrackElement_)) {
                    break;
                }

                const int previousOffset = scrollOffset_;
                scrollOffset_ -= event.wheel.y;
                clampScrollOffset();
                if (scrollOffset_ != previousOffset) {
                    focusAnimationDirection_ = event.wheel.y > 0 ? -1 : 1;
                    refreshDocument();
                    playScrollSfx();
                }
            } break;

            case SDL_KEYDOWN:
                if (event.key.repeat != 0) {
                    break;
                }
                switch (event.key.keysym.sym) {
                    case SDLK_UP:
                        moveFocus(-1, true);
                        break;
                    case SDLK_DOWN:
                        moveFocus(1, true);
                        break;
                    case SDLK_PAGEUP:
                        pageFocus(-kVisibleRows, true);
                        break;
                    case SDLK_PAGEDOWN:
                        pageFocus(kVisibleRows, true);
                        break;
                    case SDLK_SPACE:
                        toggleFocusedCharacter();
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        if (canStart()) {
                            launchBattle();
                        } else {
                            toggleFocusedCharacter();
                        }
                        break;
                    case SDLK_ESCAPE:
                        request_ = Request{Request::Type::CancelToMainMenu, {}};
                        break;
                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }

    void update(float deltaSeconds) {
        if (!initialized_ || context_ == nullptr) {
            return;
        }

        if (toastTimer_ > 0.0f) {
            toastTimer_ = std::max(0.0f, toastTimer_ - std::max(deltaSeconds, 0.0f));
            if (toastTimer_ <= 0.0f && toastElement_ != nullptr) {
                toastElement_->SetClass("is-visible", false);
            }
        }

        if (confirmTimer_ > 0.0f) {
            confirmTimer_ = std::max(0.0f, confirmTimer_ - std::max(deltaSeconds, 0.0f));
            if (confirmTimer_ <= 0.0f) {
                updateAnimationClasses();
            }
        }

        if (slotSettleTimer_ > 0.0f) {
            slotSettleTimer_ = std::max(0.0f, slotSettleTimer_ - std::max(deltaSeconds, 0.0f));
            if (slotSettleTimer_ <= 0.0f) {
                updateAnimationClasses();
            }
        }

        sfxPlayer_.cleanupFinishedPlayback();
        context_->Update();
    }

    void render() {
        if (!initialized_ || context_ == nullptr || renderInterface_ == nullptr || window_ == nullptr) {
            return;
        }

        SDL_GL_MakeCurrent(window_, glContext_);
        glViewport(0, 0, drawableWidth_, drawableHeight_);
        glClearColor(1.0f, 0.992f, 0.995f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        loadingOverlay_.apply(loadingOverlayState_);
        context_->Update();
        renderInterface_->BeginFrame();
        context_->Render();
        renderInterface_->EndFrame();
    }

    void setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) {
        loadingOverlayState_ = state;
        loadingOverlay_.apply(loadingOverlayState_);
    }

    void setUiMusicVisualState(const game::audio::UiMusicVisualState& state) {
        uiMusicVisualState_ = state;
        graphics::applyUiMusicBarStrip(uiMusicBars_, uiMusicVisualState_);
    }

    std::optional<Request> consumeRequest() {
        std::optional<Request> request = request_;
        request_.reset();
        return request;
    }

private:
    bool loadFonts() const {
        bool loadedLatin = false;
        for (const std::string& path : platform::path::preferredLatinFontPaths()) {
            loadedLatin = loadRmlFontIfPresent(path, false) || loadedLatin;
        }

        bool loadedFallback = false;
        const std::string cjkPath = platform::path::findCjkFontPath();
        if (!cjkPath.empty()) {
            loadedFallback = loadRmlFontIfPresent(cjkPath, true);
        }

        return loadedLatin || loadedFallback;
    }

    bool initializeAudio() {
        scrollSfxPath_ = platform::path::resolvePath(kScrollSfxRelativePath);
        confirmSfxPath_ = platform::path::resolvePath(kConfirmSfxRelativePath);

        if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
                std::cerr << "[PartyLoader] Audio init failed: " << SDL_GetError() << "\n";
                scrollSfxPath_.clear();
                confirmSfxPath_.clear();
                audioReady_ = false;
                return false;
            }
        }

        audioReady_ = !scrollSfxPath_.empty() || !confirmSfxPath_.empty();
        return audioReady_;
    }

    void updateViewportFromWindow() {
        if (windowHost_ == nullptr) {
            return;
        }

        windowWidth_ = std::max(1, windowHost_->getWindowWidth());
        windowHeight_ = std::max(1, windowHost_->getWindowHeight());
        drawableWidth_ = std::max(1, windowHost_->getDrawableWidth());
        drawableHeight_ = std::max(1, windowHost_->getDrawableHeight());
        glViewport(0, 0, drawableWidth_, drawableHeight_);
    }

    void applyContextScale() {
        if (context_ == nullptr) {
            return;
        }

        const float widthScale = static_cast<float>(windowWidth_) / static_cast<float>(kReferenceWidth);
        const float heightScale = static_cast<float>(windowHeight_) / static_cast<float>(kReferenceHeight);
        const float scale = std::min(widthScale, heightScale);
        context_->SetDensityIndependentPixelRatio(std::max(scale, 0.01f));
    }

    /**
     * @brief Loads battle and player data into the session and prepares the roster and initial selection.
     *
     * Populates internal session state from the provided battle definition and player progression:
     * - stores the provided definitions,
     * - loads the boss definition and resolves its description,
     * - loads character definitions, filters them to the visible/allowed set, applies progression bonuses,
     *   and resolves roster/stage image paths,
     * - initializes the selected lineup (honoring locked and fixed lineup rules) and focus/scroll state.
     *
     * @param battleDefinition Source battle configuration that determines party constraints, locked lineup, and boss key.
     * @param progression Player progression used to determine visible characters and to apply per-character bonuses.
     * @return bool `true` if data loaded and session state initialized successfully; `false` on failure (e.g., missing resources or no roster available).
     */
    bool loadBattleData(const BattleDefinition& battleDefinition,
                        const PlayerProgression& progression) {
        battleDefinition_ = battleDefinition;
        progression_ = progression;
        roster_.clear();
        bossDescription_.clear();
        selectedKeys_.clear();

        normalizePlayerProgression(
            progression_,
            progression.unlockedCharacterKeys.empty() && progression.currentPartyLineup.empty()
                ? ProgressionFallbackPolicy::FullRoster
                : ProgressionFallbackPolicy::StarterRoster);

        if (battleDefinition_.partySize < 1) {
            battleDefinition_.partySize = 4;
        }

        if (!loader::loadBossDefinition(battleDefinition_.bossKey, bossDefinition_)) {
            return false;
        }
        bossDescription_ = resolveBossDescription(bossDefinition_, battleDefinition_);

        std::vector<CharacterDefinition> rosterCharacters;
        if (!loader::loadAllCharacterDefinitions(rosterCharacters)) {
            return false;
        }

        std::vector<std::string> visibleKeys = progression_.unlockedCharacterKeys;
        if (visibleKeys.empty()) {
            visibleKeys = progression_.currentPartyLineup;
        }

        std::unordered_set<std::string> visibleKeySet(visibleKeys.begin(), visibleKeys.end());
        for (const std::string& key : battleDefinition_.lockedLineup) {
            visibleKeySet.insert(key);
        }
        if (battleDefinition_.isLineupFixed) {
            for (const std::string& key : battleDefinition_.lineup) {
                visibleKeySet.insert(key);
            }
        }

        roster_.reserve(rosterCharacters.size());
        for (const CharacterDefinition& character : rosterCharacters) {
            if (visibleKeySet.find(character.key) == visibleKeySet.end()) {
                continue;
            }

            Entry entry;
            entry.character = character;
            applyCharacterProgressionBonuses(entry.character, progression_);
            entry.rosterImagePath = resolveRuntimeImagePath("icons", character.assets);
            if (entry.rosterImagePath.empty()) {
                entry.rosterImagePath = resolveRuntimeImagePath("sprites", character.assets);
            }
            entry.stageImagePath = resolveRuntimeImagePath("sprites", character.assets);
            if (entry.stageImagePath.empty()) {
                entry.stageImagePath = entry.rosterImagePath;
            }
            roster_.push_back(std::move(entry));
        }

        if (roster_.empty()) {
            return false;
        }

        const auto addIfAvailable = [this](const std::string& key) {
            if (key.empty() || selectedSlotForKey(key) >= 0) {
                return;
            }
            if (static_cast<int>(selectedKeys_.size()) >= selectionCapacity()) {
                return;
            }
            const auto rosterIt = std::find_if(roster_.begin(), roster_.end(), [&](const Entry& entry) {
                return entry.character.key == key;
            });
            if (rosterIt != roster_.end()) {
                selectedKeys_.push_back(key);
            }
        };

        for (const std::string& key : battleDefinition_.lockedLineup) {
            addIfAvailable(key);
        }
        for (const std::string& key : progression_.currentPartyLineup) {
            addIfAvailable(key);
        }
        for (const std::string& key : battleDefinition_.lineup) {
            addIfAvailable(key);
        }
        if (selectedKeys_.empty() && !roster_.empty() && !battleDefinition_.isLineupFixed) {
            selectedKeys_.push_back(roster_.front().character.key);
        }

        focusedRosterIndex_ = 0;
        scrollOffset_ = 0;
        return true;
    }

    bool loadDocument() {
        if (context_ == nullptr) {
            return false;
        }

        const std::string documentPath = platform::path::resolvePath(kDocumentPath);
        document_ = context_->LoadDocument(documentPath);
        if (document_ == nullptr) {
            std::cerr << "[PartyLoader] Failed to load document: " << documentPath << "\n";
            return false;
        }

        document_->Show();
        uiMusicBars_ = graphics::cacheUiMusicBarStrip(*document_);
        graphics::applyUiMusicBarStrip(uiMusicBars_, uiMusicVisualState_);
        cacheElements();
        refreshDocument();
        return true;
    }

    void cacheElements() {
        if (document_ == nullptr) {
            return;
        }

        rosterTrackElement_ = document_->GetElementById("roster-track");
        rosterViewportElement_ = document_->GetElementById("roster-viewport");
        scrollbarTrackElement_ = document_->GetElementById("scrollbar-track");
        scrollbarThumbElement_ = document_->GetElementById("scrollbar-thumb");
        stagePanelElement_ = document_->GetElementById("stage-panel");
        stageKickerElement_ = document_->GetElementById("stage-kicker");
        stageTitleElement_ = document_->GetElementById("stage-title");
        stageDescriptionElement_ = document_->GetElementById("stage-description");
        slotGridElement_ = document_->GetElementById("slot-grid");
        statusCodeElement_ = document_->GetElementById("status-code");
        statusTitleElement_ = document_->GetElementById("status-title");
        statusCopyElement_ = document_->GetElementById("status-copy");
        startButtonElement_ = document_->GetElementById("start-button");
        toastElement_ = document_->GetElementById("preview-toast");
    }

    void refreshDocument() {
        if (document_ == nullptr) {
            return;
        }

        updateStageCopy();
        rebuildRosterMarkup();
        rebuildSlotsMarkup();
        updateScrollbar();
        updateAnimationClasses();
        focusAnimationDirection_ = 0;
    }

    void rebuildRosterMarkup() {
        if (rosterTrackElement_ == nullptr) {
            return;
        }

        const int visibleStart = std::clamp(scrollOffset_, 0, maxScrollOffset());
        const int visibleEnd = std::min(static_cast<int>(roster_.size()), visibleStart + kVisibleRows);

        std::ostringstream markup;
        for (int index = visibleStart; index < visibleEnd; ++index) {
            const Entry& entry = roster_[static_cast<std::size_t>(index)];
            const bool selected = selectedSlotForKey(entry.character.key) >= 0;
            const bool focused = index == focusedRosterIndex_;
            const bool locked = isLockedKey(entry.character.key);

            markup << "<button class=\"roster-row";
            if (selected) {
                markup << " is-selected";
            }
            if (focused) {
                markup << " is-focused";
                if (focusAnimationDirection_ < 0) {
                    markup << " is-animated-up";
                } else if (focusAnimationDirection_ > 0) {
                    markup << " is-animated-down";
                }
            }
            if (locked) {
                markup << " is-locked";
            }
            markup << "\" id=\"roster-row-" << index << "\" style=\"top:"
                   << formatDp(static_cast<float>(index - visibleStart) * kRowStep) << ";\">"
                   << "<div class=\"roster-row-shell\"></div>"
                   << "<div class=\"roster-row-inner\">"
                   << "<div class=\"roster-row-accent\"></div>"
                   << "<div class=\"art-thumb\">"
                   << "<div class=\"art-thumb-inner\">";
            if (!entry.rosterImagePath.empty()) {
                markup << "<img class=\"art-image\" src=\"" << escapeRml(entry.rosterImagePath) << "\"/>";
            }
            markup << "</div>"
                   << "<div class=\"art-fallback\">" << escapeRml(initialsForName(entry.character.title)) << "</div>"
                   << "</div>"
                   << "<div class=\"roster-row-copy\">"
                   << "<div class=\"roster-row-name\">" << escapeRml(entry.character.title) << "</div>"
                   << "<div class=\"roster-row-stat-list\">"
                   << "<div class=\"roster-row-stat\">ATK " << entry.character.atk << "</div>"
                   << "<div class=\"roster-row-stat\">SPD " << entry.character.spd << "</div>"
                   << "<div class=\"roster-row-stat\">HP " << entry.character.hp << "</div>"
                   << "</div>"
                   << "</div>"
                   << "<div class=\"roster-row-side\">";
            if (selected) {
                markup << "<div class=\"slot-badge\">" << (selectedSlotForKey(entry.character.key) + 1) << "</div>";
            }
            if (locked) {
                markup << "<div class=\"lock-badge\">Locked</div>";
            }
            markup << "</div></div></button>";
        }

        rosterTrackElement_->SetInnerRML(markup.str());
        const float viewportHeight = static_cast<float>(kVisibleRows) * kRowStep - kRowGap;
        rosterTrackElement_->SetProperty("height", formatDp(viewportHeight));
        rosterTrackElement_->SetProperty("transform", translateY(0.0f));
        cacheDynamicElements();
    }

    void rebuildSlotsMarkup() {
        if (slotGridElement_ == nullptr) {
            return;
        }

        int maxHp = 1;
        int maxAtk = 1;
        int maxSpd = 1;
        for (const Entry& entry : roster_) {
            maxHp = std::max(maxHp, entry.character.hp);
            maxAtk = std::max(maxAtk, entry.character.atk);
            maxSpd = std::max(maxSpd, entry.character.spd);
        }

        std::ostringstream markup;
        for (int slotIndex = 0; slotIndex < selectionCapacity(); ++slotIndex) {
            const std::string key = slotIndex < static_cast<int>(selectedKeys_.size())
                ? selectedKeys_[static_cast<std::size_t>(slotIndex)]
                : std::string();
            const auto it = std::find_if(roster_.begin(), roster_.end(), [&](const Entry& entry) {
                return entry.character.key == key;
            });

            if (it == roster_.end()) {
                markup << "<button class=\"stage-card is-empty\" id=\"slot-" << slotIndex << "\">"
                       << "<div class=\"stage-card-shell\"></div>"
                       << "<div class=\"stage-card-inner\">"
                       << "<div class=\"stage-card-topline\">"
                       << "<div class=\"slot-index\">Slot " << (slotIndex + 1) << "</div>"
                       << "<div class=\"slot-pill\">Empty</div>"
                       << "</div>"
                       << "<div class=\"empty-layout\">"
                       << "<img class=\"empty-icon\" src=\"" << kPlusRelativePath << "\"/>"
                       << "<div class=\"empty-title\">Open Slot</div>"
                       << "<div class=\"empty-copy\">Select someone from the roster and the stage order fills from left to right.</div>"
                       << "</div>"
                       << "</div></button>";
                continue;
            }

            const Entry& entry = *it;
            const bool locked = isLockedKey(entry.character.key);
            markup << "<button class=\"stage-card is-filled";
            if (!locked) {
                markup << " is-hoverable";
            }
            markup << "\" id=\"slot-" << slotIndex << "\">"
                   << "<div class=\"stage-card-shell\"></div>"
                   << "<div class=\"stage-card-inner\">"
                   << "<div class=\"stage-card-topline\">"
                   << "<div class=\"slot-index\">Slot " << (slotIndex + 1) << "</div>"
                   << "<div class=\"slot-pill " << (locked ? "is-locked" : "is-remove") << "\">"
                   << escapeRml(locked ? "Locked" : "Remove") << "</div>"
                   << "</div>"
                   << "<div class=\"stage-card-main\">"
                   << "<div class=\"card-art\">"
                   << "<div class=\"card-art-inner\">";
            if (!entry.stageImagePath.empty()) {
                markup << "<img class=\"art-image\" src=\"" << escapeRml(entry.stageImagePath) << "\"/>";
            }
            markup << "</div>"
                   << "<div class=\"art-fallback\">" << escapeRml(initialsForName(entry.character.title)) << "</div>"
                   << "</div>"
                   << "<div class=\"card-copy\">"
                   << "<div class=\"card-name\">" << escapeRml(entry.character.title) << "</div>"
                   << "<div class=\"card-class\">" << escapeRml(entry.character.characterClass) << "</div>"
                   << "<div class=\"stat-list\">"
                   << "<div class=\"stat-row\">"
                   << "<img class=\"stat-icon\" src=\"" << kStatDefRelativePath << "\"/>"
                   << "<div class=\"stat-row-label\">HP</div>"
                   << "<div class=\"stat-track\"><div class=\"stat-fill hp\" style=\"width:"
                   << formatPercent(statRatio(entry.character.hp, maxHp) * 100.0f) << ";\"></div></div>"
                   << "<div class=\"stat-value\">" << entry.character.hp << "</div>"
                   << "</div>"
                   << "<div class=\"stat-row\">"
                   << "<img class=\"stat-icon\" src=\"" << kStatAtkRelativePath << "\"/>"
                   << "<div class=\"stat-row-label\">ATK</div>"
                   << "<div class=\"stat-track\"><div class=\"stat-fill atk\" style=\"width:"
                   << formatPercent(statRatio(entry.character.atk, maxAtk) * 100.0f) << ";\"></div></div>"
                   << "<div class=\"stat-value\">" << entry.character.atk << "</div>"
                   << "</div>"
                   << "<div class=\"stat-row\">"
                   << "<img class=\"stat-icon\" src=\"" << kStatSpdRelativePath << "\"/>"
                   << "<div class=\"stat-row-label\">SPD</div>"
                   << "<div class=\"stat-track\"><div class=\"stat-fill spd\" style=\"width:"
                   << formatPercent(statRatio(entry.character.spd, maxSpd) * 100.0f) << ";\"></div></div>"
                   << "<div class=\"stat-value\">" << entry.character.spd << "</div>"
                   << "</div>"
                   << "</div></div></div></div></button>";
        }

        slotGridElement_->SetInnerRML(markup.str());
        cacheDynamicElements();
    }

    void cacheDynamicElements() {
        rosterRowElements_.clear();
        rosterRowIndices_.clear();
        slotElements_.clear();

        if (document_ == nullptr) {
            return;
        }

        for (std::size_t i = 0; i < roster_.size(); ++i) {
            if (Rml::Element* element = document_->GetElementById("roster-row-" + std::to_string(i))) {
                rosterRowElements_.push_back(element);
                rosterRowIndices_.push_back(static_cast<int>(i));
            }
        }

        for (int slotIndex = 0; slotIndex < selectionCapacity(); ++slotIndex) {
            if (Rml::Element* element = document_->GetElementById("slot-" + std::to_string(slotIndex))) {
                slotElements_.push_back(element);
            }
        }
    }

    void updateStageCopy() const {
        if (stageKickerElement_ != nullptr) {
            stageKickerElement_->SetInnerRML(
                escapeRml(battleDefinition_.type == "tutorial" ? "Tutorial Battle" : "Flexible Battle"));
        }
        if (stageTitleElement_ != nullptr) {
            stageTitleElement_->SetInnerRML(
                escapeRml(battleDefinition_.name.empty() ? "Battle Setup" : battleDefinition_.name));
        }
        if (stageDescriptionElement_ != nullptr) {
            stageDescriptionElement_->SetInnerRML(escapeRml(bossDescription_));
        }
        if (statusCodeElement_ != nullptr) {
            statusCodeElement_->SetInnerRML(
                (selectedKeys_.size() < 10 ? "0" : "") + std::to_string(selectedKeys_.size()) +
                " / " +
                (selectionCapacity() < 10 ? "0" : "") + std::to_string(selectionCapacity()) +
                " Selected");
        }
        if (statusTitleElement_ != nullptr) {
            statusTitleElement_->SetInnerRML("Build The Opening Formation");
        }
        if (statusCopyElement_ != nullptr) {
            statusCopyElement_->SetInnerRML(
                "UP / DOWN move. PAGE UP / DOWN scroll. SPACE toggles. ENTER launches the battle.");
        }
        if (startButtonElement_ != nullptr) {
            startButtonElement_->SetClass("is-disabled", !canStart());
        }
    }

    void updateScrollbar() {
        if (scrollbarTrackElement_ == nullptr || scrollbarThumbElement_ == nullptr) {
            return;
        }

        if (static_cast<int>(roster_.size()) <= kVisibleRows) {
            scrollbarTrackElement_->SetProperty("opacity", "0.35");
            scrollbarThumbElement_->SetProperty("height", "100%");
            scrollbarThumbElement_->SetProperty("top", "0dp");
            return;
        }

        scrollbarTrackElement_->SetProperty("opacity", "1");
        const float trackHeight = scrollbarTrackElement_->GetClientHeight();
        const float thumbHeight = std::max(
            kScrollbarMinThumbHeight,
            trackHeight * (static_cast<float>(kVisibleRows) / static_cast<float>(roster_.size())));
        const float usableTravel = std::max(0.0f, trackHeight - thumbHeight);
        const float ratio = maxScrollOffset() > 0
            ? static_cast<float>(scrollOffset_) / static_cast<float>(maxScrollOffset())
            : 0.0f;
        scrollbarThumbElement_->SetProperty("height", formatDp(thumbHeight));
        scrollbarThumbElement_->SetProperty("top", formatDp(usableTravel * ratio));
    }

    void updateAnimationClasses() const {
        if (stagePanelElement_ != nullptr) {
            stagePanelElement_->SetClass("is-confirming", confirmTimer_ > 0.0f);
        }
        if (slotGridElement_ != nullptr) {
            slotGridElement_->SetClass("is-settling", slotSettleTimer_ > 0.0f);
        }
        if (startButtonElement_ != nullptr) {
            startButtonElement_->SetClass("is-firing", confirmTimer_ > 0.0f);
        }
    }

    void moveFocus(int delta, bool shouldPlayScrollSfx) {
        if (roster_.empty() || delta == 0) {
            return;
        }

        const int previousIndex = focusedRosterIndex_;
        focusedRosterIndex_ = (focusedRosterIndex_ + delta) % static_cast<int>(roster_.size());
        if (focusedRosterIndex_ < 0) {
            focusedRosterIndex_ += static_cast<int>(roster_.size());
        }
        ensureFocusedVisible();
        focusAnimationDirection_ = focusedRosterIndex_ < previousIndex ? -1 : 1;
        refreshDocument();
        if (shouldPlayScrollSfx) {
            playScrollSfx();
        }
    }

    void pageFocus(int deltaRows, bool shouldPlayScrollSfx) {
        if (roster_.empty() || deltaRows == 0) {
            return;
        }

        scrollOffset_ += deltaRows;
        clampScrollOffset();
        focusedRosterIndex_ = std::clamp(focusedRosterIndex_, scrollOffset_, scrollOffset_ + kVisibleRows - 1);
        focusAnimationDirection_ = deltaRows < 0 ? -1 : 1;
        refreshDocument();
        if (shouldPlayScrollSfx) {
            playScrollSfx();
        }
    }

    void toggleFocusedCharacter() {
        if (focusedRosterIndex_ < 0 || focusedRosterIndex_ >= static_cast<int>(roster_.size())) {
            return;
        }
        toggleCharacter(roster_[static_cast<std::size_t>(focusedRosterIndex_)].character.key);
    }

    void toggleCharacter(const std::string& key) {
        const int selectedIndex = selectedSlotForKey(key);
        if (selectedIndex >= 0) {
            if (isLockedKey(key)) {
                showToast("That member is locked into this lineup.");
                return;
            }
            selectedKeys_.erase(selectedKeys_.begin() + selectedIndex);
        } else {
            if (static_cast<int>(selectedKeys_.size()) >= selectionCapacity()) {
                showToast("Party limit reached.");
                return;
            }
            selectedKeys_.push_back(key);
        }

        slotSettleTimer_ = kSlotSettleDurationSeconds;
        refreshDocument();
        playScrollSfx();
    }

    void removeSelectedAt(int slotIndex) {
        if (slotIndex < 0 || slotIndex >= static_cast<int>(selectedKeys_.size())) {
            return;
        }

        const std::string key = selectedKeys_[static_cast<std::size_t>(slotIndex)];
        if (isLockedKey(key)) {
            showToast("Locked members cannot be removed.");
            return;
        }

        selectedKeys_.erase(selectedKeys_.begin() + slotIndex);
        slotSettleTimer_ = kSlotSettleDurationSeconds;
        refreshDocument();
        playScrollSfx();
    }

    void launchBattle() {
        if (!canStart()) {
            showToast("Choose at least one member first.");
            return;
        }

        confirmTimer_ = kConfirmDurationSeconds;
        updateAnimationClasses();
        playConfirmSfx();
        request_ = Request{Request::Type::ConfirmBattle, selectedKeys_};
    }

    void beginScrollbarDrag(float mouseY, bool centerThumb) {
        if (scrollbarThumbElement_ == nullptr) {
            return;
        }

        draggingScrollbar_ = true;
        if (centerThumb) {
            scrollbarDragGrabOffset_ = scrollbarThumbElement_->GetOffsetHeight() * 0.5f;
        } else {
            scrollbarDragGrabOffset_ = mouseY - scrollbarThumbElement_->GetAbsoluteTop();
        }
        if (scrollbarTrackElement_ != nullptr) {
            scrollbarTrackElement_->SetClass("is-dragging", true);
        }
    }

    void stopScrollbarDrag() {
        draggingScrollbar_ = false;
        scrollbarDragGrabOffset_ = 0.0f;
        if (scrollbarTrackElement_ != nullptr) {
            scrollbarTrackElement_->SetClass("is-dragging", false);
        }
    }

    void updateScrollFromPointer(float mouseY) {
        if (scrollbarTrackElement_ == nullptr || scrollbarThumbElement_ == nullptr) {
            return;
        }

        const int maxOffset = std::max(0, static_cast<int>(roster_.size()) - kVisibleRows);
        if (maxOffset <= 0) {
            scrollOffset_ = 0;
            return;
        }

        const float trackTop = scrollbarTrackElement_->GetAbsoluteTop() + scrollbarTrackElement_->GetClientTop();
        const float trackHeight = scrollbarTrackElement_->GetClientHeight();
        const float thumbHeight = scrollbarThumbElement_->GetOffsetHeight();
        const float usableTravel = std::max(1.0f, trackHeight - thumbHeight);
        const float pointerTop = std::clamp(mouseY - trackTop - scrollbarDragGrabOffset_, 0.0f, usableTravel);
        const float ratio = pointerTop / usableTravel;
        scrollOffset_ = static_cast<int>(std::lround(ratio * static_cast<float>(maxOffset)));
    }

    void ensureFocusedVisible() {
        clampScrollOffset();
        if (focusedRosterIndex_ < scrollOffset_) {
            scrollOffset_ = focusedRosterIndex_;
        } else if (focusedRosterIndex_ >= scrollOffset_ + kVisibleRows) {
            scrollOffset_ = focusedRosterIndex_ - kVisibleRows + 1;
        }
        clampScrollOffset();
    }

    void clampScrollOffset() {
        scrollOffset_ = std::clamp(scrollOffset_, 0, std::max(0, static_cast<int>(roster_.size()) - kVisibleRows));
    }

    bool pointInElement(float x, float y, Rml::Element* element) const {
        if (element == nullptr || !element->IsVisible(true)) {
            return false;
        }

        const float left = element->GetAbsoluteLeft();
        const float top = element->GetAbsoluteTop();
        const float width = element->GetOffsetWidth();
        const float height = element->GetOffsetHeight();
        return x >= left && x <= left + width && y >= top && y <= top + height;
    }

    int hoveredRosterIndex(float x, float y) const {
        for (std::size_t i = 0; i < rosterRowElements_.size(); ++i) {
            if (pointInElement(x, y, rosterRowElements_[i])) {
                return i < rosterRowIndices_.size() ? rosterRowIndices_[i] : -1;
            }
        }
        return -1;
    }

    int clickedSlotIndex(float x, float y) const {
        for (std::size_t i = 0; i < slotElements_.size(); ++i) {
            if (pointInElement(x, y, slotElements_[i])) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    int selectionCapacity() const {
        return std::max(1, battleDefinition_.partySize);
    }

    int maxScrollOffset() const {
        return std::max(0, static_cast<int>(roster_.size()) - kVisibleRows);
    }

    int selectedSlotForKey(const std::string& key) const {
        for (std::size_t i = 0; i < selectedKeys_.size(); ++i) {
            if (selectedKeys_[i] == key) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool isLockedKey(const std::string& key) const {
        return std::find(battleDefinition_.lockedLineup.begin(), battleDefinition_.lockedLineup.end(), key) !=
            battleDefinition_.lockedLineup.end();
    }

    bool canStart() const {
        const int count = static_cast<int>(selectedKeys_.size());
        return count >= 1 && count <= selectionCapacity();
    }

    void playScrollSfx() {
        if (!audioReady_ || scrollSfxPath_.empty()) {
            return;
        }
        (void)sfxPlayer_.playWavOneShot(scrollSfxPath_, 0.78f, true);
    }

    void playConfirmSfx() {
        if (!audioReady_ || confirmSfxPath_.empty()) {
            return;
        }
        (void)sfxPlayer_.playWavOneShot(confirmSfxPath_, 0.92f, true);
    }

    void showToast(const std::string& message) {
        if (toastElement_ == nullptr) {
            return;
        }

        toastElement_->SetInnerRML(escapeRml(message));
        toastElement_->SetClass("is-visible", true);
        toastTimer_ = kToastDurationSeconds;
    }

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool initialized_ = false;
    bool audioReady_ = false;
    bool rmlInitialized_ = false;
    bool rmlGlInitialized_ = false;
    int windowWidth_ = 1280;
    int windowHeight_ = 720;
    int drawableWidth_ = 1280;
    int drawableHeight_ = 720;
    float lastMouseX_ = 0.0f;
    float lastMouseY_ = 0.0f;
    bool draggingScrollbar_ = false;
    float scrollbarDragGrabOffset_ = 0.0f;
    float toastTimer_ = 0.0f;
    float confirmTimer_ = 0.0f;
    float slotSettleTimer_ = 0.0f;
    int focusAnimationDirection_ = 0;

    SystemInterface_SDL systemInterface_;
    std::unique_ptr<graphics::RmlUiSdlGlRenderInterface> renderInterface_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    game::audio::WavOneShotPlayer sfxPlayer_;
    std::string scrollSfxPath_;
    std::string confirmSfxPath_;

    BattleDefinition battleDefinition_{};
    BossDefinition bossDefinition_{};
    PlayerProgression progression_{};
    std::vector<Entry> roster_;
    std::vector<std::string> selectedKeys_;
    std::string bossDescription_;
    int focusedRosterIndex_ = 0;
    int scrollOffset_ = 0;
    std::optional<Request> request_;

    graphics::RmlUiLoadingOverlay loadingOverlay_;
    graphics::RmlUiLoadingOverlayState loadingOverlayState_;
    graphics::UiMusicBarStrip uiMusicBars_;
    game::audio::UiMusicVisualState uiMusicVisualState_;

    Rml::Element* rosterTrackElement_ = nullptr;
    Rml::Element* rosterViewportElement_ = nullptr;
    Rml::Element* scrollbarTrackElement_ = nullptr;
    Rml::Element* scrollbarThumbElement_ = nullptr;
    Rml::Element* stagePanelElement_ = nullptr;
    Rml::Element* stageKickerElement_ = nullptr;
    Rml::Element* stageTitleElement_ = nullptr;
    Rml::Element* stageDescriptionElement_ = nullptr;
    Rml::Element* slotGridElement_ = nullptr;
    Rml::Element* statusCodeElement_ = nullptr;
    Rml::Element* statusTitleElement_ = nullptr;
    Rml::Element* statusCopyElement_ = nullptr;
    Rml::Element* startButtonElement_ = nullptr;
    Rml::Element* toastElement_ = nullptr;
    std::vector<Rml::Element*> rosterRowElements_;
    std::vector<int> rosterRowIndices_;
    std::vector<Rml::Element*> slotElements_;
};

Session::Session()
    : impl_(std::make_unique<SessionImpl>()) {}

Session::~Session() = default;

bool Session::initialize(Window& window,
                         const BattleDefinition& battleDefinition,
                         const PlayerProgression& progression) {
    return impl_->initialize(window, battleDefinition, progression);
}

void Session::shutdown() {
    impl_->shutdown();
}

void Session::handleEvent(const SDL_Event& event) {
    impl_->handleEvent(event);
}

void Session::update(float deltaSeconds) {
    impl_->update(deltaSeconds);
}

void Session::render() {
    impl_->render();
}

void Session::setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) {
    impl_->setLoadingOverlay(state);
}

void Session::setUiMusicVisualState(const game::audio::UiMusicVisualState& state) {
    impl_->setUiMusicVisualState(state);
}

std::optional<Request> Session::consumeRequest() {
    return impl_->consumeRequest();
}

}  // namespace battle::partyloader
