## 1. Fractional wrapping

- [x] 1.1 Preserve an explicit logical allocation through Layout/Arrange and their incremental overloads; rect-only entries continue to use actual endpoint sizes.
- [x] 1.2 Propagate logical parent/content/child sizes through native containers and make Text paint use the same pure deflated budget as measurement, without origin-dependent tolerance.
- [x] 1.3 Cover 175 fractional origin/padding/width cases, close narrower controls, zero width, exact slots, identical endpoints with different allocation keys, and nested container propagation.

## 2. Explicit native subclass migration

- [x] 2.1 Mark all nine migrated native legacy wrappers final; preserve UIElement's default one-way legacy bridge.
- [x] 2.2 Document the BREAKING native-subclass migration to OnMeasure, plus volatility/notification responsibilities and rejected opaque bridge alternatives.
- [x] 2.3 Cover supported legacy and native subclass old/new query behavior, hard clamps, arrangement, cache reuse, and invalidation.

## 3. Verification

- [x] 3.1 Compile the legacy native override negative probe and confirm a final-override diagnostic.
- [x] 3.2 Run focused UI/runtime/text and relevant editor regressions through the shared integration build.
- [x] 3.3 Validate this change strictly and record exact successful/blocked checks.

Verification (2026-10-16): `OPENSPEC_TELEMETRY=0 openspec validate fix-ui-measurement-consistency --strict`
and `git diff --check` passed. An isolated Clang C++17 syntax probe of `LegacyLabel : Text`
overriding only Measure failed as intended with `declaration of 'Measure' overrides a 'final' function`.
Runtime integration results are recorded below under task 3.2.

Commit and archive require user confirmation.

Integration verification (2026-10-16): UICoreTest 150 and UITextTest 25 passed normally and under
AddressSanitizer (`detect_leaks=0`), including fractional allocation, all native container paths,
legacy/native customization, and the force-reflow wrappers. UIEditorTest 3 and EditorWidgetsTest
28 also passed normally/ASan. Full macOS and isolated Sandbox=OFF/FreeType=OFF builds passed;
the latter's UICoreTest 150/UITextTest 25/UIEditorTest 3 passed. Task 3.2 is complete.
