## 1. Design and module boundary

- [x] 1.1 Inventory active Bullet source, build/package consumers, engine contracts and existing test coverage; record compatibility gaps in design.md.
- [x] 1.2 Verify the official Jolt v5.6.0 full revision and create proposal/design/spec deltas and an implementation checklist.
- [x] 1.3 User confirmed migration on 2026-10-26 and requested/authorized the simplified adaptor + render layout on 2026-10-27. Proceed with implementation; commit/archive remain unapproved.

## 2. Third-party package and build targets

- [x] 2.1 Replace the bullet3 manifest/importer with pinned jolt; record ABI/CRT/precision/SIMD/assert/profiler/debug-renderer settings and license packaging.
- [x] 2.2 Build and verify Jolt Debug and Release using the existing bootstrap and configured third-party output root.
- [x] 2.3 Create JoltPhysicsModule (adaptor), JoltPhysicsRenderModule and JoltPhysicsTest without a separate core target in the approved plugin subdirectories; verify dependency direction and SDK header privacy.

## 3. Engine contract and lifecycle

- [x] 3.1 Add PhysicsStatus/errors/native capacity data and explicit motor mode; update Step/SetOptions call sites and test fakes, preserving serialized enum values and defaults.
- [x] 3.2 Implement explicit SDK initialization and process-shared state ownership with world leases; preserve module code leases across factory/world/callback lifetimes.
- [x] 3.3 Implement factory unregistration, world destruction, initialization retry and runtime restart; test outstanding worlds after registry removal and final SDK release.
- [x] 3.4 Validate capacities/options and report unsupported growth or requests before mutation; test failure leaves existing options and object handles intact.

## 4. Bodies and geometry

- [x] 4.1 Implement stable slot/generation/type mapping, body creation/destruction, transforms, COM conversion, forces/impulses, mass/inertia, sleep and CCD with native result checking.
- [x] 4.2 Implement box/sphere/capsule/hull/compound cooking including pivots, supported scale, nested failure cleanup and malformed-data rejection.
- [x] 4.3 Implement triangle mesh and heightfield cooking including U16/U32/stride checks, rectangular fallback, nonzero height mapping, upAxis and adjacent terrain-tile seam tests.
- [x] 4.4 Implement generation-checked material handles, supported friction/damping validation and symmetric contact combine rules; test reassignments and stale handles.

## 5. Interaction and queries

- [x] 5.1 Implement full 32-bit bilateral collision filters and runtime filter revalidation; cover high bits, sleep/wake and changed contact acceptance.
- [x] 5.2 Implement Raycast/Sweep/Overlap with ignore filters, world-space output, stable closest-hit tie breaking and overlap deduplication.
- [x] 5.3 Implement synchronized native contact collection, subshape-to-object reduction and ordered contact/trigger events; test sleep, removal, compound shapes and filter changes.
- [x] 5.4 Implement Fixed/Hinge/Slider/Distance/Generic6DOF, limits, explicit drive modes and pair-specific collision exclusion; test invalid anchors and dependent teardown.
- [x] 5.5 Implement CharacterVirtual with one-time displacement consumption, gravity/up, slopes/stairs/platforms, character-character interaction, query proxy and safe capsule replacement.

## 6. World and editor integration

- [x] 6.1 Migrate PhysicsSystem/component/config to the adaptor with unchanged type/component/subsystem identity and interface-based world resolution; propagate existing module leases.
- [x] 6.2 Map validated reflected config to world/step settings, including capacity and worker settings; reject Exact and unsupported values with visible reasons.
- [x] 6.3 Implement bounded backend-owned job execution and a single-thread path; unify Tick/SingleStep completion, event/pose publication and fault handling.
- [x] 6.4 Implement Fast snapshot restore, interpolation, engine debug geometry and statistics; validate a snapshot before mutation and reset cached events/interpolation afterward.
- [x] 6.5 Complete shape collection reflection and round-trip tests; test legacy type identity and module definition ownership across runtime restart.
- [x] 6.6 Reuse the Aurora CPU mesh bridge in JoltPhysicsRenderModule; keep it independent of native SDK types and verify provider registration/unregistration.

