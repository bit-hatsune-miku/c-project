#define GL_GLEXT_PROTOTYPES

#include "post_battle_session.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "audio/wav_one_shot.h"
#include "core/battle_loader.h"
#include "../graphics/rmlui_loading_overlay.h"
#include "../graphics/rmlui_sdl_gl_renderer.h"
#include "../graphics/ui_music_bars.h"
#include "../platform/path_resolution.h"
#include "../window.h"

namespace battle::postbattle {
namespace {

constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;
constexpr int kRewardPickCount = 4;
constexpr float kSummaryMetricsDurationSeconds = 0.84f;
constexpr float kSummaryRowDurationSeconds = 0.62f;
constexpr float kSummaryRowStaggerSeconds = 0.14f;
constexpr float kFlashDurationSeconds = 0.36f;
constexpr float kSignalDelaySeconds = 0.06f;
constexpr float kTickCooldownSeconds = 0.085f;
constexpr float kRewardPulseDurationSeconds = 0.32f;

constexpr const char* kDocumentPath = "assets/rmlui/post_battle.rml";
constexpr const char* kLoadingOverlayDocumentPath = "assets/rmlui/shared/loading_overlay.rml";
constexpr const char* kHoverSfxRelativePath = "assets/ui/sfx/UI_button-hover.wav";
constexpr const char* kSelectSfxRelativePath = "assets/ui/sfx/UI_button-select.wav";
constexpr const char* kPanelSfxRelativePath = "assets/ui/sfx/Results_statistics-panel-pop-in.wav";
constexpr const char* kTickSfxRelativePath = "assets/ui/sfx/Results_score-tick.wav";
constexpr const char* kSignalSfxRelativePath = "assets/ui/sfx/UI_bss-progress.wav";

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

struct RankingRowState {
    CharacterAnalytics analytics;
    int sharePercent = 0;
};

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float easeOutCubic(float value) {
    const float t = clamp01(value);
    const float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}

std::string formatNumber(float value, int precision = 3) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string formatDp(float value) {
    return formatNumber(value, 2) + "dp";
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

std::string formatIntegerWithCommas(int value) {
    std::string digits = std::to_string(std::max(value, 0));
    std::string formatted;
    formatted.reserve(digits.size() + digits.size() / 3);

    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count == 3) {
            formatted.push_back(',');
            count = 0;
        }
        formatted.push_back(*it);
        ++count;
    }

    std::reverse(formatted.begin(), formatted.end());
    return formatted;
}

std::string initialsForName(const std::string& name) {
    std::stringstream words(name);
    std::string word;
    std::string initials;
    while (words >> word) {
        if (!word.empty()) {
            initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(word.front()))));
        }
        if (initials.size() >= 2) {
            break;
        }
    }

    if (!initials.empty()) {
        return initials;
    }

    for (const char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (initials.size() >= 2) {
                break;
            }
        }
    }
    return initials.empty() ? "U" : initials;
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

bool hasRewardBuffs(const BattleDefinition& battleDefinition) {
    return battleDefinition.buffs.hp != 0 ||
           battleDefinition.buffs.atk != 0 ||
           battleDefinition.buffs.spd != 0;
}

}  // namespace

class SessionImpl {
public:
    bool initialize(Window& window,
                    const BattleDefinition& battleDefinition,
                    bool victory,
                    const Summary& summary,
                    const PlayerProgression& progression,
                    const std::vector<std::string>& partyLineup) {
        shutdown();

        windowHost_ = &window;
        window_ = window.getNativeWindow();
        glContext_ = window.getGlContext();
        if (window_ == nullptr || glContext_ == nullptr) {
            std::cerr << "[PostBattle] Window is not in OpenGL mode.\n";
            return false;
        }

        battleDefinition_ = battleDefinition;
        victory_ = victory;
        summary_ = summary;
        progression_ = progression;
        rewardAvailable_ = victory_ && hasRewardBuffs(battleDefinition_);

        if (!initializeAudio()) {
            std::cerr << "[PostBattle] Audio init failed; continuing without one-shots.\n";
        }

        SDL_GL_MakeCurrent(window_, glContext_);
        SDL_GL_SetSwapInterval(1);
        SDL_StopTextInput();

        Rml::String glInitMessage;
        if (!RmlGL3::Initialize(&glInitMessage)) {
            std::cerr << "[PostBattle] RmlGL3 initialization failed: " << glInitMessage << "\n";
            shutdown();
            return false;
        }
        rmlGlInitialized_ = true;

        systemInterface_.SetWindow(window_);
        renderInterface_ = std::make_unique<graphics::RmlUiSdlGlRenderInterface>();
        if (!(*renderInterface_)) {
            std::cerr << "[PostBattle] Failed to construct GL render interface.\n";
            shutdown();
            return false;
        }

        Rml::SetSystemInterface(&systemInterface_);
        Rml::SetRenderInterface(renderInterface_.get());
        if (!Rml::Initialise()) {
            std::cerr << "[PostBattle] RmlUi core initialization failed.\n";
            shutdown();
            return false;
        }
        rmlInitialized_ = true;

        if (!loadFonts()) {
            std::cerr << "[PostBattle] No usable fonts were loaded.\n";
            shutdown();
            return false;
        }

        updateViewportFromWindow();
        renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
        context_ = Rml::CreateContext("post-battle", Rml::Vector2i(windowWidth_, windowHeight_));
        if (context_ == nullptr) {
            std::cerr << "[PostBattle] Failed to create RmlUi context.\n";
            shutdown();
            return false;
        }
        applyContextScale();

        if (!loadPartyData(partyLineup)) {
            std::cerr << "[PostBattle] Failed to resolve party data.\n";
            shutdown();
            return false;
        }

        if (!loadDocument()) {
            shutdown();
            return false;
        }

        if (!loadingOverlay_.initialize(*context_, platform::path::resolvePath(kLoadingOverlayDocumentPath))) {
            std::cerr << "[PostBattle] Failed to load loading overlay document.\n";
            shutdown();
            return false;
        }
        loadingOverlayState_ = graphics::RmlUiLoadingOverlayState{};

        initialized_ = true;
        enterSummary(false);
        return true;
    }

