#include "front_ui_credits.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>
#include <utility>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include "../platform/path_resolution.h"
#include "../game/vn/vn_system.h"
#include "../game/vn/vn_script_catalog.h"

namespace graphics::frontui {
namespace {

constexpr const char* kMainMenuLogoRelativePath = "assets/vn/backgrounds/General_Art/MainMenuTitle.png";
constexpr float kReferenceWidth = 1280.0f;
constexpr float kReferenceHeight = 720.0f;
constexpr float kCreditsScrollSpeedDpPerSecond = 54.0f;
constexpr float kCreditsDefaultSpeedMultiplier = 1.5f;
constexpr float kCreditsMinimumSpeedMultiplier = 1.0f;
constexpr float kCreditsEndPaddingDp = 94.0f;
constexpr float kVisualActiveHoldDistanceDp = 144.0f;
constexpr float kVisualTransitionSeconds = 0.5f;
constexpr float kVisualTextDelaySeconds = 0.5f;
constexpr float kCreditsSpeedupMultiplier = 3.2f;
constexpr float kCreditsArrowSpeedStepFactor = 2.0f;
constexpr float kCreditsArrowSpeedMultiplierCap = 32.0f;
constexpr float kOpeningBlackSeconds = 0.8f;
constexpr float kOpeningTitleFadeInSeconds = 0.55f;
constexpr float kOpeningTitleHoldSeconds = 2.0f;
constexpr float kOpeningChromeFadeInSeconds = 0.65f;
constexpr float kTitleBodyStartGapDp = 36.0f;
constexpr float kOpeningSequenceSeconds =
    kOpeningBlackSeconds + kOpeningTitleFadeInSeconds + kOpeningTitleHoldSeconds;

class CallbackEventListener final : public Rml::EventListener {
public:
    explicit CallbackEventListener(std::function<void(Rml::Event&)> callback)
        : callback_(std::move(callback)) {}

