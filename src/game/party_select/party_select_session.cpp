#define GL_GLEXT_PROTOTYPES

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
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

#include <nlohmann/json.hpp>

#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "party_select_session.h"
#include "../render/battle_scene_renderer.h"
#include "../render/gl_screen_blitter.h"
#include "../../platform/path_resolution.h"
#include "../../window.h"

namespace {

// ---------------------------------------------------------------------------
// Scene constants
// ---------------------------------------------------------------------------

constexpr float kCameraPosX = -300.0f;
constexpr float kCameraPosY = 45.0f;
constexpr float kCameraPosZ = -175.0f;
constexpr float kCameraBasePitch = 5.0f;
constexpr float kCameraBaseYaw = 4.0f;
constexpr float kCameraFocal = 50000.0f;

constexpr float kCharacterBaseY = 300.0f;
constexpr float kCharacterSpacing = 300.0f;

// Left panel width in pixels (matches RCSS #left-panel width: 480dp at 1:1 scale).
constexpr int kLeftPanelWidth = 480;

// ---------------------------------------------------------------------------
// Shared RmlUI helpers (same pattern as app_battle_session.cpp)
// ---------------------------------------------------------------------------

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

        SDL_Surface* surface = IMG_LoadTyped_RW(
            SDL_RWFromMem(buffer.get(), static_cast<int>(bufferSize)), 1, extension.c_str());
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

// ---------------------------------------------------------------------------
// Character data
// ---------------------------------------------------------------------------

struct CharacterInfo {
    std::string key;
    std::string title;
    std::string characterClass;
    std::string assets;
    int hp = 0;
    int atk = 0;
    int spd = 0;
};

std::vector<CharacterInfo> loadCharacterRoster() {
    const std::string path = platform::path::resolvePath("assets/combat/characters.json");
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[PartySelect] Failed to open characters.json: " << path << "\n";
        return {};
    }

    nlohmann::json data;
    try {
        file >> data;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[PartySelect] JSON parse error: " << e.what() << "\n";
        return {};
    }

    std::vector<CharacterInfo> roster;
    for (auto& [key, value] : data.items()) {
        CharacterInfo info;
        info.key = key;
        info.title = value.value("title", key);
        info.characterClass = value.value("class", "");
        info.assets = value.value("assets", key);
        info.hp = value.value("hp", 0);
        info.atk = value.value("atk", 0);
        info.spd = value.value("spd", 0);
        roster.push_back(std::move(info));
    }
    return roster;
}

