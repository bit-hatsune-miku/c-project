#define GL_GLEXT_PROTOTYPES

#include "menu_preview_session.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Event.h>

#include "RmlUi_Renderer_GL3.h"

#include "../platform/path_resolution.h"
#include "../window.h"

namespace graphics::preview {
namespace {

constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr float kDriftSmoothing = 12.0f;
constexpr float kHoldInitialDelaySeconds = 0.21f;
constexpr float kHoldRepeatIntervalSeconds = 0.105f;
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
constexpr const char* kDocumentPath = "assets/rmlui/previews/play_menu_preview.rml";
constexpr const char* kScrollSfxRelativePath = "assets/ui/sfx/UI_notification-done.wav";
constexpr const char* kConfirmSfxRelativePath = "assets/ui/sfx/Selection_roulette-result.wav";

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

int actionIndex(MainMenuAction action) {
    return static_cast<int>(action);
}

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
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << "dp";
    return stream.str();
}

std::string translateDp(float x, float y) {
    return "translate(" + formatDp(x) + ", " + formatDp(y) + ")";
}

std::string translateScale(float x, float y, float scale) {
    return "translate(" + formatDp(x) + ", " + formatDp(y) + ") scale(" + formatNumber(scale) + ")";
}

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

}  // namespace

PlayMenuPreviewSession::~PlayMenuPreviewSession() {
    shutdown();
}

bool PlayMenuPreviewSession::initialize(Window& window) {
    shutdown();

    windowHost_ = &window;
    window_ = window.getNativeWindow();
    glContext_ = window.getGlContext();
    if (window_ == nullptr || glContext_ == nullptr) {
        std::cerr << "[MenuPreview] Window is not in OpenGL mode.\n";
        return false;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "[MenuPreview] SDL subsystem init failed: " << SDL_GetError() << "\n";
        shutdown();
        return false;
    }
    audioInitialized_ = true;

    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(1);
    SDL_StopTextInput();

    Rml::String glInitMessage;
    if (!RmlGL3::Initialize(&glInitMessage)) {
        std::cerr << "[MenuPreview] RmlGL3 initialization failed: " << glInitMessage << "\n";
        shutdown();
        return false;
    }
    rmlGlInitialized_ = true;

    systemInterface_.SetWindow(window_);
    renderInterface_ = std::make_unique<RmlUiSdlGlRenderInterface>();
    if (!(*renderInterface_)) {
        std::cerr << "[MenuPreview] Failed to construct GL render interface.\n";
        shutdown();
        return false;
    }

    Rml::SetSystemInterface(&systemInterface_);
    Rml::SetRenderInterface(renderInterface_.get());
    if (!Rml::Initialise()) {
        std::cerr << "[MenuPreview] RmlUi core initialization failed.\n";
        shutdown();
        return false;
    }
    rmlInitialized_ = true;

    if (!loadFonts()) {
        std::cerr << "[MenuPreview] No usable fonts were loaded.\n";
        shutdown();
        return false;
    }

    updateViewportFromWindow();
    renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
    context_ = Rml::CreateContext("menu-preview", Rml::Vector2i(windowWidth_, windowHeight_));
    if (context_ == nullptr) {
        std::cerr << "[MenuPreview] Failed to create RmlUi context.\n";
        shutdown();
        return false;
    }
    applyContextScale();

    scrollSfxPath_ = platform::path::resolvePath(kScrollSfxRelativePath);
    confirmSfxPath_ = platform::path::resolvePath(kConfirmSfxRelativePath);

    selection_ = MainMenuAction::Start;
    driftCurrentX_ = driftOffsetX(selection_);
    driftCurrentY_ = driftOffsetY(selection_);
    driftTargetX_ = driftCurrentX_;
    driftTargetY_ = driftCurrentY_;
    introElapsedSeconds_ = 0.0f;
    introActive_ = false;

    if (!loadDocument()) {
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void PlayMenuPreviewSession::shutdown() {
    initialized_ = false;
    negativeHeld_ = false;
    positiveHeld_ = false;
    holdNegativeElapsed_ = 0.0f;
    holdPositiveElapsed_ = 0.0f;
    scrollSfxPath_.clear();
    confirmSfxPath_.clear();
    sfxPlayer_.shutdown();

    detachEventListeners();
    if (document_ != nullptr) {
        document_->Close();
        document_ = nullptr;
    }

    whiteoutElement_ = nullptr;
    discLayerElement_ = nullptr;
    logoLayerElement_ = nullptr;
    uiLayerElement_ = nullptr;
    introElapsedSeconds_ = 0.0f;
    introActive_ = false;

    if (context_ != nullptr) {
        context_->UnloadAllDocuments();
        Rml::RemoveContext("menu-preview");
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

    if (audioInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO | SDL_INIT_TIMER);
        audioInitialized_ = false;
    }

    glContext_ = nullptr;
    window_ = nullptr;
    windowHost_ = nullptr;
}

void PlayMenuPreviewSession::handleEvent(const SDL_Event& event) {
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

    if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
            case SDLK_LEFT:
            case SDLK_w:
            case SDLK_a:
                negativeHeld_ = true;
                holdNegativeElapsed_ = 0.0f;
                moveSelection(-1, true);
                break;

            case SDLK_DOWN:
            case SDLK_RIGHT:
            case SDLK_s:
            case SDLK_d:
                positiveHeld_ = true;
                holdPositiveElapsed_ = 0.0f;
                moveSelection(1, true);
                break;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                activateSelection();
                break;

            default:
                break;
        }
    }

    if (event.type == SDL_KEYUP) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
            case SDLK_LEFT:
            case SDLK_w:
            case SDLK_a:
                negativeHeld_ = false;
                holdNegativeElapsed_ = 0.0f;
                break;

            case SDLK_DOWN:
            case SDLK_RIGHT:
            case SDLK_s:
            case SDLK_d:
                positiveHeld_ = false;
                holdPositiveElapsed_ = 0.0f;
                break;

            default:
                break;
        }
    }
}

