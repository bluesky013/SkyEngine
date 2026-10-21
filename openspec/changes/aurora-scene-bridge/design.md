## Context

Two scene models coexist: framework `World`/`Actor`/`ComponentAdaptor<Data>` (OOP authoring) and the renderer-owned scene ECS. Aurora adaptor components already hold `Uuid` + POD and asset holders. This change supplies the producer of the owned FramePacket protocol defined by `aurora-renderer`, extending the existing `aurora-adaptor` capability.

## Goals / Non-Goals

**Goals:**

- One-way transfer framework scene → aurora scene ECS.
- Render state lives only in aurora.
- Components stay render-free at the persistence layer.

**Non-Goals:**

- The legacy `engine/render` renderer.
- Two-way synchronization.
- Changing aurora RHI or the framework component API.

## Decisions

### D1: `RenderSceneBridge` subscribes to lifecycle and transform

Lives in `engine/aurora/adaptor`; consumes Framework lifecycle/transform events and the engine `IRenderer` contract. It batches owned create/update/destroy commands on the host thread. Only the renderer coordinator applies them to its ECS; the bridge never includes SceneStore or borrows component/World pointers into a packet.

### D2: Strictly one-way

Reads framework state and submits owned values through IRenderer; never directly accesses renderer ECS. Status/retirement events acknowledge lifetime and errors, not scene state write-back.

### D3: Persistence layer stays render-free

The bridge consumes only `Uuid` + POD members and the transform world matrix. Non-serialized runtime caches (asset handles) are allowed on adaptor components but not in persisted data (see `harden-framework-components` decoupling rules).

### D4: Frame sync point

After World::Tick, seal lifecycle/transform changes into an owned FramePacket. Accepted consumes it; Busy/Rejected preserve ownership for bounded retry or explicit error handling. FIFO application occurs on the render coordinator before prepare. Destroy acknowledgement is required before reusing a generation-qualified entity ID.

### D5: Stable identity and first coverage

Use a bridge-owned side table keyed by World instance, Actor UUID, component identity and renderable part. Store generation-qualified Renderer IDs and binding revisions; do not derive identity from Actor addresses or assume one render entity per Actor. Submeshes can remain multiple draw packets of one renderable. Retain tombstones until the renderer acknowledges retirement, and reject late events for an old generation or binding revision.

Initial extraction covers static mesh, light and camera values. World lifetime uses the RenderSceneProxy contract from aurora-renderer; component events use this bridge. Asset completion only publishes logical references or owned payload commands at the host sync point. Resource deduplication, InGraph/Resident and version leases remain renderer responsibilities.

### D6: Independent contract verification

S01/S02 use a recording fake IRenderer to verify owned values, ordering, Busy/Rejected and retirement events without depending on native rendering. S03 reuses the same cases against the production provider and verifies applied ECS/output behavior. Fake acceptance alone does not prove GPU readiness or retirement.

## Risks / Trade-offs

- [Frame sync point ordering] → Define one point after `World::Tick` and before the aurora build.
- [Asset readiness lag] → Publish logical resource references; asynchronous loading/upload readiness follows aurora-renderer's resource protocol, without blocking the host or borrowing asset pointers.
- [Transform correctness] → Depends on the fixed propagation; test world matrices survive the transfer.

## Migration Plan

- Use the archived [framework component baseline](../archive/2026-10-06-harden-framework-components/proposal.md); it is a regression prerequisite, not a new pending implementation change.
- Follow S01 → S02 → S03 in [tasks.md](tasks.md). Windows S03 has passed the aurora-renderer B12/B14 host gate and now waits only for S01/S02; see the renderer [batch plan](../archive/2026-10-17-aurora-renderer/batches.md).
- Select one host scene producer at initialization. Do not submit duplicate lifecycle commands through both old direct paths and the bridge.
- Disable by stopping new subscriptions, resolving retained packets and waiting for renderer retirement acknowledgements before releasing mappings or module code.

## Implementation checks

### Concrete Windows implementation

- `RenderSceneBridge::Append` appends to the host-owned packet, never submits or polls independently. Slots use actor/component/part identity; old World slots become tombstones and are reused only after exact generation-qualified retirement.
- Aurora components publish attach/detach through an adaptor-local base. Transform subscriptions cache world matrices. Mesh assets are asynchronously requested and polled by `SceneAssetSource`; shader compilation occurs on first material conversion and is cached with its owned result.
- Mesh parts share one vertex/index backing and select validated index sections. Pass declarations and default shader sources are embedded from their build inputs; generated C++ layouts remain the sole source for constants.
- First material coverage is one scene technique, `tint` float4, float32 vertex attributes and static indexed geometry. Unsupported textures/properties/skin return explicit errors. Cache limits are 128 mesh/material combinations, 1024 assets and 256 MiB CPU payloads. Asset reload/invalidation is a later extension; loaded assets are immutable within this source lifetime.
- Directional/spot values extend the existing owned light payload. Consumption by lighting/shadow passes belongs to the next stage. Editor component inspection and its small Framework reflection-address API addition are tracked separately in `aurora-world-editor`.

- Confirm component identity and event ordering against the archived Framework contract while implementing S01; keep the side-table key stable across attach/detach.
- Confirm light/camera payload support at S03 integration; missing renderer support is an explicit dependency failure, not permission to access renderer stores directly.
