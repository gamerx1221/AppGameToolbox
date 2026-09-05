# AppGameToolbox

Reusable C++ models and editor-oriented tools for game UI, 3D scene objects,
audio effects, and audio-timeline synchronization.

## Modules

- `UIElement2D` provides layout, visibility, hit testing, tint, opacity, and z-order.
- `Object3D` provides transform, visibility, material tint, static state, and render layer.
- `AudioPlayer`, `SpatialAudioPlayer`, `AudioFX`, `AudioTransport`, and
  `AudioSyncSystem` provide AVFoundation-backed Apple audio playback and timeline cues.
- `Toolbox` provides 2D, 3D, and audio-FX editing helpers with snapping and preview controls.

The public API uses `.hpp` headers and regular models/tools use `.cxx` source
files. AVFoundation backends use Objective-C++ `.mm` source files as required by
Apple's Objective-C framework APIs.
