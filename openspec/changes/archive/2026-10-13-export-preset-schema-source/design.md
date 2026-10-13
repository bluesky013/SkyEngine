## Context

`export_presets.json` is a project-owned file consumed by two independent code paths:

- **Editor (C++)**: `sky::ExportPreset` (`engine/framework/.../ExportPreset.h`) with a hand-written `Load`/`Save`
  (rapidjson) and a reflection registration that drives `PresetEditorPanel` (a `ReflectedFormView`).
- **Packaging CLI (Python)**: `python/package_project.py`, which reads preset keys ad hoc via
  `preset.get('<key>')` / `preset['<key>']`.

There is no shared definition of the key set, so drift is structurally possible and already present:
`package_project.py` reads `cacheVariables`, `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, and
`extraBuildArgs` (`python/package_project.py:254-262,443`), none of which `ExportPreset` models - so the editor
silently drops them on save. The only guard, `ExportPresetTest.CliSchemaKeys`, hardcoded a key list and therefore
could not catch drift.

Constraints:
- `requirements.txt` is intentionally minimal (`GitPython` only); the change must not add a Python dependency.
- The editor is C++ engine-drawn and all config UI must go through the reflected form (AGENTS.md). The reflected
  form supports scalars, strings, structs, and **sequences**, but **not maps/dictionaries**.
- The on-disk `export_presets.json` format is a shared, shipped artifact; it must not change incompatibly.
- The engine already supports **custom `JsonSave`/`JsonLoad`** per registered type (`TypeFactory::JsonSave<>`,
  `CoreReflection.cpp`), so a type can define a non-default JSON shape.

## Goals / Non-Goals

**Goals:**

- The export preset field set lives in exactly **one** place in C++: the reflection registration
  (`RegisterExportPresetReflection`). `Load`/`Save` are driven from it, so adding/removing a field touches one list.
- A machine-readable **keys-only manifest** exposes that key set to the Python CLI and is guarded against the
  reflection by a test, so drift fails a test in either language.
- The editor owns the previously CLI-only fields (preserve + edit), closing the data-loss drift.

**Non-Goals:**

- No build-time code generation of the struct/serde (the "Tier 2" path).
- No change to `ProjectDescriptor` / `.skyproj` `settings.*` keys, and no change to `export_credentials.json`.
- No packaging behavior change.

## Decisions

### Decision: Reflection is the single source; a keys-only manifest bridges to Python

`configs/export_preset.schema.json` holds just the key list (`{ "keys": [...] }`). The C++ field set lives in
`RegisterExportPresetReflection` (`.Member<&ExportPreset::x>("x")`); `ExportPresetTest.CliSchemaKeys` asserts the
reflected member set equals the manifest key set. Earlier the manifest also carried `type`/`editor` metadata, but
nothing consumed it, so it was dropped (dead metadata).

- Alternatives: **JSON Schema / full metadata** - unused by either consumer; rejected. **A Python module as the
  source** - makes C++ read Python or duplicate it, biasing ownership. **A C++ header as the source** - same bias
  inverted. A neutral keys file is the minimal Python-facing bridge.
- Placed in `configs/` (sibling of `*_build_presets.json`); the project data file stays `export_presets.json`.

### Decision: Reflection-driven custom `JsonSave`/`JsonLoad` (flat format)

The hand-written rapidjson `Load`/`Save` are replaced by custom serializers registered on the types
(`.JsonSave<&SavePreset>().JsonLoad<&LoadPreset>()` on `ExportPreset`, and the same on `ExportPresets`), mirroring
how `CoreReflection` registers `Vector2/3/4`. The serializers iterate `TypeNode::members` via
`GetValueRawConst` / `SetValueRaw` / `SequenceVisitor`, so **keys are declared only in the `.Member<>` calls**.

- The default reflection serializer emits a `classId`/`elements` envelope; the preset must stay flat, so we write
  our own flat object/array. `cacheVariables` is emitted as a JSON **object** to match the CLI.
- `JsonInputArchive` had no way to reach a bare root array (`Start`/`StartArray` resolve a *named* member), so it
  gained `IsRootArray()`, `StartRootArray()`, and `HasMember()` - small, generic accessors.
- Consequence: key order in the file becomes alphabetical (`TypeNode::members` is a `std::map`). JSON member order
  is not semantically significant; the CLI and the tests are order-independent.
- Alternatives: keep the hand-written rapidjson (the pre-existing duplication) - rejected, it is the redundancy
  this change removes. `JsonArchive`'s generic path - rejected, it forces the `classId`/`elements` envelope.

### Decision: `cacheVariables` is an ordered key/value sequence in C++, a JSON object on disk

The reflected form has no map widget, so `ExportPreset::cacheVariables` is
`std::vector<ExportPresetCacheVariable>` where `ExportPresetCacheVariable { std::string key; std::string value; }` is
reflection-registered; the form renders it as a sequence of nested structs. The serializer detects this member by
its element type and writes a JSON object; `Load` reads the object members into the sequence.

- Alternatives: **add map support to serde + the form** - broad change, out of scope. **Array of pairs on disk** -
  rejected: breaks the shared schema and the CLI.

### Decision: Conformance checked in both languages

- **C++**: `ExportPresetTest.CliSchemaKeys` loads the manifest (path injected by CMake as a compile definition, so
  it does not depend on cwd) and asserts the **reflected** member set equals the manifest key set.
- **Python**: a test reads the manifest and scans `package_project.py` for preset key string literals
  (`preset.get('k')`, `preset['k']`) and asserts every literal is in the manifest.

### Decision: The CLI-only fields become editor-editable

`cacheVariables`, `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, `extraBuildArgs` are added to
`ExportPreset` and its reflection registration (with `LABEL`/`CATEGORY`/`ORDER` under a "Build" category).
`extraBuildArgs` was not known when the proposal was written; the Python conformance check flagged it as a fifth
CLI-read key on first run (`package_project.py:443`), confirming the value of the check.

