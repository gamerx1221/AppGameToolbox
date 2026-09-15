#include "SQBAudioPreamp/AudioBuffer.hpp"

namespace sqb::audio {
AudioBuffer::AudioBuffer(std::size_t channels, std::size_t frames, unsigned sampleRate)
    : channels_(channels), sampleRate_(sampleRate), samples_(channels * frames) {}
} // namespace sqb::audio
