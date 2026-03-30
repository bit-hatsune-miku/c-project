#define GL_GLEXT_PROTOTYPES

#include "party_loader_preview_session.h"

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

#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Core.h>

#include "RmlUi_Renderer_GL3.h"

#include "../game/core/battle_loader.h"
#include "../platform/path_resolution.h"
#include "../window.h"

namespace graphics::preview {
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

constexpr const char* kDocumentPath = "assets/rmlui/previews/party_loader_preview.rml";
constexpr const char* kDefaultBattleKey = "scan_to_pay";
constexpr const char* kFallbackBattleKey = "wild_scent_flowers";
constexpr const char* kScrollSfxRelativePath = "assets/ui/sfx/UI_notification-done.wav";
constexpr const char* kConfirmSfxRelativePath = "assets/ui/sfx/Selection_roulette-result.wav";
constexpr const char* kLogoRelativePath = "../../vn/backgrounds/General_Art/MainMenuTitle.png";
constexpr const char* kStatDefRelativePath = "../../ui/party_setup/def.png";
constexpr const char* kStatAtkRelativePath = "../../ui/party_setup/atk.png";
constexpr const char* kStatSpdRelativePath = "../../ui/party_setup/spd.png";
constexpr const char* kPlusRelativePath = "../../ui/party_setup/big_plus.png";

const std::array<const char*, 7> kPreviewRosterOrder = {
    "miku",
    "cupcakke",
    "jiafei",
    "lyoo",
    "luotianyi",
    "wechatalipay",
    "ari",
};

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
    for (const char c : text) {
        switch (c) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            default: escaped.push_back(c); break;
        }
    }
    return escaped;
}

std::string formatDp(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << "dp";
    return stream.str();
}

