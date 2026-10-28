## Why

The first release does not require old cooked versions or old product-format compatibility. Public product groups, owner manifests and generation directories add a second identity/publication model to an otherwise UUID-based loader. Keep checked product writes and active resource lifetimes while removing that model.

## What Changes

- **BREAKING**: remove ProductGroup APIs, generation directories, owner manifests, group snapshots and legacy/obsolete product fallback. Clean existing cook output and recook; do not migrate it.
- Store one versioned, identity-checked and checksummed product at `<uuid-prefix>/<uuid>.bin`.
- Make `SaveAsset` and `SaveProducts` return explicit ProductStatus; validate and stage before atomic per-file replacement. A batch failure cannot report successful cook/package publication and does not promise restoration of an old complete batch.
- Preserve generated child identities and owner cook routing in the source catalog, without exposing a runtime group.
- Replace group revisions with loaded-instance identity/revision; retained CPU data, bulk reads and GPU resources remain valid while in use.
- Update runtime consumers, product auditing, packaging and Framework regression tests. No new module, target, container or loader entry is introduced.
- Preserve the rebased Jolt integration with platform/consumer-matched RTTI and SDK leases scoped to the actual native static image, while public registries and metadata retain process-wide ownership.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `asset-product-loading`: replace persistent group/generation contracts with checked flat products and active loaded-instance lifetime guarantees.
- `jolt-integration`: clarify the existing SDK ABI and lease ownership contracts needed to retain the remote Jolt functionality during integration; keep C++ exceptions disabled and add no public SDK API.

## Impact

Existing Framework asset APIs, Core checked file cleanup, Content/Aurora/plugin producers and consumers, AssetTool, package tools and their tests. This change supersedes the product-group publication/snapshot parts of `refactor-asset-product-transactions`, the group integration requirements of `add-skeletal-animation-workflow`, and the temporary visible-group layout in `verify-ios-world-packaging`. Generated source identities, cook coalescing and renderer frame leases remain in their existing owners. Older verification reports describe their tested intermediate implementation; this successor defines the current storage contract. Format migration policy is deferred until after the first formal release. Commit and archive require separate user confirmation.
