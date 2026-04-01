#include "front_ui_credits.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>
#include <utility>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include "../platform/path_resolution.h"

namespace graphics::frontui {
namespace {

constexpr const char* kCreditsJsonRelativePath = "assets/vn/json/credits.json";
constexpr const char* kMainMenuLogoRelativePath = "assets/vn/backgrounds/General_Art/MainMenuTitle.png";
constexpr float kReferenceWidth = 1280.0f;
constexpr float kReferenceHeight = 720.0f;
constexpr float kCreditsScrollSpeedDpPerSecond = 54.0f;
constexpr float kCreditsEndPaddingDp = 94.0f;
constexpr float kVisualActiveHoldDistanceDp = 144.0f;
constexpr float kVisualTransitionSeconds = 0.5f;
constexpr float kVisualTextDelaySeconds = 0.5f;
constexpr float kCreditsSpeedupMultiplier = 3.2f;
constexpr float kOpeningBlackSeconds = 0.8f;
constexpr float kOpeningTitleFadeInSeconds = 0.55f;
constexpr float kOpeningTitleHoldSeconds = 2.0f;
constexpr float kOpeningChromeFadeInSeconds = 0.65f;
constexpr float kTitleBodyStartGapDp = 36.0f;

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
    return "credits-block credits-block-center";
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

        if (block.layout == "split") {
            markup += "<div id='credits-copy-" + std::to_string(index) + "' class='credits-block-split-copy credits-block-center-copy'>";
            if (!block.kicker.empty()) {
                markup += "<div class='credits-block-kicker'>" + escapeRmlText(block.kicker) + "</div>";
            }
            markup += "<div class='credits-section-heading'>" + escapeRmlText(block.heading) + "</div>";
            for (const std::string& entry : block.entries) {
                markup += "<div class='credits-entry'>" + escapeRmlText(entry) + "</div>";
            }
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
            for (const std::string& entry : block.entries) {
                markup += "<div class='credits-entry'>" + escapeRmlText(entry) + "</div>";
            }
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
    creditsData.subtitle = "Credits JSON failed to load. This fallback exists so the screen still renders while the data file is being repaired.";
    creditsData.groupImage = "assets/vn/backgrounds/credits/group_cast_placeholder.png";
    creditsData.groupPlaceholderTitle = "GROUP ILLUSTRATION PENDING";
    creditsData.groupPlaceholderCopy =
        "Miku centered, full cast grouped around her. Slot reserved for Lyes' final art once the roster is complete.";
    creditsData.groupImageCaption = "GROUP IMAGE SLOT RESERVED FOR LYES";
    creditsData.returnPrompt = "PRESS ANY KEY OR CLICK TO RETURN TO MAIN MENU";
    creditsData.blocks = {
        {"SIGNAL ONE / OPENING", "OUR UNDERGROUND BIT IDOL", "center", "", "", "",
         {"ANMOL VARGHESE", "TIMOTHY PILLAI", "REVELL JOSE", "RUSSEL REJI"}},
        {"RETROSPECTIVE FRAME", "STORY & DIRECTION", "split", "assets/vn/backgrounds/ch0/0.jpg",
         "CHAPTER 0 / OPENING SIGNAL", "The route starts loud, awkward, and way too sincere to stop halfway through.",
         {"ANMOL VARGHESE", "TIMOTHY PILLAI", "NORA VEIL", "JUNO STATIC"}},
        {"CAST LOAD", "CHARACTER PERFORMANCE", "center", "", "", "",
         {"MIKU AOKI", "CUPCAKKE VEGA", "LYOO NIGHTSHADE", "JIAFEI STARLING", "ARI MERIDIAN", "LUOTIANYI BLUE"}},
        {"SCENE MEMORY", "VISUAL DEVELOPMENT", "split", "assets/vn/backgrounds/ch0/5.png",
         "UNDERGROUND VENUE / ENTRY POINT", "Back alleys, lecture halls, and the wrong kind of destiny all got the same glam pass.",
         {"LYES AURORA", "MIRA DOTGRID", "ELI SERRANO", "KAI LATTICE"}},
        {"SYSTEM STACK", "PROGRAMMING & BATTLE FLOW", "center", "", "", "",
         {"TIMOTHY PILLAI", "IVY CIRCUIT", "MARCEL VANTAGE", "SOREN PULSE", "YUNA CHECKSUM"}},
        {"SCENE MEMORY", "MUSIC, VOICE & CHAOS", "split", "assets/vn/backgrounds/ch4/1.png",
         "BATTLE ROUTE / STAGE PRESSURE", "Every menu click, breakdown, and impossible setpiece needed somebody to make it sing anyway.",
         {"JUNO STATIC", "RHEA AFTERGLOW", "KIKO PHASE", "MILO BACKBEAT"}},
        {"POSTER MODE", "UI, TYPE & PRESENTATION", "center", "", "", "",
         {"LINA HALATION", "ORION FADER", "PIPER COMET", "NOEL VECTOR"}},
        {"RECOVERY LOG", "QA, FIXES & LAST-MINUTE RESCUES", "split", "assets/vn/backgrounds/ch5/1.png",
         "LATE CHAPTER / SYSTEM STRESS", "The build survived because somebody kept reopening the project after it should have been called done.",
         {"SABLE HOTFIX", "TARA PATCHCORD", "RUNE NULLCHECK", "GIO ROLLBACK"}},
        {"FINAL APPROACH", "ROUTE MEMORY / FINALE", "split", "assets/vn/backgrounds/finale/1.png",
         "FINALE / AFTERGLOW", "Where the campus melodrama mutates into a last-breath spectacle and somehow still lands emotionally.",
         {"TIMOTHY PILLAI", "LYES AURORA", "ANMOL VARGHESE", "REVELL JOSE"}},
        {"CAMPUS CONSTELLATION", "BIT CAMPUS ALL-STARS", "center", "", "", "",
         {"JADEN QUASAR", "MILA CROSSFADE", "EZRA MIDNIGHT", "NIA AFTERIMAGE", "KAI STATIC", "VIOLET KEYCHANGE"}},
        {"CAMPUS ECHO", "SPECIAL THANKS", "center", "", "", "",
         {"THE BIT UNDERGROUND", "EVERYONE WHO STAYED UP TOO LATE", "EVERYONE WHO SAID THE JOKE WAS TOO MUCH", "EVERYONE WHO SAID MAKE IT WORSE"}},
    };
    return creditsData;
}

}  // namespace

bool CreditsDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    (void)state;
    document_ = &document;
    detachEventListeners(listeners_);
    creditsLoaded_ = game::credits::loadCreditsData(platform::path::resolvePath(kCreditsJsonRelativePath), creditsData_);
    if (!creditsLoaded_) {
        creditsData_ = fallbackCreditsData();
    }

