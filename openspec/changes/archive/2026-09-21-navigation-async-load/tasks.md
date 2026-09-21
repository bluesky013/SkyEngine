## 1. Async load path

- [x] 1.1 Define an async nav mesh / tile load request (source + completion)
- [x] 1.2 Implement the loader on the `Task` system
- [x] 1.3 Apply loaded data on the main thread only

## 2. Lifecycle

- [x] 2.1 Completion callback / polled status
- [x] 2.2 Cancellation (world shutdown, out-of-range tile)
- [x] 2.3 Failure handling without blocking gameplay

## 3. Budgeting

- [x] 3.1 Budget/batch tile loads per frame
- [ ] 3.2 Verify paging does not spike frame time

## 4. Related backlog

- [ ] 4.1 `navigation-tile-streaming`: paging policy consuming this loader (split into its own change)
- [ ] 4.2 `navigation-mesh-asset-pipeline`: the persisted asset to load (split into its own change)
