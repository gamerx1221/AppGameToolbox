#pragma once

#include <memory>
#include <string>

namespace appgametoolbox {

class AudioTransport {
public:
    AudioTransport();
    ~AudioTransport();
    AudioTransport(const AudioTransport&) = delete;
    AudioTransport& operator=(const AudioTransport&) = delete;

    bool load(const char* pathOrUrl);
    bool play();
    void pause();
    void stop();
    bool seek(double seconds);
    bool isLoaded() const;
    bool isPlaying() const;
    double currentTime() const;
    double duration() const;
    void setVolume(float volume);
    float volume() const;
    const std::string& lastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace appgametoolbox
