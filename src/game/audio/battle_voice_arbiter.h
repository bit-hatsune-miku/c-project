#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace game::audio {

enum class BattleVoiceKind {
    Dialogue,
    UltimateActivation,
    Ultimate,
    Ability,
    Hit,
    Healed,
    Shielded,
    Revived,
    Dead,
    Idle
};

enum class BattleVoiceChannel {
    OneShot,
    Vn
};

struct BattleVoicePlayback {
    std::string speakerKey;
    BattleVoiceKind kind = BattleVoiceKind::Idle;
    BattleVoiceChannel channel = BattleVoiceChannel::OneShot;
    std::uint64_t handleId = 0;
};

class BattleVoiceArbiter {
public:
    struct Request {
        std::string speakerKey;
        BattleVoiceKind kind = BattleVoiceKind::Idle;
        BattleVoiceChannel channel = BattleVoiceChannel::OneShot;
    };

    struct Callbacks {
        std::function<bool(const BattleVoicePlayback&)> isPlaybackActive;
        std::function<void(const BattleVoicePlayback&)> stopPlayback;
        std::function<std::optional<std::uint64_t>()> startPlayback;
    };

    bool request(const Request& request, const Callbacks& callbacks) {
        if (request.speakerKey.empty()) {
            if (!callbacks.startPlayback) {
                return false;
            }
            return callbacks.startPlayback().has_value();
        }

        SpeakerState& state = speakers_[request.speakerKey];
        cleanupInactivePlayback(state, callbacks);

        const bool isRevive = request.kind == BattleVoiceKind::Revived;
        if (state.deathLocked && !isRevive) {
            return false;
        }

        if (state.active.has_value()) {
            const bool shouldBypassPriority = isRevive && state.deathLocked;
            const bool canRetriggerActive =
                request.kind == state.active->kind &&
                canRetriggerKind(request.kind);
            if (!shouldBypassPriority &&
                !canRetriggerActive &&
                priorityFor(request.kind) <= priorityFor(state.active->kind)) {
                return false;
            }

            if (callbacks.stopPlayback) {
                callbacks.stopPlayback(*state.active);
            }
            state.active.reset();
        }

        if (request.kind == BattleVoiceKind::Revived) {
            state.deathLocked = false;
        }

        std::optional<std::uint64_t> handleId;
        if (callbacks.startPlayback) {
            handleId = callbacks.startPlayback();
        }

        if (request.kind == BattleVoiceKind::Dead) {
            state.deathLocked = true;
        }

        if (handleId.has_value()) {
            state.active = BattleVoicePlayback{
                request.speakerKey,
                request.kind,
                request.channel,
                *handleId
            };
        } else {
            state.active.reset();
        }

        pruneSpeaker(request.speakerKey, state);
        return handleId.has_value();
    }

    void stopSpeaker(const std::string& speakerKey,
                     const std::function<void(const BattleVoicePlayback&)>& stopPlayback,
                     bool clearDeathLock = false) {
        const auto it = speakers_.find(speakerKey);
        if (it == speakers_.end()) {
            return;
        }

        if (it->second.active.has_value() && stopPlayback) {
            stopPlayback(*it->second.active);
        }
        it->second.active.reset();
        if (clearDeathLock) {
            it->second.deathLocked = false;
        }
        pruneSpeaker(it->first, it->second);
    }

    void stopAll(const std::function<void(const BattleVoicePlayback&)>& stopPlayback) {
        if (stopPlayback) {
            for (const auto& [speakerKey, state] : speakers_) {
                (void)speakerKey;
                if (state.active.has_value()) {
                    stopPlayback(*state.active);
                }
            }
        }
        speakers_.clear();
    }

    void clear() {
        speakers_.clear();
    }

private:
    struct SpeakerState {
        bool deathLocked = false;
        std::optional<BattleVoicePlayback> active;
    };

    static bool canRetriggerKind(BattleVoiceKind kind) {
        switch (kind) {
            case BattleVoiceKind::Ability:
            case BattleVoiceKind::Hit:
            case BattleVoiceKind::Healed:
            case BattleVoiceKind::Shielded:
                return true;
            case BattleVoiceKind::Dialogue:
            case BattleVoiceKind::UltimateActivation:
            case BattleVoiceKind::Ultimate:
            case BattleVoiceKind::Revived:
            case BattleVoiceKind::Dead:
            case BattleVoiceKind::Idle:
            default:
                return false;
        }
    }

    static int priorityFor(BattleVoiceKind kind) {
        switch (kind) {
            case BattleVoiceKind::Dead:
                return 7;
            case BattleVoiceKind::Revived:
                return 6;
            case BattleVoiceKind::Dialogue:
                return 5;
            case BattleVoiceKind::Ultimate:
                return 4;
            case BattleVoiceKind::Ability:
                return 3;
            case BattleVoiceKind::UltimateActivation:
            case BattleVoiceKind::Hit:
                return 2;
            case BattleVoiceKind::Healed:
            case BattleVoiceKind::Shielded:
                return 1;
            case BattleVoiceKind::Idle:
            default:
                return 0;
        }
    }

    static void cleanupInactivePlayback(SpeakerState& state, const Callbacks& callbacks) {
        if (!state.active.has_value() || !callbacks.isPlaybackActive) {
            return;
        }

        if (!callbacks.isPlaybackActive(*state.active)) {
            state.active.reset();
        }
    }

    void pruneSpeaker(const std::string& speakerKey, const SpeakerState& state) {
        if (!state.deathLocked && !state.active.has_value()) {
            speakers_.erase(speakerKey);
        }
    }

    std::unordered_map<std::string, SpeakerState> speakers_;
};

} // namespace game::audio
