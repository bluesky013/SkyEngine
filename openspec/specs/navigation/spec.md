# navigation Specification

## Purpose

Define navigation geometry, baking, persistence, queries, streaming and optional renderer/editor integration.

## Scope notes

### navigation-aurora-integration

Define the ownership and lifecycle boundaries between renderer-independent navigation, plugin runtime and cooking, editor authoring, and optional Aurora debug visualization.

## Requirements

### Requirement: Budgeted resident-manifest paging

Streaming SHALL apply runtime-owned resident tile payloads on the world tick within a coordinate budget. It SHALL NOT start an asynchronous task merely to copy already resident bytes. Real asynchronous path queries SHALL retain their separate completion and cancellation contracts.

#### Scenario: Resident coordinate enters range

- **WHEN** a manifest coordinate enters the load radius
- **THEN** paging SHALL apply its layers on the main thread within the configured coordinate budget

#### Scenario: Pending coordinate leaves range

- **WHEN** an unapplied coordinate leaves the load radius
- **THEN** paging SHALL skip it without scheduling stale work

### Requirement: Path result type

The navigation API SHALL provide a path result carrying world-space points and their straight-path flags.

#### Scenario: Query returns points

- **WHEN** a path query succeeds
- **THEN** the result SHALL contain ordered world-space points and matching flags

#### Scenario: Failed query returns no points

- **WHEN** a path query fails
- **THEN** the result SHALL be invalid with no points

### Requirement: Query filter factory

The navigation API SHALL provide a way to create a `NaviQueryFilter` without exposing backend types.

#### Scenario: Caller obtains a filter

- **WHEN** a caller requests a query filter from the factory
- **THEN** it SHALL receive a usable filter, or null when no backend is loaded

### Requirement: Synchronous query variant

A synchronous query SHALL remain available for tools and tests.

#### Scenario: Sync query fills a path

- **WHEN** the synchronous query is called with a valid filter
- **THEN** it SHALL fill the provided path result

### Requirement: Asynchronous path query

Path queries SHALL be submittable off the world tick, with a polled result. Submission and service configuration SHALL occur on the world thread, while polling and cancellation SHALL be safe across threads. Submission SHALL freeze filter values before returning. Concurrent backend readers SHALL use independent Detour search contexts with shared mesh read protection; native topology mutation SHALL exclude all readers.

#### Scenario: Polled completion

- **WHEN** an async path request is submitted
- **THEN** it SHALL report completion when finished and expose the resulting path

#### Scenario: Safe concurrent access

- **WHEN** multiple async queries run
- **THEN** their mutable search contexts SHALL be independent and read-only mesh access SHALL be allowed to overlap

#### Scenario: Query budget

- **WHEN** the number of physically unfinished tracked queries reaches the budget
- **THEN** further requests SHALL be refused until earlier ones actually finish, including cancellation-requested queries

#### Scenario: Caller edits its filter after submission

- **WHEN** a caller changes filter flags or costs after RequestPath returns
- **THEN** the queued request SHALL use its submission-time values without reading the caller's later changes

### Requirement: Cancellation and invalidation

Async queries SHALL be cancellable, and a nav mesh change SHALL invalidate in-flight queries. Cancellation and terminal publication SHALL have one atomic ordering. A cancellation request SHALL NOT imply backend work has ended or release its admission slot. A terminal result SHALL remain immutable.

#### Scenario: Cancel a query

- **WHEN** an in-flight query is cancelled before terminal publication
- **THEN** it SHALL stop being applied, finish with invalid empty output and keep its slot until actual completion

#### Scenario: Mesh change invalidates queries

- **WHEN** the nav mesh changes
- **THEN** in-flight queries SHALL be cancelled

#### Scenario: Cancellation after completion

- **WHEN** Cancel is called after a successful terminal result is published
- **THEN** success and its path SHALL remain unchanged

### Requirement: Independent navigation responsibilities

Runtime SHALL expose engine contracts without editor/renderer dependencies. Plugin implementation, editor authoring, offline cooking and rendering SHALL have independent source owners. Tests SHALL link production implementation targets.

#### Scenario: Headless navigation

- **WHEN** the backend loads without editor/render modules
- **THEN** worlds SHALL create navigation, generate/load meshes and query paths without renderer calls or editor actions

#### Scenario: Consumer boundary

- **WHEN** terrain consumes navigation
- **THEN** it SHALL use an engine navigation interface instead of the Recast world implementation

### Requirement: Deferred editor build

Build Navigation SHALL register through the common action registry, use the active editor world, enqueue work and report explicit failure. Duplicate in-flight builds SHALL be suppressed. Runtime factory construction SHALL NOT register editor operations.

#### Scenario: Build action

