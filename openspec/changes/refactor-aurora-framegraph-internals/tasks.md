## Implementation

- [x] 1. Separate graph resource validation, typed pass access collection, culling and content/lifetime checks.
- [x] 2. Centralize repeated binding lookup and image-view range normalization.
- [x] 3. Extract backend-private compilation stages and preserve native allocation/synchronization ownership.
- [x] 4. Inventory legacy RDG consumers for a separate migration review.

## Validation

- [x] 5. Verify analysis-stage behavior and binding-order regressions.
- [x] 6. Build affected Windows targets and run GPU submission and RHI suites.
- [x] 7. Check scoped formatting/static analysis, diffs and OpenSpec; record platform limitations.

## Platform follow-up

- [ ] Run macOS compilation and native Metal regressions in the platform validation environment; unavailable on this Windows host.
