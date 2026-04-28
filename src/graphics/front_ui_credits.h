#pragma once

#include <string>
#include <vector>

#include "../game/audio/bgm_player.h"
#include "../game/credits/credits_data.h"
#include "front_ui_document.h"

namespace graphics::frontui {

class CreditsDocumentController final : public DocumentController {
public:
    bool bind(Rml::ElementDocument& document, const AppState& state) override;
    void unbind() override;
    void sync(const AppState& state) override;
    void update(const AppState& state, float deltaSeconds) override;
    void moveSelection(int delta) override;
    void activateSelection() override;
    void handleKeyDown(const SDL_KeyboardEvent& event) override;
    void handleKeyUp(const SDL_KeyboardEvent& event) override;
    void cancel() override;
    void applyState(AppState& state) override;
    std::optional<Command> consumeCommand() override;

private:
    struct VisualTrack {
        std::string visualElementId;
        std::string blockElementId;
        std::string visualShellElementId;
        std::string visualLabelElementId;
        std::string visualCaptionElementId;
        float contentTop = 0.0f;
        float height = 0.0f;
        float activeTranslateX = 0.0f;
        float presentationWeight = 0.0f;
        float textPresentationWeight = 0.0f;
        float textRevealDelayRemaining = 0.0f;
        bool targetActive = false;
    };

    void attachListeners();
    void populateContent();
    void updateOpening(float deltaSeconds);
    void updateScroll(float deltaSeconds);
    void updatePresentation(float deltaSeconds, bool snap = false);
    void updateSubtitle();
    void initializeScrollMetrics();
    void setScrollTop(float top);
    void setChromeOpacity(float opacity);
    void setTitleCardOpacity(float opacity);
    void setSubtitleText(const std::string& text);
    void syncPrompt() const;
    void requestReturn();
    bool startCreditsMusic(const AppState& state);
    float currentSongPlaybackSeconds() const;
    float currentSongDurationSeconds() const;

    Rml::ElementDocument* document_ = nullptr;
    std::vector<EventListenerBinding> listeners_;
    std::vector<VisualTrack> visualTracks_;
    game::credits::CreditsData creditsData_;
    game::audio::BgmPlayer musicPlayer_;
    bool creditsLoaded_ = false;
    bool pendingReturn_ = false;
    bool scrollMetricsReady_ = false;
    bool rollFinished_ = false;
    bool speedupHeld_ = false;
    bool openingFinished_ = false;
    bool syncedPlaybackActive_ = false;
    float arrowSpeedMultiplier_ = 1.0f;
    float currentTop_ = 0.0f;
    float startTop_ = 0.0f;
    float endTop_ = 0.0f;
    float viewportHeight_ = 0.0f;
    float openingElapsed_ = 0.0f;
    float layoutScale_ = 1.0f;
    float songDurationSeconds_ = 0.0f;
    float songBaseVolume_ = 1.0f;
    std::string currentSubtitleText_;
};

}  // namespace graphics::frontui
