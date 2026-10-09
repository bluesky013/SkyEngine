> Status: **blocked-on-prereq** — depends on `editor-asset-browser` and `asset-pipeline-layering-refactor`
> (both complete, pending archive). This change must be archived after them so the `editor-asset-catalog`
> and `asset-builder-settings` capabilities it extends exist in `openspec/specs/`.

## Why

The asset pipeline resolves texture cook settings only per product bundle
(`configs/image_build_presets.json`): one encode / block / srgb / quality / max-size / mip policy applies
to every asset in a bundle. Authors need **per-asset overrides** — e.g. one texture cooked as ASTC 4x4 with
no mips and clamped to 512, while everything else keeps the global bundle defaults. Today the manifest
`cook` block is only read for its `targets` list (`CookConfig::GetTargets`); the parsed `CookTarget.settings`
are never consumed, and there is no read/write path or editor UI for per-asset settings.

## What Changes

- Define a **per-asset cook override** in the source manifest `cook` block: `settings` is a map from
  **target/bundle** to a **sparse** settings object (only the keys the author overrides).
- Resolve effective settings as **global bundle preset <- per-asset per-target override**, keeping the
  existing target-set rule (asset `targets` -> project targets -> active platform preset bundles).
- Plumb the override through the build request (`AssetBuildRequest.settings`) so both the in-process and the
  out-of-process (worker) cook paths apply it **without the builder reading the manifest**; unset keys fall
  back to the global preset.
- Let builders describe a **typed setting schema** (`AssetBuilder::DescribeSettingSchema`) so the editor can
  render the right controls (enum/bool/int) and enumerate options; `DescribeSettings` reports the effective
  values.
- Expose read/write in the asset catalog (editor core module): it surfaces the effective per-target settings
  and can write the sparse override back to a **writable** asset's manifest.
- Add editor UI: the asset browser details pane edits the selected asset's per-target settings with a
  **reset-to-default** action per setting; read-only mounts are non-editable.
- **Non-goals**: per-asset cook *mode*, and changing the global preset file format/location. (ASTC 6x6
  **is** included: `block` accepts 6.)

## Capabilities

### New Capabilities

- `asset-cook-override`: the per-asset/per-target sparse override schema in the manifest, its precedence
  over the per-bundle global preset, the build-request plumbing, the manifest write API, and the
  builder-declared typed setting schema.

### Modified Capabilities

- `aurora-cook`: the image builder applies the per-asset override over the resolved bundle config and
  reports the effective settings.
- `editor-asset-catalog` (introduced by the prerequisite `editor-asset-browser`; must land first): the
  catalog exposes the effective per-target settings and writes the sparse override to writable manifests.
- `editor-ui-shell`: the asset browser provides editable per-target cook settings with reset-to-default.

## Impact

- Framework: `framework/asset/AssetCommon.h` (`AssetBuildRequest.settings`), `AssetBuilder.h`
  (`DescribeSettingSchema` + `DescribeSettings` override overload), `AssetBuilderManager.*` (fill request
  settings; `GetBuilderSettings` override parameter), `CookConfig.*` (parse `cook.settings`),
  `AssetDataBase.*` (`SetCookJson` + manifest write).
- Aurora cook: `ImageBuildConfig` / `AuroraImageBuilder` (apply the override; declare the schema).
- Editor core: `editor/core/asset/EditorAssetCatalog.*` (effective settings, write).
- Editor shell: `editor/shell/panels/AssetBrowserPanel.*` (editable per-target settings UI).
- Cook IPC: unchanged on the wire (uuid + target only); the worker re-derives the override from the
  manifest it already loads.
- Tests: `FrameworkTest` (CookConfig parse, request plumbing, manifest write), `AuroraCookTest` (override
  merge), `EditorCoreTest` (catalog read/write), `EditorShellTest` (editable pane).
- Prerequisites: `editor-asset-browser` (catalog / panel / actions) and `asset-pipeline-layering-refactor`
  (`asset-builder-settings`, mount provenance) — both complete, pending archive; this change extends them
  and assumes their capabilities land first.
