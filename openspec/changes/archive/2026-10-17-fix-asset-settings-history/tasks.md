## 1. Stable domain commands

- [x] 1.1 Add optional UI-free ReflectedForm command construction and route scalar, reset, and sequence mutation through it.
- [x] 1.2 Construct asset commands from UUID/target/schema and owned before/after sparse target settings; retain no reflected/viewer model target.
- [x] 1.3 Use one target-specific persistence/revision/observer path for Do/Undo/Redo and preserve other targets/cook fields.
- [x] 1.4 Define reject/inert behavior for missing/read-only assets, removed targets, replaced schema and no-op values without changing CommandService business responsibilities.

## 2. Session integration

- [x] 2.1 Inject viewer-owned history into reflected forms and define fresh Open/reopen/switch, close, catalog replacement, and destruction lifetime.
- [x] 2.2 Route viewer scoped action enabled state and Undo/Redo to the session.
- [x] 2.3 Reload disposable settings from persisted state after any history change, while retaining valid history across normal Any replacement and clearing invalidated session targets.
- [x] 2.4 Guard synchronous observer destruction/rebinding with copied callbacks and weak core generations; defer viewer lifecycle/tree replacement during active form edits until a safe post-event layout boundary.

## 3. Regression and validation

- [x] 3.1 Add core heap-backed snapshot/persistence/target-isolation and factory scalar-reset-sequence tests.
- [x] 3.2 Add missing asset/target/schema rejection regressions and real viewer session close/reopen/switch/multi-target/scoped action regressions.
- [x] 3.3 Reconfigure/build EditorCoreTest, EditorWidgetsTest, EditorShellTest and affected editor consumers; run relevant regression suites.
- [x] 3.4 Run disposable-settings Undo/Redo and host session regressions under ASan; record actual result.
- [x] 3.5 Run strict OpenSpec/whitespace checks, review model/service ownership and record remaining platform limits. Do not commit/archive.
- [x] 3.6 Add real catalog-observer regressions for editing-form destruction/rebind, widget-event asset switch, target replacement plus nested layout, close, and replacement-session counts during Undo.

## Verification preparation (2026-10-16, macOS)

- Strict OpenSpec validation and `git diff --check` passed; changed C++ files were clang-formatted.
- Added AssetSettingsHistoryTest.cpp to EditorCoreTest (CMake reconfigure required): **four core regressions** cover owned snapshots after heap-model release, target/persistence isolation, scalar/reset/sequence factory paths, stale identity/target/schema, and a real synchronous catalog observer destroying or rebinding the editing form without an obsolete changed callback.
- Added **six EditorAssetSettingsHistoryTest shell regressions**: session persistence/Undo/Save and target/asset isolation; scoped shell actions; an actual Generic inline Enter event with observer-requested asset switch; target replacement and close with nested synchronization during that widget event; an Undo observer switching assets without repopulating the replacement session; and combined domain history plus active color preview. The combined case checks the restored color inside synchronous persistence notification (before Viewer reload), persisted Undo/Redo maps, UI values, cursor counts, late Escape/release, and Save after Undo. Existing T03 host fixtures retain color-preview lifetime coverage.
- Build/test/ASan is coordinated by the parent task. This implementation subtask did not launch a shared build.
- At preparation these counts described persistent coverage, not completed full-suite ASan evidence. The final execution record below now closes those validation tasks; action-owner and reflected-field-storage repairs remain separate changes discovered during integration.
- Native Win32/GPU verification is not evidence supplied by this CPU/domain task. Filesystem failure has no generic command-result UI protocol; commands never fall back to writes through obsolete model targets.

Integration verification (2026-10-16): full macOS build and final affected editor-consumer rebuild
passed. EditorCoreTest 117, EditorWidgetsTest 28, and EditorShellTest 59 passed normally and under
AddressSanitizer (`detect_leaks=0`). The four core/six shell asset-history cases include disposable
heap-backed settings, persistence/Save state, session/target isolation, synchronous observer
replacement, and a joint color-preview/domain-history regression. That regression checks the
preview is restored inside persistence notification before Reload, then checks UI/cook.json/cursor
and delayed Escape/mouse-up. Save enqueues cook from the persisted snapshot; product cook output
was not validated by this CPU fixture. Strict OpenSpec/whitespace passed. No commit/archive.
