#define GL_GLEXT_PROTOTYPES

#include "boss_selector_session.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
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
constexpr const char* kDocumentPath = "assets/rmlui/previews/battle_selector_preview.rml";
constexpr const char* kLoadingOverlayDocumentPath = "assets/rmlui/shared/loading_overlay.rml";
constexpr const char* kScrollSfxRelativePath = "assets/ui/sfx/Selection_roulette-3.wav";
constexpr const char* kConfirmSfxRelativePath = "assets/ui/sfx/SongSelect_confirm-selection.wav";
constexpr const char* kFinaleBattleKey = "lyoo_plot_twist";

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
        launchRequest_.reset();
        finaleBattle_.reset();

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
        finaleButtonElement_ = nullptr;
        toastElement_ = nullptr;
        cardElements_.clear();
        cardPortraitImageElements_.clear();
        entries_.clear();
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
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    finished_ = true;
                    break;

                case SDLK_LEFT:
                    if (focusZone_ == FocusZone::Carousel) {
                        negativeHeld_ = true;
                        holdNegativeElapsed_ = 0.0f;
                        moveSelection(-1, true);
                    }
                    break;

                case SDLK_RIGHT:
                    if (focusZone_ == FocusZone::Carousel) {
                        positiveHeld_ = true;
                        holdPositiveElapsed_ = 0.0f;
                        moveSelection(1, true);
                    }
                    break;

                case SDLK_UP:
                    setFocusZone(FocusZone::Carousel, true);
                    break;

                case SDLK_DOWN:
                    setFocusZone(FocusZone::Finale, true);
                    break;

                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:
                    if (focusZone_ == FocusZone::Finale) {
                        activateFinale();
                    } else {
                        activateSelected();
                    }
                    break;

                default:
                    break;
            }
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

    void setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) {
        loadingOverlayState_ = state;
        loadingOverlay_.apply(loadingOverlayState_);
    }

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
        Finale,
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

        std::sort(battles.begin(), battles.end(), [](const battle::BattleDefinition& lhs,
                                                     const battle::BattleDefinition& rhs) {
            return lhs.id < rhs.id;
        });

        const std::unordered_set<std::string> clearedKeys(
            progression_.clearedBattleKeys.begin(),
            progression_.clearedBattleKeys.end());

        for (const battle::BattleDefinition& battle : battles) {
            if (battle.key == kFinaleBattleKey) {
                finaleBattle_ = battle;
            }

            if (!battle.selectorVisible) {
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
            entry.defeated = clearedKeys.find(battle.key) != clearedKeys.end();
            entry.instructionHint = "Preview interaction hint not available yet.";

            if (loadedBossMeta && bossRoot.is_object()) {
                const auto bossIt = bossRoot.find(battle.bossKey);
                if (bossIt != bossRoot.end() && bossIt->is_object()) {
                    const json& bossJson = *bossIt;
                    if (bossJson.contains("abilities") && bossJson.at("abilities").is_object()) {
                        const json& abilitiesJson = bossJson.at("abilities");
                        const auto skillIt = abilitiesJson.find("skill");
                        if (skillIt != abilitiesJson.end() && skillIt->is_object()) {
                            entry.instructionHint =
                                skillIt->value("instructionHint", entry.instructionHint);
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
        cacheElements();
        buildTrack();
        attachListeners();
        applySelection();
        return true;
    }

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
        finaleButtonElement_ = document_->GetElementById("finale-button");
        toastElement_ = document_->GetElementById("selector-toast");
    }

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
                   << "<div class=\"boss-card-state";
            if (entry.defeated) {
                markup << " is-cleared";
            }
            markup << "\">" << escapeRml(entry.defeated ? "Cleared" : "Open") << "</div>"
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

    void attachListeners() {
        if (document_ == nullptr) {
            return;
        }

        for (std::size_t i = 0; i < cardElements_.size(); ++i) {
            if (Rml::Element* element = cardElements_[i]) {
                auto clickListener = std::make_unique<CallbackEventListener>([this, i](Rml::Event&) {
                    focusZone_ = FocusZone::Carousel;
                    setSelectedIndex(i, false);
                    activateSelected();
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

        if (Rml::Element* finaleElement = finaleButtonElement_) {
            auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
                focusZone_ = FocusZone::Finale;
                applySelection();
                activateFinale();
            });
            finaleElement->AddEventListener(Rml::EventId::Click, clickListener.get());
            listeners_.push_back(EventListenerBinding{
                finaleElement,
                Rml::EventId::Click,
                false,
                std::move(clickListener),
            });
        }
    }

    bool isFinaleUnlocked() const {
        const std::size_t clearedVisibleCount = static_cast<std::size_t>(std::count_if(
            entries_.begin(), entries_.end(), [](const Entry& entry) { return entry.defeated; }));
        return !entries_.empty() && clearedVisibleCount == entries_.size();
    }

    void applySelection() {
        if (entries_.empty()) {
            return;
        }

        updateAnimatedLayout();

        if (finaleButtonElement_ != nullptr) {
            finaleButtonElement_->SetClass("is-focused", focusZone_ == FocusZone::Finale);
            finaleButtonElement_->SetClass("is-locked", !isFinaleUnlocked());
        }

        for (std::size_t i = 0; i < cardElements_.size(); ++i) {
            if (cardElements_[i] != nullptr) {
                cardElements_[i]->SetClass("is-selected", i == selectedIndex_ && focusZone_ == FocusZone::Carousel);
            }
        }

        const std::set<std::string> clearedKeys(
            progression_.clearedBattleKeys.begin(),
            progression_.clearedBattleKeys.end());
        int idolRank = 0;
        for (const Entry& entry : entries_) {
            if (clearedKeys.find(entry.battle.key) != clearedKeys.end()) {
                ++idolRank;
            }
        }
        if (finaleBattle_.has_value() &&
            clearedKeys.find(finaleBattle_->key) != clearedKeys.end()) {
            ++idolRank;
        }
        if (rankValueElement_ != nullptr) {
            rankValueElement_->SetInnerRML(std::to_string(idolRank));
        }

        updateInfoPanel();
    }

    void updateAnimatedLayout() {
        if (entries_.empty()) {
            return;
        }

        const float cardWidth = 164.0f;
        const float xStep = 118.0f;
        const float baseLeft = 58.0f;
        const float baseTop = 150.0f;
        const float stairStep = 18.0f;
        const int selectedSlot = static_cast<int>(entries_.size() / 2);

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

            const float slot = offset + static_cast<float>(selectedSlot);
            const float x = baseLeft + slot * xStep;
            const float absOffset = std::fabs(offset);
            const float y = baseTop - offset * stairStep + absOffset * 6.0f;
            const float scale = absOffset < 0.05f ? 1.08f : std::max(0.88f, 1.0f - absOffset * 0.04f);
            const float opacity = absOffset < 0.05f ? 1.0f : std::max(0.50f, 0.92f - absOffset * 0.10f);

            element->SetProperty("width", formatDp(cardWidth));
            element->SetProperty("height", formatDp(280.0f));
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

    void activateSelected() {
        if (entries_.empty()) {
            return;
        }

        const Entry& entry = entries_[selectedIndex_];
        playConfirmSfx();
        launchRequest_ = LaunchRequest{
            entry.battle.storyScript.empty() ? LaunchRequest::Type::Battle : LaunchRequest::Type::Story,
            entry.battle.storyScript.empty() ? entry.battle.key : entry.battle.storyScript,
        };
    }

    void activateFinale() {
        if (!isFinaleUnlocked()) {
            showToast("Defeat every rival before becoming an idol.");
            return;
        }
        if (!finaleBattle_.has_value()) {
            showToast("Finale battle is not configured.");
            return;
        }

        playConfirmSfx();
        launchRequest_ = LaunchRequest{
            finaleBattle_->storyScript.empty() ? LaunchRequest::Type::Battle : LaunchRequest::Type::Story,
            finaleBattle_->storyScript.empty() ? finaleBattle_->key : finaleBattle_->storyScript,
        };
    }

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
    std::optional<battle::BattleDefinition> finaleBattle_;
    std::optional<LaunchRequest> launchRequest_;
    std::size_t selectedIndex_ = 0;
    float visualSelectionIndex_ = 0.0f;
    float targetVisualSelectionIndex_ = 0.0f;
    FocusZone focusZone_ = FocusZone::Carousel;
    bool negativeHeld_ = false;
    bool positiveHeld_ = false;
    float holdNegativeElapsed_ = 0.0f;
    float holdPositiveElapsed_ = 0.0f;
    float toastTimer_ = 0.0f;
    std::string scrollSfxPath_;
    std::string confirmSfxPath_;
    graphics::RmlUiLoadingOverlay loadingOverlay_;
    graphics::RmlUiLoadingOverlayState loadingOverlayState_;

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
    Rml::Element* finaleButtonElement_ = nullptr;
    Rml::Element* toastElement_ = nullptr;
    std::vector<Rml::Element*> cardElements_;
    std::vector<Rml::Element*> cardPortraitImageElements_;
};

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

void Session::setLoadingOverlay(const graphics::RmlUiLoadingOverlayState& state) {
    impl_->setLoadingOverlay(state);
}

bool Session::isFinished() const {
    return impl_->isFinished();
}

std::optional<LaunchRequest> Session::consumeLaunchRequest() {
    return impl_->consumeLaunchRequest();
}

}  // namespace battle::selector
