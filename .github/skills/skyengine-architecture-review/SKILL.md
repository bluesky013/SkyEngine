---
name: skyengine-architecture-review
description: Review SkyEngine changes against the repository's current engine architecture, module layering, plugin boundaries, and runtime/editor separation.
metadata:
  audience: contributors
  source: README.md, plugins/plugins.json, configs/modules_game.json, configs/modules_editor.json, openspec terrain specs
---

# SkyEngine Architecture Review

Load this skill when reviewing design or code changes that may affect engine layering, module ownership, plugin boundaries, or runtime/editor separation.

## Goal

Keep SkyEngine aligned with its existing modular architecture instead of allowing convenient but structurally damaging shortcuts.

## Review baseline

Treat the current repository structure as the architectural baseline:

- `engine/core` — math, memory, file system, logging, async utilities
- `engine/framework` — windowing, input, assets, world/application services
- `engine/render` — RHI backends, shader compiler, render core, adaptors, ImGui rendering
- `engine/animation`, `engine/physics`, `engine/navigation` — domain-specific engine subsystems
- `engine/editor` — Qt-based editor runtime and tooling
- `engine/launcher` — runtime entry point
- `plugins` — optional modules selected by CMake/plugin configuration

## Working rules

- Review against **existing module ownership first**, not against abstract clean-architecture theory.
- Reject changes that blur logic-layer and render-layer responsibilities without a repo-backed reason.
- Prefer extending an existing subsystem or plugin over introducing a new cross-layer dependency.
- Treat compile-time plugin selection and runtime module loading as separate systems that must both remain correct.
- Require exact path/module references in review comments instead of vague architectural complaints.

---

## Rule 1 — Preserve the established engine layering

Review changes so they respect the current dependency shape implied by the repository layout and target map.

Key expectations:

- Core/runtime utilities belong in `engine/core`.
- Application services and world-facing orchestration belong in `engine/framework`.
- Render state, RHI usage, shader integration, and render submission belong in `engine/render`.
- Editor-only behavior belongs in `engine/editor` or editor-gated plugins.
- Optional feature extensions belong in `plugins`, not by hard-wiring everything into core engine layers.

Flag changes that:

- pull editor-only concerns into runtime modules
- introduce direct render/RHI coupling into gameplay-facing components
- bypass existing subsystem boundaries for convenience

---

## Rule 2 — Enforce plugin/build separation from runtime module loading

SkyEngine separates plugin inclusion from runtime module activation.

Review using these repository rules:

- `plugins/plugins.json` controls **compile-time inclusion** via `cmake_var`, `enabled`, and optional `requires_editor`
- `configs/modules_game.json` controls runtime modules for the launcher/game path
- `configs/modules_editor.json` controls runtime modules for the editor path
- enabling a plugin for a real workflow may require updating both compile-time and runtime configuration

Flag changes that:

- assume building a plugin automatically loads it at runtime
- add runtime module dependencies without matching compile-time support
- introduce editor-only plugin usage without respecting `requires_editor` / `SKY_BUILD_EDITOR`

---

## Rule 3 — Keep logic-layer data separate from render-layer state

Use the terrain spec as an explicit architectural precedent:

- gameplay/components should pass POD-style configuration and asset identifiers across the boundary
- render-layer state should live in render-side systems such as feature processors / renderers
- logic-facing headers should not absorb render-layer includes just to make a feature convenient

Concrete precedent from the repository:

- `TerrainComponent` must not include render-layer headers
- it must not hold `RDMaterialInstancePtr`, `RDTexture2DPtr`, or `rhi::*` types
- render communication should happen through POD config and asset references
- render ownership belongs to `TerrainFeatureProcessor` / `ITerrainRenderer`

Flag changes that push render state into components, gameplay objects, or framework-facing public headers.

---

## Rule 4 — Respect lifecycle ownership and feature-processor boundaries

The terrain redesign documents a repo-backed pattern for render features:

- a feature processor is registered with `RenderScene`
- the feature processor owns render-side state
- it participates in the engine's Tick/Render lifecycle
- renderer implementations sit behind an interface and can switch based on capability/configuration

Prefer this style when reviewing new render-facing systems.

Flag changes that:

- scatter render ownership across unrelated objects
- bypass `RenderScene`/feature-processor style integration where the existing pattern already fits
- remove fallback paths for capability-dependent render implementations

---

## Rule 5 — Editor gating must remain explicit

The repository already encodes editor separation:

- `SKY_BUILD_EDITOR=ON` implies `SKY_EDITOR=ON`
- some plugins require editor mode
- editor runtime/module loading differs from game runtime/module loading

Flag changes that:

- make editor dependencies unconditional in shared runtime code
- assume editor rendering (`SkyRender.Editor`) exists in game/runtime flows
- move editor tooling concerns into launcher/runtime paths

---

## Review output format

When using this skill, structure feedback like this:

1. **Architecture status** — OK / Risk / Violation
2. **Boundary affected** — module, plugin, runtime config, render-vs-logic, or editor separation
3. **Evidence** — exact repo path(s) that establish the current pattern
4. **Required correction** — the smallest structural fix that preserves the current architecture
