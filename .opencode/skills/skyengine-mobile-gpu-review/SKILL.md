---
name: skyengine-mobile-gpu-review
description: Review SkyEngine rendering changes from a mobile GPU perspective, grounded in the repository's Android/iOS backends, tex_mobile asset path, and fallback-oriented render patterns.
compatibility: opencode
metadata:
  audience: contributors
  source: README.md, configs/asset_build_presets.json, openspec terrain specs, repository structure
---

# SkyEngine Mobile GPU Review

Load this skill when reviewing rendering or asset changes that affect Android/iOS GPU cost or compatibility.

## Goal

Protect mobile rendering scalability and compatibility without importing desktop-only assumptions into SkyEngine's mobile paths.

## Review baseline

Relevant repository facts:

- Android is a supported target with arm64-v8a tooling
- iOS is a supported arm64 target
- Vulkan is used on Android, Metal on Apple, and GLES is optional on Android via `SKY_BUILD_GLES`
- Mobile asset presets currently use `common + tex_mobile`
- Mobile texture/build flow is separated from PC through ASTC-oriented `tex_mobile`
- Capability-gated render paths already exist in repo design precedent

## Working rules

- Review mobile GPU changes for both **performance** and **feature availability**.
- Prefer backend-agnostic or explicitly gated designs over desktop-first assumptions.
- Treat bandwidth, pass count, and fallback behavior as primary review concerns.
- Keep mobile/runtime render paths separate from editor or desktop-only convenience logic.

---

## Rule 1 — Respect current mobile backend reality

Mobile review must account for the current repository targets:

- Android: Vulkan primary, GLES optional
- iOS: Metal

Flag changes that:

- assume DX12-style or desktop-only behavior in mobile paths
- require a backend capability that is not obviously available on Android/iOS without a fallback
- introduce shader/resource assumptions that are only justified for desktop workflows

---

## Rule 2 — Preserve the mobile asset pipeline split

The repository already isolates mobile assets through `tex_mobile`.

Mobile review expectations:

- keep mobile texture/resource assumptions aligned with `tex_mobile`
- preserve the ASTC/mobile-oriented asset path instead of collapsing it into PC conventions
- avoid review guidance that assumes `tex_pc`/BC behavior is the mobile default

Flag changes that:

- hard-code desktop texture formats or desktop compression assumptions into mobile runtime paths
- remove the practical separation between PC and mobile asset bundles

---

## Rule 3 — Require capability checks and graceful fallback paths

The terrain spec establishes an important repository pattern:

- a GPU-driven path may exist when device capability and config allow it
- a CPU-driven or simpler renderer must remain available when capability is absent

Flag changes that:

- make the mobile path depend on optional features with no fallback
- assume advanced GPU features are always available on target devices
- delete the simpler path before the capability-gated path is production-ready

---

## Rule 4 — Guard mobile pass and bandwidth cost carefully

Use the current terrain/render precedent when reviewing mobile rendering:

- depth pre-pass and shadow participation must remain intentional
- PBR integration should reuse existing engine resources where possible
- every additional pass or heavy resource read should justify its mobile cost

Flag changes that:

- add mobile rendering work by layering on more passes without strong benefit
- increase texture/buffer traffic with no mobile-specific reasoning
- trade away broad compatibility just to mirror a desktop visual path

---

## Rule 5 — Keep editor and build-time assumptions out of mobile runtime review

When reviewing mobile GPU work:

- prioritize runtime render code, runtime modules, and runtime-facing plugins
- do not treat editor-only workflows as proof that the mobile runtime path is correct
- keep platform build flags and runtime module manifests aligned when a mobile feature depends on a plugin

---

## Review output format

When using this skill, structure feedback like this:

1. **Mobile GPU status** — OK / Risk / Regression
2. **Mobile-specific issue** — backend compatibility, pass cost, bandwidth, fallback, or asset path split
3. **Evidence** — exact repo path(s) that define the current behavior
4. **Required correction** — the smallest change that restores mobile scalability or compatibility
