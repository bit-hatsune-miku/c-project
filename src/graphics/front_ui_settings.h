#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <RmlUi/Core/EventListener.h>

#include "front_ui_document.h"

/**
 * Bind the controller to a Rml document and initialize its working state from the provided application state.
 * @param document The Rml document to manage.
 * @param state The current application state to initialize UI values from.
 * @returns `true` if the controller successfully bound to the document, `false` otherwise.
 */
/**
 * Unbind the controller from its document and remove any attached listeners.
 */
/**
 * Synchronize the controller's working settings and visible UI with the provided application state.
 * @param state The application state to synchronize from.
 */
/**
 * Perform per-frame update tasks for the UI controller.
 * @param state The current application state.
 * @param deltaSeconds Time elapsed since the last update, in seconds.
 */
/**
 * Move the current settings selection by the given delta (positive or negative) and update UI accordingly.
 * @param delta Amount to move the selection by.
 */
/**
 * Adjust the value of the currently selected setting by the given delta and update UI accordingly.
 * @param delta Amount to adjust the selected setting by.
 */
/**
 * Activate the currently selected setting (e.g., toggle, open, or commit the selection) and apply any resulting actions.
 */
/**
 * Cancel the current interaction and perform any necessary navigation or rollback.
 */
/**
 * Apply the controller's staged working settings back into the provided application state.
 * @param state The application state to apply changes to.
 */
/**
 * Consume and clear a pending command queued by the controller.
 * @returns An optional `Command` containing the queued command if one exists, or an empty optional otherwise.
 */
/**
 * Consume and clear accumulated sound requests queued by the controller.
 * @returns A vector of `SoundRequest` items that were queued since the last consumption; empty if none.
 */
/**
 * Attach UI event listeners required by the controller.
 */
/**
 * Apply visual styles to reflect the current selection.
 */
/**
 * Update row-related copy/text in the UI to match current selection and settings.
 */
/**
 * Update any UI text or copy related to focus changes.
 */
/**
 * Synchronize UI control values (sliders, toggles, etc.) with the controller's working settings.
 */
/**
 * Set the current settings selection and update associated UI; optionally play a selection sound.
 * @param selection The settings item to select.
 * @param playSound If `true`, enqueue a selection sound.
 */
/**
 * Queue a change to the display mode (fullscreen or windowed) as a command for later consumption.
 * @param fullscreen `true` to select fullscreen, `false` to select windowed mode.
 */
/**
 * Queue a navigation/return command targeting the controller's configured return screen.
 */
/**
 * Set the music volume in the controller's working settings and update UI/side effects as needed.
 * @param value New music volume value (expected range depends on application).
 */
/**
 * Set the voice volume in the controller's working settings and update UI/side effects as needed.
 * @param value New voice volume value (expected range depends on application).
 */
/**
 * Set the text speed in the controller's working settings and update UI/side effects as needed.
 * @param value New text speed value (expected range depends on application).
 */
/**
 * Enqueue a sound request with the given asset path and volume.
 * @param path Filesystem or asset path to the sound to play.
 * @param volume Playback volume for the sound; defaults to 0.9.
 */
namespace graphics::frontui {

class SettingsDocumentController final : public DocumentController {
public:
    bool bind(Rml::ElementDocument& document, const AppState& state) override;
    void unbind() override;
    void sync(const AppState& state) override;
    void update(const AppState& state, float deltaSeconds) override;
    void moveSelection(int delta) override;
    void adjustSelection(int delta) override;
    void activateSelection() override;
    void cancel() override;
    void applyState(AppState& state) override;
    std::optional<Command> consumeCommand() override;
    std::vector<SoundRequest> consumeSoundRequests() override;

private:
    void attachListeners();
    void applySelectionStyles() const;
    void applyRowCopy() const;
    void updateFocusCopy() const;
    void syncControlValues() const;
    void setSelection(SettingsItem selection, bool playSound);
    void queueDisplayMode(bool fullscreen);
    void queueReturn();
    void setMusicVolume(float value);
    void setVoiceVolume(float value);
    void setTextSpeed(float value);
    void queueSound(const char* path, float volume = 0.9f);

    Rml::ElementDocument* document_ = nullptr;
    ScreenState returnScreen_ = ScreenState::MainMenu;
    SettingsItem selection_ = SettingsItem::DisplayMode;
    GameSettings workingSettings_{};
    std::optional<Command> pendingCommand_;
    std::vector<SoundRequest> pendingSoundRequests_;
    std::vector<EventListenerBinding> listeners_;
};

}  // namespace graphics::frontui