    pendingReturn_ = false;
    scrollMetricsReady_ = false;
    rollFinished_ = false;
    speedupHeld_ = false;
    openingFinished_ = false;
    currentTop_ = 0.0f;
    startTop_ = 0.0f;
    endTop_ = 0.0f;
    viewportHeight_ = 0.0f;
    openingElapsed_ = 0.0f;
    layoutScale_ = resolveLayoutScale(document_);
    visualTracks_.clear();

    if (Rml::Element* element = document_->GetElementById("credits-return-prompt")) {
        element->SetInnerRML(escapeRmlText(creditsData_.returnPrompt));
    }

    attachListeners();
    populateContent();
    setChromeOpacity(0.0f);
    setTitleCardOpacity(0.0f);
    syncPrompt();
    return true;
}

void CreditsDocumentController::unbind() {
    detachEventListeners(listeners_);
    document_ = nullptr;
}

void CreditsDocumentController::sync(const AppState& state) {
    (void)state;
}

void CreditsDocumentController::update(const AppState& state, float deltaSeconds) {
    (void)state;
    if (document_ == nullptr) {
        return;
    }

    if (!scrollMetricsReady_) {
        initializeScrollMetrics();
    }

    updateOpening(deltaSeconds);
    if (openingFinished_) {
        updateScroll(deltaSeconds);
    }
    updatePresentation(deltaSeconds);
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

    if (event.keysym.sym == SDLK_SPACE && !rollFinished_) {
        speedupHeld_ = true;
        return;
    }

    if (rollFinished_) {
        requestReturn();
    }
}

void CreditsDocumentController::handleKeyUp(const SDL_KeyboardEvent& event) {
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

    const float speedMultiplier = speedupHeld_ ? kCreditsSpeedupMultiplier : 1.0f;
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
    openingElapsed_ += deltaSeconds;

    if (openingFinished_) {
        const float chromeFadeT = clamp01(
            (openingElapsed_ - (kOpeningBlackSeconds + kOpeningTitleFadeInSeconds + kOpeningTitleHoldSeconds)) /
            kOpeningChromeFadeInSeconds);
        setChromeOpacity(smoothstep01(chromeFadeT));
        return;
    }
    const float titleFadeT = clamp01((openingElapsed_ - kOpeningBlackSeconds) / kOpeningTitleFadeInSeconds);
    const float titleOpacity = smoothstep01(titleFadeT);
    setTitleCardOpacity(titleOpacity);
    setChromeOpacity(0.0f);

    if (openingElapsed_ >= kOpeningBlackSeconds + kOpeningTitleFadeInSeconds + kOpeningTitleHoldSeconds) {
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
        if (creditsData_.blocks[index].layout != "split") {
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
        if (creditsData_.blocks[index].layout != "split") {
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

void CreditsDocumentController::setScrollTop(float top) {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("credits-roll-content")) {
        element->SetProperty("top", formatPx(top));
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

}  // namespace graphics::frontui
