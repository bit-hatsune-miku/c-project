#include "front_ui_main_menu.h"

#include <functional>
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

}  // namespace

bool MainMenuDocumentController::bind(Rml::ElementDocument& document, const AppState& state) {
    document_ = &document;
    detachEventListeners(listeners_);
    pendingCommand_.reset();
    selection_ = state.mainSelection;

    applyButtonCopy();
    attachListeners();
    applySelectionStyles();
    updateStatusCopy();
    return true;
}

void MainMenuDocumentController::unbind() {
    detachEventListeners(listeners_);
    document_ = nullptr;
}

void MainMenuDocumentController::sync(const AppState& state) {
    if (document_ == nullptr || selection_ == state.mainSelection) {
        return;
    }
    setSelection(state.mainSelection);
}

void MainMenuDocumentController::update(const AppState& state, float deltaSeconds) {
    (void)state;
    (void)deltaSeconds;
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
        if (Rml::Element* element = document_->GetElementById(button.detailId)) {
            element->SetInnerRML(button.displayDetail);
        }
    }
}

void MainMenuDocumentController::setSelection(MainMenuAction action) {
    selection_ = action;
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
