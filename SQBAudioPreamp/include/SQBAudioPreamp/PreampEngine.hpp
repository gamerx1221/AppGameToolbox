#pragma once

#include "SQBAudioPreamp/AudioBuffer.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace sqb::audio {

enum class AssetClass { Sfx, Ambience, Ui };
enum class ComputeBackend { Scalar, SseAvx, Neon, Vulkan, Cuda, AudioDsp };

struct ModuleControls {
    bool dcRemoval = true, rumbleFilter = true, transientEnhancer = true, dynamics = true;
    bool stereoStabilizer = true, virtualBass = true, hfExciter = true, loudnessContourEnabled = true, limiter = true;
    float intensity = 0.5F;          // master enhancement, [0, 1]
    float loudnessContour = 0.35F;   // [0, 1]
    float stereoWidth = 1.0F;        // [0, 2]
    float transientSharpness = 0.35F;// [0, 1]
    float virtualBassDepth = 0.2F;   // [0, 1]
    float hfExciterAmount = 0.12F;   // [0, 1]
    float targetLufs = -16.0F;
    float ceilingDbfs = -1.0F;
};

struct OfflineOptions {
    unsigned targetSampleRate = 96000;
    AssetClass assetClass = AssetClass::Sfx;
    bool noiseShapedDither = true;
    bool addMicroAmbience = false;
};

/// A realtime-safe processing contract: implementers must not allocate in process().
class IAudioProcessor {
public:
    virtual ~IAudioProcessor() = default;
    virtual void prepare(unsigned sampleRate, std::size_t channels, std::size_t maximumFrames) = 0;
    virtual void process(AudioBuffer& buffer) noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

/// Multiple inheritance intentionally separates execution capability from DSP behavior.
class IComputeKernel {
public:
    virtual ~IComputeKernel() = default;
    [[nodiscard]] virtual ComputeBackend backend() const noexcept = 0;
    [[nodiscard]] virtual bool isAvailable() const noexcept = 0;
};

class PreampEngine final : public IAudioProcessor, public IComputeKernel {
public:
    explicit PreampEngine(ModuleControls controls = {});
    void setControls(ModuleControls controls) noexcept;
    [[nodiscard]] const ModuleControls& controls() const noexcept;
    void prepare(unsigned sampleRate, std::size_t channels, std::size_t maximumFrames) override;
    void process(AudioBuffer& buffer) noexcept override;
    [[nodiscard]] AudioBuffer conditionFile(AudioBuffer input, const OfflineOptions& options = {}) const;
    [[nodiscard]] std::string_view name() const noexcept override { return "SQBAudioPreamp"; }
    [[nodiscard]] ComputeBackend backend() const noexcept override;
    [[nodiscard]] bool isAvailable() const noexcept override { return true; }

private:
    ModuleControls controls_;
    unsigned preparedRate_{};
    std::size_t preparedChannels_{};
    std::vector<float> previous_, envelope_, dc_;
};

/// Factory seam for platform packages (CoreAudio/AVAudioEngine, WASAPI, AAudio, ALSA).
std::unique_ptr<IAudioProcessor> createPlatformProcessor(ComputeBackend preferred, ModuleControls controls = {});

} // namespace sqb::audio
