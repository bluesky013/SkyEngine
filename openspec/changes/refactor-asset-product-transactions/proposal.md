## Why

The reusable checked-file, generated-identity and child-routing changes are Core/Framework infrastructure. The original product-group/generation implementation is superseded by `simplify-asset-product-storage`; it is not the current publication contract.

This change extracts that existing implementation for commit preparation. It does not add a target, change module ownership or expand the approved feature scope.

## What Changes

- Supply checked file operations and explicit serialization/filesystem failure reporting.
- Preserve active loaded data/bulk lifetimes; the successor owns flat-product publication and instance identity.
- Persist generated child identities and resolve child cook requests through their source owner.
- Coalesce cook requests and complete every requested child with its own terminal result.
- Keep Framework test fixtures in the build directory.

## Capabilities

### Modified Capabilities

- `asset-product-loading`: generic generated-child completion contracts; publication and active-instance storage now belong to the successor.
- `asset-source-identity`: generic generated-product identity and owner metadata.

## Impact

Existing Core file operations, Framework asset/archive APIs and tests. Animation-specific import, packaging, selection UI and Mesh/Skin/Skeleton consistency remain in `add-skeletal-animation-workflow`. Persistent callback code leases remain in `refactor-process-owned-reflection`; the isolated transaction snapshot retains the existing registration lifecycle. Implementation and these specifications belong in one commit. Commit and archive require separate user confirmation.

## Successor

`simplify-asset-product-storage` removes ProductGroup APIs, generation directories, old-format fallback and persistent group snapshots. Its current contracts override the earlier implementation and verification snapshot. Clear old cook output and rebuild; no group compatibility layer is retained.
