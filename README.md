# AppGameToolbox

Reusable C++ models and editor-oriented tools for game UI, 3D scene objects,
audio effects, and audio-timeline synchronization.

## Modules

- `UIElement2D` provides layout, visibility, hit testing, tint, opacity, and z-order.
- `Object3D` provides transform, visibility, material tint, static state, and render layer.
- `AudioPlayback`, `SpatialAudioPlayback`, `AudioTransport`, `AudioFX`, and
  `AudioSyncSystem` provide platform-neutral playback contracts and timeline cues.
- `Toolbox` provides 2D, 3D, and audio-FX editing helpers with snapping and preview controls.

The library uses only `.hpp` headers and `.cxx` sources. Platform packages supply
the concrete audio backends that implement its playback interfaces.
