#define GL_GLEXT_PROTOTYPES

#include "boss_selector_session.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "audio/bgm_player.h"
#include "core/battle_loader.h"
#include "core/easing.h"
#include "core/player_progression.h"
#include "save/save.h"
#include "../platform/path_resolution.h"
#include "../window.h"

namespace {

constexpr int kDefaultWindowWidth = 1280;
constexpr int kDefaultWindowHeight = 720;

constexpr float kCardWidth = 248.0f;
constexpr float kCardBaseY = 158.0f;
constexpr float kCardSpacing = 252.0f;
constexpr float kCardDepthY = 22.0f;
constexpr float kCardIntroStaggerSeconds = 0.08f;
constexpr float kCardIntroDurationSeconds = 0.48f;
constexpr float kCarouselLerpSpeed = 10.0f;
constexpr float kDetailIntroDurationSeconds = 0.55f;
constexpr float kToastDurationSeconds = 1.8f;
constexpr float kPreviewFadeInPerSecond = 3.8f;
constexpr float kPreviewFadeOutPerSecond = 7.0f;

struct SelectorEntry {
    battle::BattleDefinition battle;
    battle::BossDefinition boss;
    std::string spritePath;
    std::string previewBgmPath;
    std::vector<std::string> lineupTitles;
    bool defeated = false;
};

enum class FocusZone {
    Carousel,
    Finale
};

struct PreviewChannel {
    game::audio::BgmPlayer player;
    std::string path;
    float volume = 0.0f;
    float targetVolume = 0.0f;
    float maxVolume = 0.0f;
    bool active = false;
};

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

class RenderInterfaceGL3SDL final : public RenderInterface_GL3 {
public:
    Rml::TextureHandle LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) override {
#ifdef BATTLE_ENABLE_IMAGE
        Rml::FileInterface* fileInterface = Rml::GetFileInterface();
        Rml::FileHandle fileHandle = fileInterface->Open(source);
        if (!fileHandle) {
            return {};
        }

        fileInterface->Seek(fileHandle, 0, SEEK_END);
        const size_t bufferSize = fileInterface->Tell(fileHandle);
        fileInterface->Seek(fileHandle, 0, SEEK_SET);

        using Rml::byte;
        Rml::UniquePtr<byte[]> buffer(new byte[bufferSize]);
        fileInterface->Read(buffer.get(), bufferSize, fileHandle);
        fileInterface->Close(fileHandle);

        const size_t extIndex = source.rfind('.');
        const Rml::String extension = (extIndex == Rml::String::npos ? Rml::String() : source.substr(extIndex + 1));

        SDL_Surface* surface = IMG_LoadTyped_RW(SDL_RWFromMem(buffer.get(), static_cast<int>(bufferSize)), 1, extension.c_str());
        if (surface == nullptr) {
            Rml::Log::Message(Rml::Log::LT_ERROR, "Could not load texture: %s", source.c_str());
            return {};
        }

        if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
            SDL_Surface* convertedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(surface);
            if (convertedSurface == nullptr) {
                return {};
            }
            surface = convertedSurface;
        }

        textureDimensions = {surface->w, surface->h};

        byte* pixels = static_cast<byte*>(surface->pixels);
        const size_t pixelBytes = static_cast<size_t>(surface->w) * static_cast<size_t>(surface->h) * 4;
        for (size_t i = 0; i < pixelBytes; i += 4) {
            const byte alpha = pixels[i + 3];
            pixels[i + 0] = byte((int(pixels[i + 0]) * int(alpha)) / 255);
            pixels[i + 1] = byte((int(pixels[i + 1]) * int(alpha)) / 255);
            pixels[i + 2] = byte((int(pixels[i + 2]) * int(alpha)) / 255);
        }

        const Rml::TextureHandle textureHandle = GenerateTexture({pixels, pixelBytes}, textureDimensions);
        SDL_FreeSurface(surface);
        return textureHandle;
#else
        return RenderInterface_GL3::LoadTexture(textureDimensions, source);
#endif
    }
};

bool isWindowResizeEvent(const SDL_Event& event) {
    return event.type == SDL_WINDOWEVENT &&
           (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            event.window.event == SDL_WINDOWEVENT_RESIZED);
}

std::string normalizeRmlAssetPath(const std::string& path) {
    if (path.empty()) {
        return {};
    }

    std::error_code ec;
    const std::filesystem::path absolutePath = std::filesystem::absolute(std::filesystem::path(path), ec);
    if (!ec && !absolutePath.empty()) {
        return absolutePath.lexically_normal().generic_string();
    }
    return std::filesystem::path(path).lexically_normal().generic_string();
}

