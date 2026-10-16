## Context

T11-A provides session-owned working values, independent command history, registration identity and guarded transitions. T11-B composes the dialog through shared containers with consistent layout/input. Persistence still lives in SandboxModule as three hard-coded page ids, while the dialog's void onApplied callback cannot distinguish validation, storage, setter and runtime-application failures.

This change supplies the portable T11-C protocol in EditorCore and an optional dialog integration. Actual native host Load/Save/Apply wiring remains deferred under the separate RDG/RHI exclusion. Existing per-user files use a root object mapping page id to a reflected `{classId, elements}` value. That envelope remains compatible; process-local registration numbers are not serialized.

## Goals / Non-Goals

**Goals:**

- Persist contributed reflected pages through one UI-free implementation and an injectable storage boundary.
- Load checked owned candidates before publishing any live setting, preserving provider defaults and unknown saved data.
- Expose result codes, page/member context, and observable partial effects for failure/retry handling.
- Preserve working objects/local history on failure and establish a successful Apply boundary only after every required stage succeeds.
- Keep page validation/application ownership with the contributing page and retain safe identity/generation checks around callbacks.
- Integrate optional portable persistence/results into the existing generic dialog without changing its field UI or actual native host wiring.

**Non-Goals:**

- Atomic rollback across arbitrary reflected setters, storage and runtime application side effects.
- A generic database, concurrent/asynchronous preference transactions, plugin/DLL unload ownership, or global Core Event changes.
- A new settings schema, custom hand-written page forms, arbitrary custom serialization execution, or a general Framework JSON archive rewrite.
- Actual SandboxModule per-user adapter/wiring, native settings application, RDG/RHI/renderer/surface code, or Windows/display/GPU acceptance.
- The separately recorded reflected ORDER-metadata correction.

## Decisions

### Keep data/results/services portable

PreferenceResult carries PreferenceCode, pageId, path, message and three conservative effect flags: storageWritten, liveWritten and applicationApplied. storageWritten becomes true only after Write explicitly succeeds. liveWritten becomes true before entering the first live setter, and applicationApplied before entering the first page application callback: either callable may produce side effects before rejecting or throwing. Those latter flags mean a potentially effectful stage was entered, not proof that a value changed or application completed. Succeeded describes complete success, and HasPartialEffects distinguishes a failed operation that crossed one of those boundaries. Codes include absent/read/write storage states, malformed/invalid/unsupported data, validation/application rejection, identity conflict and busy/reentrant rejection. Context identifies the failing page/member without embedding native UI state.

PreferencePage gains optional validate/apply callbacks. Validation receives an owned candidate view and must not mutate settings; application runs after guarded live writes and must tolerate retries. Callbacks, object/type metadata and registrations remain borrowed provider contracts. The existing cross-DLL PreferencePageRegistry Singleton owns registration metadata, and Add retains monotonically advancing registration identity. No additional process-global singleton or per-DLL static registry is introduced.

PreferencesPersistence owns a shared IPreferenceStorage adapter. Read returns document bytes or an explicit result, using NotFound for absent storage. Write's failure/exception contract preserves its previous document bytes; an adapter that can partially replace bytes before failure/throw does not satisfy this interface. A native host must eventually supply an adapter honoring that contract, while this scope tests it with injected storage. Calls on one store are serialized by its owner. This single-write contract is not a transaction across validation/live/application operations.

### Preserve the existing envelope with a checked owned-value codec

The document remains `{pageId: {classId: UUID, elements: {...}}}`. Stable page ids identify entries; classId validates a known page's reflected type. Registration counters and object addresses are runtime identity only. Empty/ambiguous ids, invalid reflection data and unsupported types yield explicit failures instead of silently claiming those pages were persisted.

