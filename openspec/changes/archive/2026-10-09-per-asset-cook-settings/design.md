## Context

Asset cook settings are currently resolved **per product bundle** from
`configs/image_build_presets.json` (`ImageBuildPresets`: encode / srgb / quality / block / maxSize /
generateMip). `AuroraImageBuilder::Request` resolves a bundle config by `request.target` and applies it
wholesale. The source manifest (`assets.jsonl`) already supports a per-entry `cook` block, but only its
`targets` array is read (`CookConfig::GetTargets`); `CookTarget.settings` is parsed from the project cook
config and never consumed, and `AssetDataBase` has `GetCookJson` but no setter.

This change adds a **global default + per-asset override** model. The global default stays the existing
per-bundle preset file; the override is a sparse, per-target map stored in the asset's manifest `cook`
block, editable both by hand (JSON) and from the editor.

Prerequisites: `editor-asset-browser` (the `EditorAssetCatalog` + `AssetBrowserPanel` this change extends)
and `asset-pipeline-layering-refactor` (`AssetBuilder::DescribeSettings` and mount provenance). Both are
complete and pending archive; this design assumes their capabilities land first.

## Goals / Non-Goals

**Goals:**

- A single precedence rule: `effective(asset, target) = global preset[bundle] <- asset cook.settings[target]`.
- Store the override as schema-agnostic, **sparse** string key/value pairs so unset keys transparently fall
  back to the global default and future asset kinds reuse the same plumbing.
- Apply the override identically for in-process and out-of-process cooks, with no manifest parsing inside
  builders and no wire-protocol change.
- Let a builder declare a typed setting schema so the editor can render enum/bool/int controls.
- Read + write from editor core, and an editable UI in the asset browser details pane.

**Non-Goals:**

- ASTC 6x6 (block stays 4/8); adding ASTC_6x6 to `PixelFormat` is a separate change.
- Per-asset cook *mode* (in-process vs out-of-process) and per-asset worker settings.
- Changing the global preset file format/location; grid/thumbnail/detail-pane layout rework.
- Nested (object/array) override values; overrides are flat scalars.

## Decisions

### D1. Override location: manifest `cook` block, per-target sparse map

The override lives in the existing per-entry `cook` block:

```json
{
  "file": "textures/hero.png",
  "id": "…",
  "cook": {
    "targets": ["common", "tex_mobile"],
    "settings": {
      "tex_mobile": { "maxSize": 512, "generateMip": false },
      "tex_pc":     { "encode": "BC7", "quality": "SLOW" }
    }
  }
}
```

- `targets` keeps its current meaning (which bundles to cook for this asset).
- `settings` is keyed by **target/bundle**; each object is **sparse** (only overridden keys). This is the
  only place the global default is not used, so "reset" is just removing a key.
- Rejected alternatives: a per-asset sidecar file (splits asset identity across files, extra mount
  bookkeeping) and a project-level overrides file keyed by uuid (a second source of truth, poor hand-edit
  ergonomics next to the asset). The manifest already carries per-asset identity and the `cook` block.

### D2. Precedence and resolution

- Target set: `cook.targets` if present, else project targets (`CookConfig`), else the active platform's
  preset bundles (unchanged).
- Effective settings for a target: start from the builder's global preset for that bundle, then overlay
  `cook.settings[target]`. A `settings` entry for a target not in the target set is ignored.
- Resolution is computed on read; nothing is cached in a way that goes stale when the global preset or the
  override changes.

### D3. Transport: flatten to `AssetBuildRequest.settings`

`AssetBuildRequest` gains `std::map<std::string, std::string> settings` holding the resolved override for
`request.target`. `AssetBuilderManager::BuildRequest(uuid, target)` and `BuildRequestSync(uuid, target)`
fill it from `AssetDataBase::GetCookJson(uuid)` via a `CookConfig` helper. Builders never read the manifest.

- Rejected: passing the raw `cook` JSON to builders (leaks the manifest format into every builder).
- Rejected: a framework-side typed image struct (leaks image-specific types into framework; the pipeline is
  kind-agnostic).

### D4. Builder-declared reflected settings type (generic form, no hand-rolled controls)

