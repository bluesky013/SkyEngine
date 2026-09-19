## Context

The archived `engine-ui-system` change delivered the render-agnostic core in `engine/ui/core`: `UIElement`
tree with dirty propagation, anchor/box layout, `UIStyle`/`UITheme`, `UIRect`, `UIDrawData` +
`UIPaintContext` (clip stack, batched quads), `UIEventRouter` (hit-test/focus/capture), and the
`IUITextureRegistry` / `IUIFontProvider` / `IUIAssetResolver` seams. `openspec/specs/ui-core` already specifies
widget primitives (container/panel, text, image, button) but no concrete widgets exist yet, so nothing can be
composed or painted end to end.

Constraints: core stays render-agnostic (no `aurora/`/`render/` includes; enforced by a configure-time guard);
widgets must be unit-testable without a GPU or assets; text layout is not available yet.

## Goals / Non-Goals

**Goals:**
- Concrete `Panel`, `Image`, and `Button` widgets built on the existing core, painted from theme styles.
- Clipping delegated to the core so a `Panel` clips its subtree without per-widget clip bookkeeping.
- Button interaction (press/release/click, disabled) driven by the existing event router.
- Unit tests covering clipping, texture binding, click semantics, and theme-driven color.

**Non-Goals:**
- The `Text` widget and text layout (belongs to a `ui-text` change; `IUIFontProvider` is the seam).
- The element-type registry and document loader (`ui-data`).
- Checkbox/slider/scroll/rich-text and the rest of the widget library.
- Animation, transitions, and multi-touch.

## Decisions

### 1. Theme access during paint
Widgets need the resolved `UIStyle`, but `UIPaintContext` carried no theme. Add `SetTheme`/`GetTheme`
(`const UITheme*`) to `UIPaintContext`, seeded by `UIContext::Paint`. Widgets resolve
`GetTheme()->Resolve(GetStyleClasses())` inside `OnPaint`. Alternative considered: store a resolved style on
each widget — rejected because it breaks the style/theme separation and runtime re-skinning.

### 2. Clipping owned by the core paint walk
`UIElement::Paint` pushes `bounds` onto the paint context clip stack when the element clips its children, paints
itself, paints children, then pops. This lets `Panel` clip by setting `clipsChildren = true` with no widget
code. Alternative considered: override `Paint` in `Panel` — rejected because child iteration lives in the base.

### 3. Panel
`Panel` sets `clipsChildren` in its constructor and paints the theme `backgroundColor` over its bounds. It has
no other behavior; layout comes from the base.

### 4. Image
`Image` stores an opaque `UITextureId`, a UV rect, and a tint color, and paints a textured quad over its bounds
via `UIPaintContext::AddTexturedQuad`. A zero/invalid texture paints nothing, so images degrade gracefully
before assets load. It does not own or load the texture (that is the render/asset seam).

### 5. Button
`Button` is a `UIElement` with an interaction state (`Normal`/`Hover`/`Pressed`/`Disabled`) and a
`std::function<void()> onClick` callback. `OnPointerEvent`: press inside sets `Pressed` and captures the press;
release fires `onClick` only if the release is still inside the bounds; disabled ignores input. Paint selects a
theme color per state (`buttonNormal`/`buttonHover`/`buttonPressed`). Click is delivered as a callback rather
than a new event type to stay within the current event model; an event-based API can wrap it later.

### 6. Source layout
Widgets live under `engine/ui/core/include/ui/widgets/` and `engine/ui/core/src/widgets/`, keeping the element
base and widgets separable as the set grows.

## Risks / Trade-offs

- **No leave event** -> `Hover` cannot be reliably cleared by the router today; it is set on pointer move and
  cleared on press/release. Hover is best-effort until the event model gains enter/leave.
- **Click uses release-inside test, not capture distance** -> consistent with common UI behavior, but a drag
  out-and-back will not click; acceptable for this milestone.
- **Theme `Resolve` is last-class-wins, not per-field cascade** -> documented limitation of the archived core;
  widget tests use whole-style replacement.
- **Fresh raw-pointer method on a move-only base** -> tests must capture the typed pointer before
  `AddChild(std::move(...))`; existing suites already follow this pattern.

## Migration Plan

Additive: new widget headers/sources and tests; no existing behavior changes. Rollback removes the widget
files. No Aurora or render changes.

## Open Questions

- Whether click/hover should become first-class UI events before `ui-data` bindings arrive.
- Whether `Text` should land in `ui-text` as a widget consuming the text-layout output (current assumption).
- Whether `Image` should support nine-slice now or wait for the styling change.