    void ProcessEvent(Rml::Event& event) override {
        if (callback_) {
            callback_(event);
        }
    }

private:
    std::function<void(Rml::Event&)> callback_;
};

std::string escapeRmlText(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (char ch : text) {
        switch (ch) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

std::string formatDp(float value) {
    std::ostringstream stream;
    stream << value << "dp";
    return stream.str();
}

std::string formatPx(float value) {
    std::ostringstream stream;
    stream << value << "px";
    return stream.str();
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep01(float value) {
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float resolveLayoutScale(Rml::ElementDocument* document) {
    if (document == nullptr) {
        return 1.0f;
    }

    const Rml::Context* context = document->GetContext();
    if (context == nullptr) {
        return 1.0f;
    }

    const Rml::Vector2i dimensions = context->GetDimensions();
    if (dimensions.x <= 0 || dimensions.y <= 0) {
        return 1.0f;
    }

    const float widthScale = static_cast<float>(dimensions.x) / kReferenceWidth;
    const float heightScale = static_cast<float>(dimensions.y) / kReferenceHeight;
    return std::max(0.01f, std::min(widthScale, heightScale));
}

float scaledDp(float value, float scale) {
    return value * scale;
}

std::string blockLayoutClass(const game::credits::CreditsBlock& block) {
    if (block.layout == "split") {
        return "credits-block credits-block-split";
    }
    if (block.textAlign == "left") {
        return "credits-block credits-block-center credits-copy-align-left";
    }
    return "credits-block credits-block-center credits-copy-align-center";
}

bool blockUsesVisualColumn(const game::credits::CreditsBlock& block) {
    return block.layout == "split";
}

bool isNicknameEntry(const std::string& entry) {
    if (entry.empty()) {
        return false;
    }

    bool hasLetter = false;
    for (char ch : entry) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch)) {
            return false;
        }
        if (std::isalpha(uch)) {
            hasLetter = true;
        }
        if (!(std::isalnum(uch) || ch == '_' || ch == '-')) {
            return false;
        }
    }

    return hasLetter;
}

void appendBlockEntries(std::string& markup, const std::vector<std::string>& entries) {
    for (std::size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex) {
        const char* entryClass = isNicknameEntry(entries[entryIndex]) ? "credits-entry credits-entry-name"
                                                                      : "credits-entry credits-entry-detail";
        markup += "<div class='";
        markup += entryClass;
        markup += "'>";
        markup += escapeRmlText(entries[entryIndex]);
        markup += "</div>";
    }
}

std::string creditsMarkup(const game::credits::CreditsData& creditsData, const Rml::ElementDocument& document) {
    std::string markup;
    markup.reserve(8192);

    const std::string resolvedTitleImage = platform::path::resolvePath(kMainMenuLogoRelativePath);
    if (!resolvedTitleImage.empty() && std::filesystem::exists(resolvedTitleImage)) {
        markup += "<div id='credits-title-card' class='credits-title-card'>";
        markup += "<img id='credits-title-card-image' class='credits-title-card-image' src='" +
                  platform::path::resolvePathForRml(kMainMenuLogoRelativePath, document.GetSourceURL()) +
                  "'/>";
        markup += "</div>";
    } else {
        markup += "<div id='credits-title-card' class='credits-roll-lead'>";
        markup += "<div class='credits-roll-title'>" + escapeRmlText(creditsData.title) + "</div>";
        if (!creditsData.subtitle.empty()) {
            markup += "<div class='credits-roll-subtitle'>" + escapeRmlText(creditsData.subtitle) + "</div>";
        }
        markup += "</div>";
    }
    markup += "<div id='credits-title-gap' class='credits-title-gap'></div>";
    markup += "<div id='credits-roll-body'>";

    for (std::size_t index = 0; index < creditsData.blocks.size(); ++index) {
        const auto& block = creditsData.blocks[index];
        const std::string blockId = "credits-block-" + std::to_string(index);
        markup += "<div id='" + blockId + "' class='" + blockLayoutClass(block) + "'>";

        if (blockUsesVisualColumn(block)) {
            markup += "<div id='credits-copy-" + std::to_string(index) +
                      "' class='credits-block-split-copy credits-copy-align-center'>";
            if (!block.kicker.empty()) {
                markup += "<div class='credits-block-kicker'>" + escapeRmlText(block.kicker) + "</div>";
            }
            markup += "<div class='credits-section-heading'>" + escapeRmlText(block.heading) + "</div>";
            appendBlockEntries(markup, block.entries);
            markup += "</div>";

            markup += "<div id='credits-visual-shell-" + std::to_string(index) + "' class='credits-block-split-visual'>";
            const std::string resolvedImage = platform::path::resolvePath(block.image);
            if (!block.imageLabel.empty()) {
                markup += "<div id='credits-visual-label-" + std::to_string(index) + "' class='credits-visual-label'>" +
                          escapeRmlText(block.imageLabel) + "</div>";
            }
            if (!resolvedImage.empty() && std::filesystem::exists(resolvedImage)) {
                markup += "<img id='credits-visual-" + std::to_string(index) + "' class='credits-visual-image' src='" +
                          platform::path::resolvePathForRml(block.image, document.GetSourceURL()) +
                          "'/>";
            } else {
                markup += "<div id='credits-visual-" + std::to_string(index) + "' class='credits-visual-image credits-visual-fallback'></div>";
            }
            if (!block.imageCaption.empty()) {
                markup += "<div id='credits-visual-caption-" + std::to_string(index) + "' class='credits-visual-caption'>" +
                          escapeRmlText(block.imageCaption) + "</div>";
            }
            markup += "</div>";
        } else {
            if (!block.kicker.empty()) {
                markup += "<div class='credits-block-kicker'>" + escapeRmlText(block.kicker) + "</div>";
            }
            markup += "<div class='credits-section-heading'>" + escapeRmlText(block.heading) + "</div>";
            appendBlockEntries(markup, block.entries);
        }

        markup += "</div>";
    }

    markup += "<div id='credits-group-block' class='credits-group-block'>";
    markup += "<div class='credits-group-heading'>FINAL FRAME / FULL CAST</div>";

    const std::string resolvedGroupImage = platform::path::resolvePath(creditsData.groupImage);
    if (!resolvedGroupImage.empty() && std::filesystem::exists(resolvedGroupImage)) {
        markup += "<img class='credits-group-image' src='" +
                  platform::path::resolvePathForRml(creditsData.groupImage, document.GetSourceURL()) +
                  "'/>";
    } else {
        markup += "<div class='credits-group-placeholder'>";
        markup += "<div class='credits-group-placeholder-title'>" +
                  escapeRmlText(creditsData.groupPlaceholderTitle) +
                  "</div>";
        markup += "<div class='credits-group-placeholder-copy'>" +
                  escapeRmlText(creditsData.groupPlaceholderCopy) +
                  "</div>";
        markup += "</div>";
    }

    if (!creditsData.groupImageCaption.empty()) {
        markup += "<div class='credits-group-caption'>" + escapeRmlText(creditsData.groupImageCaption) + "</div>";
    }
    markup += "</div>";
    markup += "</div>";

    return markup;
}

game::credits::CreditsData fallbackCreditsData() {
    game::credits::CreditsData creditsData;
    creditsData.kicker = "FINAL SIGNAL / STAFF ROLL";
    creditsData.title = "CREDITS";
    creditsData.subtitle = "Combat systems, RmlUi migration, narrative integration, and the team that built BIT's underground idol route.";
    creditsData.groupImage = "assets/vn/backgrounds/credits/group_cast_placeholder.png";
    creditsData.groupPlaceholderTitle = "TEAM SIGNAL / PHASE 1-2 BUILD";
    creditsData.groupPlaceholderCopy = "LYES / JADEN / TIMOTHY / ANMOL / REVELL / RUSSEL";
    creditsData.groupImageCaption = "IN-GAME DEVELOPMENT CREDITS DRAWN FROM THE PHASE 1 AND PHASE 2 PROJECT LOGS.";
    creditsData.returnPrompt = "PRESS ANY KEY OR CLICK TO RETURN TO MAIN MENU";
    creditsData.blocks = {
        {"FOUNDATION LAYER", "CORE COMBAT & CHARACTER SYSTEMS", "center", "center", "", "", "",
         {"LYES", "TURN-BASED CORE, BOSSES, ULTIMATES", "RHYTHM PHASE, SHIELDS, QR ATTACK",
          "2.5D CAMERA, PRESENTATION, AUDIO SYNC", "VN-BATTLE TRIGGERS AND STORY FUSION"}},
        {"PLATFORM STACK", "PLATFORM, BUILDS & BATTLE UI", "split", "center", "assets/vn/backgrounds/ch4/1.png",
         "WINDOWS PIPELINE / BATTLE UI",
         "Build stability, platform fixes, and RmlUi battle work that kept the project shippable across environments.",
         {"JADEN", "WINDOWS CI AND PACKAGING", "OPENGL FIXES AND BUILD CLEANUP", "RMLUI BATTLE HUD",
          "PARTY SELECTOR, CJK TEXT, TOOLING"}},
        {"FRONTEND SHIFT", "MENUS, SAVE FLOW & FRONTEND", "center", "center", "", "", "",
         {"TIMOTHY", "MAIN MENU, PAUSE, SETTINGS", "BATTLE ACCESS AND SAVE-LOAD FLOW",
          "RMLUI FRONTEND MIGRATION", "VN TEXT SCROLL AND UI POLISH"}},
        {"DATA BANK", "DATA, CONFIG & CHARACTER KITS", "split", "center", "assets/vn/backgrounds/ch0/5.png",
         "JSON FLOW / SUPPORT LAYER",
         "Configuration structure, save-data support, and character-kit support that held the content pipeline together.",
         {"ANMOL", "JSON SAVE-DATA SUPPORT", "CHARACTER KIT SUPPORT", "TECHNICAL REVIEW AND CONSISTENCY"}},
        {"STORY SIGNAL", "NARRATIVE, VN BOOKENDS & LORE", "center", "center", "", "", "",
         {"REVELL", "MAIN STORY, LORE, STORY BRANCHES", "BOOKEND WRITING AND CHARACTER CONTEXT", "LYES",
          "VN-BATTLE STORY INTEGRATION"}},
        {"CAST SUPPORT", "PLAYER KITS, ASSETS & BALANCE", "split", "center", "assets/vn/backgrounds/finale/1.png",
         "PLAYER KIT / FEEDBACK LOOP",
         "Support work across assets, kits, playtesting, and balance feedback that sharpened the final playable cast.",
         {"RUSSEL", "PLAYER KIT AND ASSET SUPPORT", "TIERLIST AND BALANCE FEEDBACK", "REVELL",
          "CHARACTER KIT SUPPORT AND PLAYTESTING"}},
    };
    return creditsData;
}

}  // namespace

bool CreditsDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
    creditsLoaded_ = game::credits::loadCreditsData(vn::creditsDataPath(), creditsData_);
    if (!creditsLoaded_) {
        creditsData_ = fallbackCreditsData();
    }

