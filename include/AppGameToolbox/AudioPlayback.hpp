#pragma once

#include <string>

namespace appgametoolbox {

// Platform-neutral contract for a single non-positional audio voice.
class AudioPlayback {
public:
    virtual ~AudioPlayback() = default;

    virtual bool load(const std::string& source) = 0;
    virtual bool play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual bool isLoaded() const = 0;
    virtual bool isPlaying() const = 0;
    virtual void setVolume(float volume) = 0;
    virtual float volume() const = 0;
    virtual const std::string& lastError() const = 0;
};

} // namespace appgametoolbox
