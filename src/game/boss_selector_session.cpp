#define GL_GLEXT_PROTOTYPES

#include "boss_selector_session.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <nlohmann/json.hpp>

#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "audio/wav_one_shot.h"
#include "core/battle_loader.h"
#include "save/save.h"
#include "../graphics/rmlui_loading_overlay.h"
#include "../graphics/rmlui_sdl_gl_renderer.h"
#include "../graphics/ui_music_bars.h"
#include "../platform/path_resolution.h"
#include "../window.h"

namespace battle::selector {
namespace {

using json = nlohmann::json;

constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr float kHoldInitialDelaySeconds = 0.21f;
constexpr float kHoldRepeatIntervalSeconds = 0.105f;
constexpr float kSelectionLerpSpeed = 9.0f;
constexpr float kCarouselCardWidthDp = 164.0f;
constexpr float kCarouselCardHeightDp = 280.0f;
constexpr float kCarouselStepX = 118.0f;
constexpr float kCarouselBaseLeft = 58.0f;
constexpr float kCarouselBaseTop = 150.0f;
constexpr float kCarouselStairStep = 18.0f;
constexpr float kCarouselFocusedSlotIndex = 2.0f;
constexpr const char* kDocumentPath = "assets/rmlui/previews/battle_selector_preview.rml";
constexpr const char* kLoadingOverlayDocumentPath = "assets/rmlui/shared/loading_overlay.rml";
constexpr const char* kScrollSfxRelativePath = "assets/ui/sfx/Selection_roulette-3.wav";
constexpr const char* kConfirmSfxRelativePath = "assets/ui/sfx/SongSelect_confirm-selection.wav";
class CallbackEventListener final : public Rml::EventListener {
public:
    /**
         * @brief Constructs a CallbackEventListener that forwards processed events to the given callable.
         *
         * @param callback Callable invoked with the Rml::Event when ProcessEvent is called.
         */
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

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
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

std::string translateScale(float x, float y, float scale) {
    return "translate(" + formatDp(x) + ", " + formatDp(y) + ") scale(" + formatNumber(scale) + ")";
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

std::string resolveSelectorSpritePath(const std::string& assetName) {
    if (assetName.empty()) {
        return {};
    }

    for (const char* extension : {"png", "webp"}) {
        const std::string candidate =
            platform::path::resolvePath("assets/combat/sprites/" + assetName + "." + extension);
        if (std::filesystem::exists(candidate)) {
            return "../../combat/sprites/" + assetName + "." + extension;
        }
    }

    return {};
}

std::string battleTag(const battle::BattleDefinition& battle) {
    if (battle.type == "tutorial") {
        return "Tutorial";
    }
    return "Story Boss";
}

}  // namespace

class SessionImpl {
public:
    bool initialize(Window& window) {
        shutdown();

        windowHost_ = &window;
        window_ = window.getNativeWindow();
        glContext_ = window.getGlContext();
        if (window_ == nullptr || glContext_ == nullptr) {
            std::cerr << "[BossSelector] Window is not in OpenGL mode.\n";
            return false;
        }

        initializeAudio();

        SDL_GL_MakeCurrent(window_, glContext_);
        SDL_GL_SetSwapInterval(1);
        SDL_StopTextInput();

        Rml::String glInitMessage;
        if (!RmlGL3::Initialize(&glInitMessage)) {
            std::cerr << "[BossSelector] RmlGL3 initialization failed: " << glInitMessage << "\n";
            shutdown();
            return false;
        }
        rmlGlInitialized_ = true;

        systemInterface_.SetWindow(window_);
        renderInterface_ = std::make_unique<graphics::RmlUiSdlGlRenderInterface>();
        if (!(*renderInterface_)) {
            std::cerr << "[BossSelector] Failed to construct GL render interface.\n";
            shutdown();
            return false;
        }

        Rml::SetSystemInterface(&systemInterface_);
        Rml::SetRenderInterface(renderInterface_.get());
        if (!Rml::Initialise()) {
            std::cerr << "[BossSelector] RmlUi core initialization failed.\n";
            shutdown();
            return false;
        }
        rmlInitialized_ = true;

        if (!loadFonts()) {
            std::cerr << "[BossSelector] No usable fonts were loaded.\n";
            shutdown();
            return false;
        }

        updateViewportFromWindow();
        renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
        context_ = Rml::CreateContext("boss-selector", Rml::Vector2i(windowWidth_, windowHeight_));
        if (context_ == nullptr) {
            std::cerr << "[BossSelector] Failed to create RmlUi context.\n";
            shutdown();
            return false;
        }
        applyContextScale();

        if (!loadEntries()) {
            std::cerr << "[BossSelector] Failed to load battle entries.\n";
            shutdown();
            return false;
        }

        if (!loadDocument()) {
            shutdown();
            return false;
        }

        if (!loadingOverlay_.initialize(*context_, platform::path::resolvePath(kLoadingOverlayDocumentPath))) {
            std::cerr << "[BossSelector] Failed to load loading overlay document.\n";
            shutdown();
            return false;
        }
        loadingOverlayState_ = graphics::RmlUiLoadingOverlayState{};

        initialized_ = true;
        return true;
    }

