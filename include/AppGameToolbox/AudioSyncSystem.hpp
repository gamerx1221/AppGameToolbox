#pragma once

#include "AudioTransport.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace appgametoolbox {

struct AudioSyncCue { double time = 0.0; double duration = 0.0; std::string id; };
enum class AudioSyncPhase { Started, Ended };
using AudioSyncAction = std::function<void(const AudioSyncCue&, AudioSyncPhase)>;

class AudioSyncSystem {
public:
    explicit AudioSyncSystem(AudioTransport& transport);
    std::uint64_t addCue(AudioSyncCue cue, AudioSyncAction action);
    bool removeCue(std::uint64_t handle);
    void clear();
    void reset();
    void update();

private:
    struct Binding {
        std::uint64_t handle = 0;
        AudioSyncCue cue;
        AudioSyncAction action;
        bool started = false;
        bool ended = false;
    };
    AudioTransport& m_transport;
    std::vector<Binding> m_bindings;
    std::uint64_t m_nextHandle = 1;
    double m_lastTime = -1.0;
};

} // namespace appgametoolbox