The codec uses RapidJSON, Any and reflection metadata directly. It checks JSON shape, primitive type/range, finite numeric values, class/type identity, and supported nested/sequence data before publishing. It does not call JsonInputArchive::LoadValueById on untrusted data: that path has assertion/type assumptions and append-oriented sequence behavior rather than a checked result contract. It does not blindly invoke arbitrary registered custom codecs; unsupported schemas return UnsupportedType with page/path context. Existing Framework serialization remains unchanged.

Encoding covers readable reflected fields. Decoding changes only writable candidate fields; guarded publication preserves the previous Session filter for top-level const/no-setter/READONLY/invisible members. This does not introduce a new global field-persistence metadata policy. Neither Load nor Save filters on the unreliable existing TypeFactory isStatic flag. Generic metadata ordering and reflection-registration corrections remain separate work.

Known arrays replace their existing candidate/saved arrays, including an empty array. Missing known pages or fields retain the provider-supplied candidate values. Unknown root/page/object keys are ignored for loading and retained during save merging, including pages belonging to plugins absent from the current registry. Saving starts with a fresh storage read so it does not overwrite unknown data from a stale internal document snapshot. An invalid existing document is reported rather than silently overwritten as a new empty document.

Save first clones each candidate with explicit Any::Create into operation-owned snapshots before invoking storage/getter callbacks. It does not rely on an implicit Any copy path to retain input across callbacks that can release their caller. Copy exceptions reuse the same safe incomplete-construction cleanup and return InvalidData before storage effects. Reading fields on those owned candidates prefers getterFn when present and falls back to getterConstFn, preserving reflected types that provide only a nonconst getter while retaining post-getter guards.

### Load at an explicit closed-session host boundary

Load is called when the host controls live settings and has no open editing session. It captures eligible provider pages into owned copies, parses all known data into those candidates, and validates the full candidate set before PublishPreferenceValues writes live settings or invokes application callbacks. No malformed/unsupported/validation/read failure publishes a partial decoded candidate. Missing storage is reported as NotFound and preserves existing provider values.

The copied initial values supply missing-page/field defaults; loading does not fabricate a replacement page instance or reset unspecified fields to zero. After preparation, publication retains the same guarded setter/application behavior and partial-effect boundary as Apply. A setter/application failure can therefore leave reported live/application effects even though decoding itself was all-or-nothing before publication.

This service does not silently replace an already-open session's working pages. A future native host must load before opening the dialog or perform a separately controlled close/rebase boundary. The API documents this caller responsibility; it is not a global registry lock or automatic editor-session discovery mechanism.

### Make Apply an ordered gate with explicit partial effects

PreferencesSession::ApplyDetailed uses this order:

1. Verify the current session and registration identities and capture owned candidate values from its stable working pages.
2. Validate every candidate through the checked model and copied page validation callbacks. Recheck session/registry/owner validity after each callback.
3. If a persistence service is supplied, Save the candidates through fresh-read merge/checked serialization and one storage Write.
4. Publish guarded writable reflected members and run copied per-page application callbacks, rechecking current identities/generations after each notification.
5. Successful required publication reaches one guarded history-clear boundary; complete success is reported only if its observers/current guard add no conflicting work. The legacy success notification follows that completed check.

Read, decode/serialization, candidate validation and failed Write occur before live publication. They leave live settings unchanged and keep working values/history available for correction or retry; failed Write also keeps old stored bytes by the adapter contract. With no persistence service, the same validation/publication contract operates without storage effects. Existing bool Apply remains a compatibility wrapper reporting complete success.

A successful Write followed by a setter, application or registration conflict is a failed result with potential partial effects. Already-written bytes/live values and provider application side effects are not rolled back through arbitrary callbacks. The result accumulates storageWritten after confirmed write success and conservatively marks liveWritten/applicationApplied before entering those potentially effectful callables, including failure/throw paths. The framework cannot infer whether an arbitrary callback actually changed external state. Failures before the history-clear boundary retain current working objects/history unless the owner intentionally replaced/ended the session. At a conflict caused by a Clear observer, the already-published old snapshot's history has been cleared, but the observer's newer work/history must remain and must not be cleared again. Retry may repeat earlier writes/application, so providers must make application callbacks idempotent.