std::string resolveBattleSpritePath(const std::string& assetName) {
    if (assetName.empty()) {
        return {};
    }
    return platform::path::findCombatImagePath("sprites", assetName);
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

std::string formatFloat(float value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream << value;
    return stream.str();
}

std::string formatPx(float value) {
    return formatFloat(value) + "px";
}

std::string escapeRcssString(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const char c : text) {
        if (c == '\\' || c == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

std::string makeImageDecorator(const std::string& path) {
    if (path.empty()) {
        return "none";
    }
    return "image(\"" + escapeRcssString(path) + "\" contain)";
}

std::string joinStrings(const std::vector<std::string>& values, const std::string& separator) {
    std::ostringstream stream;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            stream << separator;
        }
        stream << values[i];
    }
    return stream.str();
}

bool containsKey(const std::vector<std::string>& values, const std::string& key) {
    return std::find(values.begin(), values.end(), key) != values.end();
}

bool loadRmlFontIfPresent(const std::string& path, bool fallback = false) {
    if (path.empty() || !std::filesystem::exists(path)) {
        return false;
    }

    const std::string normalizedPath = normalizeRmlAssetPath(path);
    const std::string extension = std::filesystem::path(normalizedPath).extension().string();
    if (extension == ".ttc" || extension == ".otc") {
        bool anyLoaded = false;
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            if (!Rml::LoadFontFace(normalizedPath, fallback, Rml::Style::FontWeight::Auto, faceIndex)) {
                if (faceIndex == 0 && !anyLoaded) {
                    return false;
                }
                break;
            }
            anyLoaded = true;
        }
        return anyLoaded;
    }

    return Rml::LoadFontFace(normalizedPath, fallback);
}

} // namespace

namespace battle::selector {

class SessionImpl {
public:
    bool initialize(Window& hostWindow) {
        shutdown();

        windowHost_ = &hostWindow;
        window_ = hostWindow.getNativeWindow();
        glContext_ = hostWindow.getGlContext();
        if (window_ == nullptr || glContext_ == nullptr) {
            std::cerr << "[BossSelector] Window is not in OpenGL mode.\n";
            return false;
        }

        if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
            std::cerr << "[BossSelector] SDL subsystem init failed: " << SDL_GetError() << "\n";
            return false;
        }

        SDL_GL_MakeCurrent(window_, glContext_);
        SDL_GL_SetSwapInterval(1);
        SDL_StopTextInput();

#ifdef BATTLE_ENABLE_IMAGE
        const int requiredImageFlags = IMG_INIT_PNG | IMG_INIT_WEBP;
        if ((IMG_Init(requiredImageFlags) & requiredImageFlags) == requiredImageFlags) {
            imageInitialized_ = true;
        } else {
            std::cerr << "[BossSelector] SDL_image init failed: " << IMG_GetError() << "\n";
        }
#endif

        Rml::String glInitMessage;
        if (!RmlGL3::Initialize(&glInitMessage)) {
            std::cerr << "[BossSelector] RmlGL3 initialization failed: " << glInitMessage << "\n";
            shutdown();
            return false;
        }
        rmlGlInitialized_ = true;

        systemInterface_.SetWindow(window_);
        renderInterface_ = std::make_unique<RenderInterfaceGL3SDL>();
        if (!(*renderInterface_)) {
            std::cerr << "[BossSelector] Failed to create render interface\n";
            shutdown();
            return false;
        }

        Rml::SetSystemInterface(&systemInterface_);
        Rml::SetRenderInterface(renderInterface_.get());
        if (!Rml::Initialise()) {
            std::cerr << "[BossSelector] RmlUi core initialization failed\n";
            shutdown();
            return false;
        }
        rmlInitialized_ = true;

        const std::string fontPath = platform::path::findFontPath();
        (void)loadRmlFontIfPresent(fontPath, false);
        const std::string boldFontPath = platform::path::resolvePath("assets/rmlui/DejaVuSans-Bold.ttf");
        (void)loadRmlFontIfPresent(boldFontPath, false);

        const std::string cjkFontPath = platform::path::findCjkFontPath();
        if (!loadRmlFontIfPresent(cjkFontPath, true)) {
            std::cerr << "[BossSelector] Warning: no CJK fallback font found; Chinese text may be missing.\n";
        }

        windowWidth_ = windowHost_->getWidth();
        windowHeight_ = windowHost_->getHeight();
        renderInterface_->SetViewport(windowWidth_, windowHeight_);
        context_ = Rml::CreateContext("boss-selector", Rml::Vector2i(windowWidth_, windowHeight_));
        if (context_ == nullptr) {
            std::cerr << "[BossSelector] Failed to create RmlUi context\n";
            shutdown();
            return false;
        }