    pendingReturn_ = false;
    scrollMetricsReady_ = false;
    rollFinished_ = false;
    speedupHeld_ = false;
    openingFinished_ = false;
    arrowSpeedMultiplier_ = kCreditsDefaultSpeedMultiplier;
    currentTop_ = 0.0f;
    startTop_ = 0.0f;
    endTop_ = 0.0f;
    viewportHeight_ = 0.0f;
    openingElapsed_ = 0.0f;
    layoutScale_ = resolveLayoutScale(document_);
    songDurationSeconds_ = 0.0f;
    songBaseVolume_ = 1.0f;
    syncedPlaybackActive_ = false;
    currentSubtitleText_ = "\x01";
    visualTracks_.clear();

    if (Rml::Element* element = document_->GetElementById("credits-return-prompt")) {
        element->SetInnerRML(escapeRmlText(creditsData_.returnPrompt));
    }
    setSubtitleText("");

    attachListeners();
    populateContent();
    setChromeOpacity(0.0f);
    setTitleCardOpacity(0.0f);
    vn::stopBgmPlayback();
    syncedPlaybackActive_ = startCreditsMusic(state);
    syncPrompt();
    return true;
}

void CreditsDocumentController::unbind() {
    detachEventListeners(listeners_);
    musicPlayer_.stop();
    syncedPlaybackActive_ = false;
    songDurationSeconds_ = 0.0f;
    currentSubtitleText_.clear();
    document_ = nullptr;
}

void CreditsDocumentController::sync(const AppState& state) {
    (void)state;
}

void CreditsDocumentController::update(const AppState& state, float deltaSeconds) {
    if (document_ == nullptr) {
        return;
    }

    if (syncedPlaybackActive_) {
        musicPlayer_.setVolume(songBaseVolume_ * std::clamp(state.settings.musicVolume, 0.0f, 1.0f));
    }

    if (!scrollMetricsReady_) {
        initializeScrollMetrics();
    }

    updateOpening(deltaSeconds);
    if (openingFinished_) {
        updateScroll(deltaSeconds);
    }
    updatePresentation(deltaSeconds);
    updateSubtitle();
}

void CreditsDocumentController::moveSelection(int delta) {
    (void)delta;
}

void CreditsDocumentController::activateSelection() {
    if (rollFinished_) {
        requestReturn();
    }
}

void CreditsDocumentController::handleKeyDown(const SDL_KeyboardEvent& event) {
    if ((event.keysym.mod & KMOD_CTRL) != 0 && event.keysym.sym == SDLK_p) {
        requestReturn();
        return;
    }

    if (syncedPlaybackActive_) {
        if (rollFinished_) {
            requestReturn();
        }
        return;
    }

    if (event.keysym.sym == SDLK_SPACE && !rollFinished_) {
        speedupHeld_ = true;
        return;
    }

    if (!rollFinished_ && event.repeat == 0 && event.keysym.sym == SDLK_DOWN) {
        arrowSpeedMultiplier_ =
            std::min(kCreditsArrowSpeedMultiplierCap,
                     arrowSpeedMultiplier_ * kCreditsArrowSpeedStepFactor);
        return;
    }

    if (!rollFinished_ && event.repeat == 0 && event.keysym.sym == SDLK_UP) {
        arrowSpeedMultiplier_ =
            std::max(kCreditsMinimumSpeedMultiplier,
                     arrowSpeedMultiplier_ / kCreditsArrowSpeedStepFactor);
        return;
    }

    if (rollFinished_) {
        requestReturn();
    }
}

void CreditsDocumentController::handleKeyUp(const SDL_KeyboardEvent& event) {
    if (syncedPlaybackActive_) {
        return;
    }
    if (event.keysym.sym == SDLK_SPACE) {
        speedupHeld_ = false;
    }
}

void CreditsDocumentController::cancel() {
    if (rollFinished_) {
        requestReturn();
    }
}

void CreditsDocumentController::applyState(AppState& state) {
    if (!pendingReturn_) {
        return;
    }

    pendingReturn_ = false;
    state.pauseContext = PauseContext::Story;
    state.pauseSelection = PauseAction::Continue;
    state.confirmSelection = ConfirmAction::Cancel;
    state.mainSelection = MainMenuAction::Start;
    state.menuIntroTime = 0.0f;
    state.screen = ScreenState::MainMenu;
}

std::optional<Command> CreditsDocumentController::consumeCommand() {
    return std::nullopt;
}

void CreditsDocumentController::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* target = document_->GetElementById("credits-screen")) {
        auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
            if (rollFinished_) {
                requestReturn();
            }
        });
        target->AddEventListener(Rml::EventId::Click, clickListener.get());
        listeners_.push_back(EventListenerBinding{
            target,
            Rml::EventId::Click,
            false,
            std::move(clickListener),
        });
    }
}

