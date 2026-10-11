## Why

Packaging (`python/package_project.py`) currently builds **code only**: it neither cooks assets nor places any product
bundle into the package. The runtime reads **cooked product bundles** (`products/<bundle>/product.index` + payloads)
via `AssetManager::AddAssetProductBundle`, but `GameApplication` only adds `common` on Android and **adds nothing on
desktop**. On top of that, a shared `products/` mixes **all platforms and quality tiers**, and the same source UUID
has per-platform / per-quality outputs (e.g. a texture's BC vs ASTC, low vs HD), while
`AssetManager::OpenFile(uuid)` returns the **first bundle that matches** — so shipping `products/` wholesale selects
the wrong (or a conflicting) payload.

## What Changes

- Make product-bundle selection a property of the **distribution (export preset)**, not the platform, so the same
  platform can ship several packages (e.g. Android normal vs HD).
- Represent quality tiers as **distinct bundle names** (`targets: tex → tex_low`, `tex_hd → tex_hd`), so a shipped
  package never contains two bundles with the same UUID.
- Cook and package only the **selected** bundles for a distribution, into a package-scoped `products/<bundle>/`.
- Place the bundles at the platform's runtime location: **Android → APK `assets/products/<bundle>/`**; **desktop/iOS →
  `products/<bundle>/` next to the launcher** (`.app/Contents/Resources/products/`).
- Ship a **`products/bundles.json` manifest** (written by packaging) listing the included bundles; the runtime mounts
  **exactly** those bundles. This avoids adding a filesystem-enumeration API (the `FileSystem` interface has none).
- Fix the runtime gap: `GameApplication` mounts the manifest's bundles on every platform (Android reads via the APK
  asset filesystem; desktop/iOS via `GetBundlePath()`), instead of a hardcoded `common` / nothing.

## Capabilities

### New Capabilities
- `asset-packaging`: per-distribution cooked bundle selection, package placement, the `bundles.json` manifest, and
  manifest-driven runtime mounting.

### Modified Capabilities
<!-- asset-pipeline is a feature doc, not a spec capability; no openspec/specs capability is modified. -->

## Impact

- `python/package_project.py` (cook + bundle selection + placement + manifest) and `export_presets.json` schema
  (`bundles` override).
- Cook config keying: distribution -> bundles (today `CookConfig.presets` is keyed by platform).
- Runtime: `engine/framework/.../GameApplication` bundle mounting; Android `AndroidBundleFileSystem` usage.
- Docs: `docs/features/game-project-build.md`, `docs/features/asset-pipeline.md`.