    void shutdown() {
        initialized_ = false;
        finished_ = false;
        victory_ = false;
        rewardAvailable_ = false;
        currentView_ = View::Summary;
        footerSelection_ = FooterSelection::Continue;
        rewardFooterFocused_ = false;
        selectedCharacterIndex_ = 0;
        flashTimer_ = 0.0f;
        summaryIntroElapsed_ = 0.0f;
        summaryIntroActive_ = false;
        summaryTickCooldown_ = 0.0f;
        signalDelayRemaining_ = 0.0f;
        signalArmed_ = false;
        rewardPulseTimer_ = 0.0f;
        pulsingCharacterIndex_ = -1;
        currentSummaryDamage_ = 0;
        currentSummaryAv_ = 0;
        currentSummaryDpaTenths_ = 0;
        lastHoveredButtonId_.clear();
        pendingAllocation_.clear();
        rankingRows_.clear();
        summaryRowTickPlayed_.clear();
        partyEntries_.clear();
        battleDefinition_ = BattleDefinition{};
        summary_ = Summary{};
        progression_ = PlayerProgression{};

        detachEventListeners();

        sfxPlayer_.shutdown();
        hoverSfxPath_.clear();
        selectSfxPath_.clear();
        panelSfxPath_.clear();
        tickSfxPath_.clear();
        signalSfxPath_.clear();
        audioReady_ = false;

        if (document_ != nullptr) {
            document_->Close();
            document_ = nullptr;
        }

        summaryScreenElement_ = nullptr;
        rewardScreenElement_ = nullptr;
        summaryEyebrowElement_ = nullptr;
        summaryTitleElement_ = nullptr;
        metricTotalDamageElement_ = nullptr;
        metricAvElement_ = nullptr;
        metricDpaElement_ = nullptr;
        metricTotalFootElement_ = nullptr;
        metricAvFootElement_ = nullptr;
        metricDpaFootElement_ = nullptr;
        rankingCaptionElement_ = nullptr;
        rankingPanelElement_ = nullptr;
        rankingListElement_ = nullptr;
        summaryContinueButtonElement_ = nullptr;
        rewardTitleElement_ = nullptr;
        rewardSubtitleElement_ = nullptr;
        rewardStepCopyElement_ = nullptr;
        rewardTrackElement_ = nullptr;
        rewardGridElement_ = nullptr;
        rewardHintElement_ = nullptr;
        rewardReturnButtonElement_ = nullptr;
        rewardContinueButtonElement_ = nullptr;
        transitionScreenElement_ = nullptr;
        transitionKickerElement_ = nullptr;
        transitionTitleElement_ = nullptr;
        transitionCopyElement_ = nullptr;
        transitionRibbonLabelElement_ = nullptr;
        flashOverlayElement_ = nullptr;
        signalStripElement_ = nullptr;
        signalDotElement_ = nullptr;
        rankingFillElements_.clear();
        rankingShareElements_.clear();
        rankingTotalElements_.clear();
        cardElements_.clear();
        uiMusicBars_ = graphics::UiMusicBarStrip{};
        uiMusicVisualState_ = game::audio::UiMusicVisualState{};
        loadingOverlay_.shutdown();
        loadingOverlayState_ = graphics::RmlUiLoadingOverlayState{};

        if (context_ != nullptr) {
            context_->UnloadAllDocuments();
            Rml::RemoveContext("post-battle");
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
        if (!initialized_ || context_ == nullptr || window_ == nullptr || finished_) {
            return;
        }

        if (event.type == SDL_WINDOWEVENT &&
            (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             event.window.event == SDL_WINDOWEVENT_RESIZED)) {
            updateViewportFromWindow();
            renderInterface_->SetViewport(drawableWidth_, drawableHeight_);
            context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
            applyContextScale();
            refreshView();
        }

        SDL_Event mutableEvent = event;
        RmlSDL::InputEventHandler(context_, window_, mutableEvent);

        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            switch (currentView_) {
                case View::Summary:
                    handleSummaryKey(event.key.keysym.sym);
                    break;
                case View::Reward:
                    handleRewardKey(event.key.keysym.sym);
                    break;
            }
        }
    }

    void update(float deltaSeconds) {
        if (!initialized_ || context_ == nullptr || finished_) {
            return;
        }

        if (flashTimer_ > 0.0f) {
            flashTimer_ = std::max(0.0f, flashTimer_ - std::max(deltaSeconds, 0.0f));
            if (flashTimer_ <= 0.0f && flashOverlayElement_ != nullptr) {
                flashOverlayElement_->SetClass("is-active", false);
            }
        }

        if (summaryTickCooldown_ > 0.0f) {
            summaryTickCooldown_ = std::max(0.0f, summaryTickCooldown_ - std::max(deltaSeconds, 0.0f));
        }

        if (rewardPulseTimer_ > 0.0f) {
            rewardPulseTimer_ = std::max(0.0f, rewardPulseTimer_ - std::max(deltaSeconds, 0.0f));
            if (rewardPulseTimer_ <= 0.0f) {
                pulsingCharacterIndex_ = -1;
                updateRewardSelectionClasses();
            }
        }

        if (!signalArmed_ && signalDelayRemaining_ > 0.0f) {
            signalDelayRemaining_ = std::max(0.0f, signalDelayRemaining_ - std::max(deltaSeconds, 0.0f));
            if (signalDelayRemaining_ <= 0.0f) {
                signalArmed_ = true;
                updateSignalState();
                playOneShot(signalSfxPath_, 0.64f, true);
            }
        }

        updateSummaryAnimation(deltaSeconds);
        sfxPlayer_.cleanupFinishedPlayback();
        context_->Update();
    }

    void render() {
        if (!initialized_ ||
            (finished_ && !loadingOverlayState_.visible) ||
            context_ == nullptr ||
            renderInterface_ == nullptr ||
            window_ == nullptr) {
            return;
        }

        SDL_GL_MakeCurrent(window_, glContext_);
        glViewport(0, 0, drawableWidth_, drawableHeight_);
        glClearColor(0.972f, 0.949f, 0.961f, 1.0f);
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

    bool isFinished() const {
        return finished_;
    }

    const PlayerProgression& progression() const {
        return progression_;
    }

private:
    enum class View {
        Summary,
        Reward
    };

    enum class FooterSelection {
        Return,
        Continue
    };

    struct EventListenerBinding {
        Rml::Element* element = nullptr;
        Rml::EventId eventId = Rml::EventId::Invalid;
        bool capturePhase = false;
        std::unique_ptr<Rml::EventListener> listener;
    };

    struct PartyEntry {
        CharacterDefinition baseCharacter;
        std::string iconImagePath;
        int assignedCount = 0;
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
        hoverSfxPath_ = platform::path::resolvePath(kHoverSfxRelativePath);
        selectSfxPath_ = platform::path::resolvePath(kSelectSfxRelativePath);
        panelSfxPath_ = platform::path::resolvePath(kPanelSfxRelativePath);
        tickSfxPath_ = platform::path::resolvePath(kTickSfxRelativePath);
        signalSfxPath_ = platform::path::resolvePath(kSignalSfxRelativePath);

        if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
                std::cerr << "[PostBattle] Audio init failed: " << SDL_GetError() << "\n";
                hoverSfxPath_.clear();
                selectSfxPath_.clear();
                panelSfxPath_.clear();
                tickSfxPath_.clear();
                signalSfxPath_.clear();
                audioReady_ = false;
                return false;
            }
        }