Cancel/Close still discard the current session's working values/history. After a partial failure, they do not undo durable bytes, live writes or external application that already happened. Reopening reads current live settings, which may reflect partial publication; startup Load may later observe a successfully written file. The UI/result contract must not describe that state as a clean rollback or successful Apply baseline.

### Preserve identity, reentrancy and service lifetime

CapturePreferenceValues, ValidatePreferenceValues and PublishPreferenceValues operate on owned values and copied metadata/callbacks. They revalidate registration identity before live access and after callbacks. Captured pointers into a registry vector do not survive callbacks that may remove, reorder or replace pages. Same-address re-registration remains a conflict through registration identity.

Owned candidate capture can itself throw from a provider's reflected copy constructor. The shared capture boundary translates that failure to InvalidData before Apply enters storage/live/application stages, retaining the working object and local history for retry. The regression uses a settings object containing a long heap-backed string and a copy constructor that rejects a particular edited working value; this exercises partial subobject unwinding rather than only trivial values.

That path exposed a necessary local Any::Create cleanup bug: its temporary Any held type information before placement copy completed, so normal unwinding could invoke the complete-object destructor on unconstructed storage. The subsequent `simplify-ui-foundation-boundaries` review keeps raw allocation in an RAII owner and publishes type/storage only after successful construction. A placement-copy exception naturally unwinds through raw-storage release to the existing capture boundary, without a local catch or complete-object destruction. C++ constructor unwinding already destroys constructed subobjects; the Any wrapper must not destroy them again. Its signature and failure contract are unchanged, and generic JSON archive/loader code is not modified. This remains a bounded Framework correction; the original portable test counts below retain their checkpoint meaning.

Session generation and the optional isCurrent predicate stop an old operation after owner destruction, reentrant Begin/End/replacement, or conflicting registration. A callback cannot clear a newly opened replacement session's history by returning into the previous Apply. Busy/reentrant operations return an explicit result instead of nesting publication into the same service unsafely. Provider metadata/objects must still outlive borrowed calls; this is not DLL unload safety or multithreaded mutation support.

The isCurrent guard is itself arbitrary callback code. A false return, exception, or guard-triggered close/reopen/destruction/edit yields Conflict; returning true cannot authorize continued borrowed access if state changed during that guard. Check session/owner validity both before and after it.

CommandService exposes a monotonically changing GetRevision that advances for mutation inside transactions as well as ordinary history operations. Apply snapshots this revision alongside session generation; stack counts alone cannot detect a callback edit added to a pending transaction. An edit made by validation/storage/application/current-guard callbacks invalidates the old candidate snapshot, stops subsequent publication/clearing and preserves the newer working/history state, with conservative stage flags for anything already entered.

Only successful publication reaches the old history Clear, and that Clear itself notifies observers. Its one intrinsic revision increment is expected: the final success check accepts only the captured revision plus one. If a Clear observer or final guard creates another edit/history/transaction, the old operation returns Conflict without clearing that new work again. A replacement session's history likewise remains untouched. The existing dialog retains a shared local session/persistence during notification-capable calls and guards subsequent member access with weak owner/generation state.

### Keep dialog integration optional and generic

PreferencesDialog may receive a shared PreferencesPersistence service, retain its last structured result, and render generic failure text through existing theme/skin/container UI. It does not switch on page type or embed a native path/serializer. Failed Apply or OK leaves the dialog/session available for correction/retry, suppresses the legacy success callback, and does not complete an OK close. Any display of partial failure must preserve the structured effect information.

The legacy onApplied callback remains a success-only notification for existing callers. Its void return cannot represent persistence failure and is not presented as the new checked storage interface. Without an injected service, existing callers retain their compatibility path plus the new page validation/application protocol. Success callbacks may still close/reopen/replace/destroy the dialog under the established lifetime guards.

