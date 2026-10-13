## 1. Schema manifest

- [x] 1.1 Add `configs/export_preset.schema.json` listing the export preset field keys (keys-only; the earlier
      `type`/`editor` metadata was dropped as unused)
- [x] 1.2 Inject the manifest path into `FrameworkTest` as a CMake compile definition so the test resolves it
      without depending on cwd

## 2. Editor model

- [x] 2.1 Add `ExportPresetCacheVariable { key, value }` and the five fields (`cacheVariables`,
      `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, `extraBuildArgs`) to `ExportPreset`
- [x] 2.2 Register the new struct + fields in `RegisterExportPresetReflection` (LABEL/CATEGORY/ORDER), reusing
      existing sequence/struct form support
- [x] 2.3 Verify `PresetEditorPanel` renders/edits the new fields with no hand-written per-field widget

## 3. Reflection-driven serialization (replaces the hand-written rapidjson Load/Save)

- [x] 3.1 Register flat custom `JsonSave`/`JsonLoad` on `ExportPreset` and `ExportPresets`, driven by the reflected
      members (`GetValueRawConst` / `SetValueRaw` / `SequenceVisitor`); `cacheVariables` as a JSON object
- [x] 3.2 Add root-array/member accessors to `JsonInputArchive` (`IsRootArray`, `StartRootArray`, `HasMember`)
- [x] 3.3 Rewrite `ExportPresets::Load`/`Save` on file archives + the serializers; delete the rapidjson
      `Quote`/`ReadArray`/`ReadString`/... helpers
- [x] 3.4 Switch the `RegisterExportPresetReflection` guard from `static bool` to `sky::Once` (root AGENTS rule);
      have `Load`/`Save` call it so they do not need the panel to register first

## 4. Conformance tests

- [x] 4.1 `ExportPresetTest.CliSchemaKeys` asserts the reflected member set equals the manifest key set
- [x] 4.2 `ExportPresetTest.CliFixtureRoundTrip` loads a CLI-shaped fixture, saves, reloads, and keeps
      `cacheVariables` a JSON object
- [x] 4.3 Add a Python conformance check asserting every preset key literal in `package_project.py` exists in the
      manifest (this check found the fifth key, `extraBuildArgs`)
- [x] 4.4 Fix the `ExportPresetTest` Windows defect (ifstream left open across `remove_all`)

## 5. Verify

- [x] 5.1 Build and run `FrameworkTest` - 136/136 green
- [x] 5.2 Run the Python conformance check - 2/2 green
- [x] 5.3 Build `SandboxModule` + `SandboxEditor` and run a bounded editor session on a project whose preset
      contains the new keys (exit 0). Interactive edit not performed headlessly.
