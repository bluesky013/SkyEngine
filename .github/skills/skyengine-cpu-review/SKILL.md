---
name: skyengine-cpu-review
description: Review SkyEngine changes for CPU cost, frame-time ownership, and runtime hot-path placement using the repository's current module structure, profiling hooks, and build/runtime boundaries.
metadata:
  audience: contributors
  source: README.md, cmake/options.cmake, openspec terrain specs, plugins/plugins.json, repository structure
---

# SkyEngine CPU Review

Load this skill when reviewing changes that may affect frame-time, task placement, update frequency, or runtime CPU ownership.

## Goal

Catch CPU regressions by verifying that work is happening in the right subsystem, at the right frequency, with the right profiling/feature-gating story.

## Review baseline

Relevant repository facts:

- CPU-heavy runtime systems span `engine/core`, `engine/framework`, `engine/animation`, `engine/render`, `engine/launcher`, and runtime-facing plugins
- `SKY_USE_TRACY` exists as a build-time profiling hook
- `SKY_MATH_SIMD` exists as a build-time math optimization switch
- plugin inclusion is compile-time configurable
- runtime module loading is separately controlled by module manifests
- render-side per-frame work can live in feature processors and renderer implementations

## Working rules

- Review CPU work in terms of **ownership** first: which layer should pay for this cost every frame?
- Prefer moving heavy work to the existing subsystem that already owns the lifecycle instead of duplicating it elsewhere.
- Distinguish runtime hot-path work from build-time/tool-time work.
- If a proposed CPU optimization depends on profiling or SIMD assumptions, require that story to be explicit.

---

## Rule 1 — Put per-frame CPU work in the correct owner

The repository already separates runtime responsibilities across core/framework/render/editor/plugins.

Flag changes that:

- move render-facing per-frame work into gameplay/component classes without a strong reason
- bury frame-critical work inside editor-only or tooling code paths
- duplicate update logic across framework, feature processors, and plugins instead of choosing a single owner

Use the terrain pattern as precedent:

- feature processors own render-side Tick/Render work
- components should communicate config/state, not absorb render implementation burden

---

## Rule 2 — Keep build-time and runtime CPU costs separate

SkyEngine has explicit build/tool pipeline pieces (`ShaderTool`, `ShaderCompiler`, render builders, Python tooling) in addition to runtime code.

Flag changes that:

- move expensive preprocessing/build work into runtime startup or frame loops
- treat a build-time convenience shortcut as acceptable runtime overhead
- put asset transformation or heavy pipeline work in the launcher/game path when an existing tool/build path is a better fit

---

## Rule 3 — Use profiling and optimization switches intentionally

The repository exposes:

- `SKY_USE_TRACY`
- `SKY_MATH_SIMD`

Review expectations:

- if a change claims CPU improvement, it should be compatible with the repository's profiling story
- if a hot-path improvement depends on SIMD assumptions, it should not silently break non-SIMD builds
- do not hide CPU regressions behind unverified “it should be faster” reasoning

Flag changes that:

- make strong CPU-performance claims without any clear measurement/profiling path
- introduce SIMD-only assumptions into generic code without proper gating

---

## Rule 4 — Respect plugin/runtime granularity when adding CPU cost

Since plugin inclusion and runtime module loading are configurable, CPU review should ask whether new cost is:

- always-on for every runtime
- plugin-scoped
- editor-only
- only required when a feature is enabled

Flag changes that:

- add unconditional CPU overhead for functionality that is plugin- or editor-specific
- bypass existing plugin/module gating and force all runtimes to pay for niche features
- forget to align compile-time plugin toggles with runtime module manifests

---

## Rule 5 — Prefer scalable per-frame strategies with fallback awareness

The terrain spec gives a concrete CPU precedent:

- CPU-driven path performs frustum culling and instance-list construction
- GPU-driven path is conditional on capability/configuration
- fallback behavior is explicit rather than accidental

Review CPU-side changes for:

- avoidable per-frame iteration growth
- repeated work that could be cached or delegated to the proper owner
- removal of simpler fallback paths before faster paths are stable

---

## Review output format

When using this skill, structure feedback like this:

1. **CPU status** — OK / Risk / Regression
2. **Hot-path owner** — core, framework, render, plugin, editor, or tool pipeline
3. **Evidence** — exact repo path(s) that establish the expected ownership/gating
4. **Required correction** — the smallest change that restores sane CPU ownership or reduces always-on cost
