## 1. Extract the shared widget framework

- [x] 1.1 Add `EditorWidgets` after EditorUi and before Shell with no Shell/render/platform dependency; preserve the Core/Ui lower-layer boundaries.
- [x] 1.2 Move the fourteen UiSkin/form/field-widget/chrome groups, including StatusBar, into `engine/sandbox/widgets` with flat `editor/widgets/...` headers.
- [x] 1.3 Migrate all repository includes and target dependencies without forwarding headers; keep modal/dock/selection-driven host composition in Shell.
- [x] 1.4 Update editor framework/standards documentation to show widget ownership, form-local extension APIs, and the new dependency graph.

## 2. Correct shared defaults and form ownership

- [x] 2.1 Put default command, property metadata, and change-notifier objects in Environment-backed shared storage while retaining local service construction.
- [x] 2.2 Move the default theme and catalog override into shared storage; retain stable theme references and default catalog fallback.
- [x] 2.3 Add explicit ReflectedFormContext injection and delegate the existing constructor to process-shared defaults; route actual form operations through the selected context.
- [x] 2.4 Replace the global stateful widget registry with each form's owned instances and `Widgets()` extension access; preserve custom metadata and initialize defaults safely.
- [x] 2.5 Reset edit/drag/popup/capture/field references before Bind/Refresh rebuild, preserving extension registrations and deferred-notification behavior.
- [x] 2.6 Cancel live previews before Undo/Redo so interaction reset cannot overwrite the command's restored value.
- [x] 2.7 Retain shared ownership of active widgets, inline edit callbacks, and callback dispatch targets, including a widget replaced in the form's registry before its interaction completes.
- [x] 2.8 Reuse the immutable color wheel through reserved `WHEEL_TEXTURE_ID`/`RegisterTextureAs`, with renderer shutdown owning cleanup and no per-picker GPU release.

## 3. Integrate a feature editor

- [x] 3.1 Link Aurora.Render.Editor to EditorWidgets without EditorShell and use shared UiSkin/UiTheme in MaterialEditorView rendering and sizing.
- [x] 3.2 Preserve the shader-driven row/schema and source-save behavior; document the dynamic-schema rationale and deferred form/undo unification.

## 4. Verify behavior and module boundaries

- [x] 4.1 Add Widgets-only tests for injected form context, editing/undo, local custom widgets, and reusable action/chrome behavior.
- [x] 4.2 Add regressions for two-form state isolation and Bind/Refresh clearing old edit/popup/drag references.
- [x] 4.3 Add and run a real dynamic-module regression verifying shared defaults, metadata/undo/notification use, theme updates, and catalog injection across separately linked framework copies.
- [x] 4.4 Reconfigure/build EditorCoreTest, EditorWidgets tests, EditorShellTest, Aurora.Render.Editor, EditorRender, and SandboxModule; run applicable tests and record platform limitations.
- [x] 4.5 Verify UI-free/headless Core and editor-disabled target gating, inspect forbidden Shell includes/links in Widgets and Aurora editor, and run strict OpenSpec validation for this change.
- [x] 4.6 Add and run regressions proving Undo/Redo during a live color preview preserves the command's final value and history state.
- [x] 4.7 Add and run regressions for replacing an active or inline-editing widget, continuing dispatch, and safely releasing its retained interaction state.
- [x] 4.8 Add and run a fake-registry regression proving repeated picker instances reuse the wheel's reserved id without allocating or releasing a texture for every form.


## Verification record

- Configured the existing macOS Ninja/Release `build` directory successfully with `cmake -S . -B build`.
- Built `EditorCoreTest`, `EditorWidgetsTest`, both Widgets probe dylibs, `EditorShellTest`, `Aurora.Render.Editor`, `EditorRender`, and `SandboxModule`. The final increment after the inline-callback ownership fix also passed.
- Ran `ctest --test-dir build -R '^(EditorCoreTest|EditorWidgetsTest|EditorShellTest)$' --output-on-failure`: all 153 cases passed (104 Core, 36 Shell, 13 Widgets). The Widgets suite includes a real two-dylib test; no dynamic-module regression was skipped. After strengthening the picker-destruction assertion, rebuilt and reran all 13 Widgets cases successfully.
- Widgets regressions cover injected context/history, form-local registration and popup state, Bind/destruction preview cancellation, synchronous Refresh during commit, deferred external notifications, metadata preservation, preview Undo/Redo ordering, active and inline-editing widget replacement, and wheel reuse/reset/registry-switch/destruction behavior.
- Reconfigured `/private/tmp/sky-ui-headless-build` with its existing Sandbox=OFF, Test=ON, FreeType=OFF settings. Its generated Core/CoreTest/EditorCoreTest links contain no UI dependency, and it generates no EditorUi, EditorWidgets, EditorShellTest, WidgetsTest, or probe targets. This was a configuration/dependency-graph check, not a separate full headless rebuild.
- Inspected generated regular-build links: WidgetsTest, ProbeA/B, and Aurora.Render.Editor link EditorWidgets without EditorShell. All fourteen header/source groups moved; no old control includes remain in source. EditorCore sources/headers stay UI-free; EditorUi remains Core + UI.
- Debug syntax checks for changed core/theme/widget sources and form-widget tests passed. Strict OpenSpec validation passed for this change and the preceding `refactor-ui-foundations`; `git diff --check` passed.
- Build logs contain existing Release `SKY_ASSERT` unused-value, duplicate-static-library, and third-party macOS deployment-version warnings. No build or test errors remain.
- Platform limits: validation ran on macOS with Ninja/Release. Windows/VS 2022 compilation and interactive GPU/window visual checks were not run here.
- No commit or OpenSpec archive was performed. Both changes remain available together in the working tree for review.