- **WHEN** the action is invoked for a navigation world
- **THEN** editor Tick SHALL initiate generation and observe completion outside event dispatch

#### Scenario: Collision resources are pending

- **WHEN** a queued editor build encounters components whose world preparation is still pending
- **THEN** the editor SHALL poll their explicit state on later frames and defer geometry gathering without blocking the UI or advancing simulation to force readiness

#### Scenario: Component attachment fails

- **WHEN** a component reports a world-preparation or native binding failure
- **THEN** the editor SHALL stop that queued build and report the actor, component and cause instead of generating from incomplete geometry

#### Scenario: Editor shutdown

- **WHEN** the editor module shuts down
- **THEN** its actions SHALL be removed and surviving work SHALL retain its required lifetime

### Requirement: Optional Aurora debug consumer

The render bridge SHALL consume plain navigation triangles and submit owned Aurora mesh resources/entities through the existing frame producer. An editor action SHALL control visibility. Core types SHALL NOT hold GPU/editor objects.

#### Scenario: Debug enabled

- **WHEN** debug visibility is enabled on a built mesh
- **THEN** walkable triangles SHALL be submitted into the active editor scene

#### Scenario: Debug retired

- **WHEN** visibility is disabled, the world changes or the bridge shuts down
- **THEN** stale debug entities/resources SHALL be retired

### Requirement: Canonical debug material binding and ordering

The navigation rendering bridge SHALL bind its tint to the resolved material parameter `gMaterial.tint` and submit its translucent debug surface after ordinary scene geometry using the existing scene-pass ordering contract. It SHALL retain depth testing and SHALL NOT write scene depth.

#### Scenario: Material parameters use resolved names

- **WHEN** the source scene material exposes `gMaterial.tint` rather than the authoring alias `tint`
- **THEN** the bridge SHALL apply its navigation debug tint to that canonical parameter

#### Scenario: Debug geometry overlaps ground

- **WHEN** navigation debug triangles overlap ordinary scene ground
- **THEN** their debug surface SHALL be ordered after ordinary geometry and alpha-blended with depth testing, without replacing or writing the ground's depth

### Requirement: Geometry provider seam

The navigation core SHALL define a geometry-provider interface that supplies world-space triangles for a requested world-space bounds through a geometry sink, without referencing terrain, render, or physics types.

#### Scenario: Provider contributes geometry

- **WHEN** a registered geometry provider is asked to collect geometry for a bounds
- **THEN** it SHALL emit world-space triangles to the geometry sink

#### Scenario: No terrain or physics types in the core

- **WHEN** the navigation core module is compiled
- **THEN** its provider interface SHALL NOT include terrain, render, or physics headers

### Requirement: Backend consumes providers

The nav mesh backend SHALL gather geometry from all registered providers in addition to its existing collision-component source.

#### Scenario: Providers feed the mesh build

- **WHEN** a nav mesh build runs with registered providers
- **THEN** the backend SHALL include the providers' triangles in the rasterized geometry

#### Scenario: No providers registered

- **WHEN** a nav mesh build runs with no registered providers
- **THEN** the backend SHALL behave as before, using only the collision-component geometry

### Requirement: Provider registration ownership

The navigation system SHALL hold an opaque list of geometry providers registered by external modules, so that neither the navigation core nor the nav mesh backend depends on the provider implementations.

#### Scenario: External module registers a provider

- **WHEN** an external module registers a geometry provider with the navigation system
- **THEN** the backend SHALL consume it without linking the provider's module

### Requirement: Collision geometry source preserved

The existing collision-component triangle-mesh source SHALL remain available and unchanged alongside the provider seam.

#### Scenario: Collision components still contribute

- **WHEN** a world contains actors with triangle-mesh collision components
- **THEN** their geometry SHALL still be gathered into the nav mesh build

### Requirement: Backend-agnostic nav mesh asset payload

`engine/navigation` SHALL define a nav mesh asset payload (`NaviMeshData`) with an export mode, build params, and
either per-tile blobs (Tiled) or a single blob (Full), containing no backend-specific types.

#### Scenario: Payload has no backend types

- **WHEN** a consumer includes the nav mesh asset header
- **THEN** it SHALL compile without any backend (recast) header

#### Scenario: Export mode is recorded

- **WHEN** a `NaviMeshData` is produced
- **THEN** it SHALL carry its `NaviMeshExportMode` and build params

### Requirement: Asset serialization

`NaviMeshData` SHALL serialize to and deserialize from a binary archive, preserving build params, per-tile
blobs (`tx`, `ty`, `layer`, bytes), and the full blob.

#### Scenario: Tiled round-trip

- **WHEN** a Tiled `NaviMeshData` with per-tile blobs is saved and loaded
- **THEN** the export mode, build params, and every tile blob SHALL be preserved

