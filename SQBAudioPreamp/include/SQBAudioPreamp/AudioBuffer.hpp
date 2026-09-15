#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace sqb::audio {

/// Interleaved, normalized floating-point PCM.  One buffer owns one continuous stream.
class AudioBuffer final {
public:
    AudioBuffer() = default;
    AudioBuffer(std::size_t channels, std::size_t frames, unsigned sampleRate);

    [[nodiscard]] std::size_t channels() const noexcept { return channels_; }
    [[nodiscard]] std::size_t frames() const noexcept { return channels_ == 0 ? 0 : samples_.size() / channels_; }
    [[nodiscard]] unsigned sampleRate() const noexcept { return sampleRate_; }
    void setSampleRate(unsigned value) noexcept { sampleRate_ = value; }
    [[nodiscard]] std::span<float> samples() noexcept { return samples_; }
    [[nodiscard]] std::span<const float> samples() const noexcept { return samples_; }
    [[nodiscard]] float& at(std::size_t frame, std::size_t channel) noexcept { return samples_[frame * channels_ + channel]; }
    [[nodiscard]] float at(std::size_t frame, std::size_t channel) const noexcept { return samples_[frame * channels_ + channel]; }

private:
    std::size_t channels_{};
    unsigned sampleRate_{};
    std::vector<float> samples_;
};

} // namespace sqb::audio
