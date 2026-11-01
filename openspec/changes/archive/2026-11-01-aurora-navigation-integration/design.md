## Context

Jolt separates world integration, renderer conversion and offline builders. Recast currently compiles everything into a module, while engine/navigation contains the concrete world service, source schema, codecs, cooking and terrain integration. Existing plain debug triangles have no consumer. The factory's editor event has no consumer either.

Production source must have one compilation owner. Shared registries use Singleton, reflection has one metadata owner, and surviving objects/callbacks retain a code lease.

## Goals / Non-Goals

**Goals:** Separate runtime, offline build, rendering and editor authoring; keep consumers on engine contracts; make editor operations work; fix and test the exercised query/build/streaming paths.

**Non-Goals:** Crowd steering, obstacle or off-mesh-link authoring, a gameplay controller, unrelated directory changes, Aurora target reorganization, another game-world host, or the separate terrain integration test matrix.

## Decisions

### Consumer contracts and implementation owners

INavigationSystem exposes world navigation. Consumers resolve the named subsystem and call the interface. The engine factory remains a dispatch registry but loses editor inheritance/binding. Plugin source schema/codecs keep their domain include paths. The terrain bridge moves to the provider plugin.

Linkable static targets own backend, shared asset/base and world-service sources. Runtime module entry owns metadata and session registration. Offline cooking links shared code and engine contracts, with a declared runtime backend dependency in its host; it does not link editor/render targets. Reusing these owners avoids recompiling production source in tests.

### Deferred editor operations

EditorCore publishes the active world through a small host contract. EditorActionRegistry supplies build and visibility actions. Invocation only queues work; module Tick starts/observes generation and checks its explicit result. Actions, callbacks and work retain leases and unregister symmetrically. Reflected settings continue to use the common form panel.

Before generation, the editor polls each component's explicit world-preparation state once per frame. A pending collision asset or native shape keeps the build request queued; a failed attachment reports the actor, component and reason. This path does not block on the loader pool and does not advance world simulation to force resource readiness. Closing or replacing the active world discards its queued request.

### Offline geometry readiness

A real `.world`/`.collision`/`.navmesh` cook exposed an initialization gap: accepting an actor attachment can leave its collision assets and native shape preparing. `World::Build` and the empty `World::Init` therefore do not establish that geometry is ready to collect. Gathering immediately could publish an empty successful navigation product even when the world contains walkable collision geometry.

Components expose `PollWorldPreparation` for deferred attachment, with Ready, Preparing and Failed outcomes. Physics bodies use that hook to finish asset preparation and native binding; a binding failure remains a failure on subsequent polls. The builder collects geometry only after every attached component reports Ready, and propagates actor/component diagnostics on failure.

Offline requests execute on the cook pool while asset loading and native shape preparation execute on the loader pool. `AssetExecutor::WaitForLoads` drains only loader work and is safe for the cook worker; `WaitForAll` would wait for the request's own cook pool. The builder advances at most 64 preparation stages, draining loader work between pending stages, and fails explicitly if readiness does not converge. It neither sleeps to guess readiness nor ticks world simulation. Physics inputs remain ProductsOnly, so unavailable collision products fail instead of becoming silently omitted geometry.

### Complete collision mesh data

The real-scene check also found collision decoding populated vertex/index bytes without a `TriangleMeshView`. Navigation's spatial collector traverses views, so the physical shape could work while contributing no navigation triangles. The collision codec now creates a whole-mesh view with exact vertex/index ranges and an AABB derived from the decoded vertices. Empty vertices and invalid triangle indices are rejected. Views remain derived runtime data; the collision source format is unchanged.

### Compression service ownership

Recast tile payloads and Framework product codecs acquire ICompressor from Framework's CompressionManager. Framework owns the stateless built-in LZ4 implementation and conditionally links its static third-party library; consumers do not load a codec module. SKY_BUILD_COMPRESSION remains an ordinary foundation CMake option, not a plugin option. The obsolete CompressionModule target, plugin entry and host registrations are removed.

Application and standalone module-test hosts establish the Environment singleton before loading modules, keeping default-provider virtual code in the host static image. Register dispatches virtually through that host-created manager so replacement-provider control blocks are also host-owned. Acquired shared handles retain provider objects and optional implementation-code leases; provider destruction precedes lease release. Registry overrides configure quiescently, duplicate registration returns failure, unregister restores the built-in, and an explicit enabled gate supports unavailable-service policy/tests. No codec call holds the registry mutex.

LZ4 checks input/output integer bounds, flags, short buffers and zero compression result explicitly. Disabled/unavailable compression or codec failure prevents publishing success; it never silently changes the format. This preserves the current product/tile encoding and does not implement the independent block-format/async-IO proposal.

### Rendering bridge

A reusable rendering static target consumes NaviDebugGeometry and emits owned Aurora mesh resources/entities into an existing frame packet. It has no editor dependency. The editor module composes it through a generic scene-overlay registry in the editor's existing frame producer. There is one packet submitter, with no scene-source replacement or new Aurora target.

