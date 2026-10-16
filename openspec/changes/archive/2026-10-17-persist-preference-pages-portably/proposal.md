## Why

Preferences sessions and container composition now work portably, but persistence remains a host-only callback that serializes three built-in page structs. Contributed reflected pages need a generic load/save and application protocol with observable failures, injectable storage, and clear session/history behavior when only part of a multi-stage Apply succeeds. These contracts can be implemented and tested without touching the refactoring native renderer or its host wiring.

## What Changes

- Add UI-free generic persistence for registered reflected preference pages, keyed by stable page id in the host-compatible `{pageId: {classId, elements}}` document format. Use a checked codec over existing Any/reflection metadata without invoking assertion-based archive decoding.
- Define explicit load/save/application results and injectable storage. Load prepares owned candidates before publishing; Save merges known fields into a fresh read while preserving unknown object keys and absent plugin pages, replacing known arrays.
- Add optional pure candidate-validation and retry-safe page-application callbacks at the owning page/service boundary, independent of the generic reflected field UI.
- Make Apply run candidate validation, optional Save and guarded live writes/page application before its guarded history-clear boundary. New callback edits invalidate completion without erasing that new work. Pre-save failures preserve live values/history; later failures report conservative storage/live/application effect flags without unsafe rollback.
- Let PreferencesDialog receive a shared persistence service and expose structured results plus generic failure presentation while retaining the legacy bool Apply and success notification paths.
- Preserve dynamic registration identity/reconciliation, stable session working objects, reentrant callback/lifetime guards, and editor-wide history isolation.
- Make Any::Create discard only raw storage after a throwing placement copy, so candidate-copy exceptions become safe InvalidData results without destroying an incompletely constructed settings object.
- Test portable plugin-style page persistence/application and failure/retry behavior; optional Shell/Theme notification coverage may verify portable UI-scale dependency handling.

## Capabilities

### New Capabilities

- `editor-preference-persistence`: generic registered-page persistence, injectable storage, explicit result and page-application protocols, and documented partial-effect boundaries.

### Modified Capabilities

- `editor-preferences`: result-aware session Apply/history/baseline and opt-in portable persistence/application integration while retaining existing reflected/session/container behavior.

## Impact

- EditorCore preference registration/session/services and portable tests, with bounded EditorShell/Theme integration only where needed to expose or verify the protocol.
- One necessary local Framework Any::Create exception-cleanup correction preserves its signature; generic JSON archive loading is unchanged.
- The protocol does not promise an atomic transaction across arbitrary reflected setters, storage I/O, or application callbacks. A failed storage Write must preserve that adapter's previous bytes; successful storage/live/application effects from a later-stage failure are reported and are not rolled back by Cancel.
- Actual SandboxModule Load/Save/Apply wiring, native application/display/DPI/GPU validation, RDG/RHI/renderer/surface work, Aurora adaptors, and Core Event remain deferred. Windows acceptance stays open.
- T11-A/T11-B and prior changes retain their own artifacts/evidence. No generic field-ordering correction, new business-specific settings UI, commit, or OpenSpec archive is part of this scope.