#### Scenario: Full round-trip

- **WHEN** a Full `NaviMeshData` with a single blob is saved and loaded
- **THEN** the export mode and the full blob SHALL be preserved

### Requirement: Build params consistency

The persisted build params SHALL be sufficient for the runtime to reconstruct identical Detour nav mesh and tile
cache parameters: agent config, resolution, bounds, `maxSimplificationError`, `borderSize`, and a version.

#### Scenario: Runtime rebuilds parameters from the asset

- **WHEN** the runtime loads a nav mesh asset
- **THEN** it SHALL derive `dtNavMeshParams` and `dtTileCacheParams` from the persisted build params

### Requirement: Interface-level tile emission

`NaviMeshGenerator` SHALL expose the build params and a tile-emission entry so tooling can collect a nav mesh asset
without touching backend types.

#### Scenario: Backend emits tiles behind the interface

- **WHEN** `CollectTiles` is called on a completed generation
- **THEN** it SHALL return per-tile payloads keyed by `(tx, ty, layer)` through the interface type

### Requirement: Runtime tiled load

The runtime SHALL restore a Tiled nav mesh from a `NaviMeshData`: rebuild the nav mesh and tile cache from the
build params, add the tile blobs, build the nav mesh tiles, and rebuild the query.

#### Scenario: Tiled asset restores a queryable mesh

- **WHEN** a Tiled `NaviMeshData` is loaded
- **THEN** the nav mesh and query SHALL be available for queries

#### Scenario: Full load is rejected until supported

- **WHEN** a Full `NaviMeshData` is loaded before Full support exists
- **THEN** `LoadData` SHALL report failure without crashing

### Requirement: Navigation codec validates allocation bounds

The existing navigation format SHALL validate mode/version, tile count, distinct coordinate/layer keys, declared payload sizes and remaining encoded bytes before allocating corresponding containers. The codec SHALL support at most 65,536 tile records and 256 MiB aggregate tile/full payload. Invalid decode SHALL invalidate the archive and clear output; invalid save SHALL fail explicitly rather than narrow oversized counts or payload sizes.

#### Scenario: Tiny payload declares huge tile count

- **WHEN** a small otherwise framed payload declares UINT32_MAX tile records
- **THEN** decoding SHALL fail before allocating the declared tile container

#### Scenario: Tile payload consumes following records

- **WHEN** a declared tile payload exceeds remaining bytes after reserving the following tile headers and final full-data size field
- **THEN** decoding SHALL fail before resizing that payload

#### Scenario: Unsupported schema or duplicate layer

- **WHEN** mode/version is unsupported or a coordinate/layer key repeats
- **THEN** decoding SHALL fail explicitly without leaving partially loaded output

### Requirement: Framework-level nav mesh builder

The nav mesh builder SHALL be a `sky::AssetBuilder` registered by a navigation-owned plugin builder module. Its implementation SHALL link `Framework`, `Navigation` and the plugin's shared source/asset owner through `NavigationBuilder.Static`. It SHALL NOT link runtime, editor, render, Aurora or concrete Recast backend implementation targets. The cook host SHALL load the backend separately and generation SHALL use navigation interfaces. Source reflection SHALL have one backend metadata owner, separate from builder registration.

#### Scenario: Builder links framework, navigation and shared schema

- **WHEN** the navigation builder module is built
- **THEN** `SkyNavigation.Builder` SHALL link `NavigationBuilder.Static`, whose dependencies SHALL be `Framework`, `Navigation` and `RecastNavigation.Shared.Static`, without linking `RecastNavigation.Runtime.Static`, `RecastNavigation.Static`, Aurora or the recast third-party

#### Scenario: Builder registers with the framework

- **WHEN** the backend and builder module definitions are created
- **THEN** the backend metadata owner SHALL reflect the `.navmesh` source once, and the builder definition SHALL register its `sky::AssetBuilder` with `AssetBuilderManager` without repeating source reflection

### Requirement: `.navmesh` source asset

A `.navmesh` source SHALL describe the scene to cook, the agent/resolution/bounds build params, and the export
mode.

#### Scenario: Source carries scene and params

- **WHEN** a `.navmesh` source is loaded
- **THEN** it SHALL provide a scene reference, agent/resolution/bounds params, and an export mode

### Requirement: Offline scene cook

The builder SHALL load the referenced scene into a `World`, attach a `NavigationSystem`, apply the authored params,
generate through the navigation interface, and write the nav mesh asset.

#### Scenario: Cook produces a nav mesh asset

- **WHEN** the builder processes a valid `.navmesh` source
- **THEN** it SHALL generate tiles through `NaviMeshFactory`/`NaviMeshGenerator` and save a nav mesh asset

#### Scenario: Missing scene does not crash