A portable test page may implement application by updating Shell/Theme through the owning test callback and asserting the existing theme revision invalidation. This does not wire actual SandboxModule settings or exercise a native display/device. The session, persistence model and public EditorCore headers remain independent of UI/renderer classes.

## Risks / Trade-offs

- [Malformed JSON reaches asserting archive code] -> Use a checked codec over owned candidates; reject shape/range/type/unsupported schemas before publication without modifying Framework archives.
- [A failed Apply silently establishes a clean baseline] -> Clear history and issue success notification only after every required stage succeeds; preserve detailed failure/effect results.
- [Save failure changes live settings] -> Keep validation/read/serialization/Write ahead of publication and require failed Write to preserve prior bytes.
- [Later failure is mistaken for rollback] -> Report accumulated partial effects and document retry-safe callbacks and Cancel's discard-only behavior.
- [Absent plugins or new object keys are lost] -> Merge known fields into a fresh stored document and retain unknown object/page keys; replace known arrays deliberately.
- [Registry or session changes during callbacks] -> Copy callable/value state, reacquire live identities, and recheck registration/session/owner generation after callbacks.
- [A new codec becomes an unbounded framework migration] -> Limit supported reflected preference schemas explicitly; return UnsupportedType for the rest and leave custom/native integration separate.

## Validation Strategy

- Round-trip built-in/plugin-style reflected pages through injected storage, including strings, nested reflected values, enums and known sequence replacement/empty arrays; verify provider defaults for missing data and unknown-key/absent-plugin preservation.
- Exercise NotFound, read/write failure preserving old bytes, invalid JSON/root/page/member shape, type/class mismatch, numeric range/nonfinite values and unsupported schemas. Assert no live/application effects before successful candidate preparation and storage gate.
- Exercise validator rejection, failed/throwing setter/application, post-save identity conflict and application retry. Verify result code/context/conservative stage-effect flags, old bytes on failed/throwing Write, history/working preservation, success-only clearing and Cancel after partial effects. Verify readable/writable metadata boundaries without filtering on isStatic.
- Exercise callback removal/reorder/re-registration, owner destruction and session replacement during validation/storage/publication/history notifications. Preserve replacement history and the previous stable working-value lifetime contract.
- Exercise guards that return false, throw or close the session during the check. Assert Conflict and no subsequent access to stale state. Edit working values from storage/application callbacks, including an open transaction whose undo count stays unchanged. Retain the existing Clear-observer session/history replacement regressions; verify revision-based rejection and preservation of newer working/history.
- Inject a throwing copy of a nontrivial heap-backed working settings value. Verify InvalidData with no stage-effect flags, unchanged live/storage/history, safe incomplete-object cleanup under ASan, and successful retry after the throwing condition is removed.
- Verify Save's explicit owned clones survive caller release from later callbacks, copy failure uses the safe result path, and a reflected nonconst-only getter is read through the supported owned-candidate path.
- Test optional dialog injection, generic error/result access, failed OK staying open, retry success, legacy bool/onApplied compatibility and local/global history isolation. Optional Shell/Theme tests verify only portable notifications.
- Root runs affected targeted portable normal/ASan suites and strict OpenSpec/whitespace/scope checks, recording actual results. Windows/native display/GPU and actual SandboxModule adapter/wiring remain pending; no full renderer/native build is requested.

## Rollout / Remaining Work

The envelope remains compatible with existing page-id/classId/elements documents. This change ships a portable service and opt-in dialog path, not an assertion that the existing native host now persists arbitrary contributed pages. Native storage replacement implementation, SandboxModule Load/Save/Apply wiring, Windows and real settings/display application remain separately gated. T11-A/T11-B and other prior artifacts retain their own evidence. Commit/archive require explicit user confirmation.