void PlayMenuPreviewSession::update(float deltaSeconds) {
    if (!initialized_ || context_ == nullptr) {
        return;
    }

    updateHeldInput(deltaSeconds);
    updateDrift(deltaSeconds);
    updateIntro(deltaSeconds);
    applyVisualState();
    sfxPlayer_.cleanupFinishedPlayback();
    context_->Update();
}

void PlayMenuPreviewSession::render() {
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

void PlayMenuPreviewSession::detachEventListeners() {
    for (EventListenerBinding& binding : listeners_) {
        if (binding.element != nullptr && binding.listener != nullptr) {
            binding.element->RemoveEventListener(binding.eventId, binding.listener.get(), binding.capturePhase);
        }
    }
    listeners_.clear();
}

bool PlayMenuPreviewSession::loadFonts() const {
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

void PlayMenuPreviewSession::updateViewportFromWindow() {
    if (windowHost_ == nullptr) {
        return;
    }

    windowWidth_ = std::max(1, windowHost_->getWindowWidth());
    windowHeight_ = std::max(1, windowHost_->getWindowHeight());
    drawableWidth_ = std::max(1, windowHost_->getDrawableWidth());
    drawableHeight_ = std::max(1, windowHost_->getDrawableHeight());
    glViewport(0, 0, drawableWidth_, drawableHeight_);
}

void PlayMenuPreviewSession::applyContextScale() {
    if (context_ == nullptr) {
        return;
    }

    const float widthScale = static_cast<float>(windowWidth_) / static_cast<float>(kReferenceWidth);
    const float heightScale = static_cast<float>(windowHeight_) / static_cast<float>(kReferenceHeight);
    const float scale = std::min(widthScale, heightScale);
    context_->SetDensityIndependentPixelRatio(std::max(scale, 0.01f));
}

bool PlayMenuPreviewSession::loadDocument() {
    if (context_ == nullptr) {
        return false;
    }

    const std::string documentPath = platform::path::resolvePath(kDocumentPath);
    document_ = context_->LoadDocument(documentPath);
    if (document_ == nullptr) {
        std::cerr << "[MenuPreview] Failed to load document: " << documentPath << "\n";
        return false;
    }

    document_->Show();
    cacheElements();
    applyButtonCopy();
    applySelection();
    attachListeners();
    restartIntroAnimation();
    applyVisualState();
    return true;
}

void PlayMenuPreviewSession::cacheElements() {
    if (document_ == nullptr) {
        return;
    }

    whiteoutElement_ = document_->GetElementById("menu-intro-whiteout");
    discLayerElement_ = document_->GetElementById("preview-disc-layer");
    logoLayerElement_ = document_->GetElementById("preview-logo-layer");
    uiLayerElement_ = document_->GetElementById("preview-ui-layer");
}

void PlayMenuPreviewSession::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    for (const MainMenuPresentation& definition : kMainMenuPresentation) {
        Rml::Element* element = document_->GetElementById(definition.buttonId);
        if (element == nullptr) {
            continue;
        }

        auto hoverListener = std::make_unique<CallbackEventListener>([this, action = definition.action](Rml::Event&) {
            setSelection(action, false);
        });
        element->AddEventListener(Rml::EventId::Mouseover, hoverListener.get());
        listeners_.push_back(EventListenerBinding{
            element,
            Rml::EventId::Mouseover,
            false,
            std::move(hoverListener),
        });

        auto clickListener = std::make_unique<CallbackEventListener>([this, action = definition.action](Rml::Event&) {
            setSelection(action, false);
            activateSelection();
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

void PlayMenuPreviewSession::applyButtonCopy() const {
    if (document_ == nullptr) {
        return;
    }

    for (const MainMenuPresentation& definition : kMainMenuPresentation) {
        if (Rml::Element* codeElement = document_->GetElementById(definition.codeId)) {
            codeElement->SetInnerRML(definition.focusCode);
        }
        if (Rml::Element* labelElement = document_->GetElementById(definition.labelId)) {
            labelElement->SetInnerRML(definition.displayLabel);
        }
    }
}

void PlayMenuPreviewSession::applySelection() {
    if (document_ == nullptr) {
        return;
    }

    for (const MainMenuPresentation& definition : kMainMenuPresentation) {
        if (Rml::Element* buttonElement = document_->GetElementById(definition.buttonId)) {
            buttonElement->SetClass("is-selected", definition.action == selection_);
        }
    }

    updateStatusCopy();
}

void PlayMenuPreviewSession::updateStatusCopy() const {
    if (document_ == nullptr) {
        return;
    }

    const MainMenuPresentation& definition = mainMenuPresentation(selection_);
    if (Rml::Element* codeElement = document_->GetElementById("menu-focus-code")) {
        codeElement->SetInnerRML(definition.focusCode);
    }
    if (Rml::Element* titleElement = document_->GetElementById("menu-status-title")) {
        titleElement->SetInnerRML(definition.statusTitle);
    }
    if (Rml::Element* copyElement = document_->GetElementById("menu-status-copy")) {
        copyElement->SetInnerRML(definition.statusBody);
    }
}

void PlayMenuPreviewSession::setSelection(MainMenuAction action, bool shouldPlayScrollSfx) {
    if (selection_ == action) {
        return;
    }

    selection_ = action;
    driftTargetX_ = driftOffsetX(selection_);
    driftTargetY_ = driftOffsetY(selection_);
    applySelection();
    if (shouldPlayScrollSfx) {
        playScrollSfx();
    }
}

void PlayMenuPreviewSession::moveSelection(int delta, bool shouldPlayScrollSfx) {
    if (delta == 0) {
        return;
    }

    const int buttonCount = static_cast<int>(kMainMenuPresentation.size());
    int nextIndex = (actionIndex(selection_) + delta) % buttonCount;
    if (nextIndex < 0) {
        nextIndex += buttonCount;
    }

    setSelection(static_cast<MainMenuAction>(nextIndex), shouldPlayScrollSfx);
}

void PlayMenuPreviewSession::activateSelection() {
    playConfirmSfx();
}

void PlayMenuPreviewSession::updateHeldInput(float deltaSeconds) {
    auto updateDirection = [deltaSeconds](bool held, float& elapsed) -> bool {
        if (!held) {
            elapsed = 0.0f;
            return false;
        }

        elapsed += deltaSeconds;
        if (elapsed < kHoldInitialDelaySeconds) {
            return false;
        }

        if (elapsed >= kHoldInitialDelaySeconds + kHoldRepeatIntervalSeconds) {
            elapsed -= kHoldRepeatIntervalSeconds;
            return true;
        }

        return false;
    };

    if (updateDirection(negativeHeld_, holdNegativeElapsed_)) {
        moveSelection(-1, true);
    }
    if (updateDirection(positiveHeld_, holdPositiveElapsed_)) {
        moveSelection(1, true);
    }
}

void PlayMenuPreviewSession::updateDrift(float deltaSeconds) {
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

void PlayMenuPreviewSession::restartIntroAnimation() {
    introElapsedSeconds_ = 0.0f;
    introActive_ = true;
}

void PlayMenuPreviewSession::updateIntro(float deltaSeconds) {
    if (!introActive_) {
        return;
    }

    introElapsedSeconds_ += std::max(deltaSeconds, 0.0f);
    if (introElapsedSeconds_ >= kIntroDurationSeconds) {
        introElapsedSeconds_ = kIntroDurationSeconds;
        introActive_ = false;
    }
}

void PlayMenuPreviewSession::applyVisualState() const {
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
        discLayerElement_->SetProperty("opacity", formatNumber(discOpacity));
        discLayerElement_->SetProperty(
            "transform",
            translateDp(
                driftCurrentX_ * kDiscDriftXMultiplier + discIntroX,
                driftCurrentY_ * kDiscDriftYMultiplier + discIntroY));
    }
    if (logoLayerElement_ != nullptr) {
        logoLayerElement_->SetProperty("opacity", formatNumber(logoOpacity));
        logoLayerElement_->SetProperty(
            "transform",
            translateScale(
                driftCurrentX_ * kLogoDriftXMultiplier,
                driftCurrentY_ * kLogoDriftYMultiplier + logoTranslateY,
                logoScale));
    }
    if (uiLayerElement_ != nullptr) {
        uiLayerElement_->SetProperty("opacity", formatNumber(uiOpacity));
        uiLayerElement_->SetProperty(
            "transform",
            translateDp(
                driftCurrentX_ * kUiDriftXMultiplier + uiIntroX,
                driftCurrentY_ * kUiDriftYMultiplier + uiIntroY));
    }
}

void PlayMenuPreviewSession::playScrollSfx() {
    if (scrollSfxPath_.empty()) {
        return;
    }
    (void)sfxPlayer_.playWavOneShot(scrollSfxPath_, 0.78f, true);
}

void PlayMenuPreviewSession::playConfirmSfx() {
    if (confirmSfxPath_.empty()) {
        return;
    }
    (void)sfxPlayer_.playWavOneShot(confirmSfxPath_, 0.92f, true);
}

}  // namespace graphics::preview