- **WHEN** the referenced scene cannot be loaded
- **THEN** the build SHALL report failure without crashing

### Requirement: Export modes

The builder SHALL support a `Tiled` export (per-tile blobs) and a `Full` export (single monolithic blob), selected
from the source.

#### Scenario: Tiled export emits per-tile payloads

- **WHEN** the export mode is `Tiled`
- **THEN** the asset SHALL contain per-tile blobs keyed by `(tx, ty, layer)`

#### Scenario: Full export emits one blob

- **WHEN** the export mode is `Full`
- **THEN** the asset SHALL contain a single serialized nav mesh blob

### Requirement: Full mesh serialization and load

The backend SHALL serialize a built `dtNavMesh` to a versioned blob and restore it, and the runtime `LoadData`
SHALL load a `Full` asset by deserialization.

#### Scenario: Full round-trip loads and queries

- **WHEN** a `Full` asset is loaded
- **THEN** the nav mesh and query SHALL be restored from the blob

#### Scenario: Invalid blob is rejected

- **WHEN** the blob version is unknown or its sizes are inconsistent
- **THEN** loading SHALL fail without crashing

### Requirement: Incremental rebuild

The generator SHALL support restricting a build to a subset of tiles, and the nav mesh SHALL support removing a
single tile from both the nav mesh and its tile cache.

#### Scenario: Subset rebuild

- **WHEN** a set of tile coordinates is provided to the generator
- **THEN** only those tiles SHALL be built

#### Scenario: Tile removal

- **WHEN** a tile is removed
- **THEN** it SHALL be removed from the nav mesh and the tile cache

### Requirement: Plugin-owned offline cook and failure reporting

Source schema and cook implementation SHALL belong to the navigation plugin. Cooking SHALL consume engine contracts/shared asset code without linking editor/render targets. Its host SHALL load the backend dependency. Failed generation SHALL return an explicit error and SHALL NOT publish an asset.

#### Scenario: Generation failure

- **WHEN** generation fails after work completes
- **THEN** cooking SHALL report its diagnostic and SHALL NOT save a success product

### Requirement: Explicit offline world preparation

Before collecting source geometry, the offline builder SHALL require every attached component to report completed world preparation. Deferred asset or native-shape attachment SHALL have explicit Ready, Preparing and Failed states. Preparation and binding failures SHALL identify the actor, component and cause, and SHALL prevent navigation product publication.

#### Scenario: Collision resources prepare asynchronously

- **WHEN** the source world's actor attachment is accepted while collision resources are still preparing
- **THEN** the builder SHALL finish preparation before gathering geometry and SHALL NOT publish an empty success product merely because those resources were pending

#### Scenario: Collision product or native binding fails

- **WHEN** a required ProductsOnly collision input is unavailable or native body binding fails
- **THEN** cooking SHALL fail with the affected actor/component diagnostic and SHALL NOT treat the omitted geometry as ready

### Requirement: Cook preparation avoids worker self-wait

The builder SHALL advance preparation through a bounded number of explicit stages. A cook worker MAY drain the independent loader pool, but SHALL NOT wait for its own cook pool. Readiness SHALL NOT depend on guessed sleeps or advancing world simulation.

#### Scenario: Prepare from the cook worker

- **WHEN** a cook request needs loader or native shape-preparation work
- **THEN** it SHALL wait only for loader work and continue polling preparation without waiting on the request itself

#### Scenario: Preparation does not converge

- **WHEN** the source world remains Preparing after the configured stage bound
- **THEN** cooking SHALL return an explicit preparation failure instead of hanging or publishing incomplete geometry

### Requirement: Real scene geometry survives the cook boundary

Valid walkable collision triangles in a source world SHALL contribute to the persisted navigation product after resource preparation and decoding.

#### Scenario: Cook a world with walkable ground and an obstacle

- **WHEN** valid collision products provide walkable ground and a blocking obstacle to a `.navmesh` build
- **THEN** the cooked mesh SHALL contain navigation geometry and a loaded runtime path SHALL respect that obstacle

### Requirement: Tile grid dimensions

The generator SHALL compute the tile-grid height from the grid height (`gh`) returned by `rcCalcGridSize`, not
from the grid width.

#### Scenario: Non-square bounds produce correct tile counts

- **WHEN** the build bounds produce different grid width and height
- **THEN** the number of tiles along the second axis SHALL be derived from `gh`

### Requirement: Navmesh and tile-cache tile capacity agree

The navmesh `maxTiles` SHALL be at least the tile-cache tile capacity, so every accepted tile can be added to the
navmesh.

#### Scenario: All built tiles fit the navmesh

- **WHEN** the tile cache accepts a tile
- **THEN** the navmesh SHALL have capacity for it

