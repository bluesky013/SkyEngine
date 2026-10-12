## Context

The asset pipeline separates source assets (`assets/`, authoring) from cooked **product bundles**
(`products/<bundle>/product.index` + `<uuid>.bin` payloads; `docs/features/asset-pipeline.md`). The runtime mounts
bundles through `AssetManager::AddAssetProductBundle` and resolves a UUID with `OpenFile(uuid)`, which returns the
**first** bundle that contains it (`AssetManager.cpp`). `GameApplication` currently adds only `common` (Android,
from `externalDataPath`) and **nothing** on desktop/iOS (only sets `workFs = GetBundlePath()`).

Packaging (`python/package_project.py`) builds code only — it never cooks or places bundles, so packaged APKs/.app
contain no assets. A shared `products/` also mixes all platforms + quality tiers; the same source UUID has
per-platform / per-quality outputs (BC vs ASTC, low vs HD), so wholesale-adding `products/` would pick the wrong
payload.

## Goals / Non-Goals

**Goals:**
- Package the cooked asset bundles a distribution needs, at the platform's runtime location, and mount exactly those.
- Support several packages per platform (e.g. Android normal vs HD) without cross-tier UUID conflicts.
- Avoid adding a filesystem-enumeration API.

**Non-Goals:**
- Changing the cook/deserialization itself, or the on-demand cook path.
- Editor-side asset authoring.
- Streaming/patching bundles.

## Decisions

### D1. Selection is per distribution (export preset), not per platform
The export preset already is the distribution unit (name/platform/variant). Bundle selection hangs off it, so the
same platform can ship `android-normal` and `android-hd` with different content.

### D2. Quality tiers use distinct bundle names
`CookConfig.targets` maps target → bundle; quality tiers map to **different bundle names** (`tex → tex_low`,
`tex_hd → tex_hd`). A shipped package therefore never holds two bundles containing the same UUID, so
`OpenFile`'s first-match is unambiguous. (Alternative — same bundle name with per-distribution content — would need a
distribution-scoped products root and is not chosen.)

### D3. Single source: cook config `distributions` → bundles; preset override
Bundle selection is declared in the cook config keyed by **distribution** (the export preset name), extending today's
platform-keyed `CookConfig.presets`. `export_presets.json` may override with an explicit `bundles` list. The cook
config stays the single source; the preset is an optional override.

### D4. Manifest `products/bundles.json` drives runtime mounting
Packaging writes `<productsRoot>/bundles.json`:

```json
{ "platform": "android", "distribution": "android-hd", "bundles": ["common", "tex_hd"] }
```

The runtime opens this manifest (`OpenFile`, already available on every backend, including the APK asset filesystem)
and mounts **exactly** the listed bundles. Rationale: the `FileSystem` interface has **no enumeration API**, so a
"scan products/ synchronously" approach is not possible without adding one to core + every backend (notably a
recursive `AAssetDir` walk on Android). The manifest is an output of the packaging step (single writer), so it does
not become a second hand-maintained source. If the manifest is missing, the runtime logs and falls back to `common`.

### D5. Placement per platform
- **Android**: bundles go into the APK under `assets/products/<bundle>/`, read at runtime via the APK asset
  filesystem (`AndroidPlatform::GetBundleFileSystem()` / `AAssetManager`). No first-run copy.
- **Desktop / iOS**: `products/<bundle>/` next to the launcher (`GetBundlePath()/products/…`); inside a macOS/iOS
  `.app`, `Contents/Resources/products/…`.

### D6. Runtime mounting in `GameApplication`
Replace the hardcoded `common`/none logic: build the products root per platform, read `products/bundles.json`, and
`AddAssetProductBundle` each listed bundle (Android via the asset FS, desktop/iOS via `GetBundlePath()`). Fixes the
current desktop "no bundle" and Android "only common" gaps.

## Risks / Trade-offs

- [Manifest drift] -> the manifest must list exactly what packaging copied. Mitigation: packaging generates both from
  one selection in the same step.
- [Bundle-name discipline] -> correctness relies on distinct bundle names per tier; a same-name collision across
  shipped bundles reintroduces ambiguity. Mitigation: document + a packaging check that the selected bundles don't
  share a UUID (or at least are distinct names).
- [Android asset recursion] -> reading `products/bundles.json` from APK assets is a single `OpenFile`; the per-bundle
  payloads are read by the existing `HashedAssetBundle` over the asset FS (already implemented).
- [Cook cost] -> cooking per distribution may re-cook shared sources; the cook cache mitigates.
- [Missing manifest] -> runtime falls back to `common` with a warning (no crash).

## Migration Plan

- Packaging gains a cook+bundle step behind the export preset; a project without cooked `products/` produces a
  package whose `bundles.json` is `[]`+`common` fallback (or the step is skipped with a warning).
- Runtime mounting is additive; dev `--project` mode is unchanged.

## Open Questions

- Whether to also allow the editor's on-demand cook to write into a distribution-scoped products root.
- Whether `bundles.json` should also carry a content hash/version for patching.
