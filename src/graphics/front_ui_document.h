#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include "../GameMenu/menu_shared.h"

namespace graphics::frontui {

enum class ScreenId {
    MainMenu,
    Story,
    Credits,
    Settings,
    Pause,
    Load
};

enum class CommandType {
    ActivateMainMenuAction,
    ApplyDisplayMode,
    ReturnFromSettings
};

struct Command {
    CommandType type = CommandType::ActivateMainMenuAction;
    MainMenuAction mainMenuAction = MainMenuAction::Start;
    bool displayModeFullscreen = false;
};

struct SoundRequest {
    std::string relativePath;
    float volume = 0.9f;
};

struct EventListenerBinding {
    Rml::Element* element = nullptr;
    Rml::EventId eventId = Rml::EventId::Invalid;
    bool capturePhase = false;
    std::unique_ptr<Rml::EventListener> listener;
};

inline void detachEventListeners(std::vector<EventListenerBinding>& bindings) {
    for (EventListenerBinding& binding : bindings) {
        if (binding.element != nullptr && binding.listener != nullptr) {
            binding.element->RemoveEventListener(binding.eventId, binding.listener.get(), binding.capturePhase);
        }
    }
    bindings.clear();
}

class DocumentController {
public:
    virtual ~DocumentController() = default;

    virtual bool bind(Rml::ElementDocument& document, const AppState& state) = 0;
    virtual void unbind() {}
    virtual void sync(const AppState& state) = 0;
    virtual void update(const AppState& state, float deltaSeconds) = 0;
    virtual void moveSelection(int delta) = 0;
    virtual void adjustSelection(int delta) { (void)delta; }
    virtual void activateSelection() = 0;
    virtual void handleKeyDown(const SDL_KeyboardEvent& event) { (void)event; }
    virtual void handleKeyUp(const SDL_KeyboardEvent& event) { (void)event; }
    virtual void cancel() {}
    virtual void applyState(AppState& state) { (void)state; }
    virtual std::optional<Command> consumeCommand() = 0;
    virtual std::vector<SoundRequest> consumeSoundRequests() { return {}; }
    virtual std::optional<MainMenuAction> selectedMainMenuAction() const { return std::nullopt; }
};

std::unique_ptr<DocumentController> createControllerForScreen(ScreenId screen);
std::string resolveDocumentPath(ScreenId screen);
bool isScreenImplemented(ScreenId screen);

}  // namespace graphics::frontui