std::string formatNumber(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
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

std::string resolvePreviewImagePath(const std::string& folder, const std::string& assetName) {
    if (assetName.empty()) {
        return {};
    }

    for (const char* extension : {"png", "webp"}) {
        const std::string candidate =
            platform::path::resolvePath("assets/combat/" + folder + "/" + assetName + "." + extension);
        if (std::filesystem::exists(candidate)) {
            return "../../combat/" + folder + "/" + assetName + "." + extension;
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

}  // namespace

PartyLoaderPreviewSession::~PartyLoaderPreviewSession() {
    shutdown();
}

bool PartyLoaderPreviewSession::initialize(Window& window) {
    shutdown();

    windowHost_ = &window;
    window_ = window.getNativeWindow();
    glContext_ = window.getGlContext();
    if (window_ == nullptr || glContext_ == nullptr) {
        std::cerr << "[PartyLoaderPreview] Window is not in OpenGL mode.\n";
        return false;
    }

    initializeAudio();

    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(1);
    SDL_StopTextInput();

    Rml::String glInitMessage;
    if (!RmlGL3::Initialize(&glInitMessage)) {
        std::cerr << "[PartyLoaderPreview] RmlGL3 initialization failed: " << glInitMessage << "\n";
        shutdown();
        return false;
    }
    rmlGlInitialized_ = true;

    systemInterface_.SetWindow(window_);
    renderInterface_ = std::make_unique<RmlUiSdlGlRenderInterface>();
    if (!(*renderInterface_)) {
        std::cerr << "[PartyLoaderPreview] Failed to construct GL render interface.\n";
        shutdown();
        return false;
    }

    Rml::SetSystemInterface(&systemInterface_);
    Rml::SetRenderInterface(renderInterface_.get());
    if (!Rml::Initialise()) {
        std::cerr << "[PartyLoaderPreview] RmlUi core initialization failed.\n";
        shutdown();
        return false;
    }
    rmlInitialized_ = true;

    if (!loadFonts()) {
        std::cerr << "[PartyLoaderPreview] No usable fonts were loaded.\n";
        shutdown();
        return false;
    }

    updateViewportFromWindow();
    renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
    context_ = Rml::CreateContext("party-loader-preview", Rml::Vector2i(windowWidth_, windowHeight_));
    if (context_ == nullptr) {
        std::cerr << "[PartyLoaderPreview] Failed to create RmlUi context.\n";
        shutdown();
        return false;
    }
    applyContextScale();

    if (!loadPreviewData()) {
        std::cerr << "[PartyLoaderPreview] Failed to load preview battle or roster data.\n";
        shutdown();
        return false;
    }

    if (!loadDocument()) {
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void PartyLoaderPreviewSession::shutdown() {
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
    battleDefinition_ = battle::BattleDefinition{};
    bossDefinition_ = battle::BossDefinition{};
    bossDescription_.clear();

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
    stageCapacityElement_ = nullptr;
    stageModeElement_ = nullptr;
    slotGridElement_ = nullptr;
    statusCodeElement_ = nullptr;
    statusTitleElement_ = nullptr;
    statusCopyElement_ = nullptr;
    rosterCopyElement_ = nullptr;
    rosterFooterCopyElement_ = nullptr;
    startButtonElement_ = nullptr;
    toastElement_ = nullptr;
    rosterRowElements_.clear();
    slotElements_.clear();

    if (context_ != nullptr) {
        context_->UnloadAllDocuments();
        Rml::RemoveContext("party-loader-preview");
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

void PartyLoaderPreviewSession::handleEvent(const SDL_Event& event) {
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
    }

    SDL_Event mutableEvent = event;
    RmlSDL::InputEventHandler(context_, window_, mutableEvent);

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
                launchBattlePreview();
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
                        launchBattlePreview();
                    } else {
                        toggleFocusedCharacter();
                    }
                    break;
                case SDLK_ESCAPE:
                    if (windowHost_ != nullptr) {
                        windowHost_->close();
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

void PartyLoaderPreviewSession::update(float deltaSeconds) {
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

void PartyLoaderPreviewSession::render() {
    if (!initialized_ || context_ == nullptr || renderInterface_ == nullptr || window_ == nullptr) {
        return;
    }

    SDL_GL_MakeCurrent(window_, glContext_);
    glViewport(0, 0, drawableWidth_, drawableHeight_);
    glClearColor(1.0f, 0.992f, 0.995f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    renderInterface_->BeginFrame();
    context_->Render();
    renderInterface_->EndFrame();
}

bool PartyLoaderPreviewSession::loadFonts() const {
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

bool PartyLoaderPreviewSession::initializeAudio() {
    scrollSfxPath_ = platform::path::resolvePath(kScrollSfxRelativePath);
    confirmSfxPath_ = platform::path::resolvePath(kConfirmSfxRelativePath);

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "[PartyLoaderPreview] Audio init failed: " << SDL_GetError() << "\n";
            scrollSfxPath_.clear();
            confirmSfxPath_.clear();
            audioReady_ = false;
            return false;
        }
    }

    audioReady_ = !scrollSfxPath_.empty() || !confirmSfxPath_.empty();
    return audioReady_;
}

void PartyLoaderPreviewSession::updateViewportFromWindow() {
    if (windowHost_ == nullptr) {
        return;
    }

    windowWidth_ = std::max(1, windowHost_->getWindowWidth());
    windowHeight_ = std::max(1, windowHost_->getWindowHeight());
    drawableWidth_ = std::max(1, windowHost_->getDrawableWidth());
    drawableHeight_ = std::max(1, windowHost_->getDrawableHeight());
    glViewport(0, 0, drawableWidth_, drawableHeight_);
}

void PartyLoaderPreviewSession::applyContextScale() {
    if (context_ == nullptr) {
        return;
    }

    const float widthScale = static_cast<float>(windowWidth_) / static_cast<float>(kReferenceWidth);
    const float heightScale = static_cast<float>(windowHeight_) / static_cast<float>(kReferenceHeight);
    const float scale = std::min(widthScale, heightScale);
    context_->SetDensityIndependentPixelRatio(std::max(scale, 0.01f));
}

bool PartyLoaderPreviewSession::loadPreviewData() {
    battleDefinition_ = battle::BattleDefinition{};
    bossDefinition_ = battle::BossDefinition{};
    bossDescription_.clear();
    if (!battle::loader::loadBattleDefinition(kDefaultBattleKey, battleDefinition_)) {
        if (!battle::loader::loadBattleDefinition(kFallbackBattleKey, battleDefinition_)) {
            std::vector<battle::BattleDefinition> battles;
            if (!battle::loader::loadAllBattleDefinitions(battles)) {
                return false;
            }
            const auto it = std::find_if(battles.begin(), battles.end(), [](const battle::BattleDefinition& battle) {
                return !battle.isLineupFixed && battle.selectorVisible;
            });
            if (it == battles.end()) {
                return false;
            }
            battleDefinition_ = *it;
        }
    }

    if (battleDefinition_.partySize < 1) {
        battleDefinition_.partySize = 4;
    }

    if (!battle::loader::loadBossDefinition(battleDefinition_.bossKey, bossDefinition_)) {
        return false;
    }

    battle::AbilityDefinition bossAbility;
    for (const std::string* abilityId : {&bossDefinition_.skillAbility, &bossDefinition_.ability, &bossDefinition_.ultimate}) {
        if (abilityId->empty()) {
            continue;
        }
        if (battle::loader::loadAbilityDefinition(*abilityId, bossAbility) && !bossAbility.instructionHint.empty()) {
            bossDescription_ = bossAbility.instructionHint;
            break;
        }
    }
    if (bossDescription_.empty()) {
        bossDescription_ = !battleDefinition_.description.empty()
            ? battleDefinition_.description
            : "Build the opening lineup, then press ENTER to launch the battle preview.";
    }

    roster_.clear();
    std::unordered_set<std::string> addedKeys;
    for (const char* key : kPreviewRosterOrder) {
        if (key == nullptr || !addedKeys.insert(key).second) {
            continue;
        }

        battle::CharacterDefinition character;
        if (!battle::loader::loadCharacterDefinition(key, character)) {
            continue;
        }

        Entry entry;
        entry.character = std::move(character);
        entry.rosterImagePath = resolvePreviewImagePath("icons", entry.character.assets);
        if (entry.rosterImagePath.empty()) {
            entry.rosterImagePath = resolvePreviewImagePath("sprites", entry.character.assets);
        }
        entry.stageImagePath = resolvePreviewImagePath("sprites", entry.character.assets);
        if (entry.stageImagePath.empty()) {
            entry.stageImagePath = entry.rosterImagePath;
        }
        roster_.push_back(std::move(entry));
    }

    if (roster_.empty()) {
        return false;
    }

    selectedKeys_.clear();
    const auto addIfAvailable = [this](const std::string& key) {
        if (key.empty() || std::find(selectedKeys_.begin(), selectedKeys_.end(), key) != selectedKeys_.end()) {
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
    for (const std::string& key : battleDefinition_.lineup) {
        addIfAvailable(key);
    }
    if (selectedKeys_.empty() && !roster_.empty()) {
        selectedKeys_.push_back(roster_.front().character.key);
    }

    focusedRosterIndex_ = 0;
    scrollOffset_ = 0;
    return true;
}

bool PartyLoaderPreviewSession::loadDocument() {
    if (context_ == nullptr) {
        return false;
    }

    const std::string documentPath = platform::path::resolvePath(kDocumentPath);
    document_ = context_->LoadDocument(documentPath);
    if (document_ == nullptr) {
        std::cerr << "[PartyLoaderPreview] Failed to load document: " << documentPath << "\n";
        return false;
    }

    document_->Show();
    cacheElements();
    refreshDocument();
    return true;
}

void PartyLoaderPreviewSession::cacheElements() {
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
    stageCapacityElement_ = document_->GetElementById("stage-capacity");
    stageModeElement_ = document_->GetElementById("stage-mode");
    slotGridElement_ = document_->GetElementById("slot-grid");
    statusCodeElement_ = document_->GetElementById("status-code");
    statusTitleElement_ = document_->GetElementById("status-title");
    statusCopyElement_ = document_->GetElementById("status-copy");
    rosterCopyElement_ = document_->GetElementById("roster-copy");
    rosterFooterCopyElement_ = document_->GetElementById("roster-footer-copy");
    startButtonElement_ = document_->GetElementById("start-button");
    toastElement_ = document_->GetElementById("preview-toast");
}

void PartyLoaderPreviewSession::refreshDocument() {
    if (document_ == nullptr) {
        return;
    }

    updateStaticCopy();
    updateStageCopy();
    rebuildRosterMarkup();
    rebuildSlotsMarkup();
    updateScrollbar();
    updateAnimationClasses();
    focusAnimationDirection_ = 0;
}

void PartyLoaderPreviewSession::rebuildRosterMarkup() {
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
        markup << "\" id=\"roster-row-" << index << "\" style=\"top:" << formatDp(static_cast<float>(index - visibleStart) * kRowStep) << ";\">"
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

void PartyLoaderPreviewSession::rebuildSlotsMarkup() {
    if (slotGridElement_ == nullptr) {
        return;
    }

    int maxHp = 1;
    int maxAtk = 1;
    int maxSpd = 1;
    for (const auto& entry : roster_) {
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
               << "<div class=\"stat-track\"><div class=\"stat-fill hp\" style=\"width:" << formatPercent(statRatio(entry.character.hp, maxHp) * 100.0f) << ";\"></div></div>"
               << "<div class=\"stat-value\">" << entry.character.hp << "</div>"
               << "</div>"
               << "<div class=\"stat-row\">"
               << "<img class=\"stat-icon\" src=\"" << kStatAtkRelativePath << "\"/>"
               << "<div class=\"stat-row-label\">ATK</div>"
               << "<div class=\"stat-track\"><div class=\"stat-fill atk\" style=\"width:" << formatPercent(statRatio(entry.character.atk, maxAtk) * 100.0f) << ";\"></div></div>"
               << "<div class=\"stat-value\">" << entry.character.atk << "</div>"
               << "</div>"
               << "<div class=\"stat-row\">"
               << "<img class=\"stat-icon\" src=\"" << kStatSpdRelativePath << "\"/>"
               << "<div class=\"stat-row-label\">SPD</div>"
               << "<div class=\"stat-track\"><div class=\"stat-fill spd\" style=\"width:" << formatPercent(statRatio(entry.character.spd, maxSpd) * 100.0f) << ";\"></div></div>"
               << "<div class=\"stat-value\">" << entry.character.spd << "</div>"
               << "</div>"
               << "</div></div></div></div></button>";
    }

    slotGridElement_->SetInnerRML(markup.str());
    cacheDynamicElements();
}

void PartyLoaderPreviewSession::cacheDynamicElements() {
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

void PartyLoaderPreviewSession::updateStaticCopy() const {
    if (rosterCopyElement_ != nullptr) {
        rosterCopyElement_->SetInnerRML("");
    }
    if (rosterFooterCopyElement_ != nullptr) {
        rosterFooterCopyElement_->SetInnerRML("");
    }
}

void PartyLoaderPreviewSession::updateStageCopy() const {
    if (stageKickerElement_ != nullptr) {
        stageKickerElement_->SetInnerRML(escapeRml(battleDefinition_.type == "tutorial" ? "Tutorial Battle" : "Flexible Battle"));
    }
    if (stageTitleElement_ != nullptr) {
        stageTitleElement_->SetInnerRML(escapeRml(battleDefinition_.name.empty() ? "Battle Setup" : battleDefinition_.name));
    }
    if (stageDescriptionElement_ != nullptr) {
        stageDescriptionElement_->SetInnerRML(escapeRml(bossDescription_));
    }
    if (stageCapacityElement_ != nullptr) {
        stageCapacityElement_->SetInnerRML("");
    }
    if (stageModeElement_ != nullptr) {
        stageModeElement_->SetInnerRML("");
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
            "UP / DOWN move, SPACE toggles, PAGE UP / DOWN scroll, and ENTER launches the battle preview.");
    }
    if (startButtonElement_ != nullptr) {
        startButtonElement_->SetClass("is-disabled", !canStart());
    }
}

void PartyLoaderPreviewSession::updateScrollbar() {
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
    const float thumbHeight = std::max(kScrollbarMinThumbHeight,
                                       trackHeight * (static_cast<float>(kVisibleRows) / static_cast<float>(roster_.size())));
    const float usableTravel = std::max(0.0f, trackHeight - thumbHeight);
    const float ratio = maxScrollOffset() > 0
        ? static_cast<float>(scrollOffset_) / static_cast<float>(maxScrollOffset())
        : 0.0f;
    scrollbarThumbElement_->SetProperty("height", formatDp(thumbHeight));
    scrollbarThumbElement_->SetProperty("top", formatDp(usableTravel * ratio));
}

void PartyLoaderPreviewSession::updateAnimationClasses() const {
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

void PartyLoaderPreviewSession::moveFocus(int delta, bool shouldPlayScrollSfx) {
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

void PartyLoaderPreviewSession::pageFocus(int deltaRows, bool shouldPlayScrollSfx) {
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

void PartyLoaderPreviewSession::toggleFocusedCharacter() {
    if (focusedRosterIndex_ < 0 || focusedRosterIndex_ >= static_cast<int>(roster_.size())) {
        return;
    }
    toggleCharacter(roster_[static_cast<std::size_t>(focusedRosterIndex_)].character.key);
}

void PartyLoaderPreviewSession::toggleCharacter(const std::string& key) {
    const int selectedIndex = selectedSlotForKey(key);
    if (selectedIndex >= 0) {
        if (isLockedKey(key)) {
            showToast("That member is locked into this lineup");
            return;
        }
        selectedKeys_.erase(selectedKeys_.begin() + selectedIndex);
    } else {
        if (static_cast<int>(selectedKeys_.size()) >= selectionCapacity()) {
            showToast("Party limit reached");
            return;
        }
        selectedKeys_.push_back(key);
    }

    slotSettleTimer_ = kSlotSettleDurationSeconds;
    refreshDocument();
    playScrollSfx();
}

void PartyLoaderPreviewSession::removeSelectedAt(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(selectedKeys_.size())) {
        return;
    }

    const std::string key = selectedKeys_[static_cast<std::size_t>(slotIndex)];
    if (isLockedKey(key)) {
        showToast("Locked members cannot be removed");
        return;
    }

    selectedKeys_.erase(selectedKeys_.begin() + slotIndex);
    slotSettleTimer_ = kSlotSettleDurationSeconds;
    refreshDocument();
    playScrollSfx();
}

void PartyLoaderPreviewSession::launchBattlePreview() {
    if (!canStart()) {
        showToast("Choose at least one member first");
        return;
    }

    confirmTimer_ = kConfirmDurationSeconds;
    updateAnimationClasses();
    playConfirmSfx();

    std::string names;
    for (std::size_t i = 0; i < selectedKeys_.size(); ++i) {
        if (i > 0) {
            names += " / ";
        }
        const auto it = std::find_if(roster_.begin(), roster_.end(), [&](const Entry& entry) {
            return entry.character.key == selectedKeys_[i];
        });
        names += (it != roster_.end()) ? it->character.title : selectedKeys_[i];
    }

    showToast("Launch Battle Preview: " + names);
}

void PartyLoaderPreviewSession::beginScrollbarDrag(float mouseY, bool centerThumb) {
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

void PartyLoaderPreviewSession::stopScrollbarDrag() {
    draggingScrollbar_ = false;
    scrollbarDragGrabOffset_ = 0.0f;
    if (scrollbarTrackElement_ != nullptr) {
        scrollbarTrackElement_->SetClass("is-dragging", false);
    }
}

void PartyLoaderPreviewSession::updateScrollFromPointer(float mouseY) {
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

void PartyLoaderPreviewSession::ensureFocusedVisible() {
    clampScrollOffset();
    if (focusedRosterIndex_ < scrollOffset_) {
        scrollOffset_ = focusedRosterIndex_;
    } else if (focusedRosterIndex_ >= scrollOffset_ + kVisibleRows) {
        scrollOffset_ = focusedRosterIndex_ - kVisibleRows + 1;
    }
    clampScrollOffset();
}

void PartyLoaderPreviewSession::clampScrollOffset() {
    scrollOffset_ = std::clamp(scrollOffset_, 0, std::max(0, static_cast<int>(roster_.size()) - kVisibleRows));
}

bool PartyLoaderPreviewSession::pointInElement(float x, float y, Rml::Element* element) const {
    if (element == nullptr || !element->IsVisible(true)) {
        return false;
    }

    const float left = element->GetAbsoluteLeft();
    const float top = element->GetAbsoluteTop();
    const float width = element->GetOffsetWidth();
    const float height = element->GetOffsetHeight();
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

int PartyLoaderPreviewSession::hoveredRosterIndex(float x, float y) const {
    for (std::size_t i = 0; i < rosterRowElements_.size(); ++i) {
        if (pointInElement(x, y, rosterRowElements_[i])) {
            return i < rosterRowIndices_.size() ? rosterRowIndices_[i] : -1;
        }
    }
    return -1;
}

int PartyLoaderPreviewSession::clickedSlotIndex(float x, float y) const {
    for (std::size_t i = 0; i < slotElements_.size(); ++i) {
        if (pointInElement(x, y, slotElements_[i])) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int PartyLoaderPreviewSession::selectionCapacity() const {
    return std::clamp(battleDefinition_.partySize, 1, 4);
}

int PartyLoaderPreviewSession::maxScrollOffset() const {
    return std::max(0, static_cast<int>(roster_.size()) - kVisibleRows);
}

int PartyLoaderPreviewSession::selectedSlotForKey(const std::string& key) const {
    for (std::size_t i = 0; i < selectedKeys_.size(); ++i) {
        if (selectedKeys_[i] == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool PartyLoaderPreviewSession::isLockedKey(const std::string& key) const {
    return std::find(battleDefinition_.lockedLineup.begin(), battleDefinition_.lockedLineup.end(), key) !=
        battleDefinition_.lockedLineup.end();
}

bool PartyLoaderPreviewSession::canStart() const {
    const int count = static_cast<int>(selectedKeys_.size());
    return count >= 1 && count <= selectionCapacity();
}

void PartyLoaderPreviewSession::playScrollSfx() {
    if (!audioReady_ || scrollSfxPath_.empty()) {
        return;
    }
    (void)sfxPlayer_.playWavOneShot(scrollSfxPath_, 0.78f, true);
}

void PartyLoaderPreviewSession::playConfirmSfx() {
    if (!audioReady_ || confirmSfxPath_.empty()) {
        return;
    }
    (void)sfxPlayer_.playWavOneShot(confirmSfxPath_, 0.92f, true);
}

void PartyLoaderPreviewSession::showToast(const std::string& message) {
    if (toastElement_ == nullptr) {
        return;
    }

    toastElement_->SetInnerRML(escapeRml(message));
    toastElement_->SetClass("is-visible", true);
    toastTimer_ = kToastDurationSeconds;
}

}  // namespace graphics::preview