void CreditsDocumentController::populateContent() {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("credits-roll-content")) {
        element->SetInnerRML(creditsMarkup(creditsData_, *document_));
    }
}

void CreditsDocumentController::updateScroll(float deltaSeconds) {
    if (!scrollMetricsReady_ || rollFinished_) {
        return;
    }

    if (syncedPlaybackActive_) {
        const float songDuration = currentSongDurationSeconds();
        const float scrollDuration = songDuration - kOpeningSequenceSeconds;
        if (scrollDuration <= 0.0f) {
            currentTop_ = endTop_;
            setScrollTop(currentTop_);
            rollFinished_ = true;
            syncPrompt();
            return;
        }

        const float progress = clamp01((currentSongPlaybackSeconds() - kOpeningSequenceSeconds) / scrollDuration);
        currentTop_ = lerp(startTop_, endTop_, progress);
        setScrollTop(currentTop_);
        if (progress >= 1.0f || musicPlayer_.isFinished()) {
            currentTop_ = endTop_;
            setScrollTop(currentTop_);
            rollFinished_ = true;
            syncPrompt();
        }
        return;
    }

    const float speedMultiplier =
        arrowSpeedMultiplier_ *
        (speedupHeld_ ? kCreditsSpeedupMultiplier : 1.0f);
    const float scrollSpeedPxPerSecond = scaledDp(kCreditsScrollSpeedDpPerSecond, layoutScale_);
    currentTop_ = std::max(endTop_, currentTop_ - (scrollSpeedPxPerSecond * speedMultiplier * deltaSeconds));
    setScrollTop(currentTop_);

    if (currentTop_ <= endTop_ + 0.01f) {
        currentTop_ = endTop_;
        setScrollTop(currentTop_);
        rollFinished_ = true;
        syncPrompt();
    }
}