Each builder declares a **reflected** cook-settings type registered with `SerializationContext` and exposes
it via `AssetBuilder::GetSettingsType()`; `MakeSettings(bundle, override)` materializes the effective
settings (preset overlaid with the sparse override) into a reflected `Any`, and `DiffSettings(bundle,
edited)` returns the sparse keys differing from the preset. The editor renders/edits this object **only**
through `ReflectedFormView` (`Bind(PropertyObject{obj, node}, baseline)`), so enums/bools/ints get their
standard controls and edits route through `CommandService` (undoable). This is the repository's config-UI
convention (`AGENTS.md`); hand-rolled per-field widgets are prohibited.

- `ReflectedForm::Build`/`ReflectedFormView::Bind` gain an optional explicit **baseline** `PropertyObject`,
  so the form's "reset" restores the global bundle preset rather than the type's default-constructed value.
- The image builder declares `ImageCookSettings { encode, srgb, quality, block, maxSize, generateMip }` and
  registers it (plus the enums) in `Aurora.Cook`.
- Builders without a settings type expose none; the pane then shows a read-only hint.

- Rejected: a hand-declared `BuildSettingSpec` schema with bespoke widgets (duplicates the reflection
  system; violates the config-UI rule). Rejected: exposing framework/`Any` types is acceptable here because
  `EditorCore` already depends on the reflection model (`TypeNode`).

### D5. Write path in editor core (reflected object)

`AssetDataBase::SetCookJson(uuid, json)` updates the manifest entry's `extra` field in place (preserving
`file`/`id`), writes only on writable mounts, and invalidates the manifest cache. `EditorAssetCatalog`
exposes `GetCookSettings(uuid, target) -> {Any object; Any baseline; const TypeNode* type}` (built from the
builder) and `ApplyCookSettings(uuid, target, const Any& edited)` which computes the diff via the builder
and replaces the manifest `cook.settings[target]` block (empty diff clears it). Changes are persisted but do
**not** implicitly re-cook; the existing Cook action applies them. The catalog-side requirements live in the
`editor-asset-catalog` capability (introduced by `editor-asset-browser`), which must be archived first.

### D6. UI: inline editing in the asset browser details pane

The details pane gains a per-target settings editor: one row per resolved target, one control per schema
key, the global-default value shown when a key is unset, and a per-key "reset to default" affordance.
Writes go through the catalog mutators; read-only mounts render the values non-editable.

- Rejected: a separate asset-settings window — more chrome, and the details pane already shows per-target
  effective settings.

### D7. Out-of-process cook

The IPC frame protocol is unchanged (uuid + target). The worker re-derives the override because
`CookWorker` calls `AssetBuilderManager::BuildRequest(uuid, target)`, which now fills `request.settings`
from the manifest the worker already loads. Requires verifying the worker loads the source manifests
(`AssetDataBase::Load`) and the build configs before cooking.

## Risks / Trade-offs

- **[Manifest corruption on write]** The `extra`/`cook` field is raw JSON. → Parse-modify-serialize with
  rapidjson and re-emit the entry; unit-test round-trips (write override, reload, verify `file`/`id`/other
  cook keys preserved).
- **[Worker/editor config divergence]** Worker must resolve the same override and presets. → The worker
  loads the same manifest and `LoadBuildConfigs`; add an integration test that an out-of-process cook
  honors an override.
- **[Stale effective settings in the editor]** Global preset or override changes must refresh the pane. →
  Resolution is live on read; the catalog bumps its revision on write so views re-read.
- **[Sparse-override drift]** Renaming/removing a bundle leaves orphan `settings` keys. → Ignore settings
  for non-cooked targets (D2); a later "prune" can remove them, not required now.
- **[UI scope creep]** Full typed controls add widget work. → Schema is optional; fallback is generic
  key/value rows, so the pipeline can land before the polish.

## Migration Plan

Additive and backward compatible: manifests without a `cook` block, or with only `targets`, behave exactly
as before (empty override). No data migration. Rollback is removing the override from the build request and
the editor write/UI paths; stored `cook.settings` blocks are simply ignored by older builds.

## Open Questions

- Should changing a setting auto-mark the asset "stale" (needs re-cook) in the tree badge? Leaning yes later;
  out of scope here (keeps this change to storage + editing).
