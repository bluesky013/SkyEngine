## 1. Asset correctness

- [x] 1.1 A5: reconcile the complete path-resolution requirement and test published/source overlay semantics.
- [x] 1.2 A1: migrate both index caches and all borrowed entry consumers to immutable snapshots; test replacement/invalidation lifetimes and failed saves.
- [x] 1.3 A1/B1: serialize snapshot publication, keep reload read-only, and distinguish filesystem instances.
- [x] 1.4 A4/B6: partial parsing with physical diagnostics, conflict isolation, strict output preservation and batch sorting.
- [x] 1.5 A4: structured scan reports, blocked-directory handling and manifest-over-cache precedence across startup/rebuild/migration callers.
- [x] 1.6 A2: shared destination/mount validation, collision policy and checked scan-error resolution.
- [x] 1.7 A3: exclusive import and complete duplicate metadata preparation/cleanup with failure injection.
- [x] 1.8 B1: explicit durable commit, derived repair and compensation results across mutation callers.
- [x] 1.9 A6: two-UUID configuration regression preserving valid output on invalid input.

## 2. Scan and cook performance

- [x] 2.1 B2: directory batches with bounded reads/saves; test cold/warm/missing/read-only/damaged manifests.
- [x] 2.2 B4: conditional generated refresh; no writes to read-only or blocked directories.
- [x] 2.3 B5: pure Engine default-material query.
- [x] 2.4 B3: split memory/file lock phases with cancellation and bounded Busy handling; source/dependency publication checks captured incarnations.
- [x] 2.5 B3: version cook coalescing and validate before product publication, including external workers and all completion paths.
- [x] 2.6 B6: measure directory scaling and preserve deterministic serialization without per-entry sorting.

## 3. Editor follow-up

- [x] 3.1 C1/C2: add the editor domain delta, interaction scopes and complete tooltip invalidation/occlusion signals.
- [x] 3.2 C3/C4: cache text layout and consolidate only matching packed-ID implementations.
- [x] 3.3 C5: atomic settings requests, last-request-wins and cancellation of old callbacks during switch/teardown.
- [x] 3.4 C6: return and retain registration handles at all affected owners.
- [x] 3.5 C7/C8: pointer leave/re-entry/capture behavior on Windows, macOS and Linux.

## 4. Resource follow-up

- [x] 4.1 D1: add the renderer domain delta and centralize cache-owned payload budget policy.
- [x] 4.2 D2: share immutable payload storage before unique-identity accounting; test variants, epochs, failure and refresh.
- [x] 4.3 D3: late provider injection and ground defaults for future preview sessions.

## 5. Command lifetime and deferred architecture

- [x] 5.1 E1: add the editor command delta and validate document/world/actor/component incarnations before edits and dirty callbacks.
- [x] 5.2 E1: test stale commands, replacement objects and valid detach/reattach undo cycles.
- [x] 5.3 E2/E3: document the separate Assets include/target proposal and reassess ResolveCached after storage changes; do not relocate targets in this change.

## 6. Acceptance

- [x] 6.1 Run affected real libraries/modules and regression suites; validate OpenSpec and source ownership when build organization changes.
- [x] 6.2 Exercise concurrent scan/worker/cache publication and failure/recovery stages.
- [x] 6.3 Run native editor checks, retain a few key screenshots and record actual platform coverage without claiming unavailable-platform execution.

## Acceptance notes (2026-11-16)

- Native platform: macOS / Intel Metal. Windows and Linux leave/re-entry branches were reviewed; their native execution is not claimed.
- 6.3 complete: scene selection, Engine path labels, Preferences/Project Settings and create/delete/undo/redo passed. User-assisted native hover displayed the material path, type and GUID; clicking File cleared the tooltip. Four key screenshots retained. Modal/occlusion/floating-surface edge cases have automated coverage; exhaustive native coverage is not claimed. The Inspector default-object lifetime crash found during acceptance was fixed and covered by a new regression.
- No commit or OpenSpec archive has been performed. Both require the repository-mandated explicit user confirmation.
