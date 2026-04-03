#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <RmlUi/Core/EventListener.h>

#include "front_ui_document.h"

/**
 * Attach the controller to an Rml document and initialize UI state from the provided AppState.
 * @param document Rml document to bind to.
 * @param state Source application state used to initialize controller state and visuals.
 * @returns `true` if binding succeeded and the controller is ready; `false` otherwise.
 */
/**
 * Detach the controller from its bound document and remove any registered event bindings.
 */
/**
 * Synchronize the controller's internal UI state with the given AppState.
 * @param state Source application state to synchronize from.
 */
/**
 * Advance animations and apply visual updates driven by the current AppState over the given time step.
 * @param state Current application state that may influence updates.
 * @param deltaSeconds Time elapsed since the last update, in seconds.
 */
/**
 * Move the current selection by the given number of steps and update selection visuals.
 * @param delta Number of positions to move the selection (positive or negative).
 */
/**
 * Activate the currently selected menu action, queuing any resulting command or changing overlay state.
 */
/**
 * Apply controller-driven changes back into the provided AppState (for example, queued actions or overlay changes).
 * @param state Application state to modify.
 */
/**
 * Return and clear the next pending command, if any.
 * @returns An optional `Command` containing the pending command, or an empty optional if none is pending.
 */
/**
 * Retrieve the currently selected main menu action.
 * @returns An optional `MainMenuAction` containing the current selection, or an empty optional if no selection is available.
 */
/**
 * Determine whether the main menu stack should be considered active for the given AppState.
 * @param state Application state to evaluate.
 * @returns `true` if the main menu stack is active for `state`; `false` otherwise.
 */
/**
 * Compute the overlay mode that corresponds to the given AppState.
 * @param state Application state to evaluate.
 * @returns The `MainMenuOverlayMode` that best represents overlay state for `state`.
 */
/**
 * Register UI event listeners and store their bindings for later removal.
 */
/**
 * Locate and cache frequently used Rml elements from the bound document for faster access.
 */
/**
 * Update the textual/content copy for the main menu buttons based on current state.
 */
/**
 * Set the current selection to the specified action and update any related state or visuals.
 * @param action The main menu action to select.
 */
/**
 * Prepare and store a pending activation command corresponding to the specified action.
 * @param action The main menu action to queue for activation.
 */
/**
 * Apply visual styling to reflect the current selection.
 */
/**
 * Update status-related UI text (for example save/load status or notifications).
 */
/**
 * Reset and start the intro animation timing and state.
 */
/**
 * Advance the drift animation toward its target positions using the given time step.
 * @param deltaSeconds Time step in seconds to advance the drift animation.
 */
/**
 * Advance the intro animation state by the given time step.
 * @param deltaSeconds Time step in seconds to advance the intro animation.
 */
/**
 * Advance the submenu transition toward its target position using the given time step.
 * @param deltaSeconds Time step in seconds to advance the submenu transition.
 */
/**
 * Apply computed visual transforms, positions, and visibility to the cached Rml elements.
 */
namespace graphics::frontui {

enum class MainMenuOverlayMode {
    None,
    Load,
    Settings,
};

class MainMenuDocumentController final : public DocumentController {
public:
    bool bind(Rml::ElementDocument& document, const AppState& state) override;
    void unbind() override;
    void sync(const AppState& state) override;
    void update(const AppState& state, float deltaSeconds) override;
    void moveSelection(int delta) override;
    void activateSelection() override;
    void handleMouseMotion(const SDL_MouseMotionEvent& event) override;
    void handleMouseButtonDown(const SDL_MouseButtonEvent& event) override;
    void handleMouseButtonUp(const SDL_MouseButtonEvent& event) override;
    void applyState(AppState& state) override;
    std::optional<Command> consumeCommand() override;
    std::optional<MainMenuAction> selectedMainMenuAction() const override;

private:
    bool isMenuStackActive(const AppState& state) const;
    MainMenuOverlayMode overlayModeForState(const AppState& state) const;
    void attachListeners();
    void cacheElements();
    void applyButtonCopy() const;
    void setSelection(MainMenuAction action);
    void queueActivation(MainMenuAction action);
    void applySelectionStyles() const;
    void updateStatusCopy() const;
    void updateNoticeCopy() const;
    std::optional<MainMenuAction> hitTestButton(float x, float y) const;
    void restartIntroAnimation();
    void updateDrift(float deltaSeconds);
    void updateIntro(float deltaSeconds);
    void updateSubmenu(float deltaSeconds);
    void applyVisualState() const;

    Rml::ElementDocument* document_ = nullptr;
    MainMenuAction selection_ = MainMenuAction::Start;
    std::optional<Command> pendingCommand_;
    std::vector<EventListenerBinding> listeners_;
    Rml::Element* whiteoutElement_ = nullptr;
    Rml::Element* discLayerElement_ = nullptr;
    Rml::Element* logoLayerElement_ = nullptr;
    Rml::Element* uiLayerElement_ = nullptr;
    Rml::Element* fixedLayerElement_ = nullptr;
    Rml::Element* noticeElement_ = nullptr;
    std::string noticeText_;
    float driftCurrentX_ = 0.0f;
    float driftCurrentY_ = 0.0f;
    float driftTargetX_ = 0.0f;
    float driftTargetY_ = 0.0f;
    float submenuCurrent_ = 0.0f;
    float submenuTarget_ = 0.0f;
    float introElapsedSeconds_ = 0.0f;
    bool introActive_ = false;
    bool menuStackActive_ = false;
    MainMenuOverlayMode overlayMode_ = MainMenuOverlayMode::None;
    std::optional<MainMenuAction> pressedAction_;
};

}  // namespace graphics::frontui