void CreditsDocumentController::updateOpening(float deltaSeconds) {
    if (syncedPlaybackActive_) {
        openingElapsed_ = currentSongPlaybackSeconds();
    } else {
        openingElapsed_ += deltaSeconds;
    }

    if (openingFinished_) {
        const float chromeFadeT = clamp01((openingElapsed_ - kOpeningSequenceSeconds) / kOpeningChromeFadeInSeconds);
        setChromeOpacity(smoothstep01(chromeFadeT));
        return;
    }
    const float titleFadeT = clamp01((openingElapsed_ - kOpeningBlackSeconds) / kOpeningTitleFadeInSeconds);
    const float titleOpacity = smoothstep01(titleFadeT);
    setTitleCardOpacity(titleOpacity);
    setChromeOpacity(0.0f);

    if (openingElapsed_ >= kOpeningSequenceSeconds) {
        openingFinished_ = true;
        setTitleCardOpacity(1.0f);
    }
}

void CreditsDocumentController::initializeScrollMetrics() {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Context* context = document_->GetContext()) {
        context->Update();
    }

    layoutScale_ = resolveLayoutScale(document_);

    Rml::Element* viewport = document_->GetElementById("credits-roll-viewport");
    Rml::Element* content = document_->GetElementById("credits-roll-content");
    if (viewport == nullptr || content == nullptr) {
        return;
    }

    bool titleGapChanged = false;
    if (Rml::Element* titleCard = document_->GetElementById("credits-title-card")) {
        if (Rml::Element* titleGap = document_->GetElementById("credits-title-gap")) {
            const float viewportHeight = viewport->GetClientHeight();
            const float minimumGap = std::max(0.0f,
                                              (viewportHeight * 0.5f) - (titleCard->GetOffsetHeight() * 0.5f) +
                                              scaledDp(kTitleBodyStartGapDp, layoutScale_));
            titleGap->SetProperty("height", formatPx(minimumGap));
            titleGapChanged = true;
        }
    }

    bool splitBlockHeightChanged = false;
    for (std::size_t index = 0; index < creditsData_.blocks.size(); ++index) {
        if (!blockUsesVisualColumn(creditsData_.blocks[index])) {
            continue;
        }

        Rml::Element* block = document_->GetElementById("credits-block-" + std::to_string(index));
        Rml::Element* copy = document_->GetElementById("credits-copy-" + std::to_string(index));
        Rml::Element* visualShell = document_->GetElementById("credits-visual-shell-" + std::to_string(index));
        if (block == nullptr || copy == nullptr || visualShell == nullptr) {
            continue;
        }

        const float splitHeight = std::max(copy->GetOffsetHeight(), visualShell->GetOffsetHeight());
        if (splitHeight <= 0.0f) {
            continue;
        }

        block->SetProperty("height", formatPx(splitHeight));
        splitBlockHeightChanged = true;
    }

    if (titleGapChanged || splitBlockHeightChanged) {
        if (Rml::Context* context = document_->GetContext()) {
            context->Update();
        }
    }

    const float viewportHeight = viewport->GetClientHeight();
    const float contentHeight = content->GetOffsetHeight();
    if (viewportHeight <= 0.0f || contentHeight <= 0.0f) {
        return;
    }

    viewportHeight_ = viewportHeight;

    const float contentAbsoluteTop = content->GetAbsoluteTop();
    const float contentCenter = content->GetAbsoluteLeft() + (content->GetOffsetWidth() * 0.5f);

    startTop_ = viewportHeight;
    if (Rml::Element* titleCard = document_->GetElementById("credits-title-card")) {
        startTop_ = (viewportHeight_ * 0.5f) -
                    ((titleCard->GetAbsoluteTop() - contentAbsoluteTop) + (titleCard->GetOffsetHeight() * 0.5f));
    }
    const float endPaddingPx = scaledDp(kCreditsEndPaddingDp, layoutScale_);
    endTop_ = std::min(endPaddingPx, viewportHeight - contentHeight - endPaddingPx);
    currentTop_ = startTop_;
    setScrollTop(currentTop_);

    for (std::size_t index = 0; index < creditsData_.blocks.size(); ++index) {
        if (!blockUsesVisualColumn(creditsData_.blocks[index])) {
            continue;
        }

        const std::string visualElementId = "credits-visual-" + std::to_string(index);
        const std::string blockElementId = "credits-block-" + std::to_string(index);
        const std::string copyElementId = "credits-copy-" + std::to_string(index);
        const std::string visualShellElementId = "credits-visual-shell-" + std::to_string(index);
        const std::string visualLabelElementId = "credits-visual-label-" + std::to_string(index);
        const std::string visualCaptionElementId = "credits-visual-caption-" + std::to_string(index);

        Rml::Element* visual = document_->GetElementById(visualElementId);
        Rml::Element* block = document_->GetElementById(blockElementId);
        Rml::Element* copy = document_->GetElementById(copyElementId);
        Rml::Element* visualShell = document_->GetElementById(visualShellElementId);
        if (visual != nullptr) {
            float activeTranslateX = 0.0f;
            if (block != nullptr && copy != nullptr && visualShell != nullptr) {
                const float splitLeft = std::min(copy->GetAbsoluteLeft(), visualShell->GetAbsoluteLeft());
                const float splitRight = std::max(copy->GetAbsoluteLeft() + copy->GetOffsetWidth(),
                                                  visualShell->GetAbsoluteLeft() + visualShell->GetOffsetWidth());
                const float splitCenter = (splitLeft + splitRight) * 0.5f;
                activeTranslateX = contentCenter - splitCenter;
            }

            visualTracks_.push_back(VisualTrack{
                visualElementId,
                blockElementId,
                visualShellElementId,
                document_->GetElementById(visualLabelElementId) != nullptr ? visualLabelElementId : std::string{},
                document_->GetElementById(visualCaptionElementId) != nullptr ? visualCaptionElementId : std::string{},
                visual->GetAbsoluteTop() - contentAbsoluteTop,
                visual->GetOffsetHeight(),
                activeTranslateX,
                0.0f,
                0.0f,
                0.0f,
                false,
            });
        }
    }

    if (Rml::Element* groupBlock = document_->GetElementById("credits-group-block")) {
        const float centeredGroupTop = (viewportHeight_ * 0.5f) -
                                       ((groupBlock->GetAbsoluteTop() - contentAbsoluteTop) +
                                        (groupBlock->GetOffsetHeight() * 0.5f));
        endTop_ = centeredGroupTop;
    }

    scrollMetricsReady_ = true;
    updatePresentation(0.0f, true);
}

