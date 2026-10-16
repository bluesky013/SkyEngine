## Context

The confirmed Debug crash mixes the 2026-10-17 SandboxEditor binary (UUID prefix 70A751C3) with a 2026-10-13 BulletPhysicsModule binary (UUID prefix 993666EC). IModule gained RegisterMeta before its existing lifecycle methods. The host therefore indexes a different virtual table layout than that older module supplies; its PostTick call goes past the old function entries into RTTI data and raises SIGBUS. RecastNavigation is also older and must be rebuilt, although the identified crashing dispatch is Bullet's.

The editor host loads module names from configs/modules_editor.json. Dynamic loading creates no linker dependency that would make a host-only target build update those libraries. The earlier native startup checkpoint cannot establish freshness for a later Debug run or another configuration's module files.

## Goals / Non-Goals

**Goals:** make a SandboxEditor build schedule the configured module targets available in this build tree; keep host and modules in the same selected configuration; retain optional-target behavior; and record actual Debug startup verification.

**Non-goals:** a runtime ABI/version handshake, binary packaging/deployment redesign, force-enabling optional plugins, a global rebuild of every module, new plugins, or changes to renderer/RHI/RDG/Cocoa/Core Event behavior.

## Decisions

### Bind the configured host modules after target creation

Read the existing modules array from configs/modules_editor.json in top-level CMake after add_subdirectory(plugins), when both engine and plugin module targets are known. For each module name with an existing CMake target, add a build-only dependency from SandboxEditor. Do not link the module library into the executable or introduce includes of concrete plugin types.

Guard the operation when SandboxEditor is absent. The checked-in manifest is a build configuration input whenever that host is configured. Skip names without a configured target, preserving optional/minimal configurations and externally supplied module handling. This skip does not claim that the missing module is available at runtime. Existing runtime configuration, loading, dependencies and error behavior remain unchanged.

Track the manifest as a configure dependency so editing its module set updates the generated host dependency graph on the next build. The manifest remains the selection source; a separate hard-coded Bullet/Recast list would repair this instance but leave the next configured module exposed.

### Use the selected configuration's ordinary dependency graph

The dependencies schedule existing source targets through CMake's normal selected configuration. Header/source changes that affect those targets must therefore be considered before the host target finishes. Incremental builds may reuse genuinely up-to-date outputs; no always-rebuild rule or timestamps-only ABI inference is added.

The guarantee applies to available targets in this configured source build. It does not inspect arbitrary external binaries, prevent manual copying of incompatible libraries after a build, or ensure ABI compatibility across independently built trees. An IModule ABI change still requires every affected runtime module to be rebuilt against matching headers.

### Verify build freshness and actual startup separately

Inspect the generated host dependency graph/dry run for configured targets, including BulletPhysicsModule and RecastNavigation when enabled. Rebuild them and the host in Debug from the same build tree. Record the launched executable and loaded library paths/configuration plus available binary UUID/timestamp evidence; do not substitute a Release startup for the user's Debug path.

Run the actual Sandbox startup in a bounded session and capture exit/crash/log evidence. If the module ABI crash is gone but the host cannot establish a GPU/display, report those as separate outcomes. Existing Windows/native/display/DPI/GPU gates are not automatically closed by a successful module rebuild or bounded CPU startup.

## Validation

- Configure with SandboxEditor and the existing editor module manifest, then inspect the generated build dependencies for available configured targets.
- Verify no-host/missing-module-target guards and manifest change tracking without force-enabling plugins.
- Build the same Debug host/modules and record actual fresh-output and startup evidence, including whether the former Bullet PostTick/RTTI fault recurs.
- Validate this change strictly, check whitespace, and review that source changes remain build coordination rather than renderer/backend/native-window/Core Event behavior changes.

Actual build/runtime results are recorded in tasks.md and canonical §20. Commit and OpenSpec archive require separate explicit user confirmation.
