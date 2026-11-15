## Why

Projects need to choose the material assigned to newly created objects. Engine repository assets should use the normal asset database and cook/load pipeline, without adding procedural definitions or a new resource-reference schema to components.

## What Changes

- Store project editor defaults in `configs/editor.json`, using the existing reflected settings/session/storage workflow.
- Let Aurora.Editor contribute the default mesh material; creation copies its asset UUID into new objects.
- Create Cube/Plane from the existing engine `shapes/cube.glb` and `shapes/ground_plane.glb` sources. Add the ordinary `materials/lit.mat` source and its manifest identity.
- Keep the component UUID schema and resource ownership. Preserve legacy identities in real Engine asset manifests, removing Builtin catalog entries and runtime fixed-ID generation.
- Restore missing project cook configuration from engine defaults so repository assets cook normally; invalid cook JSON fails explicitly.
- Migrate the reused preference error paths to explicit results as approved by the user.

## Capabilities

### New Capabilities
- `project-editor-defaults`: Project-scoped reflected creation settings backed by repository assets.

### Modified Capabilities

## Impact

Sandbox core/shell/module settings, Aurora editor configuration, repository assets and targeted regression tests. No new targets, directory moves, full PBR implementation or scene format migration.
