---
name: skyengine-pc-gpu-review
description: Review SkyEngine rendering changes from a desktop GPU perspective, grounded in the repository's current desktop backends, shader pipeline, and tex_pc asset path.
compatibility: opencode
metadata:
  audience: contributors
  source: README.md, configs/asset_build_presets.json, openspec terrain specs, repository structure
---

# SkyEngine PC GPU Review

Load this skill when reviewing rendering or asset changes that affect desktop GPU cost on Win32/macOS/Linux paths.

## Goal

Catch regressions in desktop rendering cost while staying aligned with SkyEngine's current backends, shader pipeline, and desktop asset packaging.

## Review baseline

Relevant repository facts:

- Desktop-oriented backends include Vulkan, DX12 (Win32), and Metal (Apple)
- The render stack is centered in `engine/render`
- Shader sources live under `assets/shaders`
- Materials and techniques live under `assets/materials` and `assets/techniques`
- Desktop asset presets currently use `common + tex_pc`
- The texture-compression/build pipeline distinguishes PC (`tex_pc`, BC via `ispc_texcomp`) from mobile (`tex_mobile`, ASTC)

## Working rules

- Review desktop GPU changes against the current desktop backend matrix, not a single API.
- Treat pass count, draw-call strategy, visibility work, and desktop asset format assumptions as first-class review concerns.
- Require a fallback path when a new GPU path depends on optional capabilities.
- Prefer evidence from current render patterns in-repo over generic GPU folklore.

---

## Rule 1 — Respect the current desktop render/backend matrix

Desktop review must account for the repository's current supported paths:

- Vulkan is a primary backend
- DX12 is supported on Win32
- Metal is supported on Apple platforms

Flag changes that:

- assume one desktop API is the only meaningful path
- couple a feature too tightly to a single backend without isolation
- introduce shader/resource assumptions that cannot plausibly map across current desktop backends

---

## Rule 2 — Keep desktop asset and texture assumptions in the PC path

The current asset build presets separate desktop and mobile bundles.

Desktop review expectations:

- windows/macos presets use `common + tex_pc`
- PC texture/build assumptions should stay in the PC path
- BC-oriented compression/build logic should not leak into mobile-specific review conclusions

Flag changes that:

- hard-wire mobile texture assumptions into desktop render code
- erase the distinction between `tex_pc` and `tex_mobile`
- add platform-conditional hacks where the asset pipeline already provides a cleaner split

---

## Rule 3 — Review pass participation, not just shader code

The terrain rendering spec shows that SkyEngine rendering features are expected to integrate with:

- ForwardColor / main shading
- depth pre-pass participation
- shadow map participation
- engine PBR resources such as shadow map, irradiance, prefiltered map, and BRDF LUT

Flag changes that:

- improve one pass while silently breaking depth, shadow, or lighting integration
- remove depth/HZB-friendly behavior without justification
- add expensive extra passes where an existing pass can be extended more cleanly

---

## Rule 4 — Prefer scalable draw submission strategies with explicit fallbacks

The terrain spec provides a repo-backed precedent:

- CPU-driven path performs frustum culling, builds a visible list, uploads instance data, and issues instanced draws
- GPU-driven path is conditional on capability/configuration
- missing support requires a fallback path rather than a broken fast path

Flag changes that:

- add a desktop-only fast path with no fallback
- replace instanced/batched submission with obviously more fragmented submission without a measured reason
- depend on optional GPU features without capability checks

---

## Rule 5 — Desktop GPU review should stay render-focused, not editor-tooling-heavy

When reviewing runtime desktop GPU work:

- prioritize runtime render cost in `engine/render`, runtime modules, and runtime-facing plugins
- treat build-time tools like `ShaderTool`, `ShaderCompiler`, and render builders as pipeline enablers, not runtime cost centers
- avoid mixing editor-only convenience code into runtime render paths unless explicitly required

---

## Review output format

When using this skill, structure feedback like this:

1. **Desktop GPU status** — OK / Risk / Regression
2. **Cost driver** — passes, draws, visibility, resource path, backend portability, or feature gating
3. **Evidence** — exact repo path(s) that define the current behavior
4. **Required correction** — the smallest change that restores desktop GPU scalability
