## MODIFIED Requirements

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

## ADDED Requirements

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