        audioReady_ = !hoverSfxPath_.empty() ||
                      !selectSfxPath_.empty() ||
                      !panelSfxPath_.empty() ||
                      !tickSfxPath_.empty() ||
                      !signalSfxPath_.empty();
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

    bool loadPartyData(const std::vector<std::string>& partyLineup) {
        partyEntries_.clear();
        rankingRows_.clear();
        pendingAllocation_.clear();

        for (const std::string& characterKey : partyLineup) {
            CharacterDefinition character;
            if (!loader::loadCharacterDefinition(characterKey, character)) {
                continue;
            }

            applyCharacterProgressionBonuses(character, progression_, battleDefinition_.key);
            PartyEntry entry;
            entry.baseCharacter = character;
            entry.iconImagePath = resolveRuntimeImagePath("icons", character.assets);
            if (entry.iconImagePath.empty()) {
                entry.iconImagePath = resolveRuntimeImagePath("sprites", character.assets);
            }
            partyEntries_.push_back(std::move(entry));
        }

        rankingRows_.reserve(summary_.characters.size());
        const int totalDamage = summary_.totalDamage();
        for (CharacterAnalytics analytics : summary_.characters) {
            if (analytics.title.empty()) {
                const auto it = std::find_if(partyEntries_.begin(), partyEntries_.end(), [&](const PartyEntry& entry) {
                    return entry.baseCharacter.key == analytics.key;
                });
                if (it != partyEntries_.end()) {
                    analytics.title = it->baseCharacter.title;
                }
            }

            RankingRowState row;
            row.analytics = std::move(analytics);
            row.sharePercent = totalDamage > 0
                ? static_cast<int>(std::lround(
                    static_cast<double>(row.analytics.totalDamage) /
                    static_cast<double>(totalDamage) * 100.0))
                : 0;
            rankingRows_.push_back(std::move(row));
        }

        std::sort(rankingRows_.begin(), rankingRows_.end(), [](const RankingRowState& lhs, const RankingRowState& rhs) {
            if (lhs.analytics.totalDamage != rhs.analytics.totalDamage) {
                return lhs.analytics.totalDamage > rhs.analytics.totalDamage;
            }
            return lhs.analytics.title < rhs.analytics.title;
        });

        if (!partyEntries_.empty()) {
            selectedCharacterIndex_ = std::clamp(selectedCharacterIndex_, 0, static_cast<int>(partyEntries_.size()) - 1);
        } else {
            selectedCharacterIndex_ = 0;
        }

        return true;
    }

    bool loadDocument() {
        if (context_ == nullptr) {
            return false;
        }

        const std::string documentPath = platform::path::resolvePath(kDocumentPath);
        document_ = context_->LoadDocument(documentPath);
        if (document_ == nullptr) {
            std::cerr << "[PostBattle] Failed to load document: " << documentPath << "\n";
            return false;
        }

        document_->Show();
        uiMusicBars_ = graphics::cacheUiMusicBarStrip(*document_);
        graphics::applyUiMusicBarStrip(uiMusicBars_, uiMusicVisualState_);
        cacheElements();
        refreshView();
        return true;
    }

    void cacheElements() {
        if (document_ == nullptr) {
            return;
        }

        summaryScreenElement_ = document_->GetElementById("summary-screen");
        rewardScreenElement_ = document_->GetElementById("reward-screen");
        summaryEyebrowElement_ = document_->GetElementById("summary-eyebrow");
        summaryTitleElement_ = document_->GetElementById("summary-title");
        metricTotalDamageElement_ = document_->GetElementById("metric-total-damage");
        metricAvElement_ = document_->GetElementById("metric-av-spent");
        metricDpaElement_ = document_->GetElementById("metric-dmg-per-av");
        metricTotalFootElement_ = document_->GetElementById("metric-total-foot");
        metricAvFootElement_ = document_->GetElementById("metric-av-foot");
        metricDpaFootElement_ = document_->GetElementById("metric-dpa-foot");
        rankingCaptionElement_ = document_->GetElementById("ranking-caption");
        rankingPanelElement_ = document_->GetElementById("ranking-panel");
        rankingListElement_ = document_->GetElementById("ranking-list");
        summaryContinueButtonElement_ = document_->GetElementById("summary-continue");
        rewardTitleElement_ = document_->GetElementById("reward-title");
        rewardSubtitleElement_ = document_->GetElementById("reward-subtitle");
        rewardStepCopyElement_ = document_->GetElementById("reward-step-copy");
        rewardTrackElement_ = document_->GetElementById("reward-track");
        rewardGridElement_ = document_->GetElementById("reward-grid");
        rewardHintElement_ = document_->GetElementById("reward-hint");
        rewardReturnButtonElement_ = document_->GetElementById("reward-return");
        rewardContinueButtonElement_ = document_->GetElementById("reward-continue");
        transitionScreenElement_ = document_->GetElementById("transition-screen");
        transitionKickerElement_ = document_->GetElementById("transition-kicker");
        transitionTitleElement_ = document_->GetElementById("transition-title");
        transitionCopyElement_ = document_->GetElementById("transition-copy");
        transitionRibbonLabelElement_ = document_->GetElementById("transition-ribbon-label");
        flashOverlayElement_ = document_->GetElementById("flash-overlay");
        signalStripElement_ = document_->GetElementById("signal-strip");
        signalDotElement_ = document_->GetElementById("signal-dot");
    }

    void refreshView() {
        if (document_ == nullptr) {
            return;
        }

        rebuildSummaryMarkup();
        rebuildRewardTrack();
        rebuildRewardGrid();
        rebuildRewardHeader();
        rebuildTransitionMarkup();
        applyScreenState();
        attachEventListeners();
    }

    void attachEventListeners() {
        detachEventListeners();

        const auto attachClick = [this](Rml::Element* element, std::function<void()> callback) {
            if (element == nullptr) {
                return;
            }
            auto listener = std::make_unique<CallbackEventListener>(
                [callback = std::move(callback)](Rml::Event&) { callback(); });
            element->AddEventListener(Rml::EventId::Click, listener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Click,
                false,
                std::move(listener),
            });
        };

