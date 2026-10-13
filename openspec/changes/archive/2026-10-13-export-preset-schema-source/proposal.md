## Why

The `export_presets.json` schema is defined twice - the C++ `ExportPreset` struct/reflection (editor side) and the
ad-hoc key reads in `python/package_project.py` (packaging CLI) - and the only guard
(`ExportPresetTest.CliSchemaKeys`) hand-copies the key list, so it cannot detect drift. Drift already exists: the
CLI reads `cacheVariables`, `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, which the editor does not
model and silently drops when it saves the preset file. The backlog already flagged this as "the duplication of the
schema definition" debt; this change removes it with a single machine-readable source and makes the editor fully
own the preset.

## What Changes

- Make the C++ **reflection registration** the single source of the export preset field set, and drive
  `Load`/`Save` from it via custom `JsonSave`/`JsonLoad` serializers (replacing the hand-written rapidjson). Adding a
  field then touches one `.Member<>` list, not a loader/writer pair.
- Add a **keys-only manifest** (`configs/export_preset.schema.json`) as the Python-facing bridge, guarded against the
  reflection by a C++ test.
- Model the currently CLI-only fields in the editor so they are **preserved and editable**:
  `cacheVariables`, `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, `extraBuildArgs`.
- Keep the on-disk shape stable: `cacheVariables` remains a JSON object (`{key: value}`); the editor models it as an
  ordered key/value sequence (the reflected form already supports structs + sequences) and the serializer writes the
  object form.
- Replace the hand-copied `CliSchemaKeys` key list with a **conformance check** (reflection == manifest), and add a
  Python-side conformance check so a drift in either consumer fails a test instead of shipping.
- Fix the Windows-only defect in `ExportPresetTest` (an `std::ifstream` left open across `remove_all`).

**BREAKING**: none. The on-disk `export_presets.json` format is unchanged (array or `{ "presets": [...] }`,
`cacheVariables` an object); only new optional fields gain editor support.

## Capabilities

### New Capabilities

- `export-preset`: the distribution export preset schema, its editor reflection/form surface, its reflection-driven
  serialization, and the C++<->Python conformance guarantee derived from a single C++ reflection registration and a
  keys-only manifest.

### Modified Capabilities

<!-- none: no existing capability in openspec/specs/ has requirement changes. `game-project-build` is still an
     in-flight delta (add-game-project-build) and is not a main spec, so this change does not modify it. -->

## Impact

- `engine/framework/include/framework/project/ExportPreset.h` - struct gains the CLI-only fields (`cacheVariables` as
  an ordered key/value sequence type, plus four string sequences).
- `engine/framework/src/project/ExportPreset.cpp` - hand-written JSON replaced by custom `JsonSave`/`JsonLoad`
  driven by `TypeNode::members`; `ExportPresets::Load`/`Save` on file archives; registration guard via `sky::Once`.
- `engine/framework/include/framework/serialization/JsonArchive.h` - added `IsRootArray`/`StartRootArray`/`HasMember`.
- `engine/test/framework/ExportPresetTest.cpp` - reflection-vs-manifest conformance + fixture round-trip.
- `configs/export_preset.schema.json` - keys-only manifest.
- `python/tests/test_export_preset_schema.py` - CLI-vs-manifest check (no dependency added).
- `PresetEditorPanel` - no code change; the new fields render via existing struct/sequence support.
