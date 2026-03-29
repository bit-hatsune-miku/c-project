#include "front_ui_main_menu.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include "front_ui_load.h"
#include "front_ui_pause.h"
#include "front_ui_settings.h"
#include "front_ui_story.h"

namespace graphics::frontui {
namespace {

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

constexpr int actionIndex(MainMenuAction action) {
    return static_cast<int>(action);
}

constexpr float kDriftSmoothing = 12.0f;
constexpr float kDiscDriftXMultiplier = 0.36f;
constexpr float kDiscDriftYMultiplier = 0.48f;
constexpr float kUiDriftXMultiplier = 0.70f;
constexpr float kUiDriftYMultiplier = 0.68f;
constexpr float kLogoDriftXMultiplier = -0.08f;
constexpr float kLogoDriftYMultiplier = -0.12f;
constexpr std::array<float, 5> kDriftOffsetsX{{-16.0f, -8.0f, 0.0f, 8.0f, 16.0f}};
constexpr std::array<float, 5> kDriftOffsetsY{{-14.0f, -7.0f, 0.0f, 7.0f, 14.0f}};

constexpr float kIntroDurationSeconds = 2.80f;
constexpr float kWhiteFadeStartSeconds = 0.62f;
constexpr float kWhiteFadeEndSeconds = 1.38f;
constexpr float kLogoFadeStartSeconds = 0.86f;
constexpr float kLogoFadeEndSeconds = 1.62f;
constexpr float kLogoSlideStartSeconds = 1.70f;
constexpr float kLogoSlideEndSeconds = 2.48f;
constexpr float kDiscSlideStartSeconds = 1.96f;
constexpr float kDiscSlideEndSeconds = 2.64f;
constexpr float kUiSlideStartSeconds = 2.08f;
constexpr float kUiSlideEndSeconds = 2.74f;
constexpr float kLogoStartOffsetY = 250.0f;
constexpr float kDiscStartOffsetX = 136.0f;
constexpr float kDiscStartOffsetY = 116.0f;
constexpr float kUiStartOffsetX = -86.0f;
constexpr float kUiStartOffsetY = 148.0f;
constexpr float kLogoStartScale = 1.06f;
constexpr float kSubmenuSmoothing = 11.0f;
constexpr float kSubmenuUiOffsetX = -348.0f;
constexpr float kSubmenuUiOffsetY = 10.0f;
constexpr float kSubmenuFixedOffsetX = -214.0f;
constexpr float kSubmenuDiscOffsetX = 28.0f;
constexpr float kSubmenuDiscOffsetY = -4.0f;
constexpr float kSubmenuLogoOffsetX = 286.0f;
constexpr float kSubmenuLogoOffsetY = -22.0f;
constexpr float kSubmenuLogoScale = 0.88f;

float driftOffsetX(MainMenuAction action) {
    return kDriftOffsetsX[static_cast<std::size_t>(action)];
}

float driftOffsetY(MainMenuAction action) {
    return kDriftOffsetsY[static_cast<std::size_t>(action)];
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float rangeProgress(float value, float start, float end) {
    if (end <= start) {
        return value >= end ? 1.0f : 0.0f;
    }
    return clamp01((value - start) / (end - start));
}

float smoothstep01(float value) {
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float easeOutCubic(float value) {
    const float t = clamp01(value);
    const float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}

float lerp(float start, float end, float t) {
    return start + (end - start) * t;
}

std::string formatNumber(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::string formatDp(float value) {
    return formatNumber(value) + "dp";
}

std::string translateDp(float x, float y) {
    return "translate(" + formatDp(x) + ", " + formatDp(y) + ")";
}

std::string translateScale(float x, float y, float scale) {
    return "translate(" + formatDp(x) + ", " + formatDp(y) + ") scale(" + formatNumber(scale) + ")";
}

}  // namespace

bool MainMenuDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
    pendingCommand_.reset();
    selection_ = state.mainSelection;
    driftCurrentX_ = driftOffsetX(selection_);
    driftCurrentY_ = driftOffsetY(selection_);
    driftTargetX_ = driftCurrentX_;
    driftTargetY_ = driftCurrentY_;
    overlayMode_ = overlayModeForState(state);
    submenuCurrent_ = overlayMode_ == MainMenuOverlayMode::None ? 0.0f : 1.0f;
    submenuTarget_ = submenuCurrent_;
    introElapsedSeconds_ = 0.0f;
    introActive_ = false;

    cacheElements();
    applyButtonCopy();
    attachListeners();
    applySelectionStyles();
    updateStatusCopy();
    menuStackActive_ = isMenuStackActive(state);
    if (state.screen == ScreenState::MainMenu) {
        restartIntroAnimation();
    } else {
        applyVisualState();
    }
    return true;
}

void MainMenuDocumentController::unbind() {
    detachEventListeners(listeners_);
    whiteoutElement_ = nullptr;
    discLayerElement_ = nullptr;
    logoLayerElement_ = nullptr;
    uiLayerElement_ = nullptr;
    fixedLayerElement_ = nullptr;
    introActive_ = false;
    menuStackActive_ = false;
    overlayMode_ = MainMenuOverlayMode::None;
    document_ = nullptr;
}

void MainMenuDocumentController::sync(const AppState& state) {
    if (document_ == nullptr) {
        return;
    }

    if (selection_ != state.mainSelection) {
        setSelection(state.mainSelection);
    }

    overlayMode_ = overlayModeForState(state);
    submenuTarget_ = overlayMode_ == MainMenuOverlayMode::None ? 0.0f : 1.0f;
}

void MainMenuDocumentController::update(const AppState& state, float deltaSeconds) {
    if (document_ == nullptr) {
        return;
    }

    const bool activeNow = isMenuStackActive(state);
    if (activeNow && !menuStackActive_ && state.screen == ScreenState::MainMenu) {
        restartIntroAnimation();
    }
    menuStackActive_ = activeNow;

    if (!activeNow) {
        return;
    }

    updateDrift(deltaSeconds);
    updateIntro(deltaSeconds);
    updateSubmenu(deltaSeconds);
    applyVisualState();
}

bool MainMenuDocumentController::isMenuStackActive(const AppState& state) const {
    if (state.screen == ScreenState::MainMenu) {
        return true;
    }

    if (state.screen == ScreenState::Settings) {
        return state.settingsReturnScreen == ScreenState::MainMenu;
    }

    return (state.screen == ScreenState::LoadMenu || state.screen == ScreenState::LoadConfirmDelete) &&
           state.loadReturnScreen == ScreenState::MainMenu;
}

MainMenuOverlayMode MainMenuDocumentController::overlayModeForState(const AppState& state) const {
    if (state.screen == ScreenState::Settings && state.settingsReturnScreen == ScreenState::MainMenu) {
        return MainMenuOverlayMode::Settings;
    }

    if ((state.screen == ScreenState::LoadMenu || state.screen == ScreenState::LoadConfirmDelete) &&
        state.loadReturnScreen == ScreenState::MainMenu) {
        return MainMenuOverlayMode::Load;
    }

    return MainMenuOverlayMode::None;
}

void MainMenuDocumentController::moveSelection(int delta) {
    if (delta == 0) {
        return;
    }

    const int buttonCount = static_cast<int>(kMainMenuPresentation.size());
    int nextIndex = (actionIndex(selection_) + delta) % buttonCount;
    if (nextIndex < 0) {
        nextIndex += buttonCount;
    }

    setSelection(static_cast<MainMenuAction>(nextIndex));
}

void MainMenuDocumentController::activateSelection() {
    queueActivation(selection_);
}

void MainMenuDocumentController::applyState(AppState& state) {
    state.mainSelection = selection_;
}

std::optional<Command> MainMenuDocumentController::consumeCommand() {
    const std::optional<Command> result = pendingCommand_;
    pendingCommand_.reset();
    return result;
}

std::optional<MainMenuAction> MainMenuDocumentController::selectedMainMenuAction() const {
    return selection_;
}

void MainMenuDocumentController::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    for (const MainMenuPresentation& button : kMainMenuPresentation) {
        Rml::Element* element = document_->GetElementById(button.buttonId);
        if (element == nullptr) {
            continue;
        }

        auto hoverListener = std::make_unique<CallbackEventListener>([this, action = button.action](Rml::Event&) {
            setSelection(action);
        });
        element->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
        listeners_.push_back(EventListenerBinding{
            element,
            Rml::EventId::Mouseover,
            false,
            std::move(hoverListener),
        });

        auto clickListener = std::make_unique<CallbackEventListener>([this, action = button.action](Rml::Event&) {
            setSelection(action);
            queueActivation(action);
        });
        element->AddEventListener(Rml::EventId::Click, clickListener.get());
        listeners_.push_back(EventListenerBinding{
            element,
            Rml::EventId::Click,
            false,
            std::move(clickListener),
        });
    }
}

void MainMenuDocumentController::cacheElements() {
    if (document_ == nullptr) {
        return;
    }

    whiteoutElement_ = document_->GetElementById("menu-intro-whiteout");
    discLayerElement_ = document_->GetElementById("main-menu-disc-layer");
    logoLayerElement_ = document_->GetElementById("main-menu-logo-layer");
    uiLayerElement_ = document_->GetElementById("main-menu-ui-layer");
    fixedLayerElement_ = document_->GetElementById("main-menu-fixed-layer");
}

void MainMenuDocumentController::applyButtonCopy() const {
    if (document_ == nullptr) {
        return;
    }

    for (const MainMenuPresentation& button : kMainMenuPresentation) {
        if (Rml::Element* element = document_->GetElementById(button.codeId)) {
            element->SetInnerRML(button.focusCode);
        }
        if (Rml::Element* element = document_->GetElementById(button.labelId)) {
            element->SetInnerRML(button.displayLabel);
        }
    }
}

void MainMenuDocumentController::setSelection(MainMenuAction action) {
    selection_ = action;
    driftTargetX_ = driftOffsetX(selection_);
    driftTargetY_ = driftOffsetY(selection_);
    applySelectionStyles();
    updateStatusCopy();
}

void MainMenuDocumentController::queueActivation(MainMenuAction action) {
    pendingCommand_ = Command{CommandType::ActivateMainMenuAction, action};
}

void MainMenuDocumentController::applySelectionStyles() const {
    if (document_ == nullptr) {
        return;
    }

    for (const MainMenuPresentation& button : kMainMenuPresentation) {
        if (Rml::Element* element = document_->GetElementById(button.buttonId)) {
            element->SetClass("is-selected", button.action == selection_);
        }
    }
}

void MainMenuDocumentController::updateStatusCopy() const {
    if (document_ == nullptr) {
        return;
    }

    const MainMenuPresentation& definition = mainMenuPresentation(selection_);
    if (Rml::Element* element = document_->GetElementById("menu-focus-code")) {
        element->SetInnerRML(definition.focusCode);
    }
    if (Rml::Element* element = document_->GetElementById("menu-status-title")) {
        element->SetInnerRML(definition.statusTitle);
    }
    if (Rml::Element* element = document_->GetElementById("menu-status-copy")) {
        element->SetInnerRML(definition.statusBody);
    }
}

void MainMenuDocumentController::restartIntroAnimation() {
    introElapsedSeconds_ = 0.0f;
    introActive_ = true;
    applyVisualState();
}

void MainMenuDocumentController::updateDrift(float deltaSeconds) {
    const float blend = deltaSeconds > 0.0f
        ? std::clamp(1.0f - std::exp(-deltaSeconds * kDriftSmoothing), 0.0f, 1.0f)
        : 1.0f;

    driftCurrentX_ += (driftTargetX_ - driftCurrentX_) * blend;
    driftCurrentY_ += (driftTargetY_ - driftCurrentY_) * blend;

    if (std::fabs(driftCurrentX_ - driftTargetX_) < 0.02f) {
        driftCurrentX_ = driftTargetX_;
    }
    if (std::fabs(driftCurrentY_ - driftTargetY_) < 0.02f) {
        driftCurrentY_ = driftTargetY_;
    }
}

void MainMenuDocumentController::updateIntro(float deltaSeconds) {
    if (!introActive_) {
        return;
    }

    introElapsedSeconds_ += std::max(deltaSeconds, 0.0f);
    if (introElapsedSeconds_ >= kIntroDurationSeconds) {
        introElapsedSeconds_ = kIntroDurationSeconds;
        introActive_ = false;
    }
}

void MainMenuDocumentController::updateSubmenu(float deltaSeconds) {
    const float blend = deltaSeconds > 0.0f
        ? std::clamp(1.0f - std::exp(-deltaSeconds * kSubmenuSmoothing), 0.0f, 1.0f)
        : 1.0f;

    submenuCurrent_ += (submenuTarget_ - submenuCurrent_) * blend;
    if (std::fabs(submenuCurrent_ - submenuTarget_) < 0.002f) {
        submenuCurrent_ = submenuTarget_;
    }
}

void MainMenuDocumentController::applyVisualState() const {
    if (document_ == nullptr) {
        return;
    }

    float whiteoutOpacity = 0.0f;
    float logoOpacity = 1.0f;
    float logoTranslateY = 0.0f;
    float logoScale = 1.0f;
    float discOpacity = 1.0f;
    float discIntroX = 0.0f;
    float discIntroY = 0.0f;
    float uiOpacity = 1.0f;
    float uiIntroX = 0.0f;
    float uiIntroY = 0.0f;
    const float submenuDiscOffsetX = overlayMode_ == MainMenuOverlayMode::Settings ? 34.0f : kSubmenuDiscOffsetX;
    const float submenuDiscOffsetY = overlayMode_ == MainMenuOverlayMode::Settings ? -10.0f : kSubmenuDiscOffsetY;

    if (introActive_) {
        const float whiteFade = smoothstep01(rangeProgress(
            introElapsedSeconds_, kWhiteFadeStartSeconds, kWhiteFadeEndSeconds));
        const float logoFade = easeOutCubic(rangeProgress(
            introElapsedSeconds_, kLogoFadeStartSeconds, kLogoFadeEndSeconds));
        const float logoSlide = easeOutCubic(rangeProgress(
            introElapsedSeconds_, kLogoSlideStartSeconds, kLogoSlideEndSeconds));
        const float discSlide = easeOutCubic(rangeProgress(
            introElapsedSeconds_, kDiscSlideStartSeconds, kDiscSlideEndSeconds));
        const float uiSlide = easeOutCubic(rangeProgress(
            introElapsedSeconds_, kUiSlideStartSeconds, kUiSlideEndSeconds));

        whiteoutOpacity = 1.0f - whiteFade;
        logoOpacity = logoFade;
        logoTranslateY = lerp(kLogoStartOffsetY, 0.0f, logoSlide);
        logoScale = lerp(kLogoStartScale, 1.0f, logoSlide);
        discOpacity = discSlide;
        discIntroX = lerp(kDiscStartOffsetX, 0.0f, discSlide);
        discIntroY = lerp(kDiscStartOffsetY, 0.0f, discSlide);
        uiOpacity = uiSlide;
        uiIntroX = lerp(kUiStartOffsetX, 0.0f, uiSlide);
        uiIntroY = lerp(kUiStartOffsetY, 0.0f, uiSlide);
    }

    if (whiteoutElement_ != nullptr) {
        if (whiteoutOpacity > 0.001f) {
            whiteoutElement_->SetProperty("display", "block");
            whiteoutElement_->SetProperty("opacity", formatNumber(whiteoutOpacity));
        } else {
            whiteoutElement_->SetProperty("display", "none");
        }
    }

    if (discLayerElement_ != nullptr) {
        discLayerElement_->SetProperty("opacity", formatNumber(discOpacity * lerp(1.0f, 0.9f, submenuCurrent_)));
        discLayerElement_->SetProperty(
            "transform",
            translateDp(
                driftCurrentX_ * kDiscDriftXMultiplier + discIntroX + submenuDiscOffsetX * submenuCurrent_,
                driftCurrentY_ * kDiscDriftYMultiplier + discIntroY + submenuDiscOffsetY * submenuCurrent_));
    }

    if (logoLayerElement_ != nullptr) {
        logoLayerElement_->SetProperty("opacity", formatNumber(logoOpacity * (1.0f - submenuCurrent_)));
        logoLayerElement_->SetProperty(
            "transform",
            translateScale(
                driftCurrentX_ * kLogoDriftXMultiplier + kSubmenuLogoOffsetX * submenuCurrent_,
                driftCurrentY_ * kLogoDriftYMultiplier + logoTranslateY + kSubmenuLogoOffsetY * submenuCurrent_,
                lerp(logoScale, kSubmenuLogoScale, submenuCurrent_)));
    }

    if (uiLayerElement_ != nullptr) {
        uiLayerElement_->SetProperty("opacity", formatNumber(uiOpacity * lerp(1.0f, 0.26f, submenuCurrent_)));
        uiLayerElement_->SetProperty(
            "transform",
            translateDp(
                driftCurrentX_ * kUiDriftXMultiplier + uiIntroX + kSubmenuUiOffsetX * submenuCurrent_,
                driftCurrentY_ * kUiDriftYMultiplier + uiIntroY + kSubmenuUiOffsetY * submenuCurrent_));
    }

    if (fixedLayerElement_ != nullptr) {
        fixedLayerElement_->SetProperty("opacity", formatNumber(uiOpacity * (1.0f - submenuCurrent_)));
        fixedLayerElement_->SetProperty(
            "transform",
            translateDp(
                kSubmenuFixedOffsetX * submenuCurrent_,
                uiIntroY * 0.30f + 6.0f * submenuCurrent_));
    }
}

std::unique_ptr<DocumentController> createControllerForScreen(ScreenId screen) {
    switch (screen) {
        case ScreenId::MainMenu:
            return std::make_unique<MainMenuDocumentController>();
        case ScreenId::Story:
            return std::make_unique<StoryDocumentController>();
        case ScreenId::Settings:
            return std::make_unique<SettingsDocumentController>();
        case ScreenId::Pause:
            return std::make_unique<PauseDocumentController>();
        case ScreenId::Load:
            return std::make_unique<LoadDocumentController>();
    }

    return nullptr;
}

std::string resolveDocumentPath(ScreenId screen) {
    switch (screen) {
        case ScreenId::MainMenu:
            return "assets/rmlui/front_ui/main_menu.rml";
        case ScreenId::Story:
            return "assets/rmlui/front_ui/story.rml";
        case ScreenId::Settings:
            return "assets/rmlui/front_ui/settings.rml";
        case ScreenId::Pause:
            return "assets/rmlui/front_ui/pause.rml";
        case ScreenId::Load:
            return "assets/rmlui/front_ui/load.rml";
    }

    return std::string();
}

bool isScreenImplemented(ScreenId screen) {
    switch (screen) {
        case ScreenId::MainMenu:
        case ScreenId::Story:
        case ScreenId::Settings:
        case ScreenId::Pause:
        case ScreenId::Load:
            return true;
    }

    return false;
}

}  // namespace graphics::frontui
