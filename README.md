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
- `EffectGenerator` produces deterministic, backend-neutral circles, rings, lines,
  polygons, and polylines for procedural visual effects.
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
- Selectors: element, `.class`, `#id`, compound selectors, comma-separated lists,
  descendant chains, and `:hover`, `:active`, and `:focus` pseudo states.
- CSS: `display`, `position`, `left`, `top`, `width`, `height`, `margin`, `padding`,
  `color`, `background-color`, `background-image: url(...)`, two-stop
  `linear-gradient(...)`, uniform `border`, `border-radius`, outer `box-shadow`,
  bounded flex/grid layout, `opacity`, `font-size`, and `render-mode`.
- Values: pixels, percentages for dimensions/positioning, `auto`, bounded
  pixel/percentage `calc()` arithmetic, inherited `var(--name[, fallback])`,
  `#rgb`/`#rrggbb`, `rgb(...)`/`rgba(...)`, and a small named-color set.
- Media: `@media (min-width: Npx)` and `@media (max-width: Npx)` are evaluated
  at record time against the supplied viewport width.
- Runtime state: `nodeIdForElementId()`, `hitTest()`, and `setPseudoState()` let
  a host update retained pseudo-state before recording the next immutable frame.
- Time: `setTime(seconds)` supplies the deterministic clock for two-frame
  `@keyframes` opacity/background-color animations and `transition` interpolation
  of opacity or background color. Supported animation shorthand accepts a name,
  duration, and finite count or `infinite`; playback is linear.
- Diagnostics: unsupported selectors, declarations, and at-rules are retained in
  `compatibilityWarnings()` and do not prevent supported CSS from recording.
- `render-mode: cached-layer` emits a cached layer keyed by the element ID, or by
  stable document order when no ID is present.

The built-in Quartz playback accepts image resource IDs through
`QuartzRenderer::setImageResolver()`. It supports destination rectangles and
source-rectangle crops; resource ownership remains with the host resolver.

Unsupported forms include per-side borders/radii, inset or multi-shadow lists,
multi-stop gradients, background placement/repetition, flex wrapping/shrinking,
grid placement/spans, text wrapping, and external stylesheets. Flex supports
row/column flows, a single `gap`, `flex`/`flex-grow`,
and row `justify-content`; grid supports equal `1fr` columns and a single `gap`.
Timing does not support easing curves, delays, fill modes, or multi-property
transition lists.
This remains a deterministic UI subset, not a browser engine or a jQuery
JavaScript runtime.

## jQuery UI Fixture

`tests/fixtures/jquery-ui-1.13.2-base-subset.css` is a small, source-attributed
fixture derived from the jQuery UI 1.13.2 Base theme. It retains only selectors
and declarations exercised by this subset, including widget states and tabs;
it does not ship jQuery UI JavaScript or its image assets. The MIT notice is in
`ThirdPartyNotices/jquery-ui-1.13.2-MIT.txt`. The compatibility test loads this
fixture from disk and verifies active and retained `:hover` tab rendering.

## Effects

`Effects.hpp` defines portable `EffectSpec` and `EffectInstance` inputs, and
`EffectGenerator` samples them into an `EffectFrame`. Built-in families are
ambient drift, crystal burst, ember trail, sonar pulse, sparkle orbit, and
signal wave, with five deterministic presets per family. The API deliberately
contains no HTML, AppKit, Quartz, Skia, Direct2D, or renderer resource types.
Backends multiply `EffectPrimitive::opacity` by `color.a` when rasterizing.

This is the shared definition layer for future renderer adapters and an effects
gallery. The current `GameMenuDemo` remains on its direct Quartz renderer.