### Requirement: Tile data ownership

Tile data handed to the tile cache with `DT_COMPRESSEDTILE_FREE_DATA` SHALL have a single owner: on success the
tile cache owns it and the engine SHALL NOT free it; on failure the engine SHALL free it exactly once.

#### Scenario: Successful add does not double free

- **WHEN** `dtTileCache::addTile` succeeds with `DT_COMPRESSEDTILE_FREE_DATA`
- **THEN** the engine SHALL detach the buffer so its destructor SHALL NOT free it

#### Scenario: Failed add frees once

- **WHEN** `dtTileCache::addTile` fails
- **THEN** the engine SHALL free the buffer exactly once and SHALL NOT free it again on destruction

### Requirement: Tile cache lifetime

The generator SHALL free its `dtTileCache` on destruction and on initialization failure.

#### Scenario: Generator destruction frees the tile cache

- **WHEN** the generator is destroyed
- **THEN** `dtFreeTileCache` SHALL be called on the owned tile cache

### Requirement: Allocation and null safety

The backend SHALL check allocations before use and clean up on failure: `dtAllocNavMesh` and
`dtAllocNavMeshQuery`.

#### Scenario: Navmesh allocation failure

- **WHEN** `dtAllocNavMesh` returns null or nav mesh init fails
- **THEN** the build SHALL fail gracefully without dereferencing null and without leaking the nav mesh

#### Scenario: Query allocation failure

- **WHEN** `dtAllocNavMeshQuery` returns null or query init fails
- **THEN** no null dereference SHALL occur and the query SHALL be freed

### Requirement: Render-agnostic core

The navigation core SHALL NOT depend on any renderer. `NaviMesh` / `RecastNaviMesh` SHALL NOT include render headers or link a render module, and debug output SHALL be exposed as render-agnostic geometry through `NaviMesh::BuildDebugGeometry(NaviDebugGeometry &)`, which any renderer MAY consume later. The concrete backend MAY consume the plugin's independent shared asset/config owner without depending on runtime, editor or builder implementation targets.

#### Scenario: No render dependency in the build/query core

- **WHEN** the recast backend implementation target is built
- **THEN** it SHALL link `Navigation`, `Physics`, `RecastNavigation.Shared.Static` and the recast third-party, with `Framework` available through the shared owner and no runtime, editor, builder or render implementation dependency

#### Scenario: Debug geometry is render-agnostic data

- **WHEN** `BuildDebugGeometry` is called on a built nav mesh
- **THEN** it SHALL return a triangle list of positions and colors without referencing any renderer

#### Scenario: Build does not require a renderer

- **WHEN** the nav mesh build runs with no render subsystem attached
- **THEN** it SHALL complete without any render calls

### Requirement: Path query correctness

`FindPath` SHALL test the end polygon result, SHALL require a valid query filter and nav query, and SHALL treat
zero or failed results as failure.

#### Scenario: End polygon not found

- **WHEN** the end position has no nearby polygon
- **THEN** `FindPath` SHALL return `FAILED`

#### Scenario: Null filter or query

- **WHEN** the filter is null or the nav query is not initialized
- **THEN** `FindPath` SHALL return `FAILED` without dereferencing null

### Requirement: Complete path semantics

Success SHALL reach the destination polygon without truncation or exhausted search. Filters SHALL use backend-independent values without unchecked downcasts. Straight-path endpoints SHALL be projected onto the mesh; failure SHALL clear output.

#### Scenario: Disconnected destination

- **WHEN** start and destination polygons exist but are disconnected
- **THEN** the query SHALL fail instead of returning a partial corridor as success

#### Scenario: Projected endpoints

- **WHEN** input positions can be projected from above the mesh
- **THEN** output endpoints SHALL lie on the navigable surface

### Requirement: Consistent tile builds and explicit failures

Build, persistence, streaming and rebuild SHALL use bounds.min as tile origin. Incremental builds SHALL preserve other coordinates. Failed setup, incomplete provider coverage, tile installation or export SHALL fail explicitly and SHALL NOT produce a success asset.

#### Scenario: Nonzero bounds

- **WHEN** bounds have a negative/nonzero minimum
- **THEN** generated coordinates SHALL address the same regions as streaming/rebuild

#### Scenario: Subset rebuild

- **WHEN** one coordinate is rebuilt
- **THEN** all other tiles SHALL remain queryable

#### Scenario: Corrupt payload

- **WHEN** a tile cannot be decoded/installed
- **THEN** loading SHALL fail and allocated bytes SHALL have exactly one owner

### Requirement: Shared backend synchronization

Queries, debug snapshots and serialization SHALL use shared read protection, while mutation and reset/load SHALL hold exclusive protection. Each concurrent path search SHALL own its own Detour query context and mutable search state. A synchronous caller SHALL own its output and keep its borrowed filter unchanged during the call. The implementation SHALL NOT claim lock-free query or writer fairness.

