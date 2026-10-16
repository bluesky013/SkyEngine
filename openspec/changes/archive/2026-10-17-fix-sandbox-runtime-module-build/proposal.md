## Why

The user's Debug Sandbox startup crashes after loading an older Bullet module. The current host's IModule layout contains RegisterMeta, while the stale module uses the preceding virtual table: PostTick dispatch reaches RTTI data and faults with SIGBUS. Recast is also stale. Building SandboxEditor alone does not currently schedule all modules it loads from modules_editor.json, so a successful host build can still launch incompatible configured modules.

## What Changes

- After plugin targets are defined, add build dependencies from SandboxEditor to existing module targets named in configs/modules_editor.json.
- Keep dependency selection tied to the checked-in manifest, including reconfiguration when it changes; preserve configurations without SandboxEditor and skip module names without a configured target.
- Rebuild the stale configured modules in the same Debug build/configuration and verify the user's startup path with actual process/module evidence.
- Record startup, module freshness and GPU/display observations separately. This is build coordination, without a new plugin, binary ABI checker or runtime-loader redesign.

## Capabilities

### New Capabilities

- `editor-build`: building the editor host prepares its configured, available runtime module targets in the same build configuration.

### Modified Capabilities

None. Runtime loading and module packaging behavior remain unchanged.

## Impact

Top-level CMake build ordering/dependencies, existing configured module rebuilds, and canonical startup evidence. No new plugin or changes to RHI/RDG, rendering behavior, Cocoa/native-window ownership, Core Event or module interfaces. Externally supplied modules without source targets remain outside the build-freshness guarantee. No commit or OpenSpec archive is authorized.
