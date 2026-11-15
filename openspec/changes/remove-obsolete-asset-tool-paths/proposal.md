## Why

The inactive AssetBuilder executable still calls removed asset APIs and its otherwise unused ToolApplicationBase retains an obsolete module-config path. CPython Ninja discovery also embeds one developer's Visual Studio installation path.

## What Changes

- Remove the inactive AssetBuilder sources and its unreferenced ToolApplicationBase; keep AssetTool as the supported cook host.
- Remove the unused XR config-path constant without deleting the referenced XR application class.
- Discover Ninja through an explicit NINJA override or PATH, eliminating the machine-specific fallback.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `build`: portable Ninja discovery and a single supported asset-tool source entry.

## Impact

Four obsolete source/build files are removed. Framework is reconfigured and rebuilt to update its source list; AssetTool remains built. No directories or module ownership are moved. The broader refactor-asset-tool-module-config change retains its own outstanding work; only its historical ToolApplicationBase reference is superseded here.