        const auto attachHover = [this](Rml::Element* element, const std::string& hoverId, std::function<void()> callback) {
            if (element == nullptr) {
                return;
            }
            auto listener = std::make_unique<CallbackEventListener>(
                [this, hoverId, callback = std::move(callback)](Rml::Event&) {
                    if (lastHoveredButtonId_ != hoverId) {
                        playHoverSfx();
                        lastHoveredButtonId_ = hoverId;
                    }
                    callback();
                });
            element->AddEventListener(Rml::EventId::Mouseover, listener.get());
            listeners_.push_back(EventListenerBinding{
                element,
                Rml::EventId::Mouseover,
                false,
                std::move(listener),
            });
        };

        attachClick(summaryContinueButtonElement_, [this]() { activateSummaryContinue(); });
        attachHover(summaryContinueButtonElement_, "summary-continue", [this]() {
            if (currentView_ == View::Summary) {
                applyScreenState();
            }
        });

        attachClick(rewardReturnButtonElement_, [this]() {
            if (currentView_ != View::Reward) {
                return;
            }
            rewardFooterFocused_ = true;
            footerSelection_ = FooterSelection::Return;
            playSelectSfx();
            enterSummary(true);
        });
        attachHover(rewardReturnButtonElement_, "reward-return", [this]() {
            if (currentView_ == View::Reward) {
                rewardFooterFocused_ = true;
                footerSelection_ = FooterSelection::Return;
                applyScreenState();
            }
        });

        attachClick(rewardContinueButtonElement_, [this]() {
            if (currentView_ != View::Reward) {
                return;
            }
            if (rewardsAssigned() >= kRewardPickCount) {
                rewardFooterFocused_ = true;
                footerSelection_ = FooterSelection::Continue;
                playSelectSfx();
                completeSession();
            }
        });
        attachHover(rewardContinueButtonElement_, "reward-continue", [this]() {
            if (currentView_ == View::Reward && rewardsAssigned() >= kRewardPickCount) {
                rewardFooterFocused_ = true;
                footerSelection_ = FooterSelection::Continue;
                applyScreenState();
            }
        });

        cardElements_.clear();
        if (rewardGridElement_ == nullptr) {
            return;
        }

        cardElements_.reserve(partyEntries_.size());
        for (std::size_t i = 0; i < partyEntries_.size(); ++i) {
            Rml::Element* element = document_->GetElementById("reward-card-" + std::to_string(i));
            cardElements_.push_back(element);
            if (element == nullptr) {
                continue;
            }

            attachClick(element, [this, i]() {
                if (currentView_ != View::Reward) {
                    return;
                }
                selectCharacter(static_cast<int>(i), false);
                if (rewardsAssigned() < kRewardPickCount) {
                    assignSelectedReward();
                }
            });
            attachHover(element, "reward-card-" + std::to_string(i), [this, i]() {
                if (currentView_ == View::Reward && rewardsAssigned() < kRewardPickCount) {
                    selectCharacter(static_cast<int>(i), true);
                }
            });
        }

