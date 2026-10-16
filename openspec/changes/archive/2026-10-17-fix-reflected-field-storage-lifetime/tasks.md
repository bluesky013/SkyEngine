## 1. Stable construction

- [x] 1.1 Sort reflection metadata before constructing descriptor-backed PropertyField values.
- [x] 1.2 Reserve known field/child counts, emplace in final storage, and bind parent links during recursive struct/sequence construction.
- [x] 1.3 Remove temporary built-field moves and post-build field sorting/parent repair; preserve default capture and existing edit/generation logic.

## 2. Regression and verification

- [x] 2.1 Add inline-size/multilevel/sibling-order regressions asserting descriptors, defaults, and parent links.
- [x] 2.2 Cover nested edit/reset/Undo and a multilevel 23-element sequence's read/add/remove/Undo behavior.
- [x] 2.3 Run independent syntax checks for ReflectedForm.cpp and ReflectedFormTest.cpp.
- [x] 2.4 Run normal EditorCoreTest and EditorWidgetsTest, including the original nested expansion test.
- [x] 2.5 Run those regressions under ASan and record the actual result.
- [x] 2.6 Validate this change strictly and check whitespace. Do not commit/archive.

Independent Clang `-fsyntax-only` checks passed for both changed C++ translation units. Shared
normal/ASan execution was coordinated by the parent task; runtime results are recorded below.
Strict OpenSpec validation and `git diff --check` passed.

Integration verification (2026-10-16): EditorCoreTest 117 and EditorWidgetsTest 28 passed
normally and under AddressSanitizer (`ASAN_OPTIONS=detect_leaks=0`). This includes all three
new field-storage cases and the original NestedExpansionAndResizeUseArrangedRowsBeforeAnyPaintRefresh
regression that exposed the stack-use-after-scope. The headless full build and EditorCoreTest 117
also passed. Address-safety is covered; macOS LeakSanitizer is unavailable.