#### Scenario: Concurrent readers overlap

- **WHEN** one real backend query is held inside its read section
- **THEN** another independent query SHALL be able to finish before the first reader releases its read section

#### Scenario: Stream while querying

- **WHEN** tiles change during an asynchronous query
- **THEN** backend objects SHALL NOT be read and written concurrently

### Requirement: Collision mesh views describe supplied geometry

Decoded collision triangle meshes supplied to navigation SHALL expose complete view ranges and bounds in addition to vertex/index bytes. A collision source containing one vertex/index mesh SHALL produce a whole-mesh view whose AABB is derived from its decoded vertices. Invalid or empty geometry SHALL be rejected before collection.

#### Scenario: Collision source participates in navigation

- **WHEN** a valid triangle collision product is decoded for a navigation build
- **THEN** its view SHALL cover all decoded vertices and triangle indices with correct first offsets and bounds, so the navigation collector does not silently omit the mesh

#### Scenario: Collision source round trip

- **WHEN** a triangle collision source is serialized and decoded again
- **THEN** the restored mesh SHALL retain a complete derived view and matching bounds

### Requirement: Explicit tile compression dependency

Recast tile operations SHALL acquire ICompressor through Framework's CompressionManager. When built-in support is enabled, Framework SHALL provide default LZ4 without runtime module loading. The navigation backend SHALL remain independent of the private implementation and SHALL retain its acquired handle for each operation. Disabled/unavailable service or codec failure SHALL return failure rather than silently change the tile format or publish success.

#### Scenario: Host provides compression

- **WHEN** Framework has built-in LZ4 enabled and a navigation tile operation starts
- **THEN** the operation SHALL obtain its codec through Framework without registering or loading CompressionModule

#### Scenario: LZ4 service is unavailable

- **WHEN** a tile operation needs LZ4 but no compressor is registered
- **THEN** the navigation build or load SHALL fail explicitly and SHALL NOT publish a success product

### Requirement: Recast backend is listed in runtime module configs

`RecastNavigation` SHALL be listed in the game and editor runtime module configurations so `ModuleManager` loads
it, mirroring `BulletPhysicsModule`. The build switch remains `SKY_BUILD_RECAST`.

#### Scenario: Game runtime loads the recast backend

- **WHEN** the game application starts with `RecastNavigation` listed in `configs/modules_game.json`
- **THEN** the module SHALL be loaded and `NaviMeshFactory` SHALL have a registered implementation

#### Scenario: Editor runtime loads the recast backend

- **WHEN** the editor starts with `RecastNavigation` listed in `configs/modules_editor.json`
- **THEN** the module SHALL be loaded and `NaviMeshFactory` SHALL have a registered implementation

#### Scenario: Backend disabled

- **WHEN** `SKY_BUILD_RECAST` is OFF
- **THEN** `RecastNavigation` SHALL NOT be built and navigation SHALL remain in its null-backend state

### Requirement: Factory register/unregister lifecycle

`NaviMeshFactory` SHALL expose `UnRegister()` that clears the registered implementation, and the recast module
SHALL register on `Start()` and unregister on `Shutdown()` so the implementation never outlives its module.

#### Scenario: Module shutdown clears the factory

- **WHEN** `RecastModule::Shutdown` runs
- **THEN** `NaviMeshFactory::CreateNaviMesh` SHALL return `nullptr`

#### Scenario: Load and unload cycle is safe

- **WHEN** the module is loaded, then unloaded, then loaded again
- **THEN** no dangling implementation SHALL remain and creation SHALL succeed again after reload

### Requirement: Null-backend safety

`NavigationSystem` SHALL tolerate the absence of a registered factory and SHALL not crash when no backend is
loaded.

#### Scenario: World attaches without a backend

- **WHEN** a `NavigationSystem` is attached to a world with no registered factory
- **THEN** `GetNaviMesh()` SHALL be null and no crash SHALL occur

### Requirement: Validated runtime settings and lifetime

The runtime SHALL apply reflected settings to meshes. Invalid/mismatched settings SHALL reject creation with a diagnostic. Provider DLLs SHALL remain process-resident without callback code leases, and shutdown SHALL remove only session-owned registrations.

#### Scenario: Custom settings

- **WHEN** a world configures custom resolution and agent dimensions
- **THEN** generation SHALL use those values

#### Scenario: Invalid settings

- **WHEN** settings contain invalid or non-finite dimensions
- **THEN** navigation creation SHALL fail explicitly without assertions or exceptions

#### Scenario: Retained objects