    /**
     * @brief Cleanly shuts down the session and resets all runtime state.
     *
     * Stops audio playback and audio-related state, detaches UI event listeners,
     * closes and unloads the Rml document/context, shuts down Rml and RmlGL subsystems,
     * clears cached UI elements, entries, and music-bar visual state, shuts down the
     * loading overlay, and releases references to the GL context and window.
     */
    void shutdown() {
        initialized_ = false;
        finished_ = false;
        negativeHeld_ = false;
        positiveHeld_ = false;
        holdNegativeElapsed_ = 0.0f;
        holdPositiveElapsed_ = 0.0f;
        toastTimer_ = 0.0f;
        visualSelectionIndex_ = 0.0f;
        targetVisualSelectionIndex_ = 0.0f;
        focusZone_ = FocusZone::Carousel;
        footerSelection_ = FooterAction::ReplayStory;
        confirmVisible_ = false;
        confirmChoice_ = ConfirmChoice::Back;
        confirmTarget_.reset();
        lastShownConfirmTarget_.reset();
        pressedCardIndex_.reset();
        pressedConfirmCardIndex_.reset();
        pressedConfirmChoice_.reset();
        pressedFooterAction_.reset();
        pressedConfirmFooterAction_.reset();
        launchRequest_.reset();

        sfxPlayer_.shutdown();
        scrollSfxPath_.clear();
        confirmSfxPath_.clear();
        audioReady_ = false;

        detachEventListeners();

        if (document_ != nullptr) {
            document_->Close();
            document_ = nullptr;
        }

        trackElement_ = nullptr;
        rankValueElement_ = nullptr;
        infoPortraitImageElement_ = nullptr;
        infoNameElement_ = nullptr;
        infoBattleElement_ = nullptr;
        infoCopyElement_ = nullptr;
        infoHpElement_ = nullptr;
        infoAtkElement_ = nullptr;
        infoSpdElement_ = nullptr;
        infoHintElement_ = nullptr;
        replayStoryButtonElement_ = nullptr;
        straightToBattleButtonElement_ = nullptr;
        toastElement_ = nullptr;
        confirmOverlayElement_ = nullptr;
        confirmTitleElement_ = nullptr;
        confirmBodyElement_ = nullptr;
        confirmBackElement_ = nullptr;
        confirmProceedElement_ = nullptr;
        cardElements_.clear();
        cardPortraitImageElements_.clear();
        entries_.clear();
        uiMusicBars_ = graphics::UiMusicBarStrip{};
        uiMusicVisualState_ = game::audio::UiMusicVisualState{};
        loadingOverlay_.shutdown();
        loadingOverlayState_ = graphics::RmlUiLoadingOverlayState{};

        if (context_ != nullptr) {
            context_->UnloadAllDocuments();
            Rml::RemoveContext("boss-selector");
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

    /**
     * @brief Handle an SDL input event and update selector UI state accordingly.
     *
     * Processes window resize, keyboard, and mouse events to update focus, selection,
     * confirmation overlay, and pressed-state tracking; delegates raw input to RmlSDL.
     *
     * @param event The SDL event to handle.
     */
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
            applySelection();
        }

        SDL_Event mutableEvent = event;
        RmlSDL::InputEventHandler(context_, window_, mutableEvent);

        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (confirmVisible_) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                    case SDLK_BACKSPACE:
                        closeConfirm();
                        break;

                    case SDLK_LEFT:
                    case SDLK_UP:
                        setConfirmChoice(ConfirmChoice::Back, true);
                        break;

                    case SDLK_RIGHT:
                    case SDLK_DOWN:
                        setConfirmChoice(ConfirmChoice::Proceed, true);
                        break;

                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                    case SDLK_SPACE:
                        activateConfirmSelection();
                        break;

                    default:
                        break;
                }
                return;
            }

            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    finished_ = true;
                    break;

                case SDLK_LEFT:
                    if (focusZone_ == FocusZone::Carousel) {
                        negativeHeld_ = true;
                        holdNegativeElapsed_ = 0.0f;
                        moveSelection(-1, true);
                    } else {
                        moveFooterSelection(-1, true);
                    }
                    break;

                case SDLK_RIGHT:
                    if (focusZone_ == FocusZone::Carousel) {
                        positiveHeld_ = true;
                        holdPositiveElapsed_ = 0.0f;
                        moveSelection(1, true);
                    } else {
                        moveFooterSelection(1, true);
                    }
                    break;

                case SDLK_UP:
                    setFocusZone(FocusZone::Carousel, true);
                    break;

                case SDLK_DOWN:
                    if (focusZone_ == FocusZone::Carousel) {
                        setFocusZone(FocusZone::Footer, true);
                    }
                    break;

                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:
                    openConfirmForSelected();
                    break;

                default:
                    break;
            }
        }

        if (event.type == SDL_MOUSEMOTION) {
            const float mouseX = static_cast<float>(event.motion.x);
            const float mouseY = static_cast<float>(event.motion.y);

            if (confirmVisible_) {
                if (hitTestElement(mouseX, mouseY, confirmBackElement_)) {
                    setConfirmChoice(ConfirmChoice::Back, true);
                } else if (hitTestElement(mouseX, mouseY, confirmProceedElement_)) {
                    setConfirmChoice(ConfirmChoice::Proceed, true);
                }
            } else if (const std::optional<std::size_t> hoveredCard = hitTestCard(mouseX, mouseY);
                       hoveredCard.has_value()) {
                focusZone_ = FocusZone::Carousel;
                setSelectedIndex(*hoveredCard, true);
            } else if (const std::optional<FooterAction> hoveredAction = hitTestFooterAction(mouseX, mouseY);
                       hoveredAction.has_value()) {
                selectFooterAction(*hoveredAction, true);
            }
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
            const float mouseX = static_cast<float>(event.button.x);
            const float mouseY = static_cast<float>(event.button.y);

            if (!confirmVisible_) {
                if (const std::optional<std::size_t> hoveredCard = hitTestCard(mouseX, mouseY);
                    hoveredCard.has_value()) {
                    pressedConfirmCardIndex_ = hoveredCard;
                    focusZone_ = FocusZone::Carousel;
                    setSelectedIndex(*hoveredCard, true);
                } else if (const std::optional<FooterAction> hoveredAction = hitTestFooterAction(mouseX, mouseY);
                           hoveredAction.has_value()) {
                    pressedConfirmFooterAction_ = hoveredAction;
                    selectFooterAction(*hoveredAction, true);
                }
            }
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            const float mouseX = static_cast<float>(event.button.x);
            const float mouseY = static_cast<float>(event.button.y);
            pressedCardIndex_.reset();
            pressedConfirmCardIndex_.reset();
            pressedConfirmChoice_.reset();
            pressedFooterAction_.reset();
            pressedConfirmFooterAction_.reset();

            if (confirmVisible_) {
                if (hitTestElement(mouseX, mouseY, confirmBackElement_)) {
                    pressedConfirmChoice_ = ConfirmChoice::Back;
                    setConfirmChoice(ConfirmChoice::Back, true);
                } else if (hitTestElement(mouseX, mouseY, confirmProceedElement_)) {
                    pressedConfirmChoice_ = ConfirmChoice::Proceed;
                    setConfirmChoice(ConfirmChoice::Proceed, true);
                }
            } else if (const std::optional<std::size_t> hoveredCard = hitTestCard(mouseX, mouseY);
                       hoveredCard.has_value()) {
                pressedCardIndex_ = hoveredCard;
                focusZone_ = FocusZone::Carousel;
                setSelectedIndex(*hoveredCard, false);
                return;
            } else if (const std::optional<FooterAction> hoveredAction = hitTestFooterAction(mouseX, mouseY);
                       hoveredAction.has_value()) {
                pressedFooterAction_ = hoveredAction;
                selectFooterAction(*hoveredAction, false);
                return;
            }
        }

        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            const float mouseX = static_cast<float>(event.button.x);
            const float mouseY = static_cast<float>(event.button.y);