        const std::string documentPath = platform::path::resolvePath("assets/rmlui/boss_selector.rml");
        document_ = context_->LoadDocument(documentPath);
        if (document_ == nullptr) {
            std::cerr << "[BossSelector] Failed to load document: " << documentPath << "\n";
            shutdown();
            return false;
        }
        document_->Show();

        if (!loadEntries()) {
            shutdown();
            return false;
        }

        bindDocument();
        rebuildTrack();
        refreshProgressState();
        refreshSelection(true);
        updateAnimatedLayout(0.0f);

        initialized_ = true;
        return true;
    }

    void shutdown() {
        initialized_ = false;
        stopPreviewAudio();

        if (document_ != nullptr) {
            document_->Close();
            document_ = nullptr;
        }
        if (context_ != nullptr) {
            const Rml::String contextName = context_->GetName();
            Rml::RemoveContext(contextName);
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

        Rml::SetRenderInterface(nullptr);
        Rml::SetSystemInterface(nullptr);

        uiListeners_.clear();
        renderInterface_.reset();
        window_ = nullptr;
        glContext_ = nullptr;
        windowHost_ = nullptr;

#ifdef BATTLE_ENABLE_IMAGE
        if (imageInitialized_) {
            IMG_Quit();
            imageInitialized_ = false;
        }
#endif

        launchRequest_.reset();
        entries_.clear();
        cardShells_.clear();
        cardBodies_.clear();
        cardStatuses_.clear();
        cardPortraits_.clear();
        previewActivePath_.clear();
        selectedIndex_ = 0;
        visualIndex_ = 0.0f;
        introElapsed_ = 0.0f;
        toastTimer_ = 0.0f;
        toastText_.clear();
        clearedVisibleCount_ = 0;
        idolRank_ = 0;
        finaleUnlocked_ = false;
        focusZone_ = FocusZone::Carousel;
        trackElement_ = nullptr;
        detailPanelElement_ = nullptr;
        selectorStageElement_ = nullptr;
        selectorHeadingElement_ = nullptr;
        selectorFooterElement_ = nullptr;
        finaleButtonElement_ = nullptr;
        toastElement_ = nullptr;
        detailPortraitElement_ = nullptr;
        detailBossNameElement_ = nullptr;
        detailBattleNameElement_ = nullptr;
        detailDescriptionElement_ = nullptr;
        detailStatusElement_ = nullptr;
        detailLaunchModeElement_ = nullptr;
        detailProgressElement_ = nullptr;
        rankValueElement_ = nullptr;
        rankCaptionElement_ = nullptr;
        finaleSubtitleElement_ = nullptr;
        finished_ = false;
    }

    void handleEvent(const SDL_Event& event) {
        if (!initialized_ || context_ == nullptr || window_ == nullptr) {
            return;
        }

        if (isWindowResizeEvent(event)) {
            windowWidth_ = windowHost_->getWidth();
            windowHeight_ = windowHost_->getHeight();
            renderInterface_->SetViewport(windowWidth_, windowHeight_);
            context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
            updateAnimatedLayout(0.0f);
        }

        SDL_Event mutableEvent = event;
        RmlSDL::InputEventHandler(context_, window_, mutableEvent);

        if (event.type != SDL_KEYDOWN || event.key.repeat != 0) {
            return;
        }

        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                finished_ = true;
                break;
            case SDLK_LEFT:
                focusZone_ = FocusZone::Carousel;
                moveSelection(-1);
                break;
            case SDLK_RIGHT:
                focusZone_ = FocusZone::Carousel;
                moveSelection(1);
                break;
            case SDLK_UP:
            case SDLK_DOWN:
                focusZone_ = (focusZone_ == FocusZone::Carousel) ? FocusZone::Finale : FocusZone::Carousel;
                refreshFinaleButtonClass();
                updateAnimatedLayout(0.0f);
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

    void update(float deltaSeconds) {
        if (!initialized_ || finished_) {
            return;
        }

        introElapsed_ += deltaSeconds;
        visualIndex_ = easing::lerp(visualIndex_, static_cast<float>(selectedIndex_),
                                    easing::clamp01(deltaSeconds * kCarouselLerpSpeed));

        if (std::fabs(visualIndex_ - static_cast<float>(selectedIndex_)) < 0.001f) {
            visualIndex_ = static_cast<float>(selectedIndex_);
        }

        if (toastTimer_ > 0.0f) {
            toastTimer_ = std::max(0.0f, toastTimer_ - deltaSeconds);
            updateToastElement();
        }

        updatePreviewAudio(deltaSeconds);
        updateAnimatedLayout(deltaSeconds);
    }

    void render() {
        if (!initialized_ || finished_ || context_ == nullptr) {
            return;
        }

        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.024f, 0.067f, 0.090f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        renderInterface_->BeginFrame();
        context_->Update();
        context_->Render();
        renderInterface_->EndFrame();
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
    bool loadEntries() {
        progression_ = save::loadCurrentProgression();
        std::vector<BattleDefinition> loadedBattles;
        if (!loader::loadAllBattleDefinitions(loadedBattles)) {
            std::cerr << "[BossSelector] Failed to load battle definitions\n";
            return false;
        }

        entries_.clear();
        finaleBattle_.reset();

        for (const BattleDefinition& battle : loadedBattles) {
            if (battle.key == "lyoo_plot_twist") {
                finaleBattle_ = battle;
            }

            if (!battle.selectorVisible) {
                continue;
            }

            BossDefinition boss;
            if (!loader::loadBossDefinition(battle.bossKey, boss)) {
                std::cerr << "[BossSelector] Failed to load boss: " << battle.bossKey << "\n";
                return false;
            }

            SelectorEntry entry;
            entry.battle = battle;
            entry.boss = boss;
            entry.spritePath = resolveBattleSpritePath(boss.assets);
            if (const auto bgmPath = platform::path::resolveCombatBgmPath(boss.bgm); bgmPath.has_value()) {
                entry.previewBgmPath = *bgmPath;
            }
            entry.lineupTitles = resolveLineupTitles(battle);
            entry.defeated = containsKey(progression_.clearedBattleKeys, battle.key);
            entries_.push_back(std::move(entry));
        }

        if (entries_.empty()) {
            std::cerr << "[BossSelector] No selector-visible battles are configured.\n";
            return false;
        }

        if (!finaleBattle_.has_value()) {
            std::cerr << "[BossSelector] Missing hidden finale battle: lyoo_plot_twist\n";
        }

        selectedIndex_ = std::min<std::size_t>(selectedIndex_, entries_.size() - 1);
        visualIndex_ = static_cast<float>(selectedIndex_);
        return true;
    }

    std::vector<std::string> resolveLineupTitles(const BattleDefinition& battle) {
        std::vector<std::string> titles;
        std::unordered_set<std::string> seen;
        const auto appendTitle = [&](const std::string& key) {
            if (key.empty() || !seen.insert(key).second) {
                return;
            }

            CharacterDefinition definition;
            if (loader::loadCharacterDefinition(key, definition)) {
                titles.push_back(definition.title);
            } else {
                titles.push_back(key);
            }
        };

        for (const std::string& key : battle.lockedLineup) {
            appendTitle(key);
        }
        for (const std::string& key : battle.lineup) {
            appendTitle(key);
        }
        return titles;
    }

    void bindDocument() {
        trackElement_ = document_->GetElementById("boss-track");
        detailPanelElement_ = document_->GetElementById("detail-panel");
        selectorStageElement_ = document_->GetElementById("selector-stage");
        selectorHeadingElement_ = document_->GetElementById("selector-heading");
        selectorFooterElement_ = document_->GetElementById("selector-footer");
        finaleButtonElement_ = document_->GetElementById("finale-button");
        toastElement_ = document_->GetElementById("selector-toast");
        detailPortraitElement_ = document_->GetElementById("detail-portrait");
        detailBossNameElement_ = document_->GetElementById("detail-boss-name");
        detailBattleNameElement_ = document_->GetElementById("detail-battle-name");
        detailDescriptionElement_ = document_->GetElementById("detail-description");
        detailStatusElement_ = document_->GetElementById("detail-status");
        detailLaunchModeElement_ = document_->GetElementById("detail-launch-mode");
        detailProgressElement_ = document_->GetElementById("detail-progress");
        rankValueElement_ = document_->GetElementById("idol-rank-value");
        rankCaptionElement_ = document_->GetElementById("idol-rank-caption");
        finaleSubtitleElement_ = document_->GetElementById("finale-subtitle");
    }

    void attachListener(const std::string& id,
                        Rml::EventId eventId,
                        std::function<void(Rml::Event&)> callback) {
        if (document_ == nullptr) {
            return;
        }

        Rml::Element* element = document_->GetElementById(id);
        if (element == nullptr) {
            return;
        }

        auto listener = std::make_unique<CallbackEventListener>(std::move(callback));
        element->AddEventListener(eventId, listener.get());
        uiListeners_.push_back(std::move(listener));
    }

    void rebuildTrack() {
        if (trackElement_ == nullptr) {
            return;
        }

        std::ostringstream markup;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const SelectorEntry& entry = entries_[i];
            markup << "<div class=\"boss-card-shell\" id=\"boss-card-shell-" << i << "\">"
                   << "<div class=\"boss-card-frame\">"
                   << "<div class=\"boss-card-cap-left\"></div>"
                   << "<div class=\"boss-card-cap-right\"></div>"
                   << "<div class=\"boss-card-body\" id=\"boss-card-body-" << i << "\">"
                   << "<div class=\"boss-card-status\" id=\"boss-card-status-" << i << "\">"
                   << (entry.defeated ? "Defeated" : "Not defeated")
                   << "</div>"
                   << "<div class=\"boss-card-portrait\" id=\"boss-card-portrait-" << i << "\"></div>"
                   << "<div class=\"boss-card-name\">" << escapeRml(entry.boss.title) << "</div>"
                   << "<div class=\"boss-card-subtitle\">" << escapeRml(entry.battle.name) << "</div>"
                   << "</div></div></div>";
        }

        trackElement_->SetInnerRML(markup.str());

        cardShells_.clear();
        cardBodies_.clear();
        cardStatuses_.clear();
        cardPortraits_.clear();
        cardShells_.reserve(entries_.size());
        cardBodies_.reserve(entries_.size());
        cardStatuses_.reserve(entries_.size());
        cardPortraits_.reserve(entries_.size());

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            cardShells_.push_back(document_->GetElementById("boss-card-shell-" + std::to_string(i)));
            cardBodies_.push_back(document_->GetElementById("boss-card-body-" + std::to_string(i)));
            cardStatuses_.push_back(document_->GetElementById("boss-card-status-" + std::to_string(i)));
            cardPortraits_.push_back(document_->GetElementById("boss-card-portrait-" + std::to_string(i)));

            if (cardPortraits_.back() != nullptr && !entries_[i].spritePath.empty()) {
                cardPortraits_.back()->SetProperty("decorator", makeImageDecorator(entries_[i].spritePath));
            }

            attachListener("boss-card-shell-" + std::to_string(i), Rml::EventId::Click, [this, i](Rml::Event&) {
                focusZone_ = FocusZone::Carousel;
                setSelectedIndex(i, true);
                activateSelected();
            });
        }

        attachListener("finale-button", Rml::EventId::Click, [this](Rml::Event&) {
            focusZone_ = FocusZone::Finale;
            refreshFinaleButtonClass();
            activateFinale();
        });
    }

    void refreshProgressState() {
        clearedVisibleCount_ = 0;
        for (const SelectorEntry& entry : entries_) {
            if (entry.defeated) {
                ++clearedVisibleCount_;
            }
        }
        finaleUnlocked_ = !entries_.empty() && clearedVisibleCount_ == static_cast<int>(entries_.size());

        std::unordered_set<std::string> rankEligibleKeys;
        for (const SelectorEntry& entry : entries_) {
            rankEligibleKeys.insert(entry.battle.key);
        }
        if (finaleBattle_.has_value()) {
            rankEligibleKeys.insert(finaleBattle_->key);
        }

        idolRank_ = 0;
        for (const std::string& clearedKey : progression_.clearedBattleKeys) {
            if (rankEligibleKeys.find(clearedKey) != rankEligibleKeys.end()) {
                ++idolRank_;
            }
        }

        if (rankValueElement_ != nullptr) {
            rankValueElement_->SetInnerRML(std::to_string(idolRank_));
        }
        if (rankCaptionElement_ != nullptr) {
            rankCaptionElement_->SetInnerRML("Bosses defeated");
        }
        if (finaleSubtitleElement_ != nullptr) {
            if (finaleUnlocked_) {
                finaleSubtitleElement_->SetInnerRML("Unlocked. Press ENTER to launch the finale.");
            } else {
                finaleSubtitleElement_->SetInnerRML("Defeat every boss to unlock the finale.");
            }
        }

        refreshFinaleButtonClass();
    }

    void refreshSelection(bool playPreview) {
        if (entries_.empty()) {
            return;
        }

        const SelectorEntry& entry = entries_[selectedIndex_];
        if (detailPortraitElement_ != nullptr) {
            detailPortraitElement_->SetProperty("decorator", makeImageDecorator(entry.spritePath));
        }
        if (detailBossNameElement_ != nullptr) {
            detailBossNameElement_->SetInnerRML(escapeRml(entry.boss.title));
        }
        if (detailBattleNameElement_ != nullptr) {
            detailBattleNameElement_->SetInnerRML(escapeRml(entry.battle.name));
        }
        if (detailDescriptionElement_ != nullptr) {
            const std::string battleDescription = entry.battle.description;
            if (!entry.lineupTitles.empty()) {
                std::string markup;
                if (!battleDescription.empty()) {
                    markup += escapeRml(battleDescription);
                    markup += "<br/><br/>";
                }
                markup += escapeRml("Party: " + joinStrings(entry.lineupTitles, " / "));
                detailDescriptionElement_->SetInnerRML(markup);
            } else if (!battleDescription.empty()) {
                detailDescriptionElement_->SetInnerRML(escapeRml(battleDescription));
            } else {
                detailDescriptionElement_->SetInnerRML(escapeRml("Challenge " + entry.boss.title + "."));
            }
        }
        if (detailStatusElement_ != nullptr) {
            detailStatusElement_->SetInnerRML(entry.defeated ? "Defeated" : "Not defeated");
        }
        if (detailLaunchModeElement_ != nullptr) {
            detailLaunchModeElement_->SetInnerRML(entry.battle.storyScript.empty() ? "Direct battle" : "Story scene");
        }
        if (detailProgressElement_ != nullptr) {
            detailProgressElement_->SetInnerRML(
                std::to_string(clearedVisibleCount_) + " / " + std::to_string(entries_.size()) + " cleared");
        }

        if (playPreview) {
            requestPreview(entry.previewBgmPath, entry.boss.bgmVolume);
        }

        refreshFinaleButtonClass();
    }

    void moveSelection(int direction) {
        if (entries_.empty() || direction == 0) {
            return;
        }

        const int newIndex = std::clamp(static_cast<int>(selectedIndex_) + direction, 0,
                                        static_cast<int>(entries_.size()) - 1);
        if (newIndex == static_cast<int>(selectedIndex_)) {
            return;
        }

        setSelectedIndex(static_cast<std::size_t>(newIndex), true);
    }

    void setSelectedIndex(std::size_t index, bool playPreview) {
        if (entries_.empty()) {
            return;
        }

        selectedIndex_ = std::min(index, entries_.size() - 1);
        refreshSelection(playPreview);
        updateAnimatedLayout(0.0f);
    }

    void requestPreview(const std::string& path, float baseVolume) {
        const float previewVolume = std::clamp(std::max(baseVolume, 0.16f), 0.0f, 0.45f);

        if (path.empty()) {
            previewActivePath_.clear();
            if (activePreviewChannel_ >= 0) {
                previewChannels_[activePreviewChannel_].targetVolume = 0.0f;
            }
            return;
        }

        if (path == previewActivePath_ && activePreviewChannel_ >= 0) {
            previewChannels_[activePreviewChannel_].maxVolume = previewVolume;
            previewChannels_[activePreviewChannel_].targetVolume = previewVolume;
            return;
        }

        const int newChannel = activePreviewChannel_ == 0 ? 1 : 0;
        PreviewChannel& channel = previewChannels_[newChannel];
        channel.player.stop();
        channel.path.clear();
        channel.volume = 0.0f;
        channel.targetVolume = 0.0f;
        channel.maxVolume = previewVolume;
        channel.active = false;

        if (!channel.player.play(path, 0.0f)) {
            std::cerr << "[BossSelector] Failed to preview BGM: " << path << "\n";
            return;
        }

        channel.path = path;
        channel.volume = 0.0f;
        channel.targetVolume = previewVolume;
        channel.maxVolume = previewVolume;
        channel.active = true;

        if (activePreviewChannel_ >= 0) {
            previewChannels_[activePreviewChannel_].targetVolume = 0.0f;
        }

        activePreviewChannel_ = newChannel;
        previewActivePath_ = path;
    }

    void updatePreviewAudio(float deltaSeconds) {
        const auto approach = [deltaSeconds](float value, float target, float rate) {
            const float step = rate * deltaSeconds;
            if (value < target) {
                return std::min(target, value + step);
            }
            return std::max(target, value - step);
        };

        for (int i = 0; i < 2; ++i) {
            PreviewChannel& channel = previewChannels_[i];
            if (!channel.active && !channel.player.isPlaying()) {
                continue;
            }

            const float rate = channel.targetVolume > channel.volume
                ? kPreviewFadeInPerSecond
                : kPreviewFadeOutPerSecond;
            channel.volume = approach(channel.volume, channel.targetVolume, rate);
            channel.player.setVolume(channel.volume);

            if (channel.targetVolume <= 0.0f && channel.volume <= 0.001f) {
                channel.player.stop();
                channel.path.clear();
                channel.volume = 0.0f;
                channel.targetVolume = 0.0f;
                channel.maxVolume = 0.0f;
                channel.active = false;
            }
        }
    }

    void stopPreviewAudio() {
        for (PreviewChannel& channel : previewChannels_) {
            channel.player.stop();
            channel.path.clear();
            channel.volume = 0.0f;
            channel.targetVolume = 0.0f;
            channel.maxVolume = 0.0f;
            channel.active = false;
        }
        activePreviewChannel_ = -1;
        previewActivePath_.clear();
    }

    void updateAnimatedLayout(float) {
        const float panelWidth = 332.0f;
        const float panelMargin = 28.0f;
        const float stageLeft = std::max(40.0f, windowWidth_ * 0.04f);
        const float stageGap = 42.0f;
        const float detailLeft = std::max(stageLeft + 320.0f,
                                          windowWidth_ - panelMargin - panelWidth);
        const float stageRight = std::max(stageLeft + 360.0f, detailLeft - stageGap);
        const float stageWidth = std::max(320.0f, stageRight - stageLeft);
        const float centerX = stageLeft + stageWidth * 0.5f;
        const float cardSpacing = std::clamp(stageWidth * 0.31f, 224.0f, 286.0f);
        const float baseY = std::clamp(windowHeight_ * 0.20f, 138.0f, 188.0f);

        const float detailProgress = easing::easeOutCubic(
            easing::clamp01(introElapsed_ / kDetailIntroDurationSeconds));
        if (detailPanelElement_ != nullptr) {
            detailPanelElement_->SetProperty("left", "auto");
            detailPanelElement_->SetProperty("right", formatPx(panelMargin));
            detailPanelElement_->SetProperty("opacity", formatFloat(detailProgress));
            detailPanelElement_->SetProperty(
                "transform",
                "translate(" + formatPx((1.0f - detailProgress) * 36.0f) + ", 0px)");
        }
        if (selectorStageElement_ != nullptr) {
            selectorStageElement_->SetProperty(
                "transform",
                "translate(" + formatPx((1.0f - detailProgress) * -20.0f) + ", 0px)");
        }
        if (selectorHeadingElement_ != nullptr) {
            selectorHeadingElement_->SetProperty("left", formatPx(stageLeft + 8.0f));
            selectorHeadingElement_->SetProperty("width", formatPx(std::max(280.0f, stageWidth - 16.0f)));
        }
        if (selectorFooterElement_ != nullptr) {
            selectorFooterElement_->SetProperty("left", formatPx(stageLeft + 8.0f));
            selectorFooterElement_->SetProperty("width", formatPx(std::max(280.0f, stageWidth - 16.0f)));
        }
        if (finaleButtonElement_ != nullptr) {
            finaleButtonElement_->SetProperty("left", formatPx(centerX - 218.0f));
            finaleButtonElement_->SetProperty("margin-left", "0px");
        }

        for (std::size_t i = 0; i < cardShells_.size(); ++i) {
            Rml::Element* shell = cardShells_[i];
            Rml::Element* body = cardBodies_[i];
            Rml::Element* status = cardStatuses_[i];
            if (shell == nullptr || body == nullptr || status == nullptr) {
                continue;
            }

            const float introT = easing::clamp01(
                (introElapsed_ - static_cast<float>(i) * kCardIntroStaggerSeconds) / kCardIntroDurationSeconds);
            const float introEase = easing::easeOutBack(introT);
            const float relativeIndex = static_cast<float>(i) - visualIndex_;
            const float absIndex = std::fabs(relativeIndex);

            const float x = centerX + (relativeIndex * cardSpacing);
            const float y = baseY + (absIndex * kCardDepthY) + ((1.0f - introEase) * 80.0f);
            const float scale = std::max(0.70f, 1.0f - absIndex * 0.11f) * std::max(0.82f, introEase);
            const float opacity = std::clamp(1.12f - absIndex * 0.24f, 0.22f, 1.0f) * introT;
            const int zIndex = 1000 - static_cast<int>(absIndex * 10.0f);

            shell->SetProperty("left", formatPx(x - kCardWidth * 0.5f));
            shell->SetProperty("top", formatPx(y));
            shell->SetProperty("opacity", formatFloat(opacity));
            shell->SetProperty("transform", "scale(" + formatFloat(scale) + ")");
            shell->SetProperty("z-index", std::to_string(zIndex));

            std::string bodyClass = "boss-card-body";
            if (i == selectedIndex_) {
                bodyClass += " selected";
            }
            body->SetAttribute("class", bodyClass);

            std::string statusClass = "boss-card-status";
            if (entries_[i].defeated) {
                statusClass += " defeated";
            }
            status->SetAttribute("class", statusClass);
        }

        refreshFinaleButtonClass();
        updateToastElement();
    }

    void refreshFinaleButtonClass() {
        if (finaleButtonElement_ == nullptr) {
            return;
        }

        std::string classNames = "focused";
        if (focusZone_ != FocusZone::Finale) {
            classNames.clear();
        }
        if (!finaleUnlocked_) {
            if (!classNames.empty()) {
                classNames += " ";
            }
            classNames += "locked";
        }

        if (classNames.empty()) {
            finaleButtonElement_->SetAttribute("class", "");
        } else {
            finaleButtonElement_->SetAttribute("class", classNames);
        }
    }

    void updateToastElement() {
        if (toastElement_ == nullptr) {
            return;
        }

        if (toastTimer_ <= 0.0f || toastText_.empty()) {
            toastElement_->SetProperty("opacity", "0");
            toastElement_->SetInnerRML("");
            return;
        }

        const float opacity = std::min(1.0f, toastTimer_ / 0.18f);
        toastElement_->SetProperty("opacity", formatFloat(opacity));
        toastElement_->SetInnerRML(escapeRml(toastText_));
    }

    void showToast(std::string message, float durationSeconds = kToastDurationSeconds) {
        toastText_ = std::move(message);
        toastTimer_ = std::max(0.2f, durationSeconds);
        updateToastElement();
    }

    void activateSelected() {
        if (entries_.empty()) {
            return;
        }

        const SelectorEntry& entry = entries_[selectedIndex_];
        launchRequest_ = LaunchRequest{
            entry.battle.storyScript.empty() ? LaunchRequest::Type::Battle : LaunchRequest::Type::Story,
            entry.battle.storyScript.empty() ? entry.battle.key : entry.battle.storyScript
        };
        finished_ = true;
    }

    void activateFinale() {
        if (!finaleUnlocked_) {
            showToast("Defeat every rival first.");
            return;
        }
        if (!finaleBattle_.has_value()) {
            showToast("Finale battle is not configured.");
            return;
        }

        launchRequest_ = LaunchRequest{
            finaleBattle_->storyScript.empty() ? LaunchRequest::Type::Battle : LaunchRequest::Type::Story,
            finaleBattle_->storyScript.empty() ? finaleBattle_->key : finaleBattle_->storyScript
        };
        finished_ = true;
    }

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool initialized_ = false;
    bool finished_ = false;
    bool rmlInitialized_ = false;
    bool rmlGlInitialized_ = false;
    bool imageInitialized_ = false;
    int windowWidth_ = kDefaultWindowWidth;
    int windowHeight_ = kDefaultWindowHeight;
    battle::PlayerProgression progression_;
    std::vector<SelectorEntry> entries_;
    std::optional<BattleDefinition> finaleBattle_;
    std::optional<LaunchRequest> launchRequest_;
    std::vector<std::unique_ptr<Rml::EventListener>> uiListeners_;
    std::vector<Rml::Element*> cardShells_;
    std::vector<Rml::Element*> cardBodies_;
    std::vector<Rml::Element*> cardStatuses_;
    std::vector<Rml::Element*> cardPortraits_;
    float visualIndex_ = 0.0f;
    std::size_t selectedIndex_ = 0;
    float introElapsed_ = 0.0f;
    float toastTimer_ = 0.0f;
    std::string toastText_;
    int clearedVisibleCount_ = 0;
    int idolRank_ = 0;
    bool finaleUnlocked_ = false;
    FocusZone focusZone_ = FocusZone::Carousel;
    PreviewChannel previewChannels_[2];
    int activePreviewChannel_ = -1;
    std::string previewActivePath_;
    SystemInterface_SDL systemInterface_;
    std::unique_ptr<RenderInterfaceGL3SDL> renderInterface_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    Rml::Element* trackElement_ = nullptr;
    Rml::Element* detailPanelElement_ = nullptr;
    Rml::Element* selectorStageElement_ = nullptr;
    Rml::Element* selectorHeadingElement_ = nullptr;
    Rml::Element* selectorFooterElement_ = nullptr;
    Rml::Element* finaleButtonElement_ = nullptr;
    Rml::Element* toastElement_ = nullptr;
    Rml::Element* detailPortraitElement_ = nullptr;
    Rml::Element* detailBossNameElement_ = nullptr;
    Rml::Element* detailBattleNameElement_ = nullptr;
    Rml::Element* detailDescriptionElement_ = nullptr;
    Rml::Element* detailStatusElement_ = nullptr;
    Rml::Element* detailLaunchModeElement_ = nullptr;
    Rml::Element* detailProgressElement_ = nullptr;
    Rml::Element* rankValueElement_ = nullptr;
    Rml::Element* rankCaptionElement_ = nullptr;
    Rml::Element* finaleSubtitleElement_ = nullptr;
};

Session::Session() : impl_(std::make_unique<SessionImpl>()) {}
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

bool Session::isFinished() const {
    return impl_->isFinished();
}

std::optional<LaunchRequest> Session::consumeLaunchRequest() {
    return impl_->consumeLaunchRequest();
}

} // namespace battle::selector
