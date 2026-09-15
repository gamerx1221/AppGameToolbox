# SQBAudioPreamp

`SQBAudioPreamp` is a C++20 audio-conditioning library for game assets and low-latency runtime output. It is intentionally split into a portable scalar reference engine and replaceable SIMD/GPU/device adapters. The source currently builds without an SDK, audio device, or GPU.

## Included now

- Interleaved floating-point PCM container and a no-allocation-in-the-inner-loop runtime processor contract.
- DC removal, 30 Hz high-pass rumble filter, transient enhancement, virtual-bass harmonic enrichment, HF excitation, loudness contour, adaptive M/S width, and a final soft safety limiter.
- Offline conditioning with asset-class loudness defaults (`-12` SFX, `-16` ambience, `-8` UI), true peak ceiling control, and sample-rate conversion fallback.
- `IAudioProcessor` + `IComputeKernel` multiple-inheritance contracts for platform-specific WASAPI, CoreAudio, AAudio, ALSA, NEON, SSE/AVX, CUDA, Vulkan, and DSP-card implementations.
- Git submodules: [SpeexDSP](third_party/speexdsp) for production polyphase resampling and [kissfft](third_party/kissfft) for optional spectral kernels. They are registered but not compiled automatically, so platform packages can select their own optimized build settings.

## Integration

```cmake
add_subdirectory(SQBAudioPreamp)
target_link_libraries(MyGame PRIVATE SQB::AudioPreamp)
```

Call `prepare(rate, channels, maxCallbackFrames)` once when the device format changes, then call `process()` per callback. `conditionFile()` is for non-realtime import/cooking only.

## Deliberate next production steps

The reference sample-rate converter is deterministic interpolation, not the final specified linear-phase polyphase FIR. Connect SpeexDSP (or a project-selected FIR/polyphase kernel) in the offline cooker before relying on it for mastering exports. Likewise, the current loudness gain is RMS-based; add BS.1770 K-weighted integrated LUFS plus true-peak oversampling for delivery certification. CUDA/Vulkan convolution and ML artifact suppression belong in optional worker/GPU paths, never the audio callback.
