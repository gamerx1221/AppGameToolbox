#pragma once

#include "AudioPlayback.hpp"
#include "Types.hpp"

namespace appgametoolbox {

class SpatialAudioPlayback : public AudioPlayback {
public:
    virtual void setPosition(const Vector3& position) = 0;
    virtual void setListenerPosition(const Vector3& position) = 0;
    virtual void setDistanceAttenuation(float referenceDistance, float rolloffFactor) = 0;
};

} // namespace appgametoolbox
