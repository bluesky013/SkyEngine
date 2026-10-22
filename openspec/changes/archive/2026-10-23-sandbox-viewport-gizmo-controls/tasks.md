## 1. Core controls and drawing contract

- [x] 1.1 Add editor camera math/navigation and reflected bounded control settings to EditorCore.
- [x] 1.2 Add injectable gizmo visual provider, owned geometry, default handles and geometry-based hit testing.
- [x] 1.3 Implement constrained translate/rotate/scale, relative snap and invalid-transform handling with headless tests.

## 2. World editing lifecycle

- [x] 2.1 Bind unique SelectionService Actor identity to a preview/commit/cancel transform session.
- [x] 2.2 Integrate exact before/after commands, dirty/Inspector notifications and cancel-before-history/save/context changes.
- [x] 2.3 Test cancel, one-step undo/redo, parented transforms, invalidation and save/reopen.

## 3. Viewport and controls

- [x] 3.1 Replace the passive scene image with a focusable interactive viewport using UI capture and customizable overlay drawing.
- [x] 3.2 Route main/floating surfaces consistently; isolate text/modal/menu/navigation/tool input and clear state on focus loss.
- [x] 3.3 Feed editor camera snapshots to the existing renderer in Edit; preserve Play camera behavior and same-frame overlay alignment.
- [x] 3.4 Register unified tool/space/snap/focus actions, SVG icons, toolbar speed controls and reflected control settings.
- [x] 3.5 Add shell/action/custom-provider regressions including drawing/hit consistency, DPI and resize/cancel.

## 4. Build and acceptance

- [x] 4.1 Close SandboxEditor, configure cmake-build-debug and build all affected targets including SandboxModule.
- [x] 4.2 Run relevant EditorCoreTest, EditorShellTest, EditorWidgetsTest, EditorRenderTest and SandboxProjectTest regressions.
- [x] 4.3 Verify native viewport navigation, all transform modes, toolbar speed, customization seam, undo/save and floating viewport behavior; record backend evidence and limitations.
- [x] 4.4 Validate OpenSpec, inspect diff/rules and document controls plus actual test results. Leave commit/archive for explicit user confirmation.

Actual native observations, automated coverage and remaining platform/input verification limits are recorded in [verification.md](verification.md).

## 5. Review follow-up

- [x] 5.1 Preserve the initiating button's UI/native capture across mixed button input; cover main/floating routes and independent pointers.
- [x] 5.2 Preserve minimum rotation snap commits and exact undo/redo while retaining no-op gestures.
- [x] 5.3 Normalize pitched world-space flight and close filled geometry hit contours independently of the polyline flag.
- [x] 5.4 Rebuild affected targets, run regression suites, review boundaries and record the follow-up results.

## 6. Navigation latency follow-up

- [x] 6.1 Clip Gizmo triangles before snapshot serialization; cover full camera turns, transformed clips and offscreen geometry.
- [x] 6.2 Bound editor frames before snapshot production using completion acknowledgements without dropping commands or blocking input.
- [x] 6.3 Remove redundant Vulkan acquire configuration enumeration while preserving post-acquire validation and native retirement.
- [x] 6.4 Refresh action chrome on produced frames and icon changes rather than every busy host tick.
- [x] 6.5 Rebuild, run regression/native checks and repeat the same camera-motion latency measurement; remove temporary probes.
