#pragma once

#include "AudioPlayback.hpp"
#include "SpatialAudioPlayback.hpp"
#include "Types.hpp"

#include <string>

namespace appgametoolbox {

enum class AudioFXCategory { UI, Effect, Ambient };

class AudioFX {
public:
    explicit AudioFX(std::string name = {});
    const std::string& name() const;
    void setName(std::string name);
    const std::string& source() const;
    void setSource(std::string source);
    AudioFXCategory category() const;
    void setCategory(AudioFXCategory category);
    bool isSpatialized() const;
    void setSpatialized(bool spatialized);
    void setGain(float gain);
    float gain() const;
    const Vector3& position() const;
    void setPosition(const Vector3& position);
    void setDistanceAttenuation(float referenceDistance, float rolloffFactor);
    void apply(AudioPlayback& playback) const;
    void apply(SpatialAudioPlayback& playback) const;
    bool play(AudioPlayback& standardPlayback, SpatialAudioPlayback* spatialPlayback = nullptr) const;

private:
    std::string m_name;
    std::string m_source;
    AudioFXCategory m_category = AudioFXCategory::Effect;
    bool m_spatialized = false;
    float m_gain = 1.0f;
    Vector3 m_position;
    float m_referenceDistance = 1.0f;
    float m_rolloffFactor = 1.0f;
};

} // namespace appgametoolbox
