## Implementation

- [x] 1. Trace old graph construction, backend factories, templates and helpers across all repository consumers.
- [x] 2. Remove the closed old graph dependency chain in all three backends without changing target ownership.
- [x] 3. Remove exclusive tests and preserve independent cases in mixed test files and shared shader declarations.
- [x] 4. Reconcile main specs, overlapping changes, Aurora rules and current docs; record historical retirement separately.

## Validation

- [x] 5. Reconfigure and build the existing Windows Debug tree, including affected hosts and backend DLLs.
- [x] 6. Run RHI, Pipeline, Core and production GPU frame graph regression suites; correct the stale prepared-material fixture and pass its three backend cases plus variant-selection regression (4/4).
- [x] 7. Verify no dangling references, scoped formatting, Git index preservation, strict OpenSpec and delta merge previews.

## Platform follow-up

- [ ] 8. Build and run native Metal regression in the macOS validation environment; unavailable on this Windows host.
