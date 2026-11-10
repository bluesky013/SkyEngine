## Why

Curves are currently authored inline inside `.particle` files (`ParticleCurve` keys baked into the product). They cannot be reused across effects, referenced by animation/material systems, or edited with a proper key editor, which is the remaining gap in the particle editor (`add-particle-system` task 7.8).

A standalone, reusable curve asset gives every subsystem a single data and evaluation source instead of each one re-implementing keys, interpolation and wrap modes. It also gives curve keys a real editor while keeping the runtime/cook dependency floor small.

## What Changes

- Add a new `engine/curve` engine subsystem with a clean target split:
  - `Curve` (core): curve keys, interpolation, wrap modes and `Evaluate`; depends only on `Core`.
  - `Curve.Assets.Static` + `Curve.Assets` (module): `.curve` source JSON, binary product, reflection and `AssetTraits`; depends on `Curve` + `Framework`.
  - `Curve.Builder` (module): offline cook for `.curve`; depends on `Curve.Assets.Static` + `Framework`.
  - `Curve.Editor` (module): asset view with key-list editing; depends on `Curve.Assets.Static` + the Sandbox editor framework (`EditorCore`/`EditorUi`/`EditorWidgets`).
- Define the first-version curve data model: typed channels (scalar or Vector2/3/4), Linear/Step/Cubic interpolation, Clamp/Repeat/Mirror/Linear pre/post extrapolation, ordered keys in `[0, 1]`, and deterministic evaluation.
- Register the `curve` domain in `openspec/README.md` and `openspec/capability-map.json`.
- Wire build/config: `engine/CMakeLists.txt`, `configs/modules_editor.json` (editor module), AssetTool module registration, and the shared host module manifests.
- Out of scope (documented in design as follow-ups): gradient assets, additional bezier/tangent modes, LUT sampling, and particle/animation references to curve assets. Existing inline `ParticleCurve` authoring stays unchanged; the particle migration is a separate change.

- Apply the architecture review fixes: bounded transactional parsing, gesture/history lifetime, canonical dirty tracking, shared source storage, optional preview layout and UI command batching.

## Capabilities

### New Capabilities

- `curve`: reusable curve data model, evaluation semantics, `.curve` source and product formats, asset registration, cook and asset-editor behavior.

### Modified Capabilities

- `editor-core`: shared source storage and canonical document checkpoint lifecycle.
- `ui`: contiguous draw batching for compatible shape commands.

## Impact

- New code: `engine/curve/{core,assets,builder,editor}` with targets `Curve`, `Curve.Assets.Static`/`Curve.Assets`, `Curve.Builder`, `Curve.Editor`.
- Build/config: `engine/CMakeLists.txt`, `configs/modules_editor.json`, `tools/asset_tool/main.cpp` (shared cook manifest), and the OpenSpec domain registry.
- No existing module gains a new runtime dependency. Future consumers (particle, animation, material) can depend on `Curve.Assets.Static` without pulling cook or editor targets.
- `SKY_BUILD_CURVE` is not introduced: the small core/assets targets are always built; builder/editor follow the existing `SKY_DEVELOP`/`SKY_BUILD_SANDBOX` gating.
