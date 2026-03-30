#include "lyoo_plot_twist_presentation.h"

#include "../core/easing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

namespace battle {
namespace {

constexpr int kFrameCount = 11;

constexpr std::array<float, 10> kIntroFrameEndTimes{{
    0.28f, 0.56f, 0.84f, 1.14f, 1.70f,
    1.94f, 2.14f, 2.26f, 2.34f, 2.40f
}};

constexpr float kIntroDurationSeconds = 2.40f;
constexpr float kTransitionDurationSeconds = 0.55f;
constexpr float kRingDurationSeconds = 2.35f;
constexpr float kSkyDurationSeconds = 1.70f;
constexpr float kApproachDurationSeconds = 2.00f;
constexpr float kMassiveStarDurationSeconds = 2.90f;
constexpr float kBlackoutDurationSeconds = 2.00f;
constexpr float kFinalShakeDurationSeconds = 5.00f;
constexpr int kFinalShakeHitBursts = 15;

constexpr float kTransitionStartSeconds = kIntroDurationSeconds;
constexpr float kRingStartSeconds = kTransitionStartSeconds + kTransitionDurationSeconds;
constexpr float kSkyStartSeconds = kRingStartSeconds + kRingDurationSeconds;
constexpr float kApproachStartSeconds = kSkyStartSeconds + kSkyDurationSeconds;
constexpr float kMassiveStarStartSeconds = kApproachStartSeconds + kApproachDurationSeconds;
constexpr float kBlackoutStartSeconds = kMassiveStarStartSeconds + kMassiveStarDurationSeconds;
constexpr float kFinalShakeStartSeconds = kBlackoutStartSeconds + kBlackoutDurationSeconds;
constexpr float kCompleteTimeSeconds = kFinalShakeStartSeconds + kFinalShakeDurationSeconds;

constexpr float kRingOrbitRadiusPx = 155.0f;
constexpr float kTransitionScreenCenterY = 0.49f;
constexpr float kRingScreenCenterX = 0.50f;
constexpr float kRingScreenCenterY = 0.45f;
constexpr float kApproachDarknessAlpha = 105.0f;
constexpr float kRiserDurationSeconds = 3.526542f;

constexpr char kStabSfxPath[] = "assets/combat/presentations/lyooPlotTwist/stab.wav";
constexpr char kRingStarSfxPath[] = "assets/combat/presentations/lyooPlotTwist/mus_sfx_star.wav";
constexpr char kSkyStarSfxPath[] = "assets/combat/presentations/lyooPlotTwist/mus_sfx_sparkles.wav";
constexpr char kShakeLoopSfxPath[] = "assets/combat/presentations/lyooPlotTwist/shake.wav";
constexpr char kRiserSfxPath[] = "assets/combat/presentations/lyooPlotTwist/riser.wav";
constexpr char kPostSfxPath[] = "assets/combat/presentations/lyooPlotTwist/post.wav";
constexpr float kStabSfxVolume = 0.42f;
constexpr float kRingStarSfxVolume = 0.30f;
constexpr float kSkyStarSfxVolume = 1.0f;
constexpr float kShakeLoopSfxVolume = 1.0f;
constexpr float kRiserSfxVolume = 1.0f;
constexpr float kPostSfxVolume = 1.25f;

float lerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

float easeInOutCubicLocal(float t) {
    const float clamped = easing::clamp01(t);
    if (clamped < 0.5f) {
        return 4.0f * clamped * clamped * clamped;
    }

    const float f = -2.0f * clamped + 2.0f;
    return 1.0f - (f * f * f) / 2.0f;
}

} // namespace

LyooPlotTwistPresentation::LyooPlotTwistPresentation(
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
)
    : casterX_(casterWorldX)
    , casterY_(casterWorldY)
    , casterZ_(casterWorldZ)
    , targetX_(targetWorldX)
    , targetY_(targetWorldY)
    , targetZ_(targetWorldZ) {
    totalDuration_ = kCompleteTimeSeconds;
}

LyooPlotTwistPresentation::~LyooPlotTwistPresentation() {
    releaseAssets();
}

void LyooPlotTwistPresentation::start() {
    elapsedTime_ = 0.0f;
    pendingAbilityAudioCues_ = 1;
    pendingHitEvents_ = 0;
    hitBurstsEmitted_ = 0;
    pendingAudioCommands_.clear();
    stabTriggered_ = false;
    riserTriggered_ = false;
    shakeLoopStarted_ = false;
    blackoutTriggered_ = false;
    postTriggered_ = false;
    ringStarSfxIndex_ = 0;
    skyStarSfxIndex_ = 0;

    frontCamera_ = Camera3D{};
    frontCamera_.posX = casterX_;
    frontCamera_.posY = casterY_ - 210.0f;
    frontCamera_.posZ = -175.0f;
    frontCamera_.pitchDegrees = -2.0f;
    frontCamera_.yawDegrees = 0.0f;
    frontCamera_.focalLength = 44000.0f;

    skyCamera_ = frontCamera_;
    skyCamera_.posY = casterY_ - 170.0f;
    skyCamera_.posZ = -235.0f;
    skyCamera_.pitchDegrees = -42.0f;
    skyCamera_.focalLength = 36000.0f;

    wideCamera_ = frontCamera_;
    wideCamera_.posX = 0.0f;
    wideCamera_.posY = 60.0f;
    wideCamera_.posZ = -180.0f;
    wideCamera_.pitchDegrees = -3.0f;
    wideCamera_.yawDegrees = 0.0f;
    wideCamera_.focalLength = 28000.0f;

    seedStarTimelines();
}

void LyooPlotTwistPresentation::update(float deltaTime) {
    elapsedTime_ += deltaTime;

    if (!stabTriggered_ && currentIntroFrameIndex() >= 4) {
        queueAudioCommand(PresentationAudioCommandType::PlayOneShot, kStabSfxPath, kStabSfxVolume);
        stabTriggered_ = true;
    }

    const float ringPhaseTime = elapsedTime_ - kRingStartSeconds;
    while (ringStarSfxIndex_ < ringStars_.size() &&
           ringPhaseTime >= ringStars_[ringStarSfxIndex_].spawnTime) {
        queueAudioCommand(
            PresentationAudioCommandType::PlayOneShotAllowOverlap,
            kRingStarSfxPath,
            kRingStarSfxVolume
        );
        ++ringStarSfxIndex_;
    }

    const float skyPhaseTime = elapsedTime_ - kSkyStartSeconds;
    while (skyStarSfxIndex_ < skyStars_.size() &&
           skyPhaseTime >= skyStars_[skyStarSfxIndex_].spawnTime) {
        queueAudioCommand(
            PresentationAudioCommandType::PlayOneShotAllowOverlap,
            kSkyStarSfxPath,
            kSkyStarSfxVolume
        );
        ++skyStarSfxIndex_;
    }

    if (!shakeLoopStarted_ && elapsedTime_ >= kApproachStartSeconds) {
        queueAudioCommand(PresentationAudioCommandType::StartLoop, kShakeLoopSfxPath, kShakeLoopSfxVolume);
        shakeLoopStarted_ = true;
    }

    if (!riserTriggered_ && elapsedTime_ >= (kBlackoutStartSeconds - kRiserDurationSeconds)) {
        queueAudioCommand(PresentationAudioCommandType::PlayOneShot, kRiserSfxPath, kRiserSfxVolume);
        riserTriggered_ = true;
    }

    if (!blackoutTriggered_ && elapsedTime_ >= kBlackoutStartSeconds) {
        queueAudioCommand(PresentationAudioCommandType::StopAllSfx, "");
        queueAudioCommand(PresentationAudioCommandType::PauseBgm, "");
        blackoutTriggered_ = true;
    }

    if (!postTriggered_ && elapsedTime_ >= kFinalShakeStartSeconds) {
        queueAudioCommand(PresentationAudioCommandType::ResumeBgm, "");
        queueAudioCommand(PresentationAudioCommandType::PlayOneShot, kPostSfxPath, kPostSfxVolume);
        postTriggered_ = true;
    }

    if (elapsedTime_ >= kFinalShakeStartSeconds) {
        const float finalShakeProgress = easing::clamp01(
            (elapsedTime_ - kFinalShakeStartSeconds) / std::max(0.001f, kFinalShakeDurationSeconds)
        );
        const int desiredBursts = std::clamp(
            static_cast<int>(std::floor(finalShakeProgress * static_cast<float>(kFinalShakeHitBursts))),
            0,
            kFinalShakeHitBursts
        );
        if (desiredBursts > hitBurstsEmitted_) {
            pendingHitEvents_ += desiredBursts - hitBurstsEmitted_;
            hitBurstsEmitted_ = desiredBursts;
        }
    }
}

void LyooPlotTwistPresentation::preload(SDL_Renderer* renderer) {
    ensureAssetsLoaded(renderer);
}

void LyooPlotTwistPresentation::render(SDL_Renderer* renderer, int screenW, int screenH, const Camera3D& camera) {
    ensureAssetsLoaded(renderer);

    switch (currentPhase()) {
        case Phase::IntroFrames:
            renderIntroFrames(renderer, screenW, screenH);
            break;
        case Phase::TransitionToVoid:
            renderTransitionToVoid(renderer, screenW, screenH);
            break;
        case Phase::RingStars:
            renderRingStars(renderer, screenW, screenH, camera);
            break;
        case Phase::SkyStars:
            renderRingStars(renderer, screenW, screenH, camera);
            renderSkyStars(renderer, screenW, screenH);
            break;
        case Phase::ApproachAllies:
            renderSkyStars(renderer, screenW, screenH);
            renderApproachOverlay(renderer, screenW, screenH);
            break;
        case Phase::MassiveStar:
            renderScatterStars(renderer, screenW, screenH);
            renderMassiveStar(renderer, screenW, screenH);
            break;
        case Phase::FullBlackout:
            renderFullBlackout(renderer, screenW, screenH);
            break;
        case Phase::FinalShake:
        case Phase::Complete:
            break;
    }
}

bool LyooPlotTwistPresentation::isComplete() const {
    return elapsedTime_ >= kCompleteTimeSeconds;
}

bool LyooPlotTwistPresentation::overridesCamera() const {
    return true;
}

void LyooPlotTwistPresentation::applyCameraState(Camera3D& camera) const {
    camera = frontCamera_;

    const Phase phase = currentPhase();
    if (phase == Phase::IntroFrames || phase == Phase::TransitionToVoid) {
        return;
    }

    if (phase == Phase::RingStars) {
        const float localT = easing::clamp01((elapsedTime_ - kRingStartSeconds) / kRingDurationSeconds);
        const float tiltT = easing::clamp01((localT - 0.70f) / 0.30f);
        const float easedTilt = easeInOutCubicLocal(tiltT);

        camera.posY = lerpF(frontCamera_.posY, skyCamera_.posY, easedTilt);
        camera.posZ = lerpF(frontCamera_.posZ, skyCamera_.posZ, easedTilt);
        camera.pitchDegrees = lerpF(frontCamera_.pitchDegrees, skyCamera_.pitchDegrees, easedTilt);
        camera.focalLength = lerpF(frontCamera_.focalLength, skyCamera_.focalLength, easedTilt);
        return;
    }

    if (phase == Phase::SkyStars) {
        const float localT = easing::clamp01((elapsedTime_ - kSkyStartSeconds) / kSkyDurationSeconds);
        camera = skyCamera_;
        camera.yawDegrees += std::sin(localT * 5.0f) * 0.8f;
        camera.pitchDegrees += std::cos(localT * 4.0f) * 0.6f;
        return;
    }

    if (phase == Phase::ApproachAllies) {
        const float localT = easing::clamp01((elapsedTime_ - kApproachStartSeconds) / kApproachDurationSeconds);
        const float eased = easeInOutCubicLocal(localT);
        camera.posX = lerpF(skyCamera_.posX, wideCamera_.posX, eased);
        camera.posY = lerpF(skyCamera_.posY, wideCamera_.posY, eased);
        camera.posZ = lerpF(skyCamera_.posZ, wideCamera_.posZ, eased);
        camera.pitchDegrees = lerpF(skyCamera_.pitchDegrees, wideCamera_.pitchDegrees, eased);
        camera.yawDegrees = lerpF(skyCamera_.yawDegrees, wideCamera_.yawDegrees, eased);
        camera.focalLength = lerpF(skyCamera_.focalLength, wideCamera_.focalLength, eased);
        applyShake(camera, elapsedTime_, lerpF(8.0f, 22.0f, localT));
        return;
    }

    camera = wideCamera_;
    if (phase == Phase::MassiveStar) {
        applyShake(camera, elapsedTime_, 14.0f);
        return;
    }
    if (phase == Phase::FinalShake || phase == Phase::Complete) {
        applyShake(camera, elapsedTime_, 86.0f);
        applyShake(camera, elapsedTime_ * 1.75f, 42.0f);
    }
}

bool LyooPlotTwistPresentation::shouldHideNonCasterCharacters() const {
    return elapsedTime_ < kApproachStartSeconds;
}

bool LyooPlotTwistPresentation::shouldRenderCasterEntity() const {
    return false;
}

bool LyooPlotTwistPresentation::shouldRenderAboveHud() const {
    const Phase phase = currentPhase();
    return phase == Phase::MassiveStar || phase == Phase::FullBlackout;
}

bool LyooPlotTwistPresentation::shouldBlackoutWorld() const {
    const Phase phase = currentPhase();
    return phase == Phase::RingStars || phase == Phase::SkyStars;
}

bool LyooPlotTwistPresentation::shouldRenderFloor() const {
    return true;
}

int LyooPlotTwistPresentation::consumeAbilityAudioCues() {
    const int cues = pendingAbilityAudioCues_;
    pendingAbilityAudioCues_ = 0;
    return cues;
}

int LyooPlotTwistPresentation::consumeHitEvents() {
    const int hits = pendingHitEvents_;
    pendingHitEvents_ = 0;
    return hits;
}

int LyooPlotTwistPresentation::getDamageLabelHitCount() const {
    return kFinalShakeHitBursts;
}

std::vector<PresentationAudioCommand> LyooPlotTwistPresentation::consumeAudioCommands() {
    std::vector<PresentationAudioCommand> commands;
    commands.swap(pendingAudioCommands_);
    return commands;
}

LyooPlotTwistPresentation::NativeState LyooPlotTwistPresentation::buildNativeState() const {
    auto copyStars = [](const std::vector<StarInstance>& source) {
        std::vector<NativeStarState> result;
        result.reserve(source.size());
        for (const StarInstance& star : source) {
            result.push_back(NativeStarState{
                star.spawnTime,
                star.lifetime,
                star.orbitAngleDegrees,
                star.orbitRadius,
                star.baseSize,
                star.rotationSpeed,
                star.normalizedX,
                star.normalizedY,
                star.endNormalizedX,
                star.endNormalizedY
            });
        }
        return result;
    };

    NativeState state;
    switch (currentPhase()) {
        case Phase::IntroFrames:
            state.phase = NativePhase::IntroFrames;
            break;
        case Phase::TransitionToVoid:
            state.phase = NativePhase::TransitionToVoid;
            break;
        case Phase::RingStars:
            state.phase = NativePhase::RingStars;
            break;
        case Phase::SkyStars:
            state.phase = NativePhase::SkyStars;
            break;
        case Phase::ApproachAllies:
            state.phase = NativePhase::ApproachAllies;
            break;
        case Phase::MassiveStar:
            state.phase = NativePhase::MassiveStar;
            break;
        case Phase::FullBlackout:
            state.phase = NativePhase::FullBlackout;
            break;
        case Phase::FinalShake:
            state.phase = NativePhase::FinalShake;
            break;
        case Phase::Complete:
        default:
            state.phase = NativePhase::Complete;
            break;
    }
    state.introFrameIndex = currentIntroFrameIndex();
    state.elapsedTime = elapsedTime_;
    state.ringStars = copyStars(ringStars_);
    state.skyStars = copyStars(skyStars_);
    state.scatterStars = copyStars(scatterStars_);
    return state;
}

float LyooPlotTwistPresentation::computeNativeStarScale(float progress) const {
    return starScaleForLifeProgress(progress);
}

std::string LyooPlotTwistPresentation::resolvePath(const std::string& relativePath) {
    const std::array<std::string, 3> candidates = {
        relativePath,
        "../" + relativePath,
        "../../" + relativePath
    };

    for (const std::string& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return relativePath;
}

void LyooPlotTwistPresentation::queueAudioCommand(PresentationAudioCommandType type,
                                                  const std::string& id,
                                                  float volume) {
    pendingAudioCommands_.push_back(PresentationAudioCommand{type, id, volume});
}

void LyooPlotTwistPresentation::ensureAssetsLoaded(SDL_Renderer* renderer) {
    if (renderer == nullptr) {
        return;
    }

    if (loadedRenderer_ != nullptr && loadedRenderer_ != renderer) {
        releaseAssets();
        attemptedLoad_ = false;
        loadedRenderer_ = nullptr;
    }

    if (attemptedLoad_) {
        return;
    }
    attemptedLoad_ = true;
    loadedRenderer_ = renderer;

    frames_.assign(kFrameCount, nullptr);

#ifdef BATTLE_ENABLE_IMAGE
    for (int i = 0; i < kFrameCount; ++i) {
        char frameName[160];
        std::snprintf(frameName, sizeof(frameName),
                      "assets/combat/presentations/lyooPlotTwist/frame%04d.png", i);
        const std::string path = resolvePath(frameName);
        if (!std::filesystem::exists(path)) {
            continue;
        }

        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface == nullptr) {
            continue;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        frames_[static_cast<size_t>(i)] = texture;
    }

    const std::string starPath = resolvePath("assets/combat/presentations/lyooPlotTwist/star.png");
    if (std::filesystem::exists(starPath)) {
        SDL_Surface* surface = IMG_Load(starPath.c_str());
        if (surface != nullptr) {
            starTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
            starTextureWidth_ = surface->w;
            starTextureHeight_ = surface->h;
            SDL_FreeSurface(surface);
        }
    }
#else
    (void)renderer;
#endif
}

void LyooPlotTwistPresentation::releaseAssets() {
    for (SDL_Texture*& texture : frames_) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }
    frames_.clear();

    if (starTexture_ != nullptr) {
        SDL_DestroyTexture(starTexture_);
        starTexture_ = nullptr;
    }

    starTextureWidth_ = 0;
    starTextureHeight_ = 0;
    loadedRenderer_ = nullptr;
}

void LyooPlotTwistPresentation::seedStarTimelines() {
    ringStars_.clear();
    skyStars_.clear();
    scatterStars_.clear();

    ringStars_.reserve(10);
    for (int i = 0; i < 10; ++i) {
        StarInstance star;
        star.spawnTime = 0.12f + static_cast<float>(i) * 0.15f;
        star.lifetime = 1.25f;
        star.orbitAngleDegrees = -90.0f + static_cast<float>(i) * 36.0f;
        star.orbitRadius = kRingOrbitRadiusPx;
        star.baseSize = 34.0f + static_cast<float>(i % 3) * 8.0f;
        star.rotationSpeed = 84.0f + static_cast<float>(i) * 10.0f;
        ringStars_.push_back(star);
    }

    std::mt19937 rng(9071984u);
    std::uniform_real_distribution<float> xDist(0.08f, 0.92f);
    std::uniform_real_distribution<float> yDist(0.10f, 0.70f);
    std::uniform_real_distribution<float> sizeDist(18.0f, 42.0f);
    std::uniform_real_distribution<float> spinDist(180.0f, 520.0f);

    skyStars_.reserve(28);
    for (int i = 0; i < 28; ++i) {
        StarInstance star;
        star.spawnTime = 0.04f + static_cast<float>(i) * 0.045f;
        star.lifetime = 0.50f;
        star.normalizedX = xDist(rng);
        star.normalizedY = yDist(rng);
        star.baseSize = sizeDist(rng);
        star.rotationSpeed = spinDist(rng);
        skyStars_.push_back(star);
    }

    std::uniform_real_distribution<float> scatterStartX(0.20f, 0.80f);
    std::uniform_real_distribution<float> scatterStartY(0.14f, 0.86f);
    std::uniform_real_distribution<float> scatterDrift(-0.28f, 0.28f);
    std::uniform_real_distribution<float> scatterSize(26.0f, 72.0f);
    std::uniform_real_distribution<float> scatterSpin(90.0f, 260.0f);

    scatterStars_.reserve(96);
    for (int i = 0; i < 96; ++i) {
        StarInstance star;
        star.spawnTime = 0.01f + static_cast<float>(i) * 0.016f;
        star.lifetime = 1.55f;
        star.normalizedX = scatterStartX(rng);
        star.normalizedY = scatterStartY(rng);
        star.endNormalizedX = std::clamp(star.normalizedX + scatterDrift(rng), 0.03f, 0.97f);
        star.endNormalizedY = std::clamp(star.normalizedY + scatterDrift(rng), 0.03f, 0.97f);
        star.baseSize = scatterSize(rng) * 0.82f;
        star.rotationSpeed = scatterSpin(rng) * 0.78f;
        scatterStars_.push_back(star);
    }
}

LyooPlotTwistPresentation::Phase LyooPlotTwistPresentation::currentPhase() const {
    if (elapsedTime_ < kTransitionStartSeconds) {
        return Phase::IntroFrames;
    }
    if (elapsedTime_ < kRingStartSeconds) {
        return Phase::TransitionToVoid;
    }
    if (elapsedTime_ < kSkyStartSeconds) {
        return Phase::RingStars;
    }
    if (elapsedTime_ < kApproachStartSeconds) {
        return Phase::SkyStars;
    }
    if (elapsedTime_ < kMassiveStarStartSeconds) {
        return Phase::ApproachAllies;
    }
    if (elapsedTime_ < kBlackoutStartSeconds) {
        return Phase::MassiveStar;
    }
    if (elapsedTime_ < kFinalShakeStartSeconds) {
        return Phase::FullBlackout;
    }
    if (elapsedTime_ < kCompleteTimeSeconds) {
        return Phase::FinalShake;
    }
    return Phase::Complete;
}

int LyooPlotTwistPresentation::currentIntroFrameIndex() const {
    for (size_t i = 0; i < kIntroFrameEndTimes.size(); ++i) {
        if (elapsedTime_ < kIntroFrameEndTimes[i]) {
            return static_cast<int>(i);
        }
    }
    return 9;
}

void LyooPlotTwistPresentation::renderIntroFrames(SDL_Renderer* renderer, int screenW, int screenH) {
    const int frameIndex = std::clamp(currentIntroFrameIndex(), 0, 9);
    SDL_Texture* frame = frameIndex < static_cast<int>(frames_.size())
        ? frames_[static_cast<size_t>(frameIndex)]
        : nullptr;

    if (frame != nullptr) {
        SDL_Rect dst{0, 0, screenW, screenH};
        SDL_RenderCopy(renderer, frame, nullptr, &dst);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
    SDL_Rect full{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &full);
}

void LyooPlotTwistPresentation::renderTransitionToVoid(SDL_Renderer* renderer, int screenW, int screenH) {
    const float localT = easing::clamp01((elapsedTime_ - kTransitionStartSeconds) / kTransitionDurationSeconds);
    const float eased = easing::easeInCubic(localT);
    const Uint8 blackAlpha = static_cast<Uint8>(std::lround(lerpF(0.0f, 255.0f, eased)));

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, blackAlpha);
    SDL_Rect full{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &full);

    SDL_Texture* frame = nullptr;
    if (frames_.size() > 10) {
        // Keep frame0009 stable; only start the "fake sprite zoom-out" once frame0010 is reached.
        constexpr float kZoomStartT = 0.28f;
        frame = frames_[localT < kZoomStartT ? 9 : 10];
    }

    constexpr float kZoomStartT = 0.28f;
    const float zoomT = localT < kZoomStartT
        ? 0.0f
        : easing::clamp01((localT - kZoomStartT) / std::max(0.001f, 1.0f - kZoomStartT));
    const float sizeT = std::pow(std::max(0.0f, 1.0f - zoomT), 3.4f);
    const float height = std::max(1.0f, static_cast<float>(screenH) * 0.76f * sizeT);
    const float width = height * (320.0f / 170.0f);
    drawCenteredTexture(
        renderer,
        frame,
        static_cast<float>(screenW) * 0.5f,
        static_cast<float>(screenH) * kTransitionScreenCenterY,
        width,
        height,
        0.0,
        static_cast<Uint8>(255.0f * (1.0f - zoomT * 0.2f))
    );
}

void LyooPlotTwistPresentation::renderRingStars(SDL_Renderer* renderer,
                                                int screenW,
                                                int screenH,
                                                const Camera3D& camera) {
    const float ringPhaseT = easing::clamp01((elapsedTime_ - kRingStartSeconds) / kRingDurationSeconds);
    const float centerX = static_cast<float>(screenW) * kRingScreenCenterX;
    const float centerY = lerpF(
        static_cast<float>(screenH) * kRingScreenCenterY,
        static_cast<float>(screenH) * 1.10f,
        easeInOutCubicLocal(easing::clamp01((ringPhaseT - 0.34f) / 0.50f))
    );
    const float phaseTime = elapsedTime_ - kRingStartSeconds;
    const float baseRadius = kRingOrbitRadiusPx * lerpF(0.95f, 1.04f, std::sin(phaseTime * 1.7f) * 0.5f + 0.5f);

    (void)camera;
    for (const StarInstance& star : ringStars_) {
        const float localTime = phaseTime - star.spawnTime;
        if (localTime < 0.0f || localTime > star.lifetime) {
            continue;
        }

        const float progress = easing::clamp01(localTime / std::max(0.001f, star.lifetime));
        const float scale = starScaleForLifeProgress(progress);
        if (scale <= 0.001f) {
            continue;
        }

        const float angleRadians = star.orbitAngleDegrees * (3.14159265f / 180.0f);
        const float starX = centerX + std::cos(angleRadians) * baseRadius;
        const float starY = centerY + std::sin(angleRadians) * baseRadius * 0.72f;
        const double rotation = static_cast<double>(localTime * star.rotationSpeed);
        const float size = std::max(1.0f, star.baseSize * scale);
        const Uint8 alpha = static_cast<Uint8>(std::lround(255.0f * std::min(1.0f, scale)));
        drawStarSprite(renderer, starX, starY, size, rotation, alpha);
    }
}

void LyooPlotTwistPresentation::renderSkyStars(SDL_Renderer* renderer, int screenW, int screenH) {
    const float phaseTime = elapsedTime_ - kSkyStartSeconds;
    for (const StarInstance& star : skyStars_) {
        const float localTime = phaseTime - star.spawnTime;
        if (localTime < 0.0f || localTime > star.lifetime) {
            continue;
        }

        const float progress = easing::clamp01(localTime / std::max(0.001f, star.lifetime));
        const float scale = starScaleForLifeProgress(progress);
        if (scale <= 0.001f) {
            continue;
        }

        const float size = std::max(1.0f, star.baseSize * scale);
        const double rotation = static_cast<double>(localTime * star.rotationSpeed);
        const Uint8 alpha = static_cast<Uint8>(std::lround(255.0f * std::min(1.0f, scale)));
        drawStarSprite(
            renderer,
            star.normalizedX * static_cast<float>(screenW),
            star.normalizedY * static_cast<float>(screenH),
            size,
            rotation,
            alpha
        );
    }
}

void LyooPlotTwistPresentation::renderApproachOverlay(SDL_Renderer* renderer, int screenW, int screenH) {
    const float localT = easing::clamp01((elapsedTime_ - kApproachStartSeconds) / kApproachDurationSeconds);
    const Uint8 alpha = static_cast<Uint8>(std::lround(lerpF(kApproachDarknessAlpha, 25.0f, localT)));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
    SDL_Rect full{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &full);
}

void LyooPlotTwistPresentation::renderScatterStars(SDL_Renderer* renderer, int screenW, int screenH) {
    const float phaseTime = elapsedTime_ - kMassiveStarStartSeconds;
    for (const StarInstance& star : scatterStars_) {
        const float localTime = phaseTime - star.spawnTime;
        if (localTime < 0.0f || localTime > star.lifetime) {
            continue;
        }

        const float progress = easing::clamp01(localTime / std::max(0.001f, star.lifetime));
        const float moveT = easeInOutCubicLocal(progress);
        const float x = lerpF(star.normalizedX, star.endNormalizedX, moveT) * static_cast<float>(screenW);
        const float y = lerpF(star.normalizedY, star.endNormalizedY, moveT) * static_cast<float>(screenH);
        const float sizeGrowth = lerpF(0.05f, 1.95f, easing::easeOutCubic(progress));
        const float size = std::max(1.0f, star.baseSize * sizeGrowth);
        const double rotation = static_cast<double>(localTime * star.rotationSpeed);
        const float fadeOut = progress < 0.82f ? 1.0f : (1.0f - (progress - 0.82f) / 0.18f);
        const Uint8 alpha = static_cast<Uint8>(std::lround(220.0f * std::clamp(fadeOut, 0.0f, 1.0f)));
        drawStarSprite(renderer, x, y, size, rotation, alpha);
    }
}

void LyooPlotTwistPresentation::renderMassiveStar(SDL_Renderer* renderer, int screenW, int screenH) {
    const float localT = easing::clamp01((elapsedTime_ - kMassiveStarStartSeconds) / kMassiveStarDurationSeconds);
    const float giantStartT = easing::clamp01((localT - 0.40f) / 0.60f);
    if (giantStartT <= 0.0f) {
        return;
    }

    const float eased = easing::easeInQuint(giantStartT);
    const float size = lerpF(1.0f, static_cast<float>(std::max(screenW, screenH)) * 3.3f, eased);
    const double rotation = static_cast<double>(-28.0f + giantStartT * 260.0f);
    const float alphaT = giantStartT < 0.74f
        ? easing::easeInCubic(giantStartT / 0.74f)
        : 1.0f;
    const Uint8 alpha = static_cast<Uint8>(std::lround(lerpF(24.0f, 255.0f, alphaT)));
    drawStarSprite(renderer,
                   static_cast<float>(screenW) * 0.5f,
                   static_cast<float>(screenH) * 0.5f,
                   size,
                   rotation,
                   alpha);
}

void LyooPlotTwistPresentation::renderFullBlackout(SDL_Renderer* renderer, int screenW, int screenH) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect full{0, 0, screenW, screenH};
    SDL_RenderFillRect(renderer, &full);
}