### Decision: Registration guard uses `sky::Once`, not a function-local `static bool`

`RegisterExportPresetReflection` previously used a function-local `static bool` guard, which is per-DLL and can
re-register into the shared `SerializationContext` (assert). Switched to `sky::Once` per the root AGENTS rule.
`Load`/`Save` now call it so they work without the editor panel having registered first.

### Decision: Fix the Windows test defect as part of this change

The old `CliSchemaKeys` held an `std::ifstream` open across `std::filesystem::remove_all`, which throws on Windows.
The stream is scoped so it closes first.

## Risks / Trade-offs

- [Alphabetical key order changes the file's appearance] -> Acceptable; JSON order is not semantic and both
  consumers are order-independent. Existing files still load.
- [The generic serializer falls back to the `classId`/`elements` envelope for a member of an unhandled type] ->
  Current members are scalar/string/string-sequence/cache-variable-sequence only; a future non-trivial member needs
  its own custom serializer (same as any type in this engine).
- [Test cannot locate the manifest] -> Injected as a CMake compile definition rather than relying on cwd.
- [The Python literal scan is heuristic (misses dynamically built keys)] -> Acceptable: a drift guard, not a proof.
- [On-disk format compatibility] -> Preserved: bare array (or `{ "presets": [...] }`), `cacheVariables` object, same
  key names and defaults (the per-field defaults now come from the struct's default initializers instead of the
  loader's fallbacks).

## Migration Plan

1. Register a flat custom `JsonSave`/`JsonLoad` on `ExportPreset`/`ExportPresets`; add the archive root accessors.
2. `ExportPresets::Load`/`Save` use file archives + the custom serializers; `RegisterExportPresetReflection` uses
   `sky::Once`.
3. No data migration: on-disk format and key names are unchanged.

## Open Questions

- Should the manifest be shared with `asset_build_presets.json` / cook config (same editor-surface class of
  problem), or stay preset-specific? Deferred; this change is scoped to the export preset.
