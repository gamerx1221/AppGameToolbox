# AppGameToolbox

Reusable C++ models and editor-oriented tools for game UI, 3D scene objects,
audio effects, and audio-timeline synchronization.

## Modules

- `UIElement2D` provides layout, visibility, hit testing, tint, opacity, and z-order.
- `Object3D` provides transform, visibility, material tint, static state, and render layer.
- `AudioPlayback`, `SpatialAudioPlayback`, `AudioTransport`, `AudioFX`, and
  `AudioSyncSystem` provide platform-neutral playback contracts and timeline cues.
- `Render2DRecorder` produces immutable, backend-neutral 2D command frames, while
  `Render2DPlayer` replays them to a platform renderer. Cached layers use stable
  keys and content versions so unchanged nested content can be composited directly.
- `HtmlCssPipeline` parses a documented HTML/CSS subset into `Render2DRecorder`
  commands, including `render-mode: cached-layer` for cacheable subtrees.
- `Toolbox` provides 2D, 3D, and audio-FX editing helpers with snapping and preview controls.

The library uses only `.hpp` headers and `.cxx` sources. Platform packages supply
the concrete audio backends that implement its playback interfaces.

## HTML/CSS Recording

`HtmlCssPipeline` records a small, deterministic UI subset rather than embedding
a browser engine. Use `load(html, css)`, optionally provide an image resolver, and
then call `record(recorder, viewport)` before finalizing the recorder frame.

- HTML: nested elements, text, `<img src>`, and embedded `<style>` elements.
- Selectors: element, `.class`, `#id`, compound selectors, and comma-separated lists.
- CSS: `display`, `position`, `left`, `top`, `width`, `height`, `margin`, `padding`,
  `color`, `background-color`, `opacity`, `font-size`, and `render-mode`.
- Values: pixels, percentages for dimensions/positioning, `auto`, `#rgb`/`#rrggbb`,
  and a small named-color set.
- `render-mode: cached-layer` emits a cached layer keyed by the element ID, or by
  stable document order when no ID is present.

Flex/grid layout, CSS functions, descendant selectors, text wrapping, and external
stylesheets are intentionally outside this initial portable subset.