- **WHEN** the module shuts down with referenced meshes/tasks
- **THEN** new factory creation SHALL stop and retained meshes, generators and tasks SHALL preserve their data ownership and completion constraints while implementation code remains process-resident

### Requirement: Cooked mesh runtime binding

The reflected navigation config SHALL accept a NaviMesh asset reference and choose eager loading or tile streaming. Runtime initialization SHALL verify asset loading and backend installation explicitly. An empty reference SHALL preserve programmatic generation. Offline cooking SHALL NOT load a previous product as its input navigation mesh.

#### Scenario: Eager cooked mesh

- **WHEN** a valid cooked mesh is configured with streaming disabled
- **THEN** runtime initialization SHALL load an independent queryable mesh before world attachment

#### Scenario: Streamed cooked mesh

- **WHEN** a tiled mesh is configured with streaming enabled
- **THEN** runtime initialization SHALL own its manifest and page it around the streaming focus

#### Scenario: Missing configured mesh

- **WHEN** the asset is missing, fails loading or cannot be installed
- **THEN** navigation initialization SHALL fail with a diagnostic

### Requirement: LOD0 residency for nav builds

Navigation builds SHALL require terrain LOD0 samples for the affected region; a nav tile build SHALL be deferred and retried when the required terrain LOD0 is not yet resident.

#### Scenario: Defer when LOD0 missing

- **WHEN** a nav tile build is requested but its overlapping terrain LOD0 tiles are not resident
- **THEN** the build SHALL be deferred and retried on a later tick once the terrain LOD0 is resident

#### Scenario: Never use render LOD

- **WHEN** a nav mesh is built for a terrain region
- **THEN** it SHALL use terrain LOD0 geometry and SHALL NOT use the render clipmap LOD

### Requirement: Incremental rebuild on terrain change

When a terrain tile's data changes, the navigation system SHALL invalidate and rebuild only the nav tiles overlapping that terrain tile, using the existing incremental rebuild path.

#### Scenario: Rebuild affected nav tiles

- **WHEN** a terrain tile is edited, regenerated, or otherwise marked changed
- **THEN** the overlapping nav tiles SHALL be invalidated and rebuilt, and unaffected nav tiles SHALL be left intact

#### Scenario: No rebuild for unchanged streamed data

- **WHEN** terrain LOD0 becomes resident for a region whose data matches what nav was built from
- **THEN** no nav rebuild SHALL be triggered for that region

### Requirement: Deterministic and cacheable builds

Terrain-driven nav mesh builds SHALL be deterministic for the same terrain data and build configuration, so results can be cached and reproduced.

#### Scenario: Reproducible output

- **WHEN** the same terrain data and build configuration are used to build a nav tile twice
- **THEN** the resulting tile payload SHALL be identical

### Requirement: Per-tile runtime add and remove

The nav mesh SHALL support adding and removing a single tile at runtime, affecting both the nav mesh and its tile
cache, and SHALL support initializing an empty streamable mesh from build params.

#### Scenario: Add a tile at runtime

- **WHEN** a persisted tile payload is added to a streamable mesh
- **THEN** the tile SHALL be present in the tile cache and the nav mesh

#### Scenario: Remove a tile at runtime

- **WHEN** a tile coordinate is removed
- **THEN** the tile SHALL be removed from both the nav mesh and the tile cache

#### Scenario: Streamable mesh starts empty

- **WHEN** a mesh is prepared for streaming with build params
- **THEN** it SHALL have a nav mesh, tile cache, and query but no tiles

### Requirement: Manifest-driven proximity paging

`NavigationSystem` SHALL page tiles around a focus position using a load radius and a larger unload radius
(hysteresis), driven from the world tick.

#### Scenario: Tiles near the focus load

- **WHEN** the focus is set near available tiles and streaming is enabled
- **THEN** tiles within the load radius SHALL be loaded

#### Scenario: Distant tiles unload

- **WHEN** the focus moves so a loaded tile is beyond the unload radius
- **THEN** that tile SHALL be unloaded

#### Scenario: Hysteresis prevents thrashing

- **WHEN** a tile is between the load and unload radii
- **THEN** its loaded/unloaded state SHALL be retained

### Requirement: Loaded and available tile tracking

The system SHALL track which tiles are available in the manifest and which are currently loaded.

#### Scenario: Loaded count reflects paging

- **WHEN** tiles are added or removed by paging
- **THEN** the loaded-tile count SHALL reflect the current set

### Requirement: Per-world isolation

Tile-cache allocator and mesh-processor state SHALL be per nav mesh instance, not process-global.

#### Scenario: Two meshes do not share state

- **WHEN** two nav meshes exist (for example two worlds)
- **THEN** each SHALL own its tile-cache allocator and mesh processor

### Requirement: Invalidation on mesh change

