## 1. Container composition and theme inputs

- [x] 1.1 Compose Preferences category/page/footer allocation with existing shared containers and explicit logical sizes, retaining the modal/session host.
- [x] 1.2 Replace independent CategoryRect/PageRect/ButtonRect placement formulas with compatibility queries over arranged bounds and clips.
- [x] 1.3 Derive panel/header/footer/category/action dimensions and gaps from UiTheme; add only necessary scaled metrics or small editor presentation/routing adapters.
- [x] 1.4 Constrain the panel/category allocation and adapt footer configuration for smaller surfaces without negative extents or overlapping active targets.

## 2. Independent scrolling and pre-paint correctness

- [x] 2.1 Add a bounded category scrolling region with container-owned rows; preserve the reflected form's independent content scrolling and fixed footer.
- [x] 2.2 Route wheel/selection/input by arranged visible viewport, rejecting clipped rows while preserving reflected popup priority, pointer-id capture isolation, cancellation forwarding and subtree-input cleanup.
- [x] 2.3 Synchronize safe pending transitions, page registration, form data, and theme before allocation; refresh copied container inputs after resize/reopen/theme changes.
- [x] 2.4 Keep paint free of layout/tree repair and preserve the current session/history, Apply/Cancel/Reset, callback lifetime, and form-before-service destruction order.
- [x] 2.5 Paint the category scrollbar through UiSkin's explicitly reserved-track contract with bounded theme-scaled thumb geometry.

## 3. Portable regression coverage

- [x] 3.1 Cover first arrangement/input before Paint, explicit logical size/fractional origin, and consistent query/paint/hit bounds.
- [x] 3.2 Cover small/degenerate windows, resize back to normal, long category labels, footer adaptation, finite extents, containment and clipped hit rejection.
- [x] 3.3 Cover independent category/form scrolling, selecting late categories, fixed footer, active popup/capture routing, foreign-pointer isolation and matching cancellation followed by fresh capture.
- [x] 3.4 Cover page switching, registry reorder/removal/replacement, reopen, same-scale metrics and scale changes before first paint.
- [x] 3.5 Rerun actual session/portable-shell behavior for local history isolation, cross-page Undo/Redo, Apply/OK/Cancel/Reset, deferred transitions and callback destruction/reopen.
- [x] 3.6 Cover Paint consuming a prepared composition without registration/layout/tree repair and shared UiSkin reserved-track scrollbar draw data at 0.5x/1x/2x, tiny tracks and fitting content.

## 4. Verification and scope review

- [x] 4.1 Configure/build affected portable CPU targets and run focused normal tests; root records actual targets/counts/results.
- [x] 4.2 Run the changed session/input/layout lifetime paths under the available ASan profile and record the actual sanitizer configuration/results.
- [x] 4.3 Review module boundaries, absence of duplicate per-control geometry, theme scaling, and unchanged excluded renderer/module/event scope.
- [x] 4.4 Run strict OpenSpec validation and tracked/untracked whitespace checks; root records verification without committing or archiving.

Windows/native display/DPI/GPU acceptance remains pending in the shared backlog. T11-C persistence/application and other dialog migrations are outside this change. The portable implementation acceptance above is complete; platform/display gates remain separate.


Verification (2026-10-17, macOS): targeted normal and AddressSanitizer builds/tests passed for
EditorCoreTest 117, EditorWidgetsTest 32, EditorShellTest 99, UICoreTest 185, UITextTest 25 and
UIEditorTest 3: **461 tests per profile**. New coverage comprises 13 PreferencesLayoutTest cases
and four EditorScrollbarTest cases. The normal/ASan counts overlap, rather than representing 922
unique scenarios. ASan used `ASAN_OPTIONS=detect_leaks=0`; macOS LeakSanitizer is unavailable.

Both selected-target build dry-runs contained 40 steps and no excluded renderer/RDG/RHI/native
integration targets; final source/test increments rebuilt successfully in both profiles. No full
build, SandboxModule/native editor startup, GPU verification or Windows build was requested.
All 20 active UI foundation/repair changes passed strict OpenSpec validation. The tracked diff and
257 untracked files passed whitespace checks. Content hashes of 264 excluded-scope files remain
unchanged, including existing renderer/RDG/RHI/module/Core Event and UI interface contracts.

Paint consumes a prepared tree; layout/input/geometry queries perform reconciliation. Regression
covers fractional origins, exact logical allocations at a large origin, category and form scrolling,
clipped labels/hits, popup priority and surface-bound painting, pointer cancellation/isolation,
4/2/1-column footer adaptation, same-scale/DPI metric changes, degenerate surfaces and recovery,
and the existing local-session history/Apply/Cancel callback paths.

PreferencesDialog's layout changed. Rebuild EditorShell and consuming editor modules together
when native integration resumes after the separate refactor. Windows/native/cross-display/GPU
acceptance remains in docs/editor/ui-layout-and-chrome-plan.md §12.3 and §16. The independent
ReflectedForm ORDER-metadata finding is recorded there and remains open; this change does not
alter generic form ordering or add persistence/application wiring. No commit or archive performed.
