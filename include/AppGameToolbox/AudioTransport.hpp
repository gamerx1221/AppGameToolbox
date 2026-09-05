#pragma once

#include "AudioPlayback.hpp"

namespace appgametoolbox {

// A playback backend that exposes a sample-accurate timeline for game sync.
class AudioTransport : public AudioPlayback {
public:
    virtual bool seek(double seconds) = 0;
    virtual double currentTime() const = 0;
    virtual double duration() const = 0;
};

} // namespace appgametoolbox
