#include "front_ui_credits.h"

#include <algorithm>
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
constexpr float kCreditsScrollSpeedDpPerSecond = 36.0f;
constexpr float kCreditsStartPaddingDp = 140.0f;
constexpr float kCreditsEndPaddingDp = 94.0f;

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

std::string blockLayoutClass(const game::credits::CreditsBlock& block) {
    if (block.layout == "split") {
        return "credits-block credits-block-split";
    }
    return "credits-block credits-block-center";
}

std::string creditsMarkup(const game::credits::CreditsData& creditsData, const Rml::ElementDocument& document) {
    std::string markup;
    markup.reserve(8192);

    markup += "<div class='credits-roll-lead'>";
    markup += "<div class='credits-roll-kicker'>" + escapeRmlText(creditsData.kicker) + "</div>";
    markup += "<div class='credits-roll-title'>" + escapeRmlText(creditsData.title) + "</div>";
    if (!creditsData.subtitle.empty()) {
        markup += "<div class='credits-roll-subtitle'>" + escapeRmlText(creditsData.subtitle) + "</div>";
    }
    markup += "</div>";

    for (const auto& block : creditsData.blocks) {
        markup += "<div class='" + blockLayoutClass(block) + "'>";

        if (block.layout == "split") {
            markup += "<div class='credits-block-split-copy'>";
            if (!block.kicker.empty()) {
                markup += "<div class='credits-block-kicker'>" + escapeRmlText(block.kicker) + "</div>";
            }
            markup += "<div class='credits-section-heading'>" + escapeRmlText(block.heading) + "</div>";
            for (const std::string& entry : block.entries) {
                markup += "<div class='credits-entry'>" + escapeRmlText(entry) + "</div>";
            }
            markup += "</div>";

            markup += "<div class='credits-block-split-visual'>";
            const std::string resolvedImage = platform::path::resolvePath(block.image);
            if (!block.imageLabel.empty()) {
                markup += "<div class='credits-visual-label'>" + escapeRmlText(block.imageLabel) + "</div>";
            }
            if (!resolvedImage.empty() && std::filesystem::exists(resolvedImage)) {
                markup += "<img class='credits-visual-image' src='" +
                          platform::path::resolvePathForRml(block.image, document.GetSourceURL()) +
                          "'/>";
            } else {
                markup += "<div class='credits-visual-image credits-visual-fallback'></div>";
            }
            if (!block.imageCaption.empty()) {
                markup += "<div class='credits-visual-caption'>" + escapeRmlText(block.imageCaption) + "</div>";
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

    markup += "<div class='credits-group-block'>";
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
    currentTop_ = 0.0f;
    startTop_ = 0.0f;
    endTop_ = 0.0f;

    if (Rml::Element* element = document_->GetElementById("credits-logo")) {
        element->SetAttribute("src",
                              platform::path::resolvePathForRml(kMainMenuLogoRelativePath, document_->GetSourceURL()));
    }
    if (Rml::Element* element = document_->GetElementById("credits-return-prompt")) {
        element->SetInnerRML(escapeRmlText(creditsData_.returnPrompt));
    }

    attachListeners();
    populateContent();
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

    updateScroll(deltaSeconds);
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
    (void)event;
    if (rollFinished_) {
        requestReturn();
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

    currentTop_ = std::max(endTop_, currentTop_ - (kCreditsScrollSpeedDpPerSecond * deltaSeconds));
    setScrollTop(currentTop_);

    if (currentTop_ <= endTop_ + 0.01f) {
        currentTop_ = endTop_;
        setScrollTop(currentTop_);
        rollFinished_ = true;
        syncPrompt();
    }
}

void CreditsDocumentController::initializeScrollMetrics() {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Context* context = document_->GetContext()) {
        context->Update();
    }

    Rml::Element* viewport = document_->GetElementById("credits-roll-viewport");
    Rml::Element* content = document_->GetElementById("credits-roll-content");
    if (viewport == nullptr || content == nullptr) {
        return;
    }

    const float viewportHeight = viewport->GetClientHeight();
    const float contentHeight = content->GetOffsetHeight();
    if (viewportHeight <= 0.0f || contentHeight <= 0.0f) {
        return;
    }

    startTop_ = viewportHeight + kCreditsStartPaddingDp;
    endTop_ = std::min(kCreditsEndPaddingDp, viewportHeight - contentHeight - kCreditsEndPaddingDp);
    currentTop_ = startTop_;
    setScrollTop(currentTop_);
    scrollMetricsReady_ = true;
}

void CreditsDocumentController::setScrollTop(float top) {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* element = document_->GetElementById("credits-roll-content")) {
        element->SetProperty("top", formatDp(top));
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
