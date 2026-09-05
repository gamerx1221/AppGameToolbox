#pragma once

#include <memory>
#include <string>

namespace appgametoolbox {

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    bool load(const char* pathOrUrl);
    bool play();
    void pause();
    void stop();
    bool isLoaded() const;
    bool isPlaying() const;
    void setVolume(float volume);
    float volume() const;
    double duration() const;
    double currentTime() const;
    void setCurrentTime(double seconds);
    const std::string& lastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace appgametoolbox
