## Context

Builds on `UIContext` and element properties (transform/opacity from `ui-transform`).

## Decisions

### 1. Easing and tracks
`ApplyEasing` provides LINEAR, EASE_IN_QUAD, EASE_OUT_QUAD, EASE_IN_OUT_QUAD, EASE_OUT_CUBIC.
`UIFloatTrack` stores sorted `(time, value)` keys and linearly interpolates between them.

### 2. Animation
`UIAnimation(duration, update, easing)` calls `update(easedT)` each advance (t in 0..1). It supports loop and an
on-complete callback. `Advance(delta)` returns whether the animation is still active.

### 3. Context ownership
`UIContext::Tick(delta)` advances all animations and removes completed ones; `AddAnimation` returns a raw
pointer for convenience. Alternative: per-element animation lists — rejected for a single frame driver.

## Risks / Trade-offs

- **Callback-based, not declarative** -> simple and composable; a property-path animator is a follow-up.
- **No sample-accurate timeline** -> fine for UI transitions.
