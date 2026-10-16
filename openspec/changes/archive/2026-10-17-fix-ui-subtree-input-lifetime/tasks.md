## 1. Attachment-scoped tree/input lifetime

- [x] 1.1 Track context membership, weak element lifetime and attachment invalidation on subtree detach/reattach.
- [x] 1.2 Register/unregister all routers with their context; permit context-first/router-first teardown.
- [x] 1.3 Clear only affected focus/hover/per-pointer and direct-child capture before cancellation; deduplicate actual target/pointer pairs and preserve silent Reset.
- [x] 1.4 Extract RemoveChild/ClearChildren ownership before callbacks and preserve callback-added children; settle pending old cancellation before callback-driven transfer establishes a fresh attachment.

## 2. Callback-safe routing

- [x] 2.1 Guard pointer/key/text bubbling and hover transitions with element and attachment validity after every user callback.
- [x] 2.2 Guard native direct-child routing, capture release and WantsPointerCapture callbacks; retain single capture while ignoring unrelated pointer events/cancellation without changing geometry, clipping or z order.
- [x] 2.3 Preserve MoveChild interaction state, reject detached/foreign focus, and require fresh input after same/cross-context reattachment.

## 3. Regression and verification

- [x] 3.1 Add portable router regressions for exact cancellation, unaffected siblings/multiple pointers, clear/reparent/transfer, multiple routers and destruction order.
- [x] 3.2 Cover destructive pointer/key/text/hover/cancel callbacks, old-attachment bubbling and real Button press cancellation.
- [x] 3.3 Add native direct-container capture and destructive direct-routing regressions.
- [x] 3.4 Run affected portable CPU suites normally and under ASan; record actual counts and limitations.
- [x] 3.5 Validate the change strictly and check whitespace; verify excluded renderer/RDG/RHI files remain untouched by this batch. Do not commit/archive.

Verification (2026-10-16, macOS): 35 new router/direct-container lifecycle regressions pass.
Targeted normal and AddressSanitizer builds/tests pass for EditorCoreTest 117, EditorWidgetsTest 28,
EditorShellTest 86, UICoreTest 185, UITextTest 25 and UIEditorTest 3: **444 per profile**. These are
matching normal/ASan runs, not 888 unique scenarios. ASan uses `ASAN_OPTIONS=detect_leaks=0`;
macOS LeakSanitizer is unavailable. Pending-cancellation transfer covers both clearing the old
Button press and preserving a fresh DOWN after transfer. Singular-capture cancellation stops when
the callback detaches the target, even if a live old ancestor remains.

All 19 active UI foundation/repair changes passed strict OpenSpec validation. Tracked diff and all
250 untracked files passed whitespace checks. Content hashes of the 255 excluded-scope files are
unchanged. Both selected-target build dry-runs avoided the excluded renderer/RDG/RHI/native targets;
no full/default, renderer/GPU/native-window or Windows build/acceptance was requested.

Windows remains recorded in docs/editor/ui-layout-and-chrome-plan.md §12.3. UIElement's layout/
virtual contract and the UIContext/UIEventRouter layouts changed; rebuild all consuming plugins/editor modules
together when integration resumes. No commit or OpenSpec archive was performed. The next portable
scope is T11-B Preferences container composition; T09-A1 does not close Core Event, native ownership,
renderer resource lifetime or scope/plugin-unload tasks.
