## ADDED Requirements

### Requirement: Single export preset schema manifest

There SHALL be exactly one machine-readable manifest listing the export preset field keys, checked into the
repository. The C++ field set SHALL live in the reflection registration (`RegisterExportPresetReflection`); the C++
test SHALL assert the reflected member set equals the manifest, and the packaging CLI SHALL be validated against the
manifest. Neither consumer SHALL define the key set independently.

#### Scenario: Manifest lists the keys

- **WHEN** the manifest is read
- **THEN** it SHALL list every key that appears in `export_presets.json`

#### Scenario: Located without depending on cwd

- **WHEN** the C++ conformance test runs from any working directory
- **THEN** it SHALL resolve the manifest path from a build-provided location, not from the process cwd

### Requirement: Reflection-driven export preset serialization

`ExportPreset` and `ExportPresets` SHALL serialize through custom `JsonSave`/`JsonLoad` registered with the type and
driven by the reflected members, in the flat `export_presets.json` format (a bare array of preset objects, or an
object with a `presets` array). Adding a field SHALL require only registering it - no separate loader/writer edit.

#### Scenario: Adding a field touches one list

- **WHEN** a field is added to the export preset
- **THEN** it SHALL be declared once in the reflection registration and listed in the manifest, and both load and save SHALL pick it up

#### Scenario: cacheVariables stays an object

- **WHEN** a preset is saved
- **THEN** `cacheVariables` SHALL be written as a JSON object, not an array

### Requirement: Editor models and round-trips every export preset field

`sky::ExportPreset` SHALL model and round-trip every manifest field, including the previously CLI-only
`cacheVariables`, `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, and `extraBuildArgs`. Loading a preset
that contains these fields and saving it again SHALL preserve their values.

#### Scenario: Previously dropped fields are preserved

- **WHEN** a preset containing `cacheVariables`, `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, and
  `extraBuildArgs` is loaded and saved by the editor model
- **THEN** all five fields SHALL be present with their original values in the saved output

#### Scenario: cacheVariables keeps its on-disk object shape

- **WHEN** a preset with `"cacheVariables": { "KEY": "VALUE" }` is loaded and saved
- **THEN** the saved `cacheVariables` SHALL remain a JSON object with the same key/value entries

### Requirement: Export preset fields are editable through the reflected form

Every export preset field SHALL be editable through the reflected form without hand-written per-field widgets.
`cacheVariables` SHALL be edited as an ordered key/value sequence.

#### Scenario: Editing a previously CLI-only field

- **WHEN** the user edits `cacheVariables` (add/remove/change an entry) in the export presets panel
- **THEN** the edit SHALL go through the reflected form and `CommandService` and be persisted on save

### Requirement: Schema conformance enforced in both consumers

A drift between the manifest and either consumer SHALL fail an automated test.

#### Scenario: C++ reflected key set matches the manifest

- **WHEN** the C++ conformance test compares the reflected members of `ExportPreset` to the manifest
- **THEN** the two key sets SHALL be equal, and a mismatch SHALL fail the test

#### Scenario: CLI key set matches the manifest

- **WHEN** the packaging CLI's preset field accesses are checked
- **THEN** every preset key literal referenced by `python/package_project.py` SHALL exist in the manifest, and any
  unknown key SHALL fail the Python conformance test

### Requirement: On-disk export preset format stays compatible

This change SHALL NOT change the on-disk `export_presets.json` schema for existing fields; existing files SHALL
continue to load and their keys SHALL remain valid for the packaging CLI.

#### Scenario: Existing preset still loads and packages

- **WHEN** an existing `export_presets.json` written before this change is opened by the editor and consumed by the
  CLI
- **THEN** it SHALL load without migration and the CLI SHALL read the same keys as before