        updateRewardSelectionClasses();
    }

    void rebuildSummaryMarkup() {
        if (summaryEyebrowElement_ != nullptr) {
            summaryEyebrowElement_->SetInnerRML(escapeRml(
                battleDefinition_.name.empty()
                    ? std::string("Post Combat Analysis / Trial Record")
                    : "Post Combat Analysis / " + battleDefinition_.name));
        }

        if (summaryTitleElement_ != nullptr) {
            summaryTitleElement_->SetInnerRML(escapeRml(victory_ ? "Combat Dossier" : "Failure Dossier"));
        }

        if (metricTotalFootElement_ != nullptr) {
            metricTotalFootElement_->SetInnerRML(
                std::to_string(summary_.characters.size()) +
                " active unit" + (summary_.characters.size() == 1 ? "" : "s"));
        }
        if (metricAvFootElement_ != nullptr) {
            metricAvFootElement_->SetInnerRML("Action value spent this fight");
        }
        if (metricDpaFootElement_ != nullptr) {
            metricDpaFootElement_->SetInnerRML("Damage efficiency snapshot");
        }
        if (rankingCaptionElement_ != nullptr) {
            rankingCaptionElement_->SetInnerRML("Sorted by total dealt");
        }

        const int rowsToRender = std::max(4, static_cast<int>(rankingRows_.size()));
        if (rankingPanelElement_ != nullptr) {
            rankingPanelElement_->SetProperty("height", formatDp(110.0f + static_cast<float>(rowsToRender) * 36.0f));
        }
        if (rankingListElement_ != nullptr) {
            rankingListElement_->SetProperty("height", formatDp(static_cast<float>(rowsToRender) * 36.0f));
        }

        if (rankingListElement_ == nullptr) {
            return;
        }

        std::ostringstream markup;
        for (int i = 0; i < rowsToRender; ++i) {
            markup << "<div class=\"ranking-row\" style=\"top:" << formatDp(static_cast<float>(i) * 36.0f) << ";\">";
            if (i < static_cast<int>(rankingRows_.size())) {
                const RankingRowState& row = rankingRows_[static_cast<std::size_t>(i)];
                markup << "<div class=\"ranking-rank\">#" << (i + 1) << "</div>"
                       << "<div class=\"ranking-name\">" << escapeRml(row.analytics.title.empty() ? row.analytics.key : row.analytics.title) << "</div>"
                       << "<div class=\"ranking-bar-shell\"><div class=\"ranking-bar-fill row-" << std::min(i, 4)
                       << "\" id=\"ranking-fill-" << i << "\"></div></div>"
                       << "<div class=\"ranking-share\" id=\"ranking-share-" << i << "\">0%</div>"
                       << "<div class=\"ranking-total\" id=\"ranking-total-" << i << "\">0</div>";
            } else {
                markup << "<div class=\"ranking-rank\">--</div>"
                       << "<div class=\"ranking-name empty\">EMPTY</div>"
                       << "<div class=\"ranking-bar-shell\"></div>"
                       << "<div class=\"ranking-share\">0%</div>"
                       << "<div class=\"ranking-total\">0</div>";
            }
            markup << "</div>";
        }

        rankingListElement_->SetInnerRML(markup.str());
        rankingFillElements_.clear();
        rankingShareElements_.clear();
        rankingTotalElements_.clear();
        rankingFillElements_.reserve(rankingRows_.size());
        rankingShareElements_.reserve(rankingRows_.size());
        rankingTotalElements_.reserve(rankingRows_.size());
        for (std::size_t i = 0; i < rankingRows_.size(); ++i) {
            rankingFillElements_.push_back(document_->GetElementById("ranking-fill-" + std::to_string(i)));
            rankingShareElements_.push_back(document_->GetElementById("ranking-share-" + std::to_string(i)));
            rankingTotalElements_.push_back(document_->GetElementById("ranking-total-" + std::to_string(i)));
        }
    }

    void rebuildRewardTrack() {
        if (rewardTrackElement_ == nullptr) {
            return;
        }

        const int assigned = rewardsAssigned();
        const float fillWidth = (static_cast<float>(assigned) / static_cast<float>(kRewardPickCount)) * 772.0f;
        const float dotLeft = 254.0f + fillWidth;

        std::ostringstream markup;
        for (int i = 0; i < kRewardPickCount; ++i) {
            const float tokenLeft = 147.0f + static_cast<float>(i) * 257.0f;
            markup << "<div class=\"reward-token";
            if (i < assigned) {
                markup << " is-complete";
            } else if (i == assigned) {
                markup << " is-current";
            }
            markup << "\" style=\"left:" << formatDp(tokenLeft) << ";\">"
                   << "<div class=\"reward-token-label\">Bloom " << (i + 1) << "</div></div>";
        }
        markup << "<div class=\"reward-progress-rail\">"
               << "<div class=\"reward-progress-fill\" style=\"width:" << formatDp(fillWidth) << ";\"></div>"
               << "<div class=\"reward-progress-dot\" style=\"left:" << formatDp(dotLeft) << ";\"></div>"
               << "</div>";
        rewardTrackElement_->SetInnerRML(markup.str());
    }

    void rebuildRewardGrid() {
        if (rewardGridElement_ == nullptr) {
            return;
        }

        std::ostringstream markup;
        const int count = static_cast<int>(partyEntries_.size());
        const float gap = count >= 4 ? 24.0f : (count == 3 ? 28.0f : 36.0f);
        const float cardWidth =
            count <= 1 ? 560.0f :
            count == 2 ? 530.0f :
            count == 3 ? 346.0f :
            256.0f;
        const float totalWidth = cardWidth * static_cast<float>(std::max(count, 1)) +
                                 gap * static_cast<float>(std::max(0, count - 1));
        const float leftStart = (1096.0f - totalWidth) * 0.5f;

        for (int i = 0; i < count; ++i) {
            const PartyEntry& entry = partyEntries_[static_cast<std::size_t>(i)];
            const CharacterDefinition current = displayedCharacter(entry);
            const float left = leftStart + static_cast<float>(i) * (cardWidth + gap);

            markup << "<button class=\"character-card\" id=\"reward-card-" << i
                   << "\" style=\"left:" << formatDp(left) << ";width:" << formatDp(cardWidth) << ";\">"
                   << "<div class=\"character-shell\"></div>"
                   << "<div class=\"character-accent\"></div>"
                   << "<div class=\"character-inner\">"
                   << "<div class=\"character-topline\">"
                   << "<div class=\"character-header\">"
                   << "<div class=\"character-icon\">";
            if (!entry.iconImagePath.empty()) {
                markup << "<img class=\"character-icon-image\" src=\"" << escapeRml(entry.iconImagePath) << "\"/>";
            } else {
                markup << "<div class=\"character-icon-fallback\">" << escapeRml(initialsForName(current.title)) << "</div>";
            }
            markup << "</div>"
                   << "<div class=\"character-name\">" << escapeRml(current.title) << "</div>"
                   << "</div>"
                   << "<div class=\"buff-badge\">" << entry.assignedCount << "</div>"
                   << "</div>"
                   << statRowMarkup("HP", current.hp, current.hp + battleDefinition_.buffs.hp)
                   << statRowMarkup("ATK", current.atk, current.atk + battleDefinition_.buffs.atk)
                   << statRowMarkup("SPD", current.spd, current.spd + battleDefinition_.buffs.spd)
                   << "</div></button>";
        }

        rewardGridElement_->SetInnerRML(markup.str());
        updateRewardSelectionClasses();
    }

    static std::string statRowMarkup(const std::string& label, int baseValue, int nextValue) {
        std::ostringstream markup;
        markup << "<div class=\"stat-row";
        if (label == "HP") {
            markup << " stat-row-first";
        }
        markup << "\">"
               << "<div class=\"stat-label\">" << label << "</div>"
               << "<div class=\"stat-base\">" << baseValue << "</div>"
               << "<div class=\"stat-arrow\">&gt;</div>"
               << "<div class=\"stat-next\">" << nextValue << "</div>"
               << "</div>";
        return markup.str();
    }

    void rebuildRewardHeader() {
        if (rewardTitleElement_ == nullptr ||
            rewardSubtitleElement_ == nullptr ||
            rewardStepCopyElement_ == nullptr ||
            rewardHintElement_ == nullptr) {
            return;
        }

        const int assigned = rewardsAssigned();
        if (assigned < kRewardPickCount) {
            rewardTitleElement_->SetInnerRML("Allocate Growth");
            rewardSubtitleElement_->SetInnerRML(
                "Choose who receives each all-stat combat bloom. The same character can be picked multiple times. "
                "Practice Mode can re-route a cleared stage to different unlocked units, but it never creates more than four picks.");
            rewardStepCopyElement_->SetInnerRML(
                "Buff " + std::to_string(assigned + 1) + " / " + std::to_string(kRewardPickCount));
            rewardHintElement_->SetInnerRML(
                "Use Left / Right to pick a character, Enter to assign, and Up / Down to move to the footer.");
        } else {
            rewardTitleElement_->SetInnerRML("Allocation Complete");
            rewardSubtitleElement_->SetInnerRML(
                "All four combat blooms have been assigned. Review the upgraded cards, then continue.");
            rewardStepCopyElement_->SetInnerRML(
                std::to_string(kRewardPickCount) + " / " + std::to_string(kRewardPickCount) + " routed");
            rewardHintElement_->SetInnerRML(
                "Use Up / Down to move between cards and footer, then Left / Right to choose a footer action.");
        }
    }

    void rebuildTransitionMarkup() {
        if (transitionKickerElement_ == nullptr ||
            transitionTitleElement_ == nullptr ||
            transitionCopyElement_ == nullptr ||
            transitionRibbonLabelElement_ == nullptr) {
            return;
        }

        transitionKickerElement_->SetInnerRML(rewardAvailable_ ? "Story Link Ready" : "Session Link Ready");
        transitionTitleElement_->SetInnerRML(
            rewardAvailable_ ? "Transition To Story" : "Transition Ready");
        transitionCopyElement_->SetInnerRML(
            rewardAvailable_
                ? "Combat loop complete. Handing back to the linked scene or next story transition."
                : "Combat loop complete. Handing back to the linked scene or practice flow.");
        transitionRibbonLabelElement_->SetInnerRML("Combat Loop Complete");
    }

    void applyScreenState() {
        const auto applyScreenVisibility = [this](Rml::Element* element, bool active) {
            if (element == nullptr) {
                return;
            }
            element->SetProperty("display", active ? "block" : "none");
            element->SetClass("screen-active", active);
        };

        applyScreenVisibility(summaryScreenElement_, currentView_ == View::Summary);
        applyScreenVisibility(rewardScreenElement_, currentView_ == View::Reward);
        applyScreenVisibility(transitionScreenElement_, false);

        if (summaryContinueButtonElement_ != nullptr) {
            summaryContinueButtonElement_->SetClass("is-focused", currentView_ == View::Summary);
        }
        if (rewardReturnButtonElement_ != nullptr) {
            const bool enabled = currentView_ == View::Reward;
            rewardReturnButtonElement_->SetClass("is-disabled", false);
            rewardReturnButtonElement_->SetClass(
                "is-focused",
                enabled && rewardFooterFocused_ && footerSelection_ == FooterSelection::Return);
        }
        if (rewardContinueButtonElement_ != nullptr) {
            const bool enabled = currentView_ == View::Reward && rewardsAssigned() >= kRewardPickCount;
            rewardContinueButtonElement_->SetClass("is-disabled", !enabled);
            rewardContinueButtonElement_->SetClass(
                "is-focused",
                enabled && rewardFooterFocused_ && footerSelection_ == FooterSelection::Continue);
        }

        updateSignalState();
        updateSummaryDisplay();
        updateRewardSelectionClasses();
    }

    void updateSignalState() {
        if (signalStripElement_ != nullptr) {
            signalStripElement_->SetClass("is-armed", signalArmed_);
        }
        if (signalDotElement_ != nullptr) {
            signalDotElement_->SetClass("is-armed", signalArmed_);
        }
    }

    void updateRewardSelectionClasses() {
        for (std::size_t i = 0; i < cardElements_.size(); ++i) {
            if (cardElements_[i] == nullptr) {
                continue;
            }

            const bool selectable = currentView_ == View::Reward && !rewardFooterFocused_;
            cardElements_[i]->SetClass("is-selected", selectable && static_cast<int>(i) == selectedCharacterIndex_);
            cardElements_[i]->SetClass("is-pulsing", static_cast<int>(i) == pulsingCharacterIndex_);
        }
    }

    void updateSummaryDisplay() {
        if (metricTotalDamageElement_ != nullptr) {
            metricTotalDamageElement_->SetInnerRML(formatIntegerWithCommas(currentSummaryDamage_));
        }
        if (metricAvElement_ != nullptr) {
            metricAvElement_->SetInnerRML(formatIntegerWithCommas(currentSummaryAv_));
        }
        if (metricDpaElement_ != nullptr) {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1)
                   << static_cast<double>(currentSummaryDpaTenths_) / 10.0;
            metricDpaElement_->SetInnerRML(stream.str());
        }

        const int totalDamage = std::max(summary_.totalDamage(), 1);
        for (std::size_t i = 0; i < rankingRows_.size(); ++i) {
            const float rowElapsed = std::max(0.0f, summaryIntroElapsed_ - static_cast<float>(i) * kSummaryRowStaggerSeconds);
            const float rowProgress = summaryIntroActive_
                ? easeOutCubic(rowElapsed / kSummaryRowDurationSeconds)
                : 1.0f;
            const int animatedDamage = static_cast<int>(std::lround(
                static_cast<float>(rankingRows_[i].analytics.totalDamage) * rowProgress));
            const int animatedShare = static_cast<int>(std::lround(
                static_cast<float>(rankingRows_[i].sharePercent) * rowProgress));
            const float fullWidth =
                static_cast<float>(rankingRows_[i].analytics.totalDamage) /
                static_cast<float>(totalDamage) * 514.0f;

            if (i < rankingTotalElements_.size() && rankingTotalElements_[i] != nullptr) {
                rankingTotalElements_[i]->SetInnerRML(formatIntegerWithCommas(animatedDamage));
            }
            if (i < rankingShareElements_.size() && rankingShareElements_[i] != nullptr) {
                rankingShareElements_[i]->SetInnerRML(std::to_string(animatedShare) + "%");
            }
            if (i < rankingFillElements_.size() && rankingFillElements_[i] != nullptr) {
                rankingFillElements_[i]->SetProperty("width", formatDp(fullWidth * rowProgress));
            }
        }
    }

    void updateSummaryAnimation(float deltaSeconds) {
        if (!summaryIntroActive_) {
            return;
        }

        summaryIntroElapsed_ += std::max(deltaSeconds, 0.0f);
        const int previousDamage = currentSummaryDamage_;
        const int previousAv = currentSummaryAv_;
        const int previousDpa = currentSummaryDpaTenths_;

        const int targetDamage = summary_.totalDamage();
        const int targetAv = std::max(0, static_cast<int>(std::lround(summary_.totalActionValueConsumed)));
        const int targetDpaTenths = targetAv > 0
            ? static_cast<int>(std::lround(
                static_cast<double>(targetDamage) / static_cast<double>(targetAv) * 10.0))
            : 0;
        const float metricProgress = easeOutCubic(summaryIntroElapsed_ / kSummaryMetricsDurationSeconds);

        currentSummaryDamage_ = static_cast<int>(std::lround(static_cast<float>(targetDamage) * metricProgress));
        currentSummaryAv_ = static_cast<int>(std::lround(static_cast<float>(targetAv) * metricProgress));
        currentSummaryDpaTenths_ = static_cast<int>(std::lround(static_cast<float>(targetDpaTenths) * metricProgress));

        if ((currentSummaryDamage_ != previousDamage ||
             currentSummaryAv_ != previousAv ||
             currentSummaryDpaTenths_ != previousDpa) &&
            summaryTickCooldown_ <= 0.0f) {
            playOneShot(tickSfxPath_, 0.64f, false);
            summaryTickCooldown_ = kTickCooldownSeconds;
        }

        bool rowsDone = true;
        for (std::size_t i = 0; i < rankingRows_.size(); ++i) {
            const float rowElapsed = std::max(0.0f, summaryIntroElapsed_ - static_cast<float>(i) * kSummaryRowStaggerSeconds);
            const float rowProgress = clamp01(rowElapsed / kSummaryRowDurationSeconds);
            const int animatedDamage = static_cast<int>(std::lround(
                static_cast<float>(rankingRows_[i].analytics.totalDamage) * rowProgress));
            const int animatedShare = static_cast<int>(std::lround(
                static_cast<float>(rankingRows_[i].sharePercent) * rowProgress));
            if (i < summaryRowTickPlayed_.size() &&
                !summaryRowTickPlayed_[i] &&
                rowElapsed > 0.0f) {
                playOneShot(tickSfxPath_, 0.58f, false);
                summaryRowTickPlayed_[i] = true;
            }
            if (animatedDamage < rankingRows_[i].analytics.totalDamage ||
                animatedShare < rankingRows_[i].sharePercent) {
                rowsDone = false;
                break;
            }
        }

        updateSummaryDisplay();
        if (currentSummaryDamage_ >= targetDamage &&
            currentSummaryAv_ >= targetAv &&
            currentSummaryDpaTenths_ >= targetDpaTenths &&
            rowsDone) {
            summaryIntroActive_ = false;
        }
    }

    void handleSummaryKey(SDL_Keycode key) {
        switch (key) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                activateSummaryContinue();
                break;
            case SDLK_ESCAPE:
                if (!rewardAvailable_) {
                    playSelectSfx();
                    completeSession();
                }
                break;
            default:
                break;
        }
    }

    void handleRewardKey(SDL_Keycode key) {
        const bool allRewardsAssigned = rewardsAssigned() >= kRewardPickCount;

        if (key == SDLK_ESCAPE) {
            playSelectSfx();
            enterSummary(true);
            return;
        }

        if (!rewardFooterFocused_) {
            switch (key) {
                case SDLK_LEFT:
                    moveSelectedCharacter(-1);
                    break;
                case SDLK_RIGHT:
                    moveSelectedCharacter(1);
                    break;
                case SDLK_UP:
                case SDLK_DOWN:
                    rewardFooterFocused_ = true;
                    footerSelection_ = allRewardsAssigned ? FooterSelection::Continue : FooterSelection::Return;
                    applyScreenState();
                    playHoverSfx();
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:
                    if (!allRewardsAssigned) {
                        assignSelectedReward();
                    } else {
                        rewardFooterFocused_ = true;
                        footerSelection_ = FooterSelection::Continue;
                        applyScreenState();
                        playHoverSfx();
                    }
                    break;
                default:
                    break;
            }
            return;
        }

        switch (key) {
            case SDLK_LEFT:
                if (allRewardsAssigned && footerSelection_ != FooterSelection::Return) {
                    footerSelection_ = FooterSelection::Return;
                    applyScreenState();
                    playHoverSfx();
                }
                break;
            case SDLK_RIGHT:
                if (allRewardsAssigned && footerSelection_ != FooterSelection::Continue) {
                    footerSelection_ = FooterSelection::Continue;
                    applyScreenState();
                    playHoverSfx();
                }
                break;
            case SDLK_UP:
            case SDLK_DOWN:
                rewardFooterFocused_ = false;
                applyScreenState();
                playHoverSfx();
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (footerSelection_ == FooterSelection::Return) {
                    playSelectSfx();
                    enterSummary(true);
                } else if (allRewardsAssigned) {
                    playSelectSfx();
                    completeSession();
                }
                break;
            default:
                break;
        }
    }

    void activateSummaryContinue() {
        playSelectSfx();
        if (rewardAvailable_) {
            enterReward();
        } else {
            completeSession();
        }
    }

    void moveSelectedCharacter(int step) {
        if (partyEntries_.empty()) {
            return;
        }

        int nextIndex = selectedCharacterIndex_ + step;
        if (nextIndex < 0) {
            nextIndex = static_cast<int>(partyEntries_.size()) - 1;
        } else if (nextIndex >= static_cast<int>(partyEntries_.size())) {
            nextIndex = 0;
        }
        if (nextIndex == selectedCharacterIndex_) {
            return;
        }

        selectedCharacterIndex_ = nextIndex;
        rewardFooterFocused_ = false;
        applyScreenState();
        playHoverSfx();
    }

    void selectCharacter(int index, bool hovered) {
        if (index < 0 || index >= static_cast<int>(partyEntries_.size())) {
            return;
        }
        if (selectedCharacterIndex_ == index && !rewardFooterFocused_) {
            return;
        }

        selectedCharacterIndex_ = index;
        rewardFooterFocused_ = false;
        applyScreenState();
        if (hovered) {
            playHoverSfx();
        }
    }

    void assignSelectedReward() {
        if (!rewardAvailable_ ||
            selectedCharacterIndex_ < 0 ||
            selectedCharacterIndex_ >= static_cast<int>(partyEntries_.size()) ||
            rewardsAssigned() >= kRewardPickCount) {
            return;
        }

        PartyEntry& entry = partyEntries_[static_cast<std::size_t>(selectedCharacterIndex_)];
        ++entry.assignedCount;
        pendingAllocation_[entry.baseCharacter.key] += 1;
        rewardFooterFocused_ = false;
        footerSelection_ = FooterSelection::Return;
        pulsingCharacterIndex_ = selectedCharacterIndex_;
        rewardPulseTimer_ = kRewardPulseDurationSeconds;

        if (rewardsAssigned() >= kRewardPickCount) {
            rewardFooterFocused_ = true;
            footerSelection_ = FooterSelection::Continue;
        }

        rebuildRewardTrack();
        rebuildRewardHeader();
        rebuildRewardGrid();
        attachEventListeners();
        applyScreenState();
        playSelectSfx();
    }

    void enterSummary(bool flash) {
        currentView_ = View::Summary;
        rewardFooterFocused_ = false;
        currentSummaryDamage_ = 0;
        currentSummaryAv_ = 0;
        currentSummaryDpaTenths_ = 0;
        summaryIntroElapsed_ = 0.0f;
        summaryIntroActive_ = true;
        summaryRowTickPlayed_.assign(rankingRows_.size(), false);
        signalArmed_ = false;
        signalDelayRemaining_ = 0.0f;
        updateSignalState();
        applyScreenState();
        if (flash) {
            triggerFlash();
        }
        playOneShot(panelSfxPath_, 0.72f, true);
    }

    void enterReward() {
        if (!rewardAvailable_) {
            completeSession();
            return;
        }

        currentView_ = View::Reward;
        rewardFooterFocused_ = false;
        signalArmed_ = false;
        signalDelayRemaining_ = kSignalDelaySeconds;
        updateSignalState();
        rebuildRewardHeader();
        rebuildRewardTrack();
        rebuildRewardGrid();
        attachEventListeners();
        applyScreenState();
        triggerFlash();
        playOneShot(panelSfxPath_, 0.72f, true);
    }

    void completeSession() {
        if (rewardAvailable_) {
            replaceStageBuffAllocation(progression_, battleDefinition_.key, pendingAllocation_);
        }
        finished_ = true;
    }

    int rewardsAssigned() const {
        int total = 0;
        for (const PartyEntry& entry : partyEntries_) {
            total += entry.assignedCount;
        }
        return total;
    }

    CharacterDefinition displayedCharacter(const PartyEntry& entry) const {
        CharacterDefinition displayed = entry.baseCharacter;
        displayed.hp += battleDefinition_.buffs.hp * entry.assignedCount;
        displayed.atk += battleDefinition_.buffs.atk * entry.assignedCount;
        displayed.spd += battleDefinition_.buffs.spd * entry.assignedCount;
        return displayed;
    }

    void triggerFlash() {
        flashTimer_ = kFlashDurationSeconds;
        if (flashOverlayElement_ != nullptr) {
            flashOverlayElement_->SetClass("is-active", true);
        }
    }

    void playHoverSfx() {
        playOneShot(hoverSfxPath_, 0.74f, true);
    }

    void playSelectSfx() {
        playOneShot(selectSfxPath_, 0.92f, true);
    }

    void playOneShot(const std::string& path, float volume, bool replaceExisting) {
        if (!audioReady_ || path.empty()) {
            return;
        }
        (void)sfxPlayer_.playWavOneShot(path, volume, replaceExisting);
    }

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    int windowWidth_ = kReferenceWidth;
    int windowHeight_ = kReferenceHeight;
    int drawableWidth_ = kReferenceWidth;
    int drawableHeight_ = kReferenceHeight;
    bool initialized_ = false;
    bool finished_ = false;
    bool victory_ = false;
    bool rewardAvailable_ = false;
    bool rmlInitialized_ = false;
    bool rmlGlInitialized_ = false;
    bool audioReady_ = false;

    BattleDefinition battleDefinition_{};
    Summary summary_{};
    PlayerProgression progression_{};
    std::vector<PartyEntry> partyEntries_;
    StageBuffAllocation pendingAllocation_;
    std::vector<RankingRowState> rankingRows_;

    View currentView_ = View::Summary;
    FooterSelection footerSelection_ = FooterSelection::Continue;
    bool rewardFooterFocused_ = false;
    int selectedCharacterIndex_ = 0;
    float flashTimer_ = 0.0f;
    float summaryIntroElapsed_ = 0.0f;
    bool summaryIntroActive_ = false;
    float summaryTickCooldown_ = 0.0f;
    float signalDelayRemaining_ = 0.0f;
    bool signalArmed_ = false;
    float rewardPulseTimer_ = 0.0f;
    int pulsingCharacterIndex_ = -1;
    int currentSummaryDamage_ = 0;
    int currentSummaryAv_ = 0;
    int currentSummaryDpaTenths_ = 0;
    std::string lastHoveredButtonId_;
    std::vector<bool> summaryRowTickPlayed_;

    std::unique_ptr<graphics::RmlUiSdlGlRenderInterface> renderInterface_;
    SystemInterface_SDL systemInterface_{};
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;

    Rml::Element* summaryScreenElement_ = nullptr;
    Rml::Element* rewardScreenElement_ = nullptr;
    Rml::Element* summaryEyebrowElement_ = nullptr;
    Rml::Element* summaryTitleElement_ = nullptr;
    Rml::Element* metricTotalDamageElement_ = nullptr;
    Rml::Element* metricAvElement_ = nullptr;
    Rml::Element* metricDpaElement_ = nullptr;
    Rml::Element* metricTotalFootElement_ = nullptr;
    Rml::Element* metricAvFootElement_ = nullptr;
    Rml::Element* metricDpaFootElement_ = nullptr;
    Rml::Element* rankingCaptionElement_ = nullptr;
    Rml::Element* rankingPanelElement_ = nullptr;
    Rml::Element* rankingListElement_ = nullptr;
    Rml::Element* summaryContinueButtonElement_ = nullptr;
    Rml::Element* rewardTitleElement_ = nullptr;
    Rml::Element* rewardSubtitleElement_ = nullptr;
    Rml::Element* rewardStepCopyElement_ = nullptr;
    Rml::Element* rewardTrackElement_ = nullptr;
    Rml::Element* rewardGridElement_ = nullptr;
    Rml::Element* rewardHintElement_ = nullptr;
    Rml::Element* rewardReturnButtonElement_ = nullptr;
    Rml::Element* rewardContinueButtonElement_ = nullptr;
    Rml::Element* transitionScreenElement_ = nullptr;
    Rml::Element* transitionKickerElement_ = nullptr;
    Rml::Element* transitionTitleElement_ = nullptr;
    Rml::Element* transitionCopyElement_ = nullptr;
    Rml::Element* transitionRibbonLabelElement_ = nullptr;
    Rml::Element* flashOverlayElement_ = nullptr;
    Rml::Element* signalStripElement_ = nullptr;
    Rml::Element* signalDotElement_ = nullptr;
    std::vector<Rml::Element*> rankingFillElements_;
    std::vector<Rml::Element*> rankingShareElements_;
    std::vector<Rml::Element*> rankingTotalElements_;
    std::vector<Rml::Element*> cardElements_;

    graphics::UiMusicBarStrip uiMusicBars_{};
    game::audio::UiMusicVisualState uiMusicVisualState_{};
    graphics::RmlUiLoadingOverlay loadingOverlay_{};
    graphics::RmlUiLoadingOverlayState loadingOverlayState_{};

    game::audio::WavOneShotPlayer sfxPlayer_{};
    std::string hoverSfxPath_;
    std::string selectSfxPath_;
    std::string panelSfxPath_;
    std::string tickSfxPath_;
    std::string signalSfxPath_;
};

Session::Session()
    : impl_(std::make_unique<SessionImpl>()) {}

Session::~Session() = default;

bool Session::initialize(Window& window,
                         const BattleDefinition& battleDefinition,
                         bool victory,
                         const Summary& summary,
                         const PlayerProgression& progression,
                         const std::vector<std::string>& partyLineup) {
    return impl_->initialize(window, battleDefinition, victory, summary, progression, partyLineup);
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

bool Session::isFinished() const {
    return impl_->isFinished();
}

const PlayerProgression& Session::progression() const {
    return impl_->progression();
}

}  // namespace battle::postbattle
