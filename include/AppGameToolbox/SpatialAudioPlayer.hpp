#pragma once

#include <memory>
#include <string>

namespace appgametoolbox {

class SpatialAudioPlayer {
public:
    SpatialAudioPlayer();
    ~SpatialAudioPlayer();
    SpatialAudioPlayer(const SpatialAudioPlayer&) = delete;
    SpatialAudioPlayer& operator=(const SpatialAudioPlayer&) = delete;

    bool load(const char* pathOrUrl);
    bool play();
    void pause();
    void stop();
    bool isLoaded() const;
    bool isPlaying() const;
    void setVolume(float volume);
    float volume() const;
    void setPosition(float x, float y, float z);
    void setListenerPosition(float x, float y, float z);
    void setDistanceAttenuation(float referenceDistance, float rolloffFactor);
    const std::string& lastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace appgametoolbox