            if (confirmVisible_) {
                std::optional<ConfirmChoice> releasedChoice;
                if (hitTestElement(mouseX, mouseY, confirmBackElement_)) {
                    releasedChoice = ConfirmChoice::Back;
                } else if (hitTestElement(mouseX, mouseY, confirmProceedElement_)) {
                    releasedChoice = ConfirmChoice::Proceed;
                }

                if (pressedConfirmChoice_.has_value() && releasedChoice == pressedConfirmChoice_) {
                    setConfirmChoice(*releasedChoice, false);
                    activateConfirmSelection();
                }

                pressedConfirmChoice_.reset();
                pressedCardIndex_.reset();
                pressedConfirmCardIndex_.reset();
                pressedFooterAction_.reset();
                pressedConfirmFooterAction_.reset();
                return;
            }

            const std::optional<std::size_t> releasedCard = hitTestCard(mouseX, mouseY);
            const std::optional<FooterAction> releasedAction = hitTestFooterAction(mouseX, mouseY);
            if (pressedCardIndex_.has_value() && releasedCard == pressedCardIndex_) {
                focusZone_ = FocusZone::Carousel;
                setSelectedIndex(*releasedCard, true);
            } else if (pressedFooterAction_.has_value() && releasedAction == pressedFooterAction_) {
                selectFooterAction(*releasedAction, false);
                openConfirmForSelected();
            }

