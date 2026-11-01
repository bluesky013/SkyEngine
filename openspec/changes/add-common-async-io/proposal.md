## Why

Navigation, scene textures/meshes and material shader artifacts still reach synchronous file reads despite having background tasks and GPU upload queues. A shared, bounded asynchronous byte service is needed before these consumers can provide predictable streaming without blocking their producer threads or eagerly retaining entire products.

Status: detailed design prepared on 2026-11-01; production implementation has not started. The source review is recorded in [review.md](review.md), verified primary sources in [industry-references.md](industry-references.md), operation contracts in [io-contract.md](io-contract.md), and the binary layout in [product-format.md](product-format.md).

## What Changes

- Add checked, stable read handles, a bounded IO scheduler and replaceable storage backends in the existing Core target; reuse Core completion/continuation contracts. A small blocking-worker backend is the first implementation.
- Separate file waiting, CPU decode/cook, and consumer publication. Preserve Aurora's existing upload, fence and resource-retirement owners.
- Add Framework product snapshots with lightweight metadata, typed dependency edges and independently checked physical blocks; logical mip/LOD/tile blobs can span blocks. Keep flat UUID products and retained loaded-instance lifetimes.
- **BREAKING**: introduce a chunk-addressable product format revision and recook affected products; do not silently read old products through an eager fallback.
- Make runtime admission nonblocking with explicit capacity, priority donation, cancellation linearization and shutdown behavior. Reserve required-closure bytes/control/handle credits before partial dependencies can create wait cycles.
- Stage navigation, scene image/mesh and shader-artifact adoption. Material metadata and dependency preparation use the same scheduler; each domain retains its own residency policy.
- Fix the reviewed navigation failure/accounting and decoder validation defects before using that implementation as a streaming reference.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `async`: checked 64-bit reads, stable handle lifetime, bounded scheduling, owned buffers and completion/drain semantics independent of asset or renderer types.
  immutable product read snapshots, metadata/chunk verification, shared range requests and consumer-specific readiness/publication boundaries.
  nonblocking runtime admission and separation of byte reads from CPU dependency/decode work, preserving existing explicit blocking tool operations.

## Impact

Core file/async utilities; Framework AssetManager, AssetExecutor, product encoding/inspection and application drain; Recast shared/runtime/builder; Aurora's existing adaptor, shader-store and cook consumers; corresponding tests and packaging audits. No directory relocation, new target, Aurora module ownership change, second GPU uploader, ProductGroup or persistent generation directory is proposed.

The current flat-product contract in `simplify-asset-product-storage` remains authoritative over older group-oriented main-spec text. `aurora-navigation-integration` owns the already implemented navigation/editor separation; this change owns the shared byte-service seam. Concurrent Detour queries without a mesh mutex require a separate reviewed mesh-publication change after bounded byte preparation is integrated; adding an IO pool alone does not satisfy that goal.
