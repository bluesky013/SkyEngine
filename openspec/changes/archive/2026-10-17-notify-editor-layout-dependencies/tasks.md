## 1. Reflected form layout dependencies

- [x] 1.1 Compute preferred/content extent while preparing rows so Bind/Refresh and structural changes are correct before Paint.
- [x] 1.2 Notify layout for header, expansion, sequence, and per-form widget registration/replacement changes; retain explicit Refresh/volatility for unnotified internal custom-widget state and classify scroll versus appearance-only invalidation.
- [x] 1.3 Add safe public pre-layout synchronization with injected property/command subscriptions, pending refresh across interactions, and destruction cleanup.
- [x] 1.4 Wire explicit controlled policy for audited host-owned forms while retaining conservative standalone/derived extension behavior.
- [x] 1.5 Exclude indexed sequence elements from container IsSequence classification and use valueType TypeInfo for their descriptor type so scalar form rows retain element type/value/editor semantics and cannot construct container-add commands.

## 2. Document and viewer hosts

- [x] 2.1 Add explicit WorldConfigPanel document binding/refresh and child synchronization; retain volatile provider compatibility.
- [x] 2.2 Migrate AssetViewer's owned composition to notified incremental layout, synchronizing before skip decisions and inside executing arrangement for standalone forced-call compatibility; notify open/reload/resize/scroll changes and update copied chrome metrics through a live-child DockPanel slot update and menu bar height.
- [x] 2.3 Preserve unknown custom-provider volatility and actual-width measurement/arrangement; avoid declaring external content stable from the host.
- [x] 2.4 Synchronize known hosts and use incremental allocation from main/floating shell paths; propagate explicit document updates and same-pointer refresh notifications to all applicable instances.

## 3. Shared theme invalidation

- [x] 3.1 Add a cross-DLL shared default-theme revision that advances for same-scale setter updates while preserving the theme value's stable address.
- [x] 3.2 Apply revision changes before layout by invalidating every live node in each affected main/floating surface after selecting its effective DPI theme; synchronize entire adopted/transferred subtrees even when the destination revision is current, and update copied main MenuBar height.

## 4. Regression coverage

- [x] 4.1 Cover first-frame preferred height, structural/header/registry changes, and host allocation before any preliminary Paint.
- [x] 4.2 Cover property changes/external undo-redo before layout, deferred refresh during interaction, and notifier/command callback cleanup.
- [x] 4.3 Cover controlled unchanged-frame work reuse, viewer reopen/reload/resize/scroll and forced-arrangement synchronization, and default-volatile provider compatibility.
- [x] 4.4 Cover explicit document switch/same-address refresh through panel and shell notifications, legacy provider changes without notification, and applicable floating host integration.
- [x] 4.5 Cover shared theme revision across DLLs, same-scale nested/chrome metric changes, main/floating surface revision/DPI behavior, and hide/theme-change/show reattachment.
- [x] 4.6 Cover actual sequence add/remove/undo UI with Float child editors and indexed scalar descriptor element type/value/container-command behavior in EditorCoreTest.

## 5. Validation and review

- [x] 5.1 Configure/build affected editor/runtime consumers and run relevant EditorCoreTest and UI core/text/editor/widgets/shell/render regression suites.
- [x] 5.2 Review notification ownership, callback lifetime, conservative extension behavior, and module boundaries; record actual skipped/unavailable platform/GPU checks.
- [x] 5.3 Run strict OpenSpec validation and whitespace checks, and record verification without committing or archiving.


## Verification record (2026-10-16, macOS)

- Configured the existing Ninja Release build and built EditorCoreTest, UICoreTest, UITextTest, UIEditorTest, EditorWidgetsTest, EditorShellTest, EditorRender, Aurora.Render.Editor, SandboxModule, Content.Views, and SandboxEditor. The final confirmation build completed successfully with no pending work.
- EditorCoreTest: 104 passed. UICoreTest: 142 passed. UITextTest: 16 passed. UIEditorTest: 3 passed. EditorWidgetsTest: 21 passed. EditorShellTest: 48 passed. UIShaderTest: 6 passed, 3 DXIL cases skipped on this platform. Total: 340 passed, 3 skipped.
- Seven added reflected-form regressions cover pre-paint preferred height and row geometry, structural/header/widget replacement notifications, actual sequence add/remove/undo/redo, deferred active edits, injected services, and destruction during notifier/command callback snapshots. Existing cross-DLL probes now verify a shared theme revision as well as stable theme storage.
- Ten added dependency/theme regressions cover explicit and legacy world-document binding, current-document dirty ownership, same-address reload through panel/shell, attached/hidden/floating notification, clean-frame hook reuse, nested same-scale theme cache invalidation, hidden subtree reattachment, and alternating 1x/2x/1.5x surface DPI. Direct main Layout after floating paint restores the main scale.
- Viewer regressions cover idle measurement/arrangement reuse, actual scrollbar-column measurement, outer scrolling, window and panel resizing, same-scale toolbar/menu metric updates, same-bounds reopen/reload before Paint, and conservative custom-provider layout. DockPanel live-child slot updates keep copied toolbar allocation current.
- A real sequence bug found by the UI regression was corrected in the UI-independent property model: indexed scalar descriptors expose their element value type and do not identify as the original container. Core and widget regressions check readable float values, editor kind, rejected container-add commands, and real UI add/remove/undo behavior.
- Reviewed callback ownership and weak lifetime guards, safe synchronization outside active interactions, conservative unknown/derived/provider policies, live-tree theme invalidation, and statically linked cross-DLL singleton state. No new UI backend dependency was introduced. At this checkpoint runtime DockPanel allocation lived in `plugins/ui/core`, while root engine/ui contained contracts/pure utilities and editor services stayed in their existing Sandbox layers.
- Strict OpenSpec validation and git diff whitespace checks passed. Win32/VS2022 and native interactive GPU/window visual validation were not run. Existing third-party macOS deployment-target and duplicate-library warnings remain.
- No commit or OpenSpec archive was performed. The preceding five change artifacts and main specs were not modified; implementation builds on the existing uncommitted UI work. Commit/archive still require explicit user confirmation.

Placement update (2026-10-17): DockPanel and the other runtime implementations now live in `engine/ui/core` under the user's UI foundation exception; editor services retain their existing Sandbox layers. The counts/builds above belong to the earlier plugin layout. Current relocation/review verification is recorded separately in `simplify-ui-foundation-boundaries` and canonical §19.
