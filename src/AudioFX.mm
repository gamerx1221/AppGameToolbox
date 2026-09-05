#include <AppGameToolbox/AudioFX.hpp>

#include <algorithm>
#include <utility>

namespace appgametoolbox {

AudioFX::AudioFX(std::string name) : m_name(std::move(name)) {}
bool AudioFX::load(const char* pathOrUrl) {
    m_source = pathOrUrl == nullptr ? "" : pathOrUrl;
    const bool standardLoaded = m_player.load(pathOrUrl);
    const bool spatialLoaded = m_spatialPlayer.load(pathOrUrl);
    if (!standardLoaded || !spatialLoaded) { m_error = standardLoaded ? m_spatialPlayer.lastError() : m_player.lastError(); return false; }
    setGain(m_gain); setPosition(m_position); m_error.clear(); return true;
}
bool AudioFX::play() {
    const bool played = m_spatialized ? m_spatialPlayer.play() : m_player.play();
    if (!played) m_error = m_spatialized ? m_spatialPlayer.lastError() : m_player.lastError();
    return played;
}
void AudioFX::pause() { if (m_spatialized) m_spatialPlayer.pause(); else m_player.pause(); }
void AudioFX::stop() { m_player.stop(); m_spatialPlayer.stop(); }
const std::string& AudioFX::name() const { return m_name; }
void AudioFX::setName(std::string name) { m_name = std::move(name); }
const std::string& AudioFX::source() const { return m_source; }
AudioFXCategory AudioFX::category() const { return m_category; }
void AudioFX::setCategory(AudioFXCategory category) { m_category = category; }
bool AudioFX::isSpatialized() const { return m_spatialized; }
void AudioFX::setSpatialized(bool spatialized) { if (m_spatialized != spatialized) { stop(); m_spatialized = spatialized; } }
bool AudioFX::isLoaded() const { return m_spatialized ? m_spatialPlayer.isLoaded() : m_player.isLoaded(); }
bool AudioFX::isPlaying() const { return m_spatialized ? m_spatialPlayer.isPlaying() : m_player.isPlaying(); }
void AudioFX::setGain(float gain) { m_gain = std::clamp(gain, 0.0f, 1.0f); m_player.setVolume(m_gain); m_spatialPlayer.setVolume(m_gain); }
float AudioFX::gain() const { return m_gain; }
const Vector3& AudioFX::position() const { return m_position; }
void AudioFX::setPosition(const Vector3& position) { m_position = position; m_spatialPlayer.setPosition(position.x, position.y, position.z); }
void AudioFX::setDistanceAttenuation(float referenceDistance, float rolloffFactor) { m_spatialPlayer.setDistanceAttenuation(referenceDistance, rolloffFactor); }
const std::string& AudioFX::lastError() const { return m_error; }

} // namespace appgametoolbox
