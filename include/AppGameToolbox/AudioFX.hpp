#pragma once

#include "AudioPlayer.hpp"
#include "SpatialAudioPlayer.hpp"
#include "Types.hpp"

#include <string>

namespace appgametoolbox {

enum class AudioFXCategory { UI, Effect, Ambient };

class AudioFX {
public:
    explicit AudioFX(std::string name = {});
    AudioFX(const AudioFX&) = delete;
    AudioFX& operator=(const AudioFX&) = delete;

    bool load(const char* pathOrUrl);
    bool play();
    void pause();
    void stop();
    const std::string& name() const;
    void setName(std::string name);
    const std::string& source() const;
    AudioFXCategory category() const;
    void setCategory(AudioFXCategory category);
    bool isSpatialized() const;
    void setSpatialized(bool spatialized);
    bool isLoaded() const;
    bool isPlaying() const;
    void setGain(float gain);
    float gain() const;
    const Vector3& position() const;
    void setPosition(const Vector3& position);
    void setDistanceAttenuation(float referenceDistance, float rolloffFactor);
    const std::string& lastError() const;

private:
    std::string m_name;
    std::string m_source;
    std::string m_error;
    AudioFXCategory m_category = AudioFXCategory::Effect;
    bool m_spatialized = false;
    float m_gain = 1.0f;
    Vector3 m_position;
    AudioPlayer m_player;
    SpatialAudioPlayer m_spatialPlayer;
};

} // namespace appgametoolbox
