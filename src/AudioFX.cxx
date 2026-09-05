#include <AppGameToolbox/AudioFX.hpp>

#include <algorithm>
#include <utility>

namespace appgametoolbox {

AudioFX::AudioFX(std::string name) : m_name(std::move(name)) {}
const std::string& AudioFX::name() const { return m_name; }
void AudioFX::setName(std::string name) { m_name = std::move(name); }
const std::string& AudioFX::source() const { return m_source; }
void AudioFX::setSource(std::string source) { m_source = std::move(source); }
AudioFXCategory AudioFX::category() const { return m_category; }
void AudioFX::setCategory(AudioFXCategory category) { m_category = category; }
bool AudioFX::isSpatialized() const { return m_spatialized; }
void AudioFX::setSpatialized(bool spatialized) { m_spatialized = spatialized; }
void AudioFX::setGain(float gain) { m_gain = std::clamp(gain, 0.0f, 1.0f); }
float AudioFX::gain() const { return m_gain; }
const Vector3& AudioFX::position() const { return m_position; }
void AudioFX::setPosition(const Vector3& position) { m_position = position; }
void AudioFX::setDistanceAttenuation(float referenceDistance, float rolloffFactor) {
    m_referenceDistance = std::max(0.0f, referenceDistance);
    m_rolloffFactor = std::max(0.0f, rolloffFactor);
}
void AudioFX::apply(AudioPlayback& playback) const { playback.setVolume(m_gain); }
void AudioFX::apply(SpatialAudioPlayback& playback) const {
    apply(static_cast<AudioPlayback&>(playback));
    playback.setPosition(m_position);
    playback.setDistanceAttenuation(m_referenceDistance, m_rolloffFactor);
}
bool AudioFX::play(AudioPlayback& standardPlayback, SpatialAudioPlayback* spatialPlayback) const {
    AudioPlayback* playback = &standardPlayback;
    if (m_spatialized && spatialPlayback != nullptr) playback = spatialPlayback;
    if (m_source.empty() || !playback->load(m_source)) return false;
    if (m_spatialized && spatialPlayback != nullptr) apply(*spatialPlayback);
    else apply(*playback);
    return playback->play();
}

} // namespace appgametoolbox