## 7. Remove Bullet and update consumers

- [x] 7.1 Replace plugin selection, manifests, game-template module names and package smoke options with Jolt names; verify the physics-disabled path.
- [x] 7.2 Port useful BulletBackendTest coverage to Jolt tests, then delete all plugins/bullet sources, headers, build files, module entries and tests.
- [x] 7.3 Remove active bullet3 dependency/importer and update PhysicsMesh comments, helper examples, current module/build documentation and repository build references.
- [x] 7.4 Reconcile current softbody/network planning statements only where the selected backend or unshipped Exact claim changed; preserve historical comparisons/archives and unrelated local edits.
- [x] 7.5 Verify resolved paths, then remove only identified generated Bullet module artifacts in the workspace output; regenerate module lists and audit deployment inputs.

## 8. Validation and review

- [x] 8.1 Close SandboxEditor, run third-party version preflight, reconfigure cmake-build-debug preserving 3RD_PATH/generator, then build Physics/PhysicsTest/JoltPhysicsModule/JoltPhysicsRenderModule/JoltPhysicsTest and affected consumers.
- [x] 8.2 Run PhysicsTest, JoltPhysicsTest and relevant terrain/navigation/Framework/editor contract tests, including the negative/native-failure coverage recorded in verification.md.
- [x] 8.3 Build SandboxModule and SandboxEditor, run a bounded startup/shutdown smoke, and verify the runtime module list contains only Jolt physics modules.
- [x] 8.4 Verify a generated game-template/package configuration and physics-disabled build; verify Debug/Release ABI separately from platform support not exercised locally.
- [x] 8.5 Audit changed source for exceptions, RTTI downcasts, non-ASCII C++, invalid includes, duplicate reflection owners and reversed target dependencies; scan active source/build/package paths for Bullet.
- [x] 8.6 Record exact commands/results and any unresolved platform checks in verification.md; run strict OpenSpec validation and review the scoped diff. Commit and archive remain separate user-confirmed actions.

## 9. Strict integration review closure

The initial strict review blocked acceptance despite the earlier backend checks. The user-requested root-cause repairs and their current verification are recorded in review-2026-10-27.md and verification.md; all findings below are closed in the documented Windows scope.

- [x] 9.1 R1: Decode the actual Aurora vertex layout and U16/U32 indices in the mesh provider; cover real cooked products through the render bridge.
- [x] 9.2 R2: Preserve configuration lifetime across edits/rebuilds and make Undo/Redo resolve valid document objects.
- [x] 9.3 R3: Propagate subsystem validation/creation failures through World construction, document loading and Play without assertions or partial-success publication.
- [x] 9.4 R4: Route UUID-based component construction and deserialization through the existing type-safe component registration.
- [x] 9.5 R5: Make body edits transactional and preserve the promised body identity, velocity and constraints.
- [x] 9.6 R6: Implement static/kinematic scene input and dynamic physics output with explicit update ordering and feedback prevention.
- [x] 9.7 R7: Collect collision-only and compound mesh asset references and verify missing products fail package auditing.
- [x] 9.8 R8: Align World initialization and editor configuration application semantics; verify modified/disabled settings take effect at the declared application point.
- [x] 9.9 R9: Declare component subsystem requirements and apply component/configuration changes through one undoable editor operation.
- [x] 9.10 R10: Connect the existing actor creation registry to editor actions and common creation commands, removing host enum/switch coupling.
- [x] 9.11 R11: Define physics event ownership and consumption so ordinary host execution does not retain an unbounded event history.
- [x] 9.12 Re-review the corrected integration and run the meaningful regression/acceptance cases recorded in the review before declaring the change complete.

Validation scope: execution of the original Windows Debug/Release checks is recorded in verification.md. Section 9 is complete with current Debug integration tests and Debug/Release Jolt verification. Global dependency preflight and cross-platform packaging limitations are documented in verification.md; other platforms/static shipping packaging remain unverified. Commit/archive require separate explicit user confirmation and have not been performed.
