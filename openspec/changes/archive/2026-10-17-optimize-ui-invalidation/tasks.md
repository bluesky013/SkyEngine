## 1. Runtime invalidation and incremental entries

- [x] 1.1 Separate upward desired-size, arrangement-only, and paint invalidation; always invalidate generations/cache and Layout parent-input keys on repeated size mutations.
- [x] 1.2 Add conservative layout volatility, live subtree counts, policy toggles, and correct attach/remove/clear accounting.
- [x] 1.3 Add bounded four-entry normalized-constraint measurement caching only for nonvolatile branches while preserving query state invariants.
- [x] 1.4 Add LayoutIfNeeded/ArrangeIfNeeded and nested incremental traversal scope; preserve external forced Layout/Arrange behavior and migrate UIContext's stable root.
- [x] 1.5 Assign final bounds without reverse measurement invalidation, keep render transforms paint-only, and preserve callback-generated invalidation across both Layout's measurement stage and ArrangeChildren.

## 2. Native widget policy and notification audit

- [x] 2.1 Opt audited native controls/containers into stable layout while retaining default volatility for custom/editor consumers.
- [x] 2.2 Classify native setters by desired-size, arrangement-only, or paint effects; clear existing rows and invalidate layout when replacing the list factory, while scrolling uses arrangement-only invalidation.
- [x] 2.3 Keep localized Text volatile while its translation key is nonempty and restore the controlled policy when the key is cleared.
- [x] 2.4 Keep Paint producing complete visible draw output on every call and document the compatibility boundary in the layout/chrome plan.

## 3. Behavior and work-reuse regressions

- [x] 3.1 Cover unchanged incremental passes, isolated dirty branches, changed rectangles, nested containers, explicit forced-call compatibility, and old-bounds Arrange after size invalidation using hook counters and actual bounds.
- [x] 3.2 Cover normalized cache keys, bounded capacity, generation invalidation while already dirty, and unchanged query bounds/tree/scroll/dirty state.
- [x] 3.3 Cover volatile legacy output, localized Text, policy toggles, detach/clear/reattach accounting, and stable siblings beside volatile content.
- [x] 3.4 Cover scrolling without measurement invalidation, late/replaced list factories, and custom measurement/arrangement callback or tree mutation invalidation surviving into a settling pass.
- [x] 3.5 Cover transform-only changes and complete repeated paint output after layout becomes clean.

## 4. Validation and review

- [x] 4.1 Configure/build relevant UI/runtime/editor consumers and run the focused core/text/editor/widgets/shell regression suites.
- [x] 4.2 Run relevant rendering regressions and dependency/layering checks; record actual skipped or unavailable platform/GPU verification.
- [x] 4.3 Run strict OpenSpec validation for optimize-ui-invalidation and whitespace checks; record final verification without committing or archiving.


## Verification record (2026-10-16, macOS)

- Configured the existing Ninja Release `build` and built UICoreTest, UITextTest, UIEditorTest, EditorWidgetsTest, EditorShellTest, EditorRender, Aurora.Render.Editor, SandboxModule, Content.Views, and SandboxEditor. Header/vtable consumers were rebuilt together; the final test-only callback regression was rebuilt and rerun afterward.
- UICoreTest: 142 passed, including 20 new UIInvalidationTest cases. UITextTest: 16 passed. UIEditorTest: 3 passed. EditorWidgetsTest: 14 passed. EditorShellTest: 37 passed. UIShaderTest: 6 passed, 3 DXIL cases skipped on this platform. Total: 218 passed, 3 skipped.
- Counter/geometry regressions prove zero measurement/arrangement hooks on repeated unchanged controlled frames, AUTO ancestor resizing with stable sibling reuse, resize propagation without recursive predirtying, bounded normalized cache reuse/eviction, repeated invalidation while already dirty, scrolling with cached measurement, and forced/default-volatile compatibility.
- Mutation regressions cover invalidation inside both measurement and arrangement, a child added during arrangement followed by a settling pass, and an explicit Arrange consuming a flag without reviving an obsolete parent-input key. Late/replaced list factories create rows only during arrangement. Locale changes update keyed Text without another widget setter.
- Repeated Paint still emits complete draw data; transform/opacity changes keep layout reusable and update emitted geometry. This change does not retain draw lists or claim frame-time/GPU benchmarks.
- Reviewed implementation and CMake boundaries at this checkpoint: new state/algorithms lived in `plugins/ui/core`, root engine/ui data and pure functions were unchanged, and UI linked only UI.Interface, Core, and rapidjson. Native UI core public-header backend/editor exclusion checks passed during configuration. Existing custom/editor hosts remain conservatively volatile; native-derived custom classes with unnotified dependencies must restore volatility explicitly.
- Strict OpenSpec validation and `git diff --check` passed. Native interactive GPU/window validation and Win32/VS2022 were not run. Existing third-party macOS deployment-target/duplicate-library warnings remain.
- No commit or OpenSpec archive was performed. The four preceding uncommitted UI changes remain intact; commit/archive require explicit user confirmation.

Placement update (2026-10-17): those runtime algorithms now live in `engine/ui/core` under the user's UI foundation exception, with the same separate `UI.Interface`/runtime/editor/backend target directions. The historical counts/builds above used the earlier plugin placement; relocation/review verification is recorded separately in `simplify-ui-foundation-boundaries` and canonical §19.