The bridge sets the resolved material parameter `gMaterial.tint`; the source alias `tint` is accepted only as a compatibility input. Its translucent debug surface uses the existing scene-pass sort tags to draw after ordinary geometry, retains depth testing and disables depth writes. This keeps debug material binding and draw ordering inside the rendering bridge without changing Aurora's module ownership. Packet assertions and native startup are distinct from visual confirmation of the final scene image.

### Backend correctness and streaming

Filters expose renderer/backend-independent values instead of unchecked downcasts. Complete success requires the destination polygon with no truncation/exhausted search. Straight-path endpoints use projected positions. Each QueryPath owns a separate Detour query context/node pool/open list; query/debug/serialization readers share a mesh read lock, while reset/build/load/tile mutations take exclusive protection. This enables query/query concurrency without concurrently reading changing topology.

Asynchronous submissions copy flags and all representable area costs on the world thread. Their terminal result and cancellation share one atomic state: cancellation winning before publication yields an empty cancelled result; cancellation after terminal publication cannot rewrite success. Cancelled-but-running jobs keep admission slots until actual completion. Service/configuration operations belong to the world thread; task completion/cancellation may be polled from other threads. Direct synchronous callers must serialize edits of their borrowed filter and exclusively own output.

The reflected world config can bind a cooked NaviMesh asset for eager loading or streaming. Initialization verifies loading and installation; an empty reference preserves programmatic generation. Cooking clears this asset reference so a build does not consume its previous product.

All tile coordinates are relative to bounds.min. Incremental builds replace only requested coordinates. Failed setup, incomplete provider coverage, tile installation or export is explicit and prevents asset publication.

Streaming owns all layers grouped by coordinate and applies a per-frame coordinate budget. The old prefetch only copied already resident bytes, with no IO; removing that task eliminates a dangling-pointer and cancellation race without losing asynchronous IO. Real path queries remain asynchronous and retain their inputs until completion. The shared Task callback releases its captured task after completion to break the Task/handle/callback reference cycle while dependency handles remain valid.

Fresh desired coordinates are scheduled before retries. Failed coordinates back off by 1/2/4/8/16/32/64 ticks; a failed rollback/removal is retried before installing more layers, even if the coordinate leaves the focus. Pending reports desired uninstalled coordinates, including backoff, rather than pretending to measure disk requests. These are coordinate budgets, not time/byte budgets.

The existing asset format is retained. Save/load reject unknown mode/version, more than 65,536 tile records, duplicate coordinate/layer keys and more than 256 MiB aggregate payload. Decode validates remaining encoded bytes, including following records, before resizing. Invalid input fails the archive and clears output; no catch/throw or runtime product v2 migration is introduced.

TerrainNavigationBridge unregisters its geometry provider before the rebuild coordinator releases its owned WorldPtr, so a bridge that owns the last world reference cannot access a destroyed subsystem during Shutdown.

### Checkpoint boundaries and follow-up design

This checkpoint does not implement range-based disk IO, a general IO pool, immutable mesh generations, transactional incremental replacement, query priority/fair writer scheduling, demand-driven focus binding or a GPU/controller feature. Streaming still eagerly loads compressed payloads and installs/unloads native tiles on Tick. Shared-mutex writers may wait for readers, and each query initializes a Detour context; no lock-free or speedup claim is made. Incremental build failure can leave already applied coordinates changed; error propagation prevents successful asset publication but does not roll back the whole live mesh.

The independent common-async-IO design is kept outside this checkpoint commit. It plans range IO, block products, whole-unit budgets and owner publication; its detailed future candidate/TryPublish strategy is not delivered here. Current parallel readers provide a safe intermediate step and do not eliminate the need for versioning if streaming and old queries must run without a shared mesh guard.

## Risks / Trade-offs

- [Additional targets] -> Give each source one owner and build both production modules and tests.
- [Debug geometry cost] -> Opt-in rendering and revision-aware updates; no GPU objects in core types.
- [Cooperative cancellation] -> Retain tasks/mesh/filter/code until actual completion and suppress canceled output.
- [Shared read guard] -> Readers use independent Detour contexts; native writers still wait and fairness/context-allocation performance need measurement.
- [Asynchronous attachments omitted from offline geometry] -> Require explicit component readiness and fail with context; use loader-only draining in cook and nonblocking polling in the editor.
- [Compression disabled/unavailable] -> Query Framework capability and preserve explicit failures; no runtime module or uncompressed fallback is needed.
- [Dependency cache differs from pins] -> Record preflight and exact native evidence; do not claim pinned-version validation.

## Migration Plan

Preserve domain includes, update manifests/cook dependencies with implementation changes, build real targets and run source ownership/backend/runtime/editor checks. Keep the change active for review; commit and archive require explicit confirmation under AGENTS.md.
