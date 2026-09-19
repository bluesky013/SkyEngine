## Why

The archived `engine-ui-system` change established the render-agnostic UI core (element tree, layout, style
theme, input routing, draw-data contract) but shipped no concrete widgets. `openspec/specs/ui-core` already
requires widget primitives (container/panel, text, image, button). This change implements the first concrete
set -- Panel, Image, and Button -- so screens can be composed, laid out, and unit-tested before the Aurora
render pass and text layout land.

## What Changes

- Add a `Panel` widget: a container that clips its children to its bounds and paints a theme-driven background.
- Add an `Image` widget: paints a texture handle over its bounds with a UV rect.
- Add a `Button` widget: normal/hover/pressed/disabled states, theme-driven colors, disabled ignores input, and
  a click that fires when a press and release both occur inside its bounds.
- Give widgets access to the context theme during paint so appearance resolves from style classes rather than
  hard-coded constants.
- Wire the widgets into the existing layout/paint/event flow and cover them with tests.

**Non-goals**: the `Text` widget and text layout (deferred to a `ui-text` change), the element-type registry and
document loading (`ui-data`), the full widget library (checkbox/slider/scroll/etc.), and animation.

## Capabilities

### New Capabilities
- `ui-widgets`: concrete Panel, Image, and Button widget contracts and their theme-driven behavior.

### Modified Capabilities
<!-- None: `ui-core` widget-primitive requirements are implemented here, not changed. -->

## Impact

- New sources under `engine/ui/core/include/ui/widgets/` and `engine/ui/core/src/widgets/`.
- Extends `UIPaintContext` with theme access; `UIContext::Paint` seeds it.
- New tests in `engine/test/ui/` (widget suite).
- No changes to `UIRender` or the Aurora dependencies.