std::string findPortraitPath(const std::string& assetName) {
    std::string path = platform::path::findCombatImagePath("icons", assetName);
    if (path.empty()) {
        path = platform::path::findCombatImagePath("sprites", assetName);
    }
    return path;
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string resolveBattleSpritePath(const std::string& assetName) {
    const std::array<std::string, 2> candidates = {
        platform::path::resolvePath("assets/combat/sprites/" + assetName + ".png"),
        platform::path::resolvePath("assets/combat/sprites/" + assetName + ".webp")
    };
    for (const std::string& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return std::string();
}

// Deterministic hash for per-character bobbing parameters.
unsigned fnvHash(const std::string& str) {
    unsigned hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<unsigned>(static_cast<unsigned char>(c));
        hash *= 16777619u;
    }
    return hash;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SessionImpl
// ---------------------------------------------------------------------------

namespace party_select {

using battle::Camera3D;
using battle::render::GlScreenBlitter;
using battle::render::SoftwareSceneRenderer;
using battle::render::WorldEntity;
using battle::render::colorFromKey;
using battle::render::renderBattleScene;

class SessionImpl {
public:
    bool initialize(Window& hostWindow) {
        shutdown();

        windowHost_ = &hostWindow;
        window_ = hostWindow.getNativeWindow();
        glContext_ = hostWindow.getGlContext();
        windowWidth_ = hostWindow.getWidth();
        windowHeight_ = hostWindow.getHeight();

        if (window_ == nullptr || glContext_ == nullptr) {
            std::cerr << "[PartySelect] Window is not in OpenGL mode.\n";
            return false;
        }

        SDL_GL_MakeCurrent(window_, glContext_);
        SDL_GL_SetSwapInterval(1);
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

#ifdef BATTLE_ENABLE_IMAGE
        const int requiredFlags = IMG_INIT_PNG | IMG_INIT_WEBP;
        if ((IMG_Init(requiredFlags) & requiredFlags) == requiredFlags) {
            imageInitialized_ = true;
        } else {
            std::cerr << "[PartySelect] SDL_image init warning: " << IMG_GetError() << "\n";
        }
#endif

        // --- RmlUI ---
        Rml::String glInitMessage;
        if (!RmlGL3::Initialize(&glInitMessage)) {
            std::cerr << "[PartySelect] RmlGL3 init failed: " << glInitMessage << "\n";
            shutdown();
            return false;
        }
        rmlGlInitialized_ = true;

        if (!screenBlitter_.initialize()) {
            std::cerr << "[PartySelect] Screen blitter init failed\n";
            shutdown();
            return false;
        }

        systemInterface_.SetWindow(window_);
        renderInterface_ = std::make_unique<RenderInterfaceGL3SDL>();
        if (!(*renderInterface_)) {
            std::cerr << "[PartySelect] RmlUi GL3 render interface failed\n";
            shutdown();
            return false;
        }

        Rml::SetSystemInterface(&systemInterface_);
        Rml::SetRenderInterface(renderInterface_.get());
        if (!Rml::Initialise()) {
            std::cerr << "[PartySelect] RmlUi core init failed\n";
            shutdown();
            return false;
        }
        rmlInitialized_ = true;

        const std::string fontPath = platform::path::findFontPath();
        if (!fontPath.empty()) {
            Rml::LoadFontFace(fontPath);
        }

        renderInterface_->SetViewport(windowWidth_, windowHeight_);
        context_ = Rml::CreateContext("party-select",
                                      Rml::Vector2i(windowWidth_, windowHeight_));
        if (context_ == nullptr) {
            std::cerr << "[PartySelect] Failed to create RmlUi context\n";
            shutdown();
            return false;
        }

        const std::string docPath = platform::path::resolvePath("assets/rmlui/party_select.rml");
        document_ = context_->LoadDocument(docPath);
        if (document_ == nullptr) {
            std::cerr << "[PartySelect] Failed to load document: " << docPath << "\n";
            shutdown();
            return false;
        }
        document_->Show();

        // --- Roster data ---
        roster_ = loadCharacterRoster();
        if (roster_.empty()) {
            std::cerr << "[PartySelect] No characters loaded from roster.\n";
            shutdown();
            return false;
        }

        // --- 3D scene: pre-load all character sprites ---
        std::vector<std::string> allAssets;
        allAssets.reserve(roster_.size());
        for (const CharacterInfo& info : roster_) {
            allAssets.push_back(info.assets);
        }
        if (!sceneRenderer_.initialize(windowWidth_, windowHeight_, allAssets, resolveBattleSpritePath)) {
            std::cerr << "[PartySelect] Scene renderer init failed\n";
            shutdown();
            return false;
        }

        // --- Camera ---
        camera_.posX = kCameraPosX;
        camera_.posY = kCameraPosY;
        camera_.posZ = kCameraPosZ;
        camera_.pitchDegrees = kCameraBasePitch;
        camera_.yawDegrees = kCameraBaseYaw;
        camera_.focalLength = kCameraFocal;
        camera_.screenCenterX = kLeftPanelWidth + (windowWidth_ - kLeftPanelWidth) / 2;
        camera_.screenCenterY = windowHeight_ / 2;

        // --- RmlUI bindings ---
        buildRosterUI();

        attachListener("start-button", Rml::EventId::Click, [this](Rml::Event&) {
            if (!selectedKeys_.empty()) {
                confirmed_ = true;
                finished_ = true;
            }
        });

        for (int i = 0; i < 4; ++i) {
            attachListener("party-slot-" + std::to_string(i + 1), Rml::EventId::Click,
                           [this, i](Rml::Event&) {
                               if (i < static_cast<int>(selectedKeys_.size())) {
                                   selectedKeys_.erase(selectedKeys_.begin() + i);
                                   rebuildEntities();
                                   updateUI();
                               }
                           });
        }

        initialized_ = true;
        return true;
    }

    void shutdown() {
        initialized_ = false;
        if (document_ != nullptr) {
            document_->Close();
            document_ = nullptr;
        }
        uiListeners_.clear();
        if (rmlInitialized_) {
            Rml::Shutdown();
            rmlInitialized_ = false;
        }
        if (rmlGlInitialized_) {
            RmlGL3::Shutdown();
            rmlGlInitialized_ = false;
        }
        renderInterface_.reset();
        screenBlitter_.destroy();
        sceneRenderer_.destroy();
#ifdef BATTLE_ENABLE_IMAGE
        if (imageInitialized_) {
            IMG_Quit();
            imageInitialized_ = false;
        }
#endif
        context_ = nullptr;
        window_ = nullptr;
        glContext_ = nullptr;
        windowHost_ = nullptr;
        finished_ = false;
        confirmed_ = false;
    }

    void handleEvent(const SDL_Event& event) {
        if (!initialized_) {
            return;
        }

        if (event.type == SDL_WINDOWEVENT &&
            (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             event.window.event == SDL_WINDOWEVENT_RESIZED)) {
            windowWidth_ = windowHost_->getWidth();
            windowHeight_ = windowHost_->getHeight();
            renderInterface_->SetViewport(windowWidth_, windowHeight_);
            context_->SetDimensions(Rml::Vector2i(windowWidth_, windowHeight_));
            camera_.screenCenterX = kLeftPanelWidth + (windowWidth_ - kLeftPanelWidth) / 2;
            camera_.screenCenterY = windowHeight_ / 2;

            std::vector<std::string> allAssets;
            for (const CharacterInfo& info : roster_) {
                allAssets.push_back(info.assets);
            }
            sceneRenderer_.initialize(windowWidth_, windowHeight_, allAssets, resolveBattleSpritePath);
        }

        SDL_Event mutableEvent = event;
        RmlSDL::InputEventHandler(context_, window_, mutableEvent);

        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            finished_ = true;
        }
    }

    void update(float deltaSeconds) {
        if (!initialized_) {
            return;
        }

        bobTime_ += deltaSeconds;
        frameAccumulator_ += deltaSeconds;

        // Camera oscillation — subtle yaw sway.
        camera_.yawDegrees = kCameraBaseYaw + std::sin(bobTime_ * 0.55f) * 1.8f;

        // Per-character bobbing: each character gets a unique frequency/phase/amplitude
        // derived from their key hash so it looks organic.
        for (size_t i = 0; i < entities_.size(); ++i) {
            const unsigned h = fnvHash(entities_[i].key);
            const float phase = static_cast<float>(h % 1000) / 1000.0f * 6.2832f;
            const float freq = 0.7f + static_cast<float>((h >> 10) % 100) / 100.0f * 0.6f;
            const float amplitude = 12.0f + static_cast<float>((h >> 20) % 100) / 100.0f * 8.0f;
            entities_[i].worldZ = amplitude * std::sin(bobTime_ * freq + phase);
        }
    }

    void render() {
        if (!initialized_) {
            return;
        }

        // 1. Software-render the 3D scene (floor grid + characters).
        renderBattleScene(sceneRenderer_, camera_, entities_, -1, frameAccumulator_);

        // 2. Upload to GL texture and draw as background.
        screenBlitter_.uploadSurface(sceneRenderer_.surface);

        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.031f, 0.055f, 0.094f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        screenBlitter_.draw();

        // 3. RmlUI on top.
        renderInterface_->BeginFrame();
        context_->Update();
        context_->Render();
        renderInterface_->EndFrame();
    }

    bool isFinished() const { return finished_; }
    bool isConfirmed() const { return confirmed_; }
    std::vector<std::string> getSelectedPartyKeys() const { return selectedKeys_; }

private:
    // ----- 3D entity management -----

    void rebuildEntities() {
        entities_.clear();
        if (selectedKeys_.empty()) {
            return;
        }

        const float totalWidth = std::max(0, static_cast<int>(selectedKeys_.size()) - 1) * kCharacterSpacing;
        const float startX = -totalWidth * 0.5f;

        for (size_t i = 0; i < selectedKeys_.size(); ++i) {
            const CharacterInfo* info = findCharacter(selectedKeys_[i]);
            if (info == nullptr) {
                continue;
            }

            WorldEntity entity;
            entity.key = info->key;
            entity.assetName = info->assets;
            entity.isBoss = false;
            entity.worldX = startX + static_cast<float>(i) * kCharacterSpacing;
            entity.worldY = kCharacterBaseY;
            entity.worldZ = 0.0f;
            entity.fallbackColor = colorFromKey(info->key, false);
            entities_.push_back(std::move(entity));
        }
    }

    // ----- RmlUI construction -----

    void buildRosterUI() {
        Rml::Element* grid = document_->GetElementById("roster-grid");
        if (grid == nullptr) {
            return;
        }

        for (const CharacterInfo& info : roster_) {
            Rml::ElementPtr card = document_->CreateElement("div");
            card->SetId("roster-" + info.key);
            card->SetClass("roster-card", true);

            std::string inner;
            inner += "<div class='roster-portrait' id='roster-portrait-" + info.key + "'></div>";
            inner += "<div class='roster-info'>";
            inner += "<div class='roster-name'>" + uppercase(info.title) + "</div>";
            inner += "<div class='roster-class'>" + info.characterClass + "</div>";
            inner += "</div>";

            card->SetInnerRML(inner);

            Rml::Element* rawCard = grid->AppendChild(std::move(card));

            const std::string portraitPath = findPortraitPath(info.assets);
            if (!portraitPath.empty()) {
                if (Rml::Element* portrait = document_->GetElementById("roster-portrait-" + info.key)) {
                    portrait->SetProperty("decorator", "image(" + portraitPath + " cover center center)");
                }
            }

            const std::string key = info.key;
            auto listener = std::make_unique<CallbackEventListener>([this, key](Rml::Event&) {
                toggleCharacter(key);
            });
            rawCard->AddEventListener(Rml::EventId::Click, listener.get());
            uiListeners_.push_back(std::move(listener));
        }
    }

    // ----- Selection logic -----

    void toggleCharacter(const std::string& key) {
        auto it = std::find(selectedKeys_.begin(), selectedKeys_.end(), key);
        if (it != selectedKeys_.end()) {
            selectedKeys_.erase(it);
        } else if (selectedKeys_.size() < 4) {
            selectedKeys_.push_back(key);
        }
        rebuildEntities();
        updateUI();
    }

    void updateUI() {
        updateRosterHighlights();
        updatePartySlots();
        updateStartButton();
        updateEmptyHint();
    }

    void updateRosterHighlights() {
        for (const CharacterInfo& info : roster_) {
            if (Rml::Element* card = document_->GetElementById("roster-" + info.key)) {
                const bool selected =
                    std::find(selectedKeys_.begin(), selectedKeys_.end(), info.key) != selectedKeys_.end();
                card->SetClass("selected", selected);
            }
        }
    }

    void updatePartySlots() {
        for (int i = 0; i < 4; ++i) {
            const std::string slotId = std::to_string(i + 1);
            Rml::Element* slot = document_->GetElementById("party-slot-" + slotId);
            Rml::Element* portrait = document_->GetElementById("slot-portrait-" + slotId);
            Rml::Element* name = document_->GetElementById("slot-name-" + slotId);

            if (slot == nullptr) {
                continue;
            }

            if (i < static_cast<int>(selectedKeys_.size())) {
                const CharacterInfo* info = findCharacter(selectedKeys_[static_cast<size_t>(i)]);
                slot->SetClass("empty", false);
                slot->SetClass("filled", true);
                if (portrait != nullptr && info != nullptr) {
                    const std::string path = findPortraitPath(info->assets);
                    if (!path.empty()) {
                        portrait->SetProperty("decorator", "image(" + path + " cover center center)");
                    }
                }
                if (name != nullptr && info != nullptr) {
                    name->SetInnerRML(uppercase(info->title));
                }
            } else {
                slot->SetClass("empty", true);
                slot->SetClass("filled", false);
                if (portrait != nullptr) {
                    portrait->SetProperty("decorator", "none");
                }
                if (name != nullptr) {
                    name->SetInnerRML("");
                }
            }
        }
    }

    void updateStartButton() {
        if (Rml::Element* btn = document_->GetElementById("start-button")) {
            btn->SetClass("disabled", selectedKeys_.empty());
        }
    }

    void updateEmptyHint() {
        if (Rml::Element* hint = document_->GetElementById("empty-hint")) {
            hint->SetClass("hidden", !selectedKeys_.empty());
        }
    }

    // ----- Helpers -----

    const CharacterInfo* findCharacter(const std::string& key) const {
        for (const CharacterInfo& info : roster_) {
            if (info.key == key) {
                return &info;
            }
        }
        return nullptr;
    }

    void attachListener(const std::string& id, Rml::EventId eventId,
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

    // ----- State -----

    bool initialized_ = false;
    bool finished_ = false;
    bool confirmed_ = false;
    bool rmlGlInitialized_ = false;
    bool rmlInitialized_ = false;
    bool imageInitialized_ = false;

    Window* windowHost_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    int windowWidth_ = 0;
    int windowHeight_ = 0;

    // RmlUI
    SystemInterface_SDL systemInterface_;
    std::unique_ptr<RenderInterfaceGL3SDL> renderInterface_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    std::vector<std::unique_ptr<CallbackEventListener>> uiListeners_;

    // 3D scene
    SoftwareSceneRenderer sceneRenderer_;
    GlScreenBlitter screenBlitter_;
    Camera3D camera_;
    std::vector<WorldEntity> entities_;
    float bobTime_ = 0.0f;
    float frameAccumulator_ = 0.0f;

    // Data
    std::vector<CharacterInfo> roster_;
    std::vector<std::string> selectedKeys_;
};

// ---------------------------------------------------------------------------
// Pimpl forwarding
// ---------------------------------------------------------------------------

Session::Session() : impl_(std::make_unique<SessionImpl>()) {}
Session::~Session() { if (impl_) impl_->shutdown(); }

bool Session::initialize(Window& window) { return impl_->initialize(window); }
void Session::shutdown() { impl_->shutdown(); }
void Session::handleEvent(const SDL_Event& event) { impl_->handleEvent(event); }
void Session::update(float deltaSeconds) { impl_->update(deltaSeconds); }
void Session::render() { impl_->render(); }
bool Session::isFinished() const { return impl_->isFinished(); }
bool Session::isConfirmed() const { return impl_->isConfirmed(); }
std::vector<std::string> Session::getSelectedPartyKeys() const { return impl_->getSelectedPartyKeys(); }

} // namespace party_select