void CreditsDocumentController::updateSubtitle() {
    if (document_ == nullptr) {
        return;
    }

    if (!syncedPlaybackActive_ || creditsData_.subtitles.empty()) {
        setSubtitleText("");
        return;
    }

    const float playbackSeconds = currentSongPlaybackSeconds();
    for (const auto& cue : creditsData_.subtitles) {
        if (playbackSeconds >= cue.startSeconds && playbackSeconds < cue.endSeconds) {
            setSubtitleText(cue.text);
            return;
        }
    }

    setSubtitleText("");
}

void CreditsDocumentController::setScrollTop(float top) {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("credits-roll-content")) {
        element->SetProperty("top", formatPx(top));
    }
}

void CreditsDocumentController::setSubtitleText(const std::string& text) {
    if (document_ == nullptr || text == currentSubtitleText_) {
        return;
    }

    currentSubtitleText_ = text;
    if (Rml::Element* element = document_->GetElementById("credits-subtitle")) {
        element->SetInnerRML(escapeRmlText(text));
        element->SetClass("is-hidden", text.empty());
    }
}

void CreditsDocumentController::setChromeOpacity(float opacity) {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("credits-chrome-layer")) {
        element->SetProperty("opacity", std::to_string(clamp01(opacity)));
    }
}

void CreditsDocumentController::updatePresentation(float deltaSeconds, bool snap) {
    if (document_ == nullptr || !scrollMetricsReady_) {
        return;
    }

    const float viewportCenter = viewportHeight_ * 0.5f;
    const float activeHoldDistancePx = scaledDp(kVisualActiveHoldDistanceDp, layoutScale_);
    for (VisualTrack& track : visualTracks_) {
        if (Rml::Element* element = document_->GetElementById(track.visualElementId)) {
            const float visualCenter = currentTop_ + track.contentTop + (track.height * 0.5f);
            const float distance = std::fabs(viewportCenter - visualCenter);
            const float targetWeight = distance <= activeHoldDistancePx ? 1.0f : 0.0f;
            const bool targetActive = targetWeight > 0.5f;

            if (snap || deltaSeconds <= 0.0f) {
                track.presentationWeight = targetWeight;
                track.textPresentationWeight = targetWeight;
                track.textRevealDelayRemaining = 0.0f;
                track.targetActive = targetActive;
            } else {
                const float step = deltaSeconds / kVisualTransitionSeconds;
                if (targetWeight > track.presentationWeight) {
                    track.presentationWeight = std::min(targetWeight, track.presentationWeight + step);
                } else if (targetWeight < track.presentationWeight) {
                    track.presentationWeight = std::max(targetWeight, track.presentationWeight - step);
                }

                if (targetActive) {
                    if (!track.targetActive) {
                        track.textRevealDelayRemaining = kVisualTextDelaySeconds;
                    }
                    track.targetActive = true;

                    if (track.textRevealDelayRemaining > 0.0f) {
                        track.textRevealDelayRemaining = std::max(0.0f, track.textRevealDelayRemaining - deltaSeconds);
                    } else if (targetWeight > track.textPresentationWeight) {
                        track.textPresentationWeight = std::min(targetWeight, track.textPresentationWeight + step);
                    }
                } else {
                    track.targetActive = false;
                    track.textRevealDelayRemaining = 0.0f;
                    if (targetWeight < track.textPresentationWeight) {
                        track.textPresentationWeight = std::max(targetWeight, track.textPresentationWeight - step);
                    }
                }
            }

            const float easedWeight = smoothstep01(track.presentationWeight);
            const float easedTextWeight = smoothstep01(track.textPresentationWeight);
            element->SetProperty("opacity", std::to_string(easedWeight));

            if (Rml::Element* block = document_->GetElementById(track.blockElementId)) {
                block->SetProperty("transform",
                                   "translate(" + formatPx(track.activeTranslateX * easedWeight) + ", 0px)");
            }

            if (Rml::Element* visualShell = document_->GetElementById(track.visualShellElementId)) {
                visualShell->SetProperty("opacity", "1.0");
            }

            if (!track.visualLabelElementId.empty()) {
                if (Rml::Element* visualLabel = document_->GetElementById(track.visualLabelElementId)) {
                    visualLabel->SetProperty("opacity", std::to_string(easedTextWeight));
                }
            }

            if (!track.visualCaptionElementId.empty()) {
                if (Rml::Element* visualCaption = document_->GetElementById(track.visualCaptionElementId)) {
                    visualCaption->SetProperty("opacity", std::to_string(easedTextWeight));
                }
            }
        }
    }
}