void LyooPlotTwistPresentation::drawCenteredTexture(SDL_Renderer* renderer,
                                                    SDL_Texture* texture,
                                                    float centerX,
                                                    float centerY,
                                                    float width,
                                                    float height,
                                                    double angleDegrees,
                                                    Uint8 alpha) const {
    SDL_FRect dst{
        centerX - width * 0.5f,
        centerY - height * 0.5f,
        width,
        height
    };

    if (texture != nullptr) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderCopyExF(renderer, texture, nullptr, &dst, angleDegrees, nullptr, SDL_FLIP_NONE);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
    SDL_RenderFillRectF(renderer, &dst);
}

void LyooPlotTwistPresentation::drawStarSprite(SDL_Renderer* renderer,
                                               float centerX,
                                               float centerY,
                                               float size,
                                               double angleDegrees,
                                               Uint8 alpha) const {
    const float width = size;
    const float height = size;
    if (starTexture_ != nullptr) {
        drawCenteredTexture(renderer, starTexture_, centerX, centerY, width, height, angleDegrees, alpha);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
    SDL_FRect vertical{centerX - size * 0.08f, centerY - size * 0.5f, size * 0.16f, size};
    SDL_FRect horizontal{centerX - size * 0.5f, centerY - size * 0.08f, size, size * 0.16f};
    SDL_RenderFillRectF(renderer, &vertical);
    SDL_RenderFillRectF(renderer, &horizontal);
}

float LyooPlotTwistPresentation::starScaleForLifeProgress(float progress) const {
    const float t = easing::clamp01(progress);
    if (t < 0.32f) {
        return lerpF(0.02f, 1.25f, easing::easeOutCubic(t / 0.32f));
    }
    if (t < 0.52f) {
        return lerpF(1.25f, 0.55f, (t - 0.32f) / 0.20f);
    }
    if (t < 0.84f) {
        return 0.55f;
    }
    return lerpF(0.55f, 0.0f, (t - 0.84f) / 0.16f);
}

void LyooPlotTwistPresentation::applyShake(Camera3D& camera, float timeSeconds, float magnitude) const {
    camera.posX += std::sin(timeSeconds * 26.0f) * magnitude;
    camera.posY += std::cos(timeSeconds * 22.0f) * magnitude * 0.72f;
    camera.posZ += std::sin(timeSeconds * 31.0f) * magnitude * 0.55f;
    camera.yawDegrees += std::sin(timeSeconds * 18.0f) * magnitude * 0.06f;
    camera.pitchDegrees += std::cos(timeSeconds * 16.0f) * magnitude * 0.05f;
}

} // namespace battle
