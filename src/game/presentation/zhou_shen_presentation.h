#pragma once
#include "ability_presentation.h"
#include "miku_rhythm_game.h"

namespace battle {
class ZhouShenPresentation : public AbilityPresentation {
public:
    bool shouldBlackoutWorld() const override { return !rhythmDone_; }
    bool shouldHideNonCasterCharacters() const override { return false; }
    bool shouldRenderCasterEntity() const override { return !rhythmDone_; }
    bool shouldRenderAboveHud() const override { return false; }
    ZhouShenPresentation(float cwx, float cwy, float cwz, float twx, float twy, float twz);
    ~ZhouShenPresentation() override;
    void start() override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) override;
    bool isComplete() const override;
    bool onKeyPressed(SDL_Keycode key) override;
    float consumeHitDamageMultiplier() override;
    int consumeHitEvents() override;
    int getDamageLabelHitCount() const override { return 5; }
    std::vector<PresentationFeedbackEvent> consumeFeedbackEvents() override;
    std::string getInputResultText() const override;
    bool overridesCamera() const override { return true; }
    void applyCameraState(Camera3D& camera) const override;
    void setPresentationValue(int value) override { phase_ = value; }

private:
    float casterX_, casterY_, casterZ_, targetX_, targetY_, targetZ_;
    int phase_ = 1;
    float elapsed_ = 0.0f;
    int hitsEmitted_ = 0;
    float damageReduction_ = 0.0f;
    bool rhythmDone_ = false;
    SDL_Texture* noteTex_ = nullptr;
    SDL_Texture* fishTex_ = nullptr;
    
    struct Note {
       int row; float spawnTime; float hitTime; float speed; bool resolved; MikuRhythmGame::NoteResult result;
    };
    std::vector<Note> notes_;
    std::vector<PresentationFeedbackEvent> pendingFeedbackEvents_;
    float duration_ = 5.0f;
};
}
