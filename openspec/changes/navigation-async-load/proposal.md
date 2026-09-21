## Why

The build is already asynchronous (`NaviMeshGenerator : Task` via taskflow), but loading a nav mesh or its tiles
has no async path: there is no completion callback, no cancellation, and no I/O budget. Paging tiles or loading a
nav mesh asset would block the main thread. This change provides the off-thread load mechanism that tile streaming
and runtime nav mesh loading build on.

## What Changes

- Add an asynchronous load path for the nav mesh / tiles based on the existing `Task` system (or the asset
  system's async load), with a completion callback or polled status.
- Support cancellation (world shutdown, tile leaves range before load completes).
- Apply loaded data on the main thread only (nav mesh/tile mutation is not thread-safe).
- Budget/batch tile loads so paging does not spike a frame.
- Report load failure without blocking gameplay (fall back to "no path" for the affected region).

## Capabilities

### New Capabilities
- `navigation-async-load`: asynchronous nav mesh / tile loading with completion, cancellation, and main-thread
  application.

## Related backlog (to split into its own change when expanded)

- `navigation-tile-streaming`: the paging policy that consumes this load mechanism.
- `navigation-mesh-asset-pipeline`: the asset the loader reads.

## Impact

- `plugins/recast` + `engine/navigation` (loader), `engine/core/async/Task` reuse, asset system read path.

## Open Questions (expand later)

- Reuse `Task` vs an asset-system async load; cancellation granularity.
- Whether to keep a persistent on-disk tile cache (uncompressed fast-load) in addition to compressed tiles.
