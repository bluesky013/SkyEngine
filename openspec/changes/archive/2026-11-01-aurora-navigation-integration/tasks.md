## 1. Interfaces and owners

- [x] 1.1 Separate engine contracts from plugin world/asset/authoring implementation and provider-owned terrain bridge.
- [x] 1.2 Add linkable source owners; separate runtime, builder, editor and render dependencies.
- [x] 1.3 Update consumers, manifests and cook-host dependencies.

## 2. Runtime and backend

- [x] 2.1 Apply/validate settings and make registrations/code leases symmetric.
- [x] 2.2 Remove filter downcasts; reject incomplete paths and project endpoints.
- [x] 2.3 Unify tile origins, preserve untouched tiles and propagate build/load errors.
- [x] 2.4 Own all streaming layers and synchronize/collect real asynchronous queries safely.

## 3. Editor and rendering

- [x] 3.1 Register a deferred Build Navigation action for the active world.
- [x] 3.2 Add a rendering bridge and debug visibility action through the existing frame producer.
- [x] 3.3 Retain callback leases and remove actions/overlays on shutdown.

## 4. Verification

- [x] 4.1 Run source ownership checks and build affected production targets.
- [x] 4.2 Run real Recast generation/query/incremental/error and runtime configuration/streaming/lifecycle tests.
- [x] 4.3 Build editor/render targets and run relevant editor contract tests.
- [x] 4.4 Record native evidence, dependency preflight and remaining visual-validation limits.

## 5. Real scene validation

- [x] 5.1 Create an actual project with matching visible/collision geometry, a detour wall and a disconnected island.
- [x] 5.2 Cook collision and navmesh products through the existing AssetTool and verify nonempty output.
- [x] 5.3 Load the cooked mesh through runtime configuration and verify synchronous/asynchronous detour and rejection paths; export evidence.
- [x] 5.4 Open the scene in the editor and inspect the navigation debug view.
- [x] 5.5 Fix deferred collision readiness exposed by scene cooking and run relevant Framework/Jolt regressions.

## 6. Checkpoint review and concurrent queries

- [x] 6.1 Replace the shared Detour search context with independent query contexts and shared-reader/exclusive-writer mesh protection.
- [x] 6.2 Freeze asynchronous filters, linearize cancellation/terminal publication and retain actual in-flight admission slots.
- [x] 6.3 Validate navigation codec counts/lengths before allocation without changing the current format.
- [x] 6.4 Back off failed coordinates, prioritize fresh work, report pending coordinates and retain failed rollback cleanup.
- [x] 6.5 Unregister the terrain provider before releasing the bridge's last world reference.
- [x] 6.6 Run the affected production build and regression tests, including real reader overlap/query-tile concurrency and hostile codec input.
- [x] 6.7 Finish the exact commit manifest/review handoff, with current limitations and the separate IO proposal clearly excluded.
- [x] 6.8 Obtain explicit user confirmation of message and file scope before commit; archive remains a separate user decision.

## 7. Framework-owned compression

- [x] 7.1 Replace the codec runtime module with Framework's default LZ4 implementation and an ordinary foundation build option.
- [x] 7.2 Establish host ownership before loading modules, acquire provider/code leases and keep override/disable behavior explicit.
- [x] 7.3 Remove CompressionModule/plugin/host entries and migrate codec consumers and tests without changing product bytes.
- [x] 7.4 Verify real default-codec/override/concurrent/edge tests, affected production modules, fresh cook, scene loading and bounded editor startup; exercise the separate OFF configuration.
- [x] 7.5 Refresh the exact staged manifest and review notes; preserve the pending user-confirmation gate.
