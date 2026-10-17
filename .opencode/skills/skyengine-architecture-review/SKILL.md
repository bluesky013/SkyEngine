---
name: skyengine-architecture-review
description: Review SkyEngine changes against the repository's current engine architecture, module layering, plugin boundaries, and runtime/editor separation.
compatibility: opencode
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
- `engine/aurora` — Aurora RHI and backends, Slang shader compiler, RDG, scene/ECS
- `engine/animation`, `engine/physics`, `engine/navigation` — domain-specific engine subsystems
- `engine/sandbox` — non-Qt (Aurora) editor: `EditorCore`, `EditorRender`, `SandboxModule`, `SandboxEditor`; launched through `Launcher`
- project `native/` — runtime entry point (engine ships launcher templates in `engine/templates/launcher/`)
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
- Render state, RHI usage, shader integration, and render submission belong in `engine/aurora`.
- Editor-only behavior belongs in `engine/sandbox` or editor-gated plugins.
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
- introduce editor-only plugin usage without respecting `requires_editor` / `SKY_BUILD_SANDBOX`
- make a plugin depend on sandbox editor targets: plugins depend only on `Launcher`, and editor runtime modules are selected through `configs/modules_editor.json`

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

- `SKY_BUILD_SANDBOX=ON` enables the sandbox editor
- some plugins require editor mode
- editor runtime/module loading differs from game runtime/module loading

Flag changes that:

- make editor dependencies unconditional in shared runtime code
- move editor tooling concerns into launcher/runtime paths

---

## Rule 6 — Asset builders are framework-level, owned and registered per subsystem

The offline asset-build abstraction belongs to `engine/framework`, and each subsystem registers its own builders
through a module it owns.

Repository baseline:

- `AssetBuilder` base and `AssetBuilderManager` singleton live in `engine/framework`
  (`engine/framework/include/framework/asset/AssetBuilder.h`, `AssetBuilderManager.h`).
- Persist the builder **implementation in the owning plugin**, never in `engine/*` (engine holds interfaces/data only):
  - render: `engine/aurora/cook/module/AuroraCookModule.cpp` → target `AuroraCook`
  - audio: `plugins/audio/builder/module/AudioBuilderModule.cpp` → target `SkyAudio.Builder` (moved out of `engine/audio`)
- A builder module registers only builders of its own subsystem (or a subsystem it legitimately owns), and is
  loaded by the asset-build tooling/editor, not by the game runtime.

Flag changes that:

- register a subsystem's builder from an unrelated subsystem's module (for example registering an audio builder
  from `Aurora.Cook` / `AuroraCookModule`)
- make a builder module depend on an unrelated renderer/subsystem (an audio or navigation builder must not pull in
  aurora; use the framework `AssetBuilder`/`AssetBuilderManager` boundary instead)
- put asset-build registration into runtime-only modules (for example the runtime `AudioModule`)

Concrete precedent: `AudioBuilder` was decoupled from `AuroraCookModule` and moved to `SkyAudio.Builder`; the
navigation builder must follow the same pattern rather than living in `Aurora.Cook`.

## Rule 7 — Module types and dependency budgets

Every engine big module and every plugin splits into up to four target types, each with a fixed dependency budget:

- **core logic** (`<feature>` / `<feature>.Static`): depends mostly on `Core` (plus engine interfaces / `Framework` interfaces). Must not depend on editor, adaptor, builder, or a concrete backend/renderer.
- **editor** (`<feature>.Editor`): may depend on the Sandbox editor framework (e.g. `EditorCore`; planned `Sandbox.framework`). Must not be depended on by runtime or core logic.
- **adaptor** (`<feature>.Adaptor`): may depend on `Framework`, `Aurora`, and `Aurora.Adaptor` when required.
- **builder / cook / chef** (`<feature>.Builder` / `<feature>.Cook`): depends only on its own core module plus needed third-party libraries.

Dependency direction (bottom-up): core logic ← adaptor / editor / builder. Reverse edges are violations.

Flag changes that:

- make a core-logic module depend on editor / adaptor / builder or a concrete backend/renderer
- let runtime or core logic depend on an editor module, or let an editor module bypass the Sandbox editor framework (e.g. bypass the sandbox shell)
- make an adaptor depend on editor / builder, or a builder/cook/chef depend on runtime / adaptor / editor
- collapse a feature into a single target that mixes core logic with editor/adaptor/builder concerns

## Known deferred items

- **PVS runtime culling**: the legacy render integration was removed; re-integrating culling on Aurora is deferred to a follow-up change.

## Review output format

When using this skill, structure feedback like this:

1. **Architecture status** — OK / Risk / Violation
2. **Boundary affected** — module, plugin, runtime config, render-vs-logic, or editor separation
3. **Evidence** — exact repo path(s) that establish the current pattern
4. **Required correction** — the smallest structural fix that preserves the current architecture