void CreditsDocumentController::setTitleCardOpacity(float opacity) {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("credits-title-card")) {
        element->SetProperty("opacity", std::to_string(clamp01(opacity)));
    }
}

void CreditsDocumentController::syncPrompt() const {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("credits-return-prompt")) {
        element->SetClass("is-hidden", !rollFinished_);
    }
}

void CreditsDocumentController::requestReturn() {
    pendingReturn_ = true;
}

bool CreditsDocumentController::startCreditsMusic(const AppState& state) {
    songBaseVolume_ = std::clamp(creditsData_.music.volume, 0.0f, 1.0f);
    if (creditsData_.music.path.empty()) {
        return false;
    }

    const std::string resolvedPath = platform::path::resolvePath(creditsData_.music.path);
    if (resolvedPath.empty() || !std::filesystem::exists(resolvedPath)) {
        return false;
    }

    const float masterVolume = std::clamp(state.settings.musicVolume, 0.0f, 1.0f);
    if (!musicPlayer_.play(resolvedPath, songBaseVolume_ * masterVolume, 0.0f, false)) {
        return false;
    }

    songDurationSeconds_ = musicPlayer_.durationSeconds();
    if (songDurationSeconds_ <= kOpeningSequenceSeconds) {
        musicPlayer_.stop();
        songDurationSeconds_ = 0.0f;
        return false;
    }

    return true;
}

float CreditsDocumentController::currentSongPlaybackSeconds() const {
    if (!syncedPlaybackActive_) {
        return openingElapsed_;
    }
    return musicPlayer_.playbackSeconds();
}

float CreditsDocumentController::currentSongDurationSeconds() const {
    if (syncedPlaybackActive_) {
        return musicPlayer_.durationSeconds();
    }
    return songDurationSeconds_;
}

}  // namespace graphics::frontui
