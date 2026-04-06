#define GL_GLEXT_PROTOTYPES

#include "battle_selector_preview_session.h"

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

#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Event.h>

#include <nlohmann/json.hpp>

#include "RmlUi_Renderer_GL3.h"

#include "../game/core/battle_loader.h"
#include "../game/save/save.h"
#include "../platform/path_resolution.h"
#include "../window.h"

namespace graphics::preview {
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
constexpr const char* kScrollSfxRelativePath = "assets/ui/sfx/Selection_roulette-3.wav";
constexpr const char* kConfirmSfxRelativePath = "assets/ui/sfx/SongSelect_confirm-selection.wav";

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

std::string resolvePreviewSpritePath(const std::string& assetName) {
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

BattleSelectorPreviewSession::~BattleSelectorPreviewSession() {
    shutdown();
}

bool BattleSelectorPreviewSession::initialize(Window& window) {
    shutdown();

    windowHost_ = &window;
    window_ = window.getNativeWindow();
    glContext_ = window.getGlContext();
    if (window_ == nullptr || glContext_ == nullptr) {
        std::cerr << "[BattleSelectorPreview] Window is not in OpenGL mode.\n";
        return false;
    }

    initializeAudio();

    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(1);
    SDL_StopTextInput();

    Rml::String glInitMessage;
    if (!RmlGL3::Initialize(&glInitMessage)) {
        std::cerr << "[BattleSelectorPreview] RmlGL3 initialization failed: " << glInitMessage << "\n";
        shutdown();
        return false;
    }
    rmlGlInitialized_ = true;

    systemInterface_.SetWindow(window_);
    renderInterface_ = std::make_unique<RmlUiSdlGlRenderInterface>();
    if (!(*renderInterface_)) {
        std::cerr << "[BattleSelectorPreview] Failed to construct GL render interface.\n";
        shutdown();
        return false;
    }

    Rml::SetSystemInterface(&systemInterface_);
    Rml::SetRenderInterface(renderInterface_.get());
    if (!Rml::Initialise()) {
        std::cerr << "[BattleSelectorPreview] RmlUi core initialization failed.\n";
        shutdown();
        return false;
    }
    rmlInitialized_ = true;

    if (!loadFonts()) {
        std::cerr << "[BattleSelectorPreview] No usable fonts were loaded.\n";
        shutdown();
        return false;
    }

    updateViewportFromWindow();
    renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
    context_ = Rml::CreateContext("battle-selector-preview", Rml::Vector2i(windowWidth_, windowHeight_));
    if (context_ == nullptr) {
        std::cerr << "[BattleSelectorPreview] Failed to create RmlUi context.\n";
        shutdown();
        return false;
    }
    applyContextScale();

    if (!loadEntries()) {
        std::cerr << "[BattleSelectorPreview] Failed to load battle entries.\n";
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

void BattleSelectorPreviewSession::shutdown() {
    initialized_ = false;
    negativeHeld_ = false;
    positiveHeld_ = false;
    holdNegativeElapsed_ = 0.0f;
    holdPositiveElapsed_ = 0.0f;
        toastTimer_ = 0.0f;
        visualSelectionIndex_ = 0.0f;
        targetVisualSelectionIndex_ = 0.0f;
        focusZone_ = FocusZone::Carousel;
        footerSelection_ = FooterAction::ReplayStory;

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
    infoPortraitElement_ = nullptr;
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
        cardElements_.clear();
        cardPortraitImageElements_.clear();
        entries_.clear();

    if (context_ != nullptr) {
        context_->UnloadAllDocuments();
        Rml::RemoveContext("battle-selector-preview");
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

void BattleSelectorPreviewSession::handleEvent(const SDL_Event& event) {
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
                if (focusZone_ == FocusZone::Footer) {
                    activateFooterAction();
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

void BattleSelectorPreviewSession::update(float deltaSeconds) {
    if (!initialized_ || context_ == nullptr) {
        return;
    }

    updateHeldInput(deltaSeconds);
    updateVisualSelection(deltaSeconds);
    updateToast(deltaSeconds);
    sfxPlayer_.cleanupFinishedPlayback();
    context_->Update();
}

void BattleSelectorPreviewSession::render() {
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

void BattleSelectorPreviewSession::detachEventListeners() {
    for (EventListenerBinding& binding : listeners_) {
        if (binding.element != nullptr && binding.listener != nullptr) {
            binding.element->RemoveEventListener(binding.eventId, binding.listener.get(), binding.capturePhase);
        }
    }
    listeners_.clear();
}

bool BattleSelectorPreviewSession::loadFonts() const {
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

bool BattleSelectorPreviewSession::initializeAudio() {
    scrollSfxPath_ = platform::path::resolvePath(kScrollSfxRelativePath);
    confirmSfxPath_ = platform::path::resolvePath(kConfirmSfxRelativePath);

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "[BattleSelectorPreview] Audio init failed: " << SDL_GetError() << "\n";
            scrollSfxPath_.clear();
            confirmSfxPath_.clear();
            audioReady_ = false;
            return false;
        }
    }

    audioReady_ = !scrollSfxPath_.empty() || !confirmSfxPath_.empty();
    return audioReady_;
}

void BattleSelectorPreviewSession::updateViewportFromWindow() {
    if (windowHost_ == nullptr) {
        return;
    }

    windowWidth_ = std::max(1, windowHost_->getWindowWidth());
    windowHeight_ = std::max(1, windowHost_->getWindowHeight());
    drawableWidth_ = std::max(1, windowHost_->getDrawableWidth());
    drawableHeight_ = std::max(1, windowHost_->getDrawableHeight());
    glViewport(0, 0, drawableWidth_, drawableHeight_);
}

void BattleSelectorPreviewSession::applyContextScale() {
    if (context_ == nullptr) {
        return;
    }

    const float widthScale = static_cast<float>(windowWidth_) / static_cast<float>(kReferenceWidth);
    const float heightScale = static_cast<float>(windowHeight_) / static_cast<float>(kReferenceHeight);
    const float scale = std::min(widthScale, heightScale);
    context_->SetDensityIndependentPixelRatio(std::max(scale, 0.01f));
}

bool BattleSelectorPreviewSession::loadEntries() {
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
        entry.spritePath = resolvePreviewSpritePath(boss.assets);
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
                        entry.instructionHint = skillIt->value("instructionHint", entry.instructionHint);
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

bool BattleSelectorPreviewSession::loadDocument() {
    if (context_ == nullptr) {
        return false;
    }

    const std::string documentPath = platform::path::resolvePath(kDocumentPath);
    document_ = context_->LoadDocument(documentPath);
    if (document_ == nullptr) {
        std::cerr << "[BattleSelectorPreview] Failed to load document: " << documentPath << "\n";
        return false;
    }

    document_->Show();
    cacheElements();
    buildTrack();
    attachListeners();
    applySelection();
    return true;
}

void BattleSelectorPreviewSession::cacheElements() {
    if (document_ == nullptr) {
        return;
    }

    trackElement_ = document_->GetElementById("boss-track");
    rankValueElement_ = document_->GetElementById("idol-rank-value");
    infoPortraitElement_ = document_->GetElementById("info-portrait");
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
}

void BattleSelectorPreviewSession::buildTrack() {
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
        cardPortraitImageElements_.push_back(document_->GetElementById("boss-card-portrait-image-" + std::to_string(i)));

        if (cardPortraitImageElements_.back() != nullptr && !entries_[i].spritePath.empty()) {
            cardPortraitImageElements_.back()->SetAttribute("src", entries_[i].spritePath);
        }
    }
}

void BattleSelectorPreviewSession::attachListeners() {
    if (document_ == nullptr) {
        return;
    }

    if (Rml::Element* replayElement = replayStoryButtonElement_) {
        auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
            selectFooterAction(FooterAction::ReplayStory, false);
            activateFooterAction();
        });
        replayElement->AddEventListener(Rml::EventId::Click, clickListener.get());
        listeners_.push_back(EventListenerBinding{
            replayElement,
            Rml::EventId::Click,
            false,
            std::move(clickListener),
        });
    }

    if (Rml::Element* battleElement = straightToBattleButtonElement_) {
        auto clickListener = std::make_unique<CallbackEventListener>([this](Rml::Event&) {
            selectFooterAction(FooterAction::StraightToBattle, false);
            activateFooterAction();
        });
        battleElement->AddEventListener(Rml::EventId::Click, clickListener.get());
        listeners_.push_back(EventListenerBinding{
            battleElement,
            Rml::EventId::Click,
            false,
            std::move(clickListener),
        });
    }
}

void BattleSelectorPreviewSession::applySelection() {
    if (entries_.empty()) {
        return;
    }

    updateAnimatedLayout();

    if (replayStoryButtonElement_ != nullptr) {
        replayStoryButtonElement_->SetClass("is-focused",
                                           focusZone_ == FocusZone::Footer &&
                                               footerSelection_ == FooterAction::ReplayStory);
    }
    if (straightToBattleButtonElement_ != nullptr) {
        straightToBattleButtonElement_->SetClass("is-focused",
                                                 focusZone_ == FocusZone::Footer &&
                                                     footerSelection_ == FooterAction::StraightToBattle);
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

void BattleSelectorPreviewSession::updateAnimatedLayout() {
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
        const float x = kCarouselBaseLeft + (slot * kCarouselStepX);
        const float absOffset = std::fabs(offset);
        const float y = kCarouselBaseTop - (offset * kCarouselStairStep) + (absOffset * 6.0f);
        const float scale = absOffset < 0.05f ? 1.08f : std::max(0.88f, 1.0f - absOffset * 0.04f);
        const float opacity = absOffset < 0.05f ? 1.0f : std::max(0.50f, 0.92f - absOffset * 0.10f);

        element->SetProperty("width", formatDp(kCarouselCardWidthDp));
        element->SetProperty("height", formatDp(kCarouselCardHeightDp));
        element->SetProperty("transform", translateScale(x, y, scale));
        element->SetProperty("opacity", formatNumber(opacity));
        element->SetProperty("z-index", std::to_string(static_cast<int>(100.0f - absOffset * 10.0f)));
    }
}

void BattleSelectorPreviewSession::updateInfoPanel() const {
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

void BattleSelectorPreviewSession::setSelectedIndex(std::size_t index, bool shouldPlayScrollSfx) {
    if (entries_.empty()) {
        return;
    }

    index %= entries_.size();
    if (index == selectedIndex_) {
        return;
    }

    selectedIndex_ = index;
    applySelection();
    if (shouldPlayScrollSfx) {
        playScrollSfx();
    }
}

void BattleSelectorPreviewSession::moveSelection(int delta, bool shouldPlayScrollSfx) {
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

void BattleSelectorPreviewSession::setFocusZone(FocusZone zone, bool shouldPlayScrollSfx) {
    if (focusZone_ == zone) {
        return;
    }

    focusZone_ = zone;
    applySelection();
    if (shouldPlayScrollSfx) {
        playScrollSfx();
    }
}

void BattleSelectorPreviewSession::selectFooterAction(FooterAction action, bool shouldPlayScrollSfx) {
    if (footerSelection_ == action && focusZone_ == FocusZone::Footer) {
        return;
    }

    footerSelection_ = action;
    focusZone_ = FocusZone::Footer;
    applySelection();
    if (shouldPlayScrollSfx) {
        playScrollSfx();
    }
}

void BattleSelectorPreviewSession::moveFooterSelection(int delta, bool shouldPlayScrollSfx) {
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

void BattleSelectorPreviewSession::activateSelected() {
    if (entries_.empty()) {
        return;
    }

    playConfirmSfx();
    showToast("Preview Confirm: " + entries_[selectedIndex_].battle.name);
}

void BattleSelectorPreviewSession::activateFooterAction() {
    if (entries_.empty()) {
        return;
    }

    playConfirmSfx();
    const std::string& actionLabel = footerSelection_ == FooterAction::ReplayStory
        ? "REPLAY STORY"
        : "STRAIGHT TO BATTLE";
    showToast("Preview Confirm: " + actionLabel + " - " + entries_[selectedIndex_].battle.name);
}

void BattleSelectorPreviewSession::updateHeldInput(float deltaSeconds) {
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

void BattleSelectorPreviewSession::updateVisualSelection(float deltaSeconds) {
    if (entries_.empty()) {
        return;
    }

    const float blend = deltaSeconds > 0.0f
        ? std::clamp(1.0f - std::exp(-deltaSeconds * kSelectionLerpSpeed), 0.0f, 1.0f)
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

void BattleSelectorPreviewSession::updateToast(float deltaSeconds) {
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

void BattleSelectorPreviewSession::playScrollSfx() {
    if (!audioReady_ || scrollSfxPath_.empty()) {
        return;
    }
    (void)sfxPlayer_.playWavOneShot(scrollSfxPath_, 0.78f, true);
}

void BattleSelectorPreviewSession::playConfirmSfx() {
    if (!audioReady_ || confirmSfxPath_.empty()) {
        return;
    }
    (void)sfxPlayer_.playWavOneShot(confirmSfxPath_, 0.92f, true);
}

void BattleSelectorPreviewSession::showToast(const std::string& message) {
    if (toastElement_ == nullptr) {
        return;
    }

    toastElement_->SetInnerRML(escapeRml(message));
    toastElement_->SetClass("is-visible", true);
    toastTimer_ = 0.98f;
}

}  // namespace graphics::preview
