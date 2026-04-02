#pragma once

#include <optional>
#include <string>
#include <vector>

#include "front_ui_document.h"

/**
 * Attach to an RML document and initialize controller state from the provided AppState.
 * @param document The RML document to bind to.
 * @param state The application state used to initialize controller fields.
 * @returns `true` on successful binding, `false` on failure.
 */
/**
 * Detach from the currently bound document and remove all registered UI listeners.
 */
/**
 * Update internal controller state to match the provided AppState and refresh the document as needed.
 * @param state The application state to sync from.
 */
/**
 * Advance time-based UI logic, handle pending transitions and animations, and apply time-dependent updates.
 * @param state The current application state.
 * @param deltaSeconds Time elapsed since the last update in seconds.
 */
/**
 * Move the current pause-menu selection by the given delta and update selection state.
 * @param delta Change in selection index (positive or negative).
 */
/**
 * Adjust the confirmation dialog selection by the given delta.
 * @param delta Change in confirmation selection index (positive or negative).
 */
/**
 * Confirm the current selection: if a confirmation dialog is visible, queue a confirm action; otherwise queue a pause-menu action.
 */
/**
 * Cancel the current interaction or dismiss an active confirmation; may queue a resume action.
 */
/**
 * Apply any queued controller outcomes into the provided AppState (for example, navigate to another screen or clear pause state).
 * @param state The application state to modify.
 */
/**
 * Consume and return the next pending Command produced by the controller.
 * @returns The next pending Command if one exists, or `std::nullopt` when none are pending.
 */
/**
 * Register UI event listeners on the bound document and store their binding handles.
 */
/**
 * Update internal controller fields (screen, selection, confirm selection, notice text, and pending flags) from AppState.
 * @param state The application state to read from.
 */
/**
 * Update the bound RML document elements to reflect the controller's current state.
 */
/**
 * Report whether the confirmation UI is currently visible.
 * @returns `true` if the confirmation dialog is visible, `false` otherwise.
 */
/**
 * Compute the PauseAction that corresponds to moving the selection by the given delta.
 * @param delta Change in selection index.
 * @returns The resulting PauseAction.
 */
/**
 * Compute the ConfirmAction that corresponds to moving the confirmation selection by the given delta.
 * @param delta Change in confirmation selection index.
 * @returns The resulting ConfirmAction.
 */
/**
 * Record a pending pause-menu action to be emitted later.
 * @param action The PauseAction to queue.
 */
/**
 * Record a pending confirmation action to be emitted later.
 * @param action The ConfirmAction to queue.
 */
/**
 * Modify the provided AppState to exit the current story and return to the main menu.
 * @param state The application state to modify.
 */
/**
 * Queue a sound playback request using the given asset path and volume.
 * @param path Filesystem or asset path identifying the sound.
 * @param volume Playback volume (typically 0.0 to 1.0).
 */
/**
 * Return and clear all queued sound requests.
 * @returns A vector of pending SoundRequest objects; the controller's queue is cleared.
 */
namespace graphics::frontui {

class PauseDocumentController final : public DocumentController {
public:
    bool bind(Rml::ElementDocument& document, const AppState& state) override;
    void unbind() override;
    void sync(const AppState& state) override;
    void update(const AppState& state, float deltaSeconds) override;
    void moveSelection(int delta) override;
    void adjustSelection(int delta) override;
    void activateSelection() override;
    void handleMouseMotion(const SDL_MouseMotionEvent& event) override;
    void handleMouseButtonDown(const SDL_MouseButtonEvent& event) override;
    void handleMouseButtonUp(const SDL_MouseButtonEvent& event) override;
    void cancel() override;
    void applyState(AppState& state) override;
    std::optional<Command> consumeCommand() override;
    std::vector<SoundRequest> consumeSoundRequests() override;

private:
    void attachListeners();
    void refreshFromState(const AppState& state);
    void refreshDocument() const;
    bool showingConfirm() const;
    PauseAction nextAction(int delta) const;
    ConfirmAction nextConfirm(int delta) const;
    void setPauseSelection(PauseAction action, bool playSound);
    void setConfirmSelection(ConfirmAction action, bool playSound);
    std::optional<PauseAction> hitTestPauseButton(float x, float y) const;
    std::optional<ConfirmAction> hitTestConfirmButton(float x, float y) const;
    void queuePauseAction(PauseAction action);
    void queueConfirmAction(ConfirmAction action);
    void exitStoryToMainMenu(AppState& state) const;

    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    ScreenState screen_ = ScreenState::PauseMenu;
    PauseAction selection_ = PauseAction::Continue;
    ConfirmAction confirmSelection_ = ConfirmAction::Cancel;
    PauseContext pauseContext_ = PauseContext::Story;
    std::string noticeText_;
    std::optional<PauseAction> pendingPauseAction_;
    std::optional<ConfirmAction> pendingConfirmAction_;
    bool pendingDismissConfirm_ = false;
    bool pendingResume_ = false;
    bool pendingBlockedLoadNotice_ = false;
    std::vector<SoundRequest> pendingSoundRequests_;
    bool needsEntryAnimation_ = false;
    std::optional<PauseAction> pressedPauseAction_;
    std::optional<ConfirmAction> pressedConfirmAction_;

    void queueSound(const char* path, float volume);
};

}  // namespace graphics::frontui
