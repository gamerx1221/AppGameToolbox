#include <AppGameToolbox/AudioSyncSystem.hpp>

#include <algorithm>
#include <utility>

namespace appgametoolbox {

AudioSyncSystem::AudioSyncSystem(AudioTransport& transport) : m_transport(transport) {}
std::uint64_t AudioSyncSystem::addCue(AudioSyncCue cue, AudioSyncAction action) {
    if (cue.time < 0.0 || cue.duration < 0.0 || !action) return 0;
    const std::uint64_t handle = m_nextHandle++;
    m_bindings.push_back({handle, std::move(cue), std::move(action)});
    return handle;
}
bool AudioSyncSystem::removeCue(std::uint64_t handle) {
    const auto it = std::remove_if(m_bindings.begin(), m_bindings.end(), [handle](const Binding& binding) { return binding.handle == handle; });
    if (it == m_bindings.end()) return false;
    m_bindings.erase(it, m_bindings.end());
    return true;
}
void AudioSyncSystem::clear() { m_bindings.clear(); m_lastTime = -1.0; }
void AudioSyncSystem::reset() {
    for (Binding& binding : m_bindings) { binding.started = false; binding.ended = false; }
    m_lastTime = -1.0;
}
void AudioSyncSystem::update() {
    const double currentTime = m_transport.currentTime();
    if (currentTime < m_lastTime) { m_lastTime = currentTime; return; }
    struct Event { double time; Binding* binding; AudioSyncPhase phase; };
    std::vector<Event> events;
    for (Binding& binding : m_bindings) {
        if (!binding.started && binding.cue.time > m_lastTime && binding.cue.time <= currentTime) {
            binding.started = true; events.push_back({binding.cue.time, &binding, AudioSyncPhase::Started});
        }
        const double endTime = binding.cue.time + binding.cue.duration;
        if (binding.cue.duration > 0.0 && !binding.ended && endTime > m_lastTime && endTime <= currentTime) {
            binding.ended = true; events.push_back({endTime, &binding, AudioSyncPhase::Ended});
        }
    }
    std::sort(events.begin(), events.end(), [](const Event& left, const Event& right) { return left.time < right.time; });
    for (const Event& event : events) event.binding->action(event.binding->cue, event.phase);
    m_lastTime = currentTime;
}

} // namespace appgametoolbox