            pressedCardIndex_.reset();
            pressedConfirmCardIndex_.reset();
            pressedConfirmChoice_.reset();
            pressedFooterAction_.reset();
            pressedConfirmFooterAction_.reset();
        }

        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
            const float mouseX = static_cast<float>(event.button.x);
            const float mouseY = static_cast<float>(event.button.y);

            if (confirmVisible_) {
                return;
            }

            const std::optional<std::size_t> releasedCard = hitTestCard(mouseX, mouseY);
            const std::optional<FooterAction> releasedAction = hitTestFooterAction(mouseX, mouseY);
            if (pressedConfirmCardIndex_.has_value() && releasedCard == pressedConfirmCardIndex_) {
                focusZone_ = FocusZone::Carousel;
                setSelectedIndex(*releasedCard, false);
                openConfirmForEntry(*releasedCard);
            } else if (pressedConfirmFooterAction_.has_value() && releasedAction == pressedConfirmFooterAction_) {
                selectFooterAction(*releasedAction, false);
                openConfirmForSelected();
            }

            pressedConfirmCardIndex_.reset();
            pressedConfirmFooterAction_.reset();
        }

        if (event.type == SDL_KEYUP) {
            switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    negativeHeld_ = false;
                    holdNegativeElapsed_ = 0.0f;
                    break;

                case SDLK_RIGHT:
                    positiveHeld_ = false;
                    holdPositiveElapsed_ = 0.0f;
                    break;

                default:
                    break;
            }
        }
    }

    void update(float deltaSeconds) {
        if (!initialized_ || finished_ || context_ == nullptr) {
            return;
        }

        updateHeldInput(deltaSeconds);
        updateVisualSelection(deltaSeconds);
        updateToast(deltaSeconds);
        sfxPlayer_.cleanupFinishedPlayback();
        context_->Update();
    }

    void render() {
        if (!initialized_ || finished_ || context_ == nullptr || renderInterface_ == nullptr || window_ == nullptr) {
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

    /**
     * @brief Updates the loading overlay state and applies it to the UI.
     *
     * Applies the provided loading overlay state (visibility, progress, and message)
     * to the internal loading overlay instance so the UI reflects the new state.
     *
     * @param state The loading overlay state to apply.
     */
    void setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) {
        loadingOverlayState_ = state;
        loadingOverlay_.apply(loadingOverlayState_);
    }

    /**
     * @brief Update the stored UI music visual state and apply it to the cached music bar strip.
     *
     * @param state New visual state to use for the UI music bars.
     */
    void setUiMusicVisualState(const game::audio::UiMusicVisualState& state) {
        uiMusicVisualState_ = state;
        graphics::applyUiMusicBarStrip(uiMusicBars_, uiMusicVisualState_);
    }

    /**
     * @brief Indicates whether the session has finished.
     *
     * @return `true` if the session has finished, `false` otherwise.
     */
    bool isFinished() const {
        return finished_;
    }

    std::optional<LaunchRequest> consumeLaunchRequest() {
        std::optional<LaunchRequest> request = launchRequest_;
        launchRequest_.reset();
        return request;
    }

private:
    enum class FocusZone {
        Carousel,
        Footer,
    };

    enum class FooterAction {
        ReplayStory,
        StraightToBattle,
    };

    enum class ConfirmChoice {
        Back,
        Proceed,
    };

    enum class ConfirmTargetType {
        Entry,
    };

    struct ConfirmTarget {
        ConfirmTargetType type = ConfirmTargetType::Entry;
        std::size_t entryIndex = 0;
    };

    struct EventListenerBinding {
        Rml::Element* element = nullptr;
        Rml::EventId eventId = Rml::EventId::Invalid;
        bool capturePhase = false;
        std::unique_ptr<Rml::EventListener> listener;
    };

    struct Entry {
        battle::BattleDefinition battle;
        battle::BossDefinition boss;
        std::string spritePath;
        std::string tag;
        std::string instructionHint;
        bool defeated = false;
    };

    void detachEventListeners() {
        for (EventListenerBinding& binding : listeners_) {
            if (binding.element != nullptr && binding.listener != nullptr) {
                binding.element->RemoveEventListener(binding.eventId, binding.listener.get(), binding.capturePhase);
            }
        }
        listeners_.clear();
    }

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
                std::cerr << "[BossSelector] Audio init failed: " << SDL_GetError() << "\n";
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
     * @brief Loads selectable boss entries from battle definitions and prepares session selection state.
     *
     * Populates the session's entries list with battles that are marked visible to the selector and
     * have been cleared in the current saved progression. For each entry the corresponding boss
     * definition and an optional sprite path are resolved; when boss metadata is available the
     * entry's instruction hint may be overridden from that metadata. The function also resets
     * selection indices and sets focus to the carousel.
     *
     * @return true if at least one entry was loaded into the selector; false if no entries were
     *         added or if battle definitions could not be loaded.
     */
    bool loadEntries() {
        entries_.clear();
        progression_ = save::loadCurrentProgression();

        std::vector<battle::BattleDefinition> battles;
        if (!battle::loader::loadAllBattleDefinitions(battles)) {
            return false;
        }

        json bossRoot;
        const std::string bossPath = battle::loader::resolveAssetPath("assets/combat/boss.json");
        const bool loadedBossMeta = battle::loader::readJsonRoot(bossPath, bossRoot, "boss");

        for (const battle::BattleDefinition& battle : battles) {
            if (!battle.selectorVisible || !::battle::hasClearedBattle(progression_, battle.key)) {
                continue;
            }

            battle::BossDefinition boss;
            if (!battle::loader::loadBossDefinition(battle.bossKey, boss)) {
                continue;
            }

            Entry entry;
            entry.battle = battle;
            entry.boss = boss;
            entry.spritePath = resolveSelectorSpritePath(boss.assets);
            entry.tag = battleTag(battle);
            entry.defeated = true;
            entry.instructionHint = "Preview interaction hint not available yet.";

            if (loadedBossMeta && bossRoot.is_object()) {
                const auto bossIt = bossRoot.find(battle.bossKey);
                if (bossIt != bossRoot.end() && bossIt->is_object()) {
                    const json& bossJson = *bossIt;
                    if (bossJson.contains("abilities") && bossJson.at("abilities").is_object()) {
                    const json& abilitiesJson = bossJson.at("abilities");
                    const auto skillIt = abilitiesJson.find("skill");
                    if (skillIt != abilitiesJson.end() && skillIt->is_object()) {
                        const std::string description = skillIt->value("description", std::string{});
                        if (!description.empty()) {
                            entry.instructionHint = description;
                        } else {
                            entry.instructionHint =
                                skillIt->value("instructionHint", entry.instructionHint);
                        }
                    }
                }
            }
            }

            entries_.push_back(std::move(entry));
        }

        selectedIndex_ = 0;
        visualSelectionIndex_ = 0.0f;
        targetVisualSelectionIndex_ = 0.0f;
        focusZone_ = FocusZone::Carousel;
        return !entries_.empty();
    }

    /**
     * @brief Loads and initializes the boss selector RML document and prepares the UI.
     *
     * Loads the RML document for the boss selector, shows it, caches the UI music bar strip
     * and applies the current music visual state, then caches element references, builds
     * the carousel track, attaches event listeners, and applies the current selection state.
     *
     * @return true on success; false if the Rml context is missing or the document failed to load.
     */
    bool loadDocument() {
        if (context_ == nullptr) {
            return false;
        }

        const std::string documentPath = platform::path::resolvePath(kDocumentPath);
        document_ = context_->LoadDocument(documentPath);
        if (document_ == nullptr) {
            std::cerr << "[BossSelector] Failed to load document: " << documentPath << "\n";
            return false;
        }

        document_->Show();
        uiMusicBars_ = graphics::cacheUiMusicBarStrip(*document_);
        graphics::applyUiMusicBarStrip(uiMusicBars_, uiMusicVisualState_);
        cacheElements();
        buildTrack();
        attachListeners();
        applySelection();
        return true;
    }

    /**
     * @brief Cache frequently accessed UI elements from the loaded Rml document.
     *
     * Queries the active document for elements with fixed IDs and stores pointers
     * to them for later UI updates. If no document is loaded, the function
     * returns without modifying cached pointers.
     *
     * Cached elements include: boss track, idol rank value, info panel portrait,
     * name, battle, copy text, HP/ATK/SPD values, instruction hint, footer action
     * buttons (replay/straight-to-battle), toast, and confirm overlay/title/body
     * and its Back/Proceed controls.
     */
    void cacheElements() {
        if (document_ == nullptr) {
            return;
        }

        trackElement_ = document_->GetElementById("boss-track");
        rankValueElement_ = document_->GetElementById("idol-rank-value");
        infoPortraitImageElement_ = document_->GetElementById("info-portrait-image");
        infoNameElement_ = document_->GetElementById("info-name");
        infoBattleElement_ = document_->GetElementById("info-battle");
        infoCopyElement_ = document_->GetElementById("info-copy");
        infoHpElement_ = document_->GetElementById("info-hp");
        infoAtkElement_ = document_->GetElementById("info-atk");
        infoSpdElement_ = document_->GetElementById("info-spd");
        infoHintElement_ = document_->GetElementById("info-hint");
        replayStoryButtonElement_ = document_->GetElementById("replay-story-button");
        straightToBattleButtonElement_ = document_->GetElementById("straight-to-battle-button");
        toastElement_ = document_->GetElementById("selector-toast");
        confirmOverlayElement_ = document_->GetElementById("selector-confirm");
        confirmTitleElement_ = document_->GetElementById("selector-confirm-title");
        confirmBodyElement_ = document_->GetElementById("selector-confirm-body");
        confirmBackElement_ = document_->GetElementById("selector-confirm-back");
        confirmProceedElement_ = document_->GetElementById("selector-confirm-proceed");
    }

    /**
     * @brief Builds the carousel markup for all entries and caches card elements.
     *
     * Constructs the inner RML for the boss track from the current `entries_`,
     * injects it into `trackElement_`, then collects and caches the button
     * elements and their portrait image elements. If an entry includes a
     * `spritePath`, that path is applied to the corresponding portrait image's
     * `src` attribute. If `trackElement_` is null, the function does nothing.
     */
    void buildTrack() {
        if (trackElement_ == nullptr) {
            return;
        }

        std::ostringstream markup;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const Entry& entry = entries_[i];
            markup << "<button class=\"boss-card\" id=\"boss-card-" << i << "\">"
                   << "<div class=\"boss-card-frame\">"
                   << "<div class=\"boss-card-topline\">"
                   << "<div class=\"boss-card-code\">" << escapeRml((i + 1 < 10 ? "0" : "") + std::to_string(i + 1)) << "</div>"
                   << "<div class=\"boss-card-state is-cleared\">Cleared</div>"
                   << "</div>"
                   << "<div class=\"boss-card-tag\">" << escapeRml(entry.tag) << "</div>"
                   << "<div class=\"boss-card-portrait\">"
                   << "<img class=\"boss-card-portrait-image\" id=\"boss-card-portrait-image-" << i << "\"/>"
                   << "</div>"
                   << "<div class=\"boss-card-copy\">"
                   << "<div class=\"boss-card-name\">" << escapeRml(entry.boss.title) << "</div>"
                   << "<div class=\"boss-card-battle\">" << escapeRml(entry.battle.name) << "</div>"
                   << "</div>"
                   << "</div>"
                   << "</button>";
        }

        trackElement_->SetInnerRML(markup.str());
        cardElements_.clear();
        cardPortraitImageElements_.clear();
        cardElements_.reserve(entries_.size());
        cardPortraitImageElements_.reserve(entries_.size());

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            cardElements_.push_back(document_->GetElementById("boss-card-" + std::to_string(i)));
            cardPortraitImageElements_.push_back(
                document_->GetElementById("boss-card-portrait-image-" + std::to_string(i)));

            if (cardPortraitImageElements_.back() != nullptr && !entries_[i].spritePath.empty()) {
                cardPortraitImageElements_.back()->SetAttribute("src", entries_[i].spritePath);
            }
        }
    }

    /**
     * @brief Attach Rml event listeners for the selector document.
     *
     * If no document is loaded, this function does nothing. When a document is present,
     * it is intended to register event handlers required by the UI; currently no handlers
     * are registered (no-op).
     */
    void attachListeners() {
        if (document_ == nullptr) {
            return;
        }
    }

    /**
     * @brief Update the UI to match the session's current selection and focus state.
     *
     * Applies the animated carousel layout and synchronizes visible UI elements with the
     * session's selection, focus zone, footer selection, confirmation visibility/choice,
     * selected card highlighting, displayed idol rank, and info panel contents.
     */
    void applySelection() {
        if (entries_.empty()) {
            return;
        }

        updateAnimatedLayout();

        if (replayStoryButtonElement_ != nullptr) {
            replayStoryButtonElement_->SetClass(
                "is-focused",
                focusZone_ == FocusZone::Footer && footerSelection_ == FooterAction::ReplayStory);
        }
        if (straightToBattleButtonElement_ != nullptr) {
            straightToBattleButtonElement_->SetClass(
                "is-focused",
                focusZone_ == FocusZone::Footer && footerSelection_ == FooterAction::StraightToBattle);
        }

        if (confirmOverlayElement_ != nullptr) {
            confirmOverlayElement_->SetClass("is-visible", confirmVisible_);
        }
        if (confirmBackElement_ != nullptr) {
            confirmBackElement_->SetClass("is-selected", confirmChoice_ == ConfirmChoice::Back);
        }
        if (confirmProceedElement_ != nullptr) {
            confirmProceedElement_->SetClass("is-selected", confirmChoice_ == ConfirmChoice::Proceed);
        }
        if (confirmTitleElement_ != nullptr) {
            confirmTitleElement_->SetInnerRML(confirmTitleRml());
        }
        if (confirmBodyElement_ != nullptr) {
            confirmBodyElement_->SetInnerRML(confirmBodyRml());
        }

        for (std::size_t i = 0; i < cardElements_.size(); ++i) {
            if (cardElements_[i] != nullptr) {
                cardElements_[i]->SetClass("is-selected", i == selectedIndex_ && focusZone_ == FocusZone::Carousel);
            }
        }

        const int idolRank = static_cast<int>(entries_.size());
        if (rankValueElement_ != nullptr) {
            rankValueElement_->SetInnerRML(std::to_string(idolRank));
        }

        updateInfoPanel();
    }

    void updateAnimatedLayout() {
        if (entries_.empty()) {
            return;
        }

        for (std::size_t i = 0; i < cardElements_.size(); ++i) {
            Rml::Element* element = cardElements_[i];
            if (element == nullptr) {
                continue;
            }

            float offset = static_cast<float>(i) - visualSelectionIndex_;
            const float count = static_cast<float>(entries_.size());
            const float half = count * 0.5f;
            while (offset > half) {
                offset -= count;
            }
            while (offset < -half) {
                offset += count;
            }

            const float slot = offset + kCarouselFocusedSlotIndex;
            const float x = kCarouselBaseLeft + slot * kCarouselStepX;
            const float absOffset = std::fabs(offset);
            const float y = kCarouselBaseTop - offset * kCarouselStairStep + absOffset * 6.0f;
            const float scale = absOffset < 0.05f ? 1.08f : std::max(0.88f, 1.0f - absOffset * 0.04f);
            const float opacity = absOffset < 0.05f ? 1.0f : std::max(0.50f, 0.92f - absOffset * 0.10f);

            element->SetProperty("width", formatDp(kCarouselCardWidthDp));
            element->SetProperty("height", formatDp(kCarouselCardHeightDp));
            element->SetProperty("transform", translateScale(x, y, scale));
            element->SetProperty("opacity", formatNumber(opacity));
            element->SetProperty("z-index", std::to_string(static_cast<int>(100.0f - absOffset * 10.0f)));
        }
    }

    void updateInfoPanel() const {
        if (entries_.empty()) {
            return;
        }

        const Entry& entry = entries_[selectedIndex_];
        if (infoPortraitImageElement_ != nullptr) {
            if (!entry.spritePath.empty()) {
                infoPortraitImageElement_->SetAttribute("src", entry.spritePath);
            } else {
                infoPortraitImageElement_->RemoveAttribute("src");
            }
        }
        if (infoNameElement_ != nullptr) {
            infoNameElement_->SetInnerRML(escapeRml(entry.boss.title));
        }
        if (infoBattleElement_ != nullptr) {
            infoBattleElement_->SetInnerRML(escapeRml(entry.battle.name));
        }
        if (infoCopyElement_ != nullptr) {
            const std::string copy = entry.battle.description.empty()
                ? "Challenge " + entry.boss.title + "."
                : entry.battle.description;
            infoCopyElement_->SetInnerRML(escapeRml(copy));
        }
        if (infoHpElement_ != nullptr) {
            infoHpElement_->SetInnerRML(std::to_string(entry.boss.hp));
        }
        if (infoAtkElement_ != nullptr) {
            infoAtkElement_->SetInnerRML(std::to_string(entry.boss.atk));
        }
        if (infoSpdElement_ != nullptr) {
            infoSpdElement_->SetInnerRML(std::to_string(entry.boss.spd));
        }
        if (infoHintElement_ != nullptr) {
            infoHintElement_->SetInnerRML(escapeRml(entry.instructionHint));
        }
    }

    void setSelectedIndex(std::size_t index, bool shouldPlayScrollSfx) {
        if (entries_.empty()) {
            return;
        }

        index %= entries_.size();
        if (index == selectedIndex_) {
            return;
        }

        const int count = static_cast<int>(entries_.size());
        const int current = static_cast<int>(selectedIndex_);
        const int destination = static_cast<int>(index);
        const int forwardDistance = (destination - current + count) % count;
        const int backwardDistance = (current - destination + count) % count;
        targetVisualSelectionIndex_ += forwardDistance <= backwardDistance ? 1.0f * static_cast<float>(forwardDistance)
                                                                           : -1.0f * static_cast<float>(backwardDistance);

        selectedIndex_ = index;
        applySelection();
        if (shouldPlayScrollSfx) {
            playScrollSfx();
        }
    }

    void moveSelection(int delta, bool shouldPlayScrollSfx) {
        if (entries_.empty() || delta == 0) {
            return;
        }

        const int count = static_cast<int>(entries_.size());
        int nextIndex = (static_cast<int>(selectedIndex_) + delta) % count;
        if (nextIndex < 0) {
            nextIndex += count;
        }

        selectedIndex_ = static_cast<std::size_t>(nextIndex);
        targetVisualSelectionIndex_ += delta > 0 ? 1.0f : -1.0f;
        applySelection();
        if (shouldPlayScrollSfx) {
            playScrollSfx();
        }
    }

    /**
     * @brief Change the current UI focus zone and update the selection layout.
     *
     * Updates the session's focus zone to the given value and reapplies selection/layout.
     * Optionally plays the scroll sound effect when the focus change should be audible.
     *
     * @param zone New focus zone to set (e.g., carousel or footer).
     * @param shouldPlayScrollSfx If `true`, play the scroll sound effect after changing focus.
     */
    void setFocusZone(FocusZone zone, bool shouldPlayScrollSfx) {
        if (focusZone_ == zone) {
            return;
        }

        focusZone_ = zone;
        applySelection();
        if (shouldPlayScrollSfx) {
            playScrollSfx();
        }
    }

    /**
     * @brief Selects a footer action and moves focus to the footer.
     *
     * Updates the current footer selection and focus zone, reapplies UI selection layout,
     * and optionally plays the scroll sound effect.
     *
     * @param action The footer action to select.
     * @param shouldPlayScrollSfx If `true`, play the scroll SFX after applying the selection.
     */
    void selectFooterAction(FooterAction action, bool shouldPlayScrollSfx) {
        if (focusZone_ == FocusZone::Footer && footerSelection_ == action) {
            return;
        }

        footerSelection_ = action;
        focusZone_ = FocusZone::Footer;
        applySelection();
        if (shouldPlayScrollSfx) {
            playScrollSfx();
        }
    }

    /**
     * @brief Moves the footer action selection by one step, toggling between replay and straight-to-battle.
     *
     * Positive `delta` advances to the next footer action; negative `delta` moves to the previous action. A `delta` of zero has no effect.
     *
     * @param delta Positive to move forward, negative to move backward, zero does nothing.
     * @param shouldPlayScrollSfx If true, play the scroll selection sound effect when the selection changes.
     */
    void moveFooterSelection(int delta, bool shouldPlayScrollSfx) {
        if (delta == 0) {
            return;
        }

        if (delta > 0) {
            selectFooterAction(footerSelection_ == FooterAction::ReplayStory
                                   ? FooterAction::StraightToBattle
                                   : FooterAction::ReplayStory,
                               shouldPlayScrollSfx);
        } else {
            selectFooterAction(footerSelection_ == FooterAction::StraightToBattle
                                   ? FooterAction::ReplayStory
                                   : FooterAction::StraightToBattle,
                               shouldPlayScrollSfx);
        }
    }

    /**
     * @brief Activates the currently selected entry.
     *
     * If a selection exists, triggers the configured footer action for that entry (for example, start the practice battle or replay the story route).
     * If no entries are loaded, this function does nothing.
     */
    void activateSelected() {
        if (entries_.empty()) {
            return;
        }

        activateFooterAction();
    }

    /**
     * @brief Initiates the footer-selected action by preparing a launch request.
     *
     * If there are no entries, this is a no-op. Otherwise plays the confirmation
     * sound and sets `launchRequest_` to a `LaunchRequest` whose mode is
     * `PracticeReplayStory` when the footer selection is `ReplayStory` and the
     * selected entry contains a non-empty `storyScript`; in all other cases the
     * mode is `PracticeStraightToBattle`. The launch payload string is the entry's
     * `storyScript` when replaying a story, or the entry's `battle.key` otherwise.
     */
    void activateFooterAction() {
        if (entries_.empty()) {
            return;
        }

        const Entry& entry = entries_[selectedIndex_];
        playConfirmSfx();
        launchRequest_ = LaunchRequest{
            footerSelection_ == FooterAction::ReplayStory && !entry.battle.storyScript.empty()
                ? LaunchRequest::Mode::PracticeReplayStory
                : LaunchRequest::Mode::PracticeStraightToBattle,
            footerSelection_ == FooterAction::ReplayStory && !entry.battle.storyScript.empty()
                ? entry.battle.storyScript
                : entry.battle.key,
        };
    }

    void openConfirmForSelected() {
        if (entries_.empty()) {
            return;
        }

        openConfirmForEntry(selectedIndex_);
    }

    /**
     * @brief Open the confirmation overlay for the specified entry.
     *
     * Focuses the carousel, selects the entry at the given index without playing scroll sound,
     * marks that entry as the pending confirmation target, and shows the confirmation dialog.
     *
     * @param index Index of the entry to confirm. If there are no entries, this is a no-op.
     */
    void openConfirmForEntry(std::size_t index) {
        if (entries_.empty()) {
            return;
        }

        focusZone_ = FocusZone::Carousel;
        setSelectedIndex(index, false);
        confirmTarget_ = ConfirmTarget{ConfirmTargetType::Entry, selectedIndex_};
        showConfirm();
    }

    /**
     * @brief Show the confirmation overlay for the current confirm target.
     *
     * Displays the confirm UI for the currently set confirm target, resets the
     * confirm choice to `Back`, clears held-input state and timers, and updates
     * the UI to reflect the change. Plays the confirm sound if the overlay is
     * not already visible or if the target has changed since the last shown
     * confirmation.
     */
    void showConfirm() {
        const bool targetChanged =
            !confirmVisible_ ||
            !confirmTarget_.has_value() ||
            (lastShownConfirmTarget_.has_value() &&
             (lastShownConfirmTarget_->type != confirmTarget_->type ||
              lastShownConfirmTarget_->entryIndex != confirmTarget_->entryIndex));

        if (!confirmVisible_ || targetChanged) {
            playConfirmSfx();
        }

        confirmVisible_ = true;
        confirmChoice_ = ConfirmChoice::Back;
        negativeHeld_ = false;
        positiveHeld_ = false;
        holdNegativeElapsed_ = 0.0f;
        holdPositiveElapsed_ = 0.0f;
        lastShownConfirmTarget_ = confirmTarget_;
        applySelection();
    }

    void closeConfirm() {
        if (!confirmVisible_) {
            return;
        }

        confirmVisible_ = false;
        pressedConfirmChoice_.reset();
        applySelection();
    }

    void setConfirmChoice(ConfirmChoice choice, bool shouldPlayScrollSfx) {
        if (confirmChoice_ == choice) {
            return;
        }

        confirmChoice_ = choice;
        if (shouldPlayScrollSfx) {
            playScrollSfx();
        }
        applySelection();
    }

    /**
     * @brief Confirms or cancels the currently visible confirmation overlay.
     *
     * If the confirmation overlay is not visible this is a no-op. If the user chose Back,
     * the overlay is closed. If the user chose Proceed, the pending confirm target is
     * consumed; when a target exists the function selects that entry (without playing the
     * scroll sound) and activates it. The UI selection state is reapplied after handling.
     */
    void activateConfirmSelection() {
        if (!confirmVisible_) {
            return;
        }

        if (confirmChoice_ == ConfirmChoice::Back) {
            closeConfirm();
            return;
        }

        confirmVisible_ = false;
        if (!confirmTarget_.has_value()) {
            applySelection();
            return;
        }

        const ConfirmTarget target = *confirmTarget_;
        confirmTarget_.reset();
        setSelectedIndex(target.entryIndex, false);
        activateSelected();
        applySelection();
    }

    bool hitTestElement(float x, float y, Rml::Element* element) const {
        if (element == nullptr || !element->IsVisible(true)) {
            return false;
        }

        Rml::Vector2f point{x, y};
        if (!element->Project(point)) {
            return false;
        }
        return element->IsPointWithinElement(point);
    }

    /**
     * @brief Finds the top-most carousel card under a point.
     *
     * Tests each cached card element for a hit at the given document-space coordinates and returns
     * the index of the visible card whose z-index is greatest when multiple cards overlap.
     *
     * @param x X coordinate in the Rml document / UI coordinate space.
     * @param y Y coordinate in the Rml document / UI coordinate space.
     * @return std::optional<std::size_t> Index of the hit card if one was found, empty otherwise.
     */
    std::optional<std::size_t> hitTestCard(float x, float y) const {
        std::optional<std::size_t> bestIndex;
        float bestZIndex = -1000000.0f;

        for (std::size_t i = 0; i < cardElements_.size(); ++i) {
            Rml::Element* element = cardElements_[i];
            if (!hitTestElement(x, y, element)) {
                continue;
            }

            const float zIndex = element->GetZIndex();
            if (!bestIndex.has_value() || zIndex >= bestZIndex) {
                bestIndex = i;
                bestZIndex = zIndex;
            }
        }

        return bestIndex;
    }

    /**
     * @brief Determine which footer action (if any) is under the given point.
     *
     * @param x X coordinate in UI coordinate space (pixels).
     * @param y Y coordinate in UI coordinate space (pixels).
     * @return std::optional<FooterAction> `FooterAction::ReplayStory` if the point hits the replay button,
     * `FooterAction::StraightToBattle` if it hits the straight-to-battle button, `std::nullopt` otherwise.
     *
     * The replay button is tested before the straight-to-battle button and takes precedence if both overlap.
     */
    std::optional<FooterAction> hitTestFooterAction(float x, float y) const {
        if (hitTestElement(x, y, replayStoryButtonElement_)) {
            return FooterAction::ReplayStory;
        }
        if (hitTestElement(x, y, straightToBattleButtonElement_)) {
            return FooterAction::StraightToBattle;
        }
        return std::nullopt;
    }

    /**
     * @brief Produce the localized title text for the confirmation overlay.
     *
     * Returns a prompt appropriate to the current confirmation target and selected footer action:
     * - If there is no confirmation target or there are no entries, returns "Pick This Rival?".
     * - If the selected footer action is ReplayStory and the targeted entry has a non-empty story script, returns "Replay This Story?".
     * - Otherwise, returns "Start Practice Battle?".
     *
     * @return std::string The confirmation overlay title.
     */
    std::string confirmTitleRml() const {
        if (!confirmTarget_.has_value() || entries_.empty()) {
            return "Pick This Rival?";
        }

        const Entry& entry = entries_[confirmTarget_->entryIndex % entries_.size()];
        const bool replayStory =
            footerSelection_ == FooterAction::ReplayStory && !entry.battle.storyScript.empty();
        return replayStory ? "Replay This Story?" : "Start Practice Battle?";
    }

    /**
     * @brief Builds the Rml-formatted body text for the confirmation dialog.
     *
     * When no confirm target is set or there are no entries, returns a generic prompt.
     * Otherwise returns either a "Replay the story route for:" message (when the selected
     * footer action is ReplayStory and the entry has a non-empty story script) or a
     * "Jump straight into practice battle against:" message. Both variants include the
     * escaped destination text "{boss title} / {battle name}" with a line break before it.
     *
     * @return std::string The Rml/HTML body to display in the confirm overlay.
     */
    std::string confirmBodyRml() const {
        if (!confirmTarget_.has_value() || entries_.empty()) {
            return "Are you sure you want to pick this rival?";
        }

        const Entry& entry = entries_[confirmTarget_->entryIndex % entries_.size()];
        const bool replayStory =
            footerSelection_ == FooterAction::ReplayStory && !entry.battle.storyScript.empty();
        const std::string destination = escapeRml(entry.boss.title) + " / " + escapeRml(entry.battle.name);
        if (replayStory) {
            return "Replay the story route for:<br/><br/>" + destination;
        }
        return "Jump straight into practice battle against:<br/><br/>" + destination;
    }

    /**
     * @brief Process held directional input and generate repeated selection moves based on hold timing.
     *
     * Updates internal hold timers for the negative and positive directions and, when the initial
     * hold delay and subsequent repeat intervals elapse, invokes moveSelection to step the carousel
     * (playing scroll SFX when movements occur).
     *
     * @param deltaSeconds Time elapsed since the last update, in seconds.
     */
    void updateHeldInput(float deltaSeconds) {
        const auto updateDirection = [deltaSeconds](bool held, float& elapsed) -> bool {
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

    void updateVisualSelection(float deltaSeconds) {
        if (entries_.empty()) {
            return;
        }

        const float blend = deltaSeconds > 0.0f
            ? clamp01(1.0f - std::exp(-deltaSeconds * kSelectionLerpSpeed))
            : 1.0f;

        visualSelectionIndex_ += (targetVisualSelectionIndex_ - visualSelectionIndex_) * blend;

        if (std::fabs(visualSelectionIndex_ - targetVisualSelectionIndex_) < 0.001f) {
            visualSelectionIndex_ = targetVisualSelectionIndex_;
        }

        const float count = static_cast<float>(entries_.size());
        if (visualSelectionIndex_ < 0.0f && targetVisualSelectionIndex_ < 0.0f &&
            std::fabs(visualSelectionIndex_ - targetVisualSelectionIndex_) < 0.02f) {
            visualSelectionIndex_ += count;
            targetVisualSelectionIndex_ += count;
        } else if (visualSelectionIndex_ >= count && targetVisualSelectionIndex_ >= count &&
                   std::fabs(visualSelectionIndex_ - targetVisualSelectionIndex_) < 0.02f) {
            visualSelectionIndex_ -= count;
            targetVisualSelectionIndex_ -= count;
        }

        updateAnimatedLayout();
    }

    void updateToast(float deltaSeconds) {
        if (toastElement_ == nullptr) {
            return;
        }

        if (toastTimer_ > 0.0f) {
            toastTimer_ = std::max(0.0f, toastTimer_ - std::max(deltaSeconds, 0.0f));
            if (toastTimer_ <= 0.0f) {
                toastElement_->SetClass("is-visible", false);
            }
        }
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
        toastTimer_ = 1.12f;
    }

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool initialized_ = false;
    bool finished_ = false;
    bool audioReady_ = false;
    bool rmlInitialized_ = false;
    bool rmlGlInitialized_ = false;
    int windowWidth_ = 1280;
    int windowHeight_ = 720;
    int drawableWidth_ = 1280;
    int drawableHeight_ = 720;
    SystemInterface_SDL systemInterface_;
    std::unique_ptr<graphics::RmlUiSdlGlRenderInterface> renderInterface_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    game::audio::WavOneShotPlayer sfxPlayer_;
    std::vector<Entry> entries_;
    battle::PlayerProgression progression_;
    std::optional<LaunchRequest> launchRequest_;
    std::size_t selectedIndex_ = 0;
    float visualSelectionIndex_ = 0.0f;
    float targetVisualSelectionIndex_ = 0.0f;
    FocusZone focusZone_ = FocusZone::Carousel;
    FooterAction footerSelection_ = FooterAction::ReplayStory;
    bool negativeHeld_ = false;
    bool positiveHeld_ = false;
    float holdNegativeElapsed_ = 0.0f;
    float holdPositiveElapsed_ = 0.0f;
    float toastTimer_ = 0.0f;
    std::string scrollSfxPath_;
    std::string confirmSfxPath_;
    graphics::RmlUiLoadingOverlay loadingOverlay_;
    graphics::RmlUiLoadingOverlayState loadingOverlayState_;
    graphics::UiMusicBarStrip uiMusicBars_;
    game::audio::UiMusicVisualState uiMusicVisualState_;

    Rml::Element* trackElement_ = nullptr;
    Rml::Element* rankValueElement_ = nullptr;
    Rml::Element* infoPortraitImageElement_ = nullptr;
    Rml::Element* infoNameElement_ = nullptr;
    Rml::Element* infoBattleElement_ = nullptr;
    Rml::Element* infoCopyElement_ = nullptr;
    Rml::Element* infoHpElement_ = nullptr;
    Rml::Element* infoAtkElement_ = nullptr;
    Rml::Element* infoSpdElement_ = nullptr;
    Rml::Element* infoHintElement_ = nullptr;
    Rml::Element* replayStoryButtonElement_ = nullptr;
    Rml::Element* straightToBattleButtonElement_ = nullptr;
    Rml::Element* toastElement_ = nullptr;
    Rml::Element* confirmOverlayElement_ = nullptr;
    Rml::Element* confirmTitleElement_ = nullptr;
    Rml::Element* confirmBodyElement_ = nullptr;
    Rml::Element* confirmBackElement_ = nullptr;
    Rml::Element* confirmProceedElement_ = nullptr;
    std::vector<Rml::Element*> cardElements_;
    std::vector<Rml::Element*> cardPortraitImageElements_;
    std::optional<ConfirmTarget> confirmTarget_;
    std::optional<ConfirmTarget> lastShownConfirmTarget_;
    bool confirmVisible_ = false;
    ConfirmChoice confirmChoice_ = ConfirmChoice::Back;
    std::optional<std::size_t> pressedCardIndex_;
    std::optional<std::size_t> pressedConfirmCardIndex_;
    std::optional<ConfirmChoice> pressedConfirmChoice_;
    std::optional<FooterAction> pressedFooterAction_;
    std::optional<FooterAction> pressedConfirmFooterAction_;
};

/**
     * @brief Constructs a Session and initializes its private implementation.
     *
     * Allocates and stores the SessionImpl instance that encapsulates platform and
     * UI-specific session state and behavior.
     */
    Session::Session()
    : impl_(std::make_unique<SessionImpl>()) {}

Session::~Session() = default;

bool Session::initialize(Window& window) {
    return impl_->initialize(window);
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

/**
 * @brief Update the session's loading overlay display state.
 *
 * Stores the provided loading overlay state and applies it to the session's
 * RmlUi loading overlay (visible progress/blocked UI).
 *
 * @param state The loading overlay state to apply.
 */
void Session::setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) {
    impl_->setLoadingOverlay(state);
}

/**
 * @brief Update the UI music visualization state used by the selector's music bars.
 *
 * Applies the provided audio UI visual state so the cached music bar strip reflects
 * the new visualization (e.g., amplitudes, peak/decay settings).
 *
 * @param state The new visual state to apply to the UI music bars.
 */
void Session::setUiMusicVisualState(const game::audio::UiMusicVisualState& state) {
    impl_->setUiMusicVisualState(state);
}

/**
 * @brief Indicates whether the session has completed and should be closed.
 *
 * @return `true` if the session is finished and should exit, `false` otherwise.
 */
bool Session::isFinished() const {
    return impl_->isFinished();
}

std::optional<LaunchRequest> Session::consumeLaunchRequest() {
    return impl_->consumeLaunchRequest();
}

}  // namespace battle::selector
