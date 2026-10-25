## Why

The skeletal workflow requires several products from one source to become visible together. The reusable publication, snapshot and child-routing changes are Core/Framework infrastructure and can be reviewed before animation, renderer and editor integration.

This change extracts that existing implementation for commit preparation. It does not add a target, change module ownership or expand the approved feature scope.

## What Changes

- Publish immutable product generations with checked staging and one atomic manifest replacement.
- Pin product and bulk reads to a generation; report serialization and filesystem failures explicitly.
- Persist generated child identities and resolve child cook requests through their source owner.
- Coalesce cook requests and complete every requested child with its own terminal result.
- Keep Framework test fixtures in the build directory.

## Capabilities

### Modified Capabilities

- `asset-product-loading`: generic transaction, snapshot and child completion contracts.
- `asset-source-identity`: generic generated-product identity and owner metadata.

## Impact

Existing Core file operations, Framework asset/archive APIs and tests. Animation-specific import, packaging, selection UI and Mesh/Skin/Skeleton consistency remain in `add-skeletal-animation-workflow`. Persistent callback code leases remain in `refactor-process-owned-reflection`; the isolated transaction snapshot retains the existing registration lifecycle. Implementation and these specifications belong in one commit. Commit and archive require separate user confirmation.