`NavigationSystem::OnNavMeshChanged` SHALL invalidate the loaded set so paging re-evaluates against the manifest.

#### Scenario: Mesh change resets paging

- **WHEN** the nav mesh changes
- **THEN** the loaded set SHALL be cleared

### Requirement: Owned layered manifest

Streaming SHALL own payloads grouped by coordinate and preserve every layer. The per-frame budget SHALL count coordinates. Replacing a manifest or detaching a world SHALL NOT leave work referring to released payloads.

#### Scenario: Caller releases source

- **WHEN** the caller releases setup data
- **THEN** later paging SHALL remain valid

#### Scenario: Layered coordinate

- **WHEN** one coordinate contains multiple layers
- **THEN** paging SHALL load all those layers

#### Scenario: Replace manifest

- **WHEN** setup replaces a manifest
- **THEN** stale data SHALL NOT be installed into the new mesh

### Requirement: Query invalidation before streaming mutation

Streaming SHALL cancel in-flight path results before adding or removing tile geometry, while worker tasks retain their inputs until actual completion.

#### Scenario: Tile mutation during a path query

- **WHEN** paging changes mesh tiles while a path query is in flight
- **THEN** the query SHALL be marked canceled and its completed output SHALL be invalid

### Requirement: Failed coordinates retain bounded retry state

Untried desired coordinates SHALL take precedence over retries. Failures SHALL back off with a bounded retry delay so a permanently invalid nearest coordinate cannot consume every frame's installation budget. An unsuccessful partial-coordinate rollback or unload SHALL remain tracked and be retried before adding that coordinate's layers again, even after focus moves away. Coordinate removal SHALL be idempotent on an initialized mesh so an already-absent first-layer failure cannot permanently block retry. Pending load count SHALL describe desired coordinates awaiting installation, including retry delay, rather than disk IO.

#### Scenario: Bad nearest coordinate

- **WHEN** the coordinate budget is one and the nearest coordinate repeatedly fails
- **THEN** farther healthy desired coordinates SHALL still be installed and repeated failures SHALL not be attempted every tick

#### Scenario: Rollback itself fails

- **WHEN** a layer installation fails and removal also fails
- **THEN** the runtime SHALL retain cleanup state and confirm removal before attempting a new installation for the coordinate

#### Scenario: First layer fails without installing geometry

- **WHEN** first-layer installation fails and leaves the coordinate absent
- **THEN** cleanup SHALL confirm absence successfully and a later retry SHALL be allowed after the failure cause is removed

### Requirement: Terrain LOD0 geometry provider

A terrain geometry provider SHALL implement the navigation geometry-provider interface and emit triangles derived from terrain core LOD0 tile samples, without depending on the render layer or the physics heightfield.

#### Scenario: Emit terrain triangles

- **WHEN** the terrain provider collects geometry for a bounds that overlaps resident terrain LOD0 tiles
- **THEN** it SHALL emit two world-space triangles per terrain quad over the overlapped region

#### Scenario: No render or physics dependency

- **WHEN** the terrain provider module is compiled
- **THEN** it SHALL depend only on the terrain core and the navigation provider interface, not on render or physics types

### Requirement: World-space triangle generation

Terrain triangles SHALL be produced in world space using the terrain metadata addressing and height decoding, with consistent upward winding so the nav mesh builder derives correct walkable slopes.

#### Scenario: Correct world positions

- **WHEN** a terrain quad is emitted as triangles
- **THEN** each vertex position SHALL equal the tile world origin plus the local texel offset times the resolution, with height from the decoded LOD0 sample

#### Scenario: Consistent winding

- **WHEN** terrain triangles are emitted
- **THEN** their winding SHALL face the terrain's up direction so recast computes walkable slope from the correct normal

### Requirement: Build-region clipping at quad granularity

The provider SHALL clip emitted geometry to the requested bounds and map nav build tiles to terrain tiles by world-space overlap, sampling only the needed sub-rect of each terrain tile.

#### Scenario: Partial tile overlap

- **WHEN** a nav build tile overlaps only part of a terrain tile
- **THEN** the provider SHALL emit only the terrain quads that overlap the requested bounds

#### Scenario: Independent tessellation sizes

- **WHEN** the nav tile size differs from the terrain tile size
- **THEN** the provider SHALL still map the nav tile's world bounds onto the correct terrain tiles and sub-rects

### Requirement: Missing LOD0 handling

The provider SHALL skip terrain tiles whose LOD0 data is not resident and SHALL report that the requested region was not fully covered, so the caller can defer the nav tile build.

#### Scenario: Non-resident LOD0

- **WHEN** a requested region overlaps a terrain tile whose LOD0 is not resident
- **THEN** the provider SHALL omit that tile's geometry and report incomplete coverage
