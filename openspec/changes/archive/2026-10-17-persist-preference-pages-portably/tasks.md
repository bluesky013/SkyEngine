## 1. Portable result and provider protocol

- [x] 1.1 Add explicit PreferenceResult/code/context and conservative stage-effect flags, plus injected IPreferenceStorage with NotFound/read/write and failed/throwing-Write byte-preservation contracts.
- [x] 1.2 Add optional owned-candidate validation and retry-safe page application callbacks while preserving registry registration identity and borrowed provider lifetime boundaries.
- [x] 1.3 Implement owned-value capture/validation/guarded publication helpers with callback copying, registration reacquisition and owner/session current checks.
- [x] 1.4 Correct Any::Create's throwing-placement-copy cleanup through RAII raw allocation ownership and delayed type/storage publication, without destroying an incomplete object; retain its signature, failure contract and existing generic JSON loader.
- [x] 1.5 Add CommandService revision tracking for transaction-internal and ordinary history mutations; snapshot/recheck it around callback-capable Apply stages and accept only Clear's own single revision increment at completion.

## 2. Checked document persistence

- [x] 2.1 Implement the host-compatible pageId/classId/elements codec using checked JSON/Any/reflection data without assertion-based archive loading or arbitrary custom codec invocation.
- [x] 2.2 Decode into provider-value copies, preserve missing fields/pages, validate all candidates before publication, and report invalid/unsupported/range/type/class failures explicitly; retain readable/writable/publication metadata boundaries without isStatic filtering.
- [x] 2.3 Merge Save into a fresh storage read, preserve unknown root/page/object keys and absent-plugin pages, and replace known arrays including empty sequences.
- [x] 2.4 Keep Load at a closed-session host boundary and serialize one service/store's operations under documented busy/reentrancy/owner rules.
- [x] 2.5 Clone Save candidates explicitly through Any::Create before storage/getter callbacks and prefer available nonconst getters on owned candidates, preserving guarded const-getter fallback.

## 3. Session and dialog integration

- [x] 3.1 Add ApplyDetailed ordering: verify/capture/validate, optional Save, guarded live/application publication, then one guarded history-clear boundary with conflict checks before success reporting; retain bool Apply compatibility.
- [x] 3.2 Preserve retryable working/history state on failure, accumulate operation effect flags, and document post-save partial effects, idempotent retry and Cancel's discard-only boundary.
- [x] 3.3 Inject optional shared persistence into PreferencesDialog, expose structured results and generic failure presentation, retain failed OK/Apply sessions, and keep onApplied success-only.
- [x] 3.4 Preserve callback destruction/reopen/replacement guards and prevent old operations/history notifications from consuming replacement history.
- [x] 3.5 Treat guard false/exception/self-invalidation as Conflict and recheck after the guard itself; reject stale snapshots after callback edits without erasing newer working/history state.

## 4. Portable regression coverage

- [x] 4.1 Cover plugin-style reflected page round trips, provider defaults, strings/nested values/enums/sequences, unknown keys/absent plugins and known-array replacement/clear through injected storage.
- [x] 4.2 Cover NotFound/read/write failure, preserved old bytes, malformed/type/class/range/unsupported data and no live changes before complete preparation/storage success.
- [x] 4.3 Cover validator/setter/application rejection/throw, failed/throwing storage preserving bytes, post-save conflicts, conservative stage-effect flags, failure/history preservation, retry success and Cancel/reopen after partial effects.
- [x] 4.4 Cover registry mutation/ABA identity, busy/reentrant operations, owner destruction and session replacement during callbacks/history notification.
- [x] 4.5 Cover optional dialog injection/result/error presentation, failed OK remaining open, success-only bool/onApplied compatibility and local/global history isolation; add bounded Shell/Theme notification coverage where useful.
- [x] 4.6 Cover throwing nontrivial working-value copy with heap-backed string storage: safe InvalidData/no-effects result, preserved working/live/storage/history and corrected retry under normal/ASan profiles.
- [x] 4.7 Cover a guard closing the session or throwing, storage/application callbacks editing working values including pending transaction edits, and history observers replacing session/work at Apply Clear; verify Conflict and preservation of newer history.
- [x] 4.8 Cover Save's explicit snapshot ownership/copy failures and supported reflected types with nonconst-only getters.

## 5. Verification and scope closeout

- [x] 5.1 Build affected targeted portable CPU suites and run focused normal/ASan regressions; record actual targets/counts/configuration/results.
- [x] 5.2 Review checked codec/support boundaries, adapter byte-preservation contract, callback/partial-effect semantics and unchanged excluded renderer/module/event scope.
- [x] 5.3 Run strict OpenSpec validation and tracked/untracked whitespace checks; update canonical docs with actual portable behavior/evidence and deferred native integration.

Actual SandboxModule/native storage Load/Save/Apply wiring, Windows/display/GPU validation, RDG/RHI/renderer/surface work and Core Event remain pending and outside this implementation. Prior change evidence is preserved. The portable implementation and acceptance tasks above are complete. Actual native integration and platform/display gates remain separate; no commit or archive is authorized.

Verification (2026-10-17, macOS): final targeted normal and AddressSanitizer builds/tests passed
for EditorCoreTest 143, EditorWidgetsTest 32, EditorShellTest 103, UICoreTest 185, UITextTest 25
and UIEditorTest 3: **491 tests per profile**. New coverage comprises 30 cases: 26 Core
persistence/Apply-result regressions and 4 Shell integration regressions. Normal/ASan totals overlap;
they do not represent 982 distinct cases. ASan used `ASAN_OPTIONS=detect_leaks=0` and reported no
errors; macOS LeakSanitizer is unavailable.

Both final six-suite targeted builds succeeded with 47 build commands each and zero excluded
commands. All 21 active UI changes passed strict OpenSpec validation. Tracked changes and 270
untracked files passed whitespace checks. All 264 excluded-scope content hashes matched; no
RDG/RHI/renderer/native-module/Core Event scope was changed. No full/native build was requested.

Coverage includes checked document round trips/defaults/unknown-key preservation and array
replacement, invalid/unsupported/read/write failures, conservative effect flags, retry/Cancel,
throwing nontrivial candidate-copy cleanup, explicit Save snapshots, nonconst-only getters,
registration/session/owner invalidation, current-guard exceptions and self-mutation, transaction-
internal callback edits, and existing Clear/session-replacement regressions preserving new history. Optional dialog/Shell persistence and Theme application are exercised through portable
injected storage and callbacks.

Actual SandboxModule generic storage Load/Save/Apply wiring remains deferred: the native host
still uses its legacy three-built-in-page path. Portable opt-in service/API delivery does not close
Windows, native display/DPI/GPU or module integration gates. The independent reflected ORDER
metadata gap remains a bounded follow-up in the canonical plan. No commit or archive performed.
