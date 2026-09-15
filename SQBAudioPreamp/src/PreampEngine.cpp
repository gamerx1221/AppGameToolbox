#include "SQBAudioPreamp/PreampEngine.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sqb::audio {
namespace {
constexpr float kEpsilon = 1.0e-12F;
float clamp(float x, float lo, float hi) noexcept { return std::clamp(x, lo, hi); }
float dbToGain(float db) noexcept { return std::pow(10.0F, db / 20.0F); }
float defaultLufs(AssetClass kind) noexcept { return kind == AssetClass::Ui ? -8.F : kind == AssetClass::Sfx ? -12.F : -16.F; }
float onePoleCoefficient(float frequency, unsigned rate) noexcept {
    return std::exp(-2.0F * std::numbers::pi_v<float> * frequency / static_cast<float>(rate));
}
}

PreampEngine::PreampEngine(ModuleControls controls) : controls_(controls) {}
void PreampEngine::setControls(ModuleControls controls) noexcept { controls_ = controls; }
const ModuleControls& PreampEngine::controls() const noexcept { return controls_; }
void PreampEngine::prepare(unsigned rate, std::size_t channels, std::size_t) {
    preparedRate_ = rate; preparedChannels_ = channels;
    previous_.assign(channels, 0.F); envelope_.assign(channels, 0.F); dc_.assign(channels, 0.F);
}
ComputeBackend PreampEngine::backend() const noexcept {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    return ComputeBackend::Neon;
#elif defined(__AVX__) || defined(__SSE2__) || defined(_M_X64)
    return ComputeBackend::SseAvx;
#else
    return ComputeBackend::Scalar;
#endif
}

void PreampEngine::process(AudioBuffer& b) noexcept {
    if (b.channels() == 0 || b.sampleRate() == 0 || preparedRate_ != b.sampleRate() || preparedChannels_ != b.channels()) return;
    const float hpA = onePoleCoefficient(30.F, b.sampleRate());
    const float ceiling = dbToGain(controls_.ceilingDbfs);
    std::fill(dc_.begin(), dc_.end(), 0.F);
    for (std::size_t f = 0; f < b.frames(); ++f) {
        for (std::size_t c = 0; c < b.channels(); ++c) dc_[c] += b.at(f, c);
    }
    for (float& x : dc_) x /= static_cast<float>(std::max<std::size_t>(1, b.frames()));
    for (std::size_t f = 0; f < b.frames(); ++f) {
        for (std::size_t c = 0; c < b.channels(); ++c) {
            float x = b.at(f, c);
            if (controls_.dcRemoval) x -= dc_[c];
            if (controls_.rumbleFilter) { const float y = x - previous_[c] + hpA * envelope_[c]; previous_[c] = x; envelope_[c] = y; x = y; }
            const float delta = x - previous_[c];
            if (controls_.transientEnhancer) x += delta * controls_.transientSharpness * controls_.intensity;
            if (controls_.virtualBass) x += std::tanh(x * 2.0F) * 0.12F * controls_.virtualBassDepth * controls_.intensity;
            if (controls_.hfExciter) x += std::tanh(delta * 3.0F) * controls_.hfExciterAmount * controls_.intensity;
            if (controls_.loudnessContourEnabled) x *= 1.F + 0.18F * controls_.loudnessContour;
            // Soft clipping is the final runtime safety stage; it is continuous and bounded.
            if (controls_.limiter) x = ceiling * std::tanh(x / std::max(ceiling, kEpsilon));
            b.at(f, c) = x;
            previous_[c] = x;
        }
        if (controls_.stereoStabilizer && b.channels() == 2) {
            const float mid = (b.at(f, 0) + b.at(f, 1)) * .5F;
            const float side = (b.at(f, 0) - b.at(f, 1)) * .5F * clamp(controls_.stereoWidth, 0.F, 2.F);
            b.at(f, 0) = mid + side; b.at(f, 1) = mid - side;
        }
    }
}

AudioBuffer PreampEngine::conditionFile(AudioBuffer in, const OfflineOptions& options) const {
    ModuleControls settings = controls_; settings.targetLufs = defaultLufs(options.assetClass);
    PreampEngine worker(settings); worker.prepare(in.sampleRate(), in.channels(), in.frames()); worker.process(in);
    float peak = 0.F; double sumSquares = 0.0;
    for (float x : in.samples()) { peak = std::max(peak, std::abs(x)); sumSquares += static_cast<double>(x) * x; }
    if (!in.samples().empty()) {
        const float rms = static_cast<float>(std::sqrt(sumSquares / in.samples().size()));
        const float gain = std::min(dbToGain(settings.ceilingDbfs) / std::max(peak, kEpsilon), dbToGain(settings.targetLufs) / std::max(rms, kEpsilon));
        for (float& x : in.samples()) x = clamp(x * gain, -dbToGain(settings.ceilingDbfs), dbToGain(settings.ceilingDbfs));
    }
    // Deterministic linear interpolation fallback. Integrators can substitute the SpeexDSP polyphase kernel.
    if (options.targetSampleRate != 0 && options.targetSampleRate != in.sampleRate()) {
        const auto frames = static_cast<std::size_t>(std::llround(static_cast<double>(in.frames()) * options.targetSampleRate / in.sampleRate()));
        AudioBuffer out(in.channels(), frames, options.targetSampleRate);
        const double ratio = static_cast<double>(in.sampleRate()) / options.targetSampleRate;
        for (std::size_t f = 0; f < frames; ++f) { const double p = f * ratio; const auto a = std::min<std::size_t>(static_cast<std::size_t>(p), in.frames() - 1); const auto z = std::min(a + 1, in.frames() - 1); const float t = static_cast<float>(p - a); for (std::size_t c = 0; c < in.channels(); ++c) out.at(f,c) = in.at(a,c) + (in.at(z,c)-in.at(a,c))*t; }
        return out;
    }
    return in;
}
std::unique_ptr<IAudioProcessor> createPlatformProcessor(ComputeBackend, ModuleControls controls) { return std::make_unique<PreampEngine>(controls); }
} // namespace sqb::audio
