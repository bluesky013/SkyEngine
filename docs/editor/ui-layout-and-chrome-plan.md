# Editor UI — Layout & Chrome Plan

Goal: converge the editor UI on one architecture so future work is **incremental**, not a
big-bang refactor. Companion to `docs/editor/ui-standards.md` (the rules) and
`ui-sizing-and-scaling.md` (theme/sizing), `editor-toolbar.md`, `editor-framework-design.md`.

**Current scope constraint (2026-10-16): RDG and RHI are being refactored separately. All new UI
work must avoid those implementations, public contracts and renderer integration seams. §13
overrides earlier suggestions about renderer/spec/cook work and defines the temporary safe scope.**

## 1. Current baseline

- `refactor-ui-foundations` introduces a common parent-driven `UIElement::Arrange(finalRect)`
  entry. Slot containers finish nested child layout before paint or hit-testing; the migrated
  asset viewer, preferences, world config, and material editor use that entry too. T11's frozen
  four-screen outer composition is complete in §18; other screens retain their separate inventory.
- `extract-editor-widgets` places skins, forms, field widgets, and chrome below Shell in
  `EditorWidgets`. Main-window and asset-viewer actions, icons, menus, and accelerators now
  use their shared registries/builders; feature editors can reuse the controls without Shell.
- `unify-ui-geometry` aligns transformed paint, clipping, and pointer coordinates while
  preserving absolute layout rectangles. `constrain-ui-measurement` carries explicit budgets
  through containers, scroll extents, wrapped Text, and AssetViewer custom content.
  `optimize-ui-invalidation` adds opt-in incremental layout and bounded desired-size reuse while
  retaining conservative behavior for legacy/editor dependencies. `notify-editor-layout-dependencies`
  adds explicit pre-layout synchronization for audited editor forms/document/viewer hosts and
  revision-driven invalidation for shared theme changes. Geometry caching and broader hand-layout
  migration remain later work.

## 2. Target architecture (layers, bottom-up)

```
Theme        UiTheme (UiColors/UiMetrics/UiFonts) — single source of dims+colors
Layout       Layout containers: StackPanel(H/V), Grid, Dock, Overlay + slot model
Widgets      Button, Field, Label, Checkbox, Slider, Tabs, ... painted by UiSkin
Icons        IconRegistry (name -> UITextureId) + UiSkin/UiDraw icon draw helper
Chrome       EditorActionRegistry (single source) -> ToolBar / MenuBar / shortcuts
             + ActionBarBuilder (EditorAction -> ToolBar::Item / MenuBar::Item)
Views        Editor views/dialogs = containers + widgets + chrome + ReflectedFormView
             Edits flow through CommandService (undoable)
```

### 2.1 Layout and geometry contracts

Two-pass layout in the render-independent UI runtime:

- `UIElement::MeasureDesired(UIConstraints)` normalizes min/max budgets, calls protected
  `OnMeasure`, and hard-clamps its outer desired size to a finite result. New implementations
  deflate own padding before content measurement and add padding back once.
- Direct UIElement-derived legacy widgets retain the one-way default `Measure(float&, float&)`
  bridge. The nine migrated Text/container classes make their old wrapper `final`; subclasses
  override protected `OnMeasure(UIConstraints)` instead. This is an explicit source-breaking
  migration: an old override now fails at compile time rather than silently diverging between
  direct and constrained measurement. Custom mutable dependencies still need notification or volatility.
- `MeasureForLayout` resolves known Fixed/Fill/Percent/anchor axes before querying
  width-dependent content. Unbounded Fill/Percent fall back to finite intrinsic measurement.
- `UIElement::Arrange(const UIRect&)` accepts the parent's final surface-space rect and
  recursively arranges descendants through `ArrangeChildren`.
- Layout/Arrange and their incremental entries also accept an explicit logical `UISize`.
  Containers propagate this allocation separately from positioned rectangle endpoints, and
  `GetArrangedSize`/`GetContentSize` preserve the same deflated budget through text paint. Rect-only
  calls deliberately use endpoint dimensions; custom hosts needing exact slot widths pass both.
  Layout/arrangement skip keys include logical allocation, even when rounded endpoints are identical;
  desired-size entries still use normalized measurement constraints and generation.
- `UIElement::Layout(parentRect)` uses the same resolved rule budget before calling `Arrange`.
  Ordinary Fixed requests can overflow parent available size; a parent-owned tight slot instead
  measures through `MeasureDesired` and arranges the final rectangle without applying child rules again.
- Container elements implement both and position children; leaves just report size.
- **Slot model** (per child): `Width/Height = Auto | Fill(weight) | Px` and `Margin`.
  Parents own placement; alignment/min/max extensions remain a subsequent layout decision.

Containers available: `StackPanel` (orientation H/V, gap, padding, Auto/Fixed/Fill),
`GridPanel`, `DockPanel`, and `Overlay`. Slot metadata follows the actual child list when
children are removed, cleared, or reordered. Paint consumes arranged bounds and does not
perform layout.

HBox/VBox and slot containers carry available content budgets; Fixed/Auto consumption and
spacing are reserved before nonnegative remaining Fill allocation. ScrollView exposes Both
(default), Horizontal, Vertical, and None axes. AUTO content measures full extent on scrolling
axes, while non-scrolling axes have hard viewport maxima. Content Fixed/Percent/Fill rules
resolve once against the viewport, and the arranged rectangle, stored extent, and scroll clamp
agree. ListView measurement reports full fixed-item extent without creating rows; only its
arrangement materializes the visible range.

Text has default-off byte-wise soft wrap. Measurement and paint use one line-layout algorithm
at the effective padded content width, with per-line alignment; `text.wrap` uses the same
property path from JSON and C++. Existing UiDraw text calls remain no-wrap. AssetViewer
measures custom content at its actual column width after scrollbar reservation. Measurement
queries do not change bounds, scroll state, element membership, or dirty flags.

The retained tree and absolute `GetBounds`/`Arrange` coordinates stay. `UIGeometry` reports
bounds plus the accumulated layout-to-rendered-surface affine transform on demand, including
the context surface base by default; an optional compositor transform premultiplies that result.
The context surface transform is configured before paint/input; the compatibility explicit-root Paint
overload stores the same transform used by input. Each pointer receiver gets its own inverse
mapping into absolute layout coordinates, without subtracting its bounds origin. Clips and
quad culling use transformed surface AABBs; rotated clipping remains conservative scissor
clipping. Rounded-shape shader distance calculations use interpolated layout coordinates.
Direct child routing consumes parent-layout input and checks ClipsChildren ancestor AABBs;
the caller/host or UIEventRouter enforces the actual viewport. Detached root bounds are not
an implicit viewport for that helper.
The geometry contract does not imply geometry caching. Incremental measurement/arrangement is
an explicit runtime policy layered on the unchanged geometry and constraint contracts:

- `MarkLayoutDirty` invalidates desired-size entries and layout/paint toward ancestors;
  `MarkArrangeDirty` requests arrangement/paint without discarding measured size, for example
  when scrolling. Neither recursively marks all descendants. Changed final rectangles flow
  downward through actual arrangement.
- `LayoutIfNeeded` / `ArrangeIfNeeded` skip only clean branches with unchanged parent/final
  allocation inputs and no volatile descendant. `UIContext::Layout` uses this path; child
  `Layout` / `Arrange` calls inside an incremental pass participate in it. Existing external
  `Layout` / `Arrange` calls continue to force layout for compatibility.
- `UIElement` defaults to layout-volatile. Audited native widgets explicitly opt out through
  `SetLayoutVolatile(false)`, and ancestors track volatile subtree membership. Unknown custom
  widgets, editor polls, and external theme-dependent hosts remain conservative. Localized Text
  stays volatile while its key is nonempty, so external locale/table changes remain observable.
- Only branches with no volatile node cache desired sizes, with at most four normalized
  constraint/generation entries. Queries still do not mutate bounds/tree/scroll/dirty state,
  and a new size mutation invalidates entries even while flags are already dirty.
- Arrangement assigns geometry without invalidating its own ancestors' measurements and
  retains invalidation raised during callbacks for a later pass. List row membership may need
  a settling pass. Render transforms affect paint rather than layout.
- Every paint call still emits the complete visible draw list. Clean flags are not a retained
  render cache, and this work makes no GPU draw-count reduction claim.

Audited editor hosts extend that runtime contract with explicit dependency synchronization:

- `ReflectedFormView` computes content/preferred extent while preparing current rows, so Bind,
  expansion, sequence edits, header changes, and widget registration/replacement have correct height
  before any Paint. Those changes notify layout; painting is not a prerequisite for host sizing.
  Each registry invalidates only its owning form; changes inside an already-registered custom
  widget's unnotified row-count state still require Refresh or volatility.
  Indexed sequence descriptors use the element valueType and retain element semantics: scalar
  children select their scalar editor rather than their owning container or Unknown, so sequence
  controls and row extent match add/remove/undo behavior.
- Forms subscribe to injected property/command changes and retain pending refresh rather than
  rebuild fields inside notification callbacks. `SynchronizeLayout` consumes safe pending work
  before host measurement/allocation. Active text/drag/popup interactions defer refresh without
  dropping it; destruction removes service subscriptions. Arbitrary unnotified object writes
  still require an explicit Refresh.
- Audited host-owned forms explicitly adopt controlled layout after their synchronization is
  wired. Standalone/unknown derived forms, OnViewTick extensions, independently mutable custom
  themes, and custom widget dependencies remain conservative unless their owner notifies changes.
- WorldConfigPanel supports explicit document binding and refresh/rebind notifications for
  changes at the same document address. The shell forwards its document changes to applicable
  main/floating/hidden views; repeating its current document pointer requests a refresh. The
  legacy provider path stays volatile and observes unnotified changes.
- AssetViewer synchronizes forms before extent/scrollbar allocation and notifies open/reload,
  resizing, and scrolling. Its audited owned composition uses incremental allocation; unknown
  custom provider nodes retain their own volatility and can keep that branch conservative.
  Theme synchronization also refreshes copied menu heights and toolbar DockPanel slot extents.
  Shell synchronizes before skip decisions; an executing viewer arrangement also synchronizes
  before extent calculation to preserve standalone forced-Arrange callers.
- The default theme's stable-address value and revision share the existing cross-DLL Singleton.
  Every setter call advances the revision, including same-scale changes. A surface selects its
  DPI theme, then invalidates every live node's layout/cache when that revision differs from the
  one applied to its layout. Root-only invalidation cannot refresh unchanged nested allocations.
  Alternating different-DPI surfaces may conservatively re-layout when the shared theme switches.
  Adopting a preserved or transferred view synchronizes its whole subtree even if the destination
  revision is current, covering views hidden during a theme change. Main MenuBar refreshes its
  stored height from the active theme before allocation.

### 2.2 Chrome model

`EditorActionRegistry` (cross-DLL Singleton) is the declarative source for action identity,
label, icon, group/menu enums, ordering, enabled predicate, scope, accelerator, and invocation.
`ActionBarBuilder` builds toolbar items, menus, and accelerator lookup for the active scope.
Main-window and asset-viewer chrome use it; plugin extensions register through the same host.

`MenuBar::Item` supports icons, shortcut hints, separators, and checkable/checked display. The
delivered action/builder path carries enabled/visible/checkable/checked state and disabled-menu
presentation, with refreshed invocation-time checks. T10-B/UI-12 retains the separate scope and
handler-ownership decisions below.

### 2.3 Icons

`IconRegistry` maps names to `UITextureId`. The host scans extension-declared SVG roots, and
`UiSkin::DrawIcon` provides the shared tint/draw helper. `UISharedTextureRegistry` owns shared CPU
images and replays/updates them in every attached surface registry. Font pages use the atlas's
paired attachment protocol; viewport images remain surface-local. Native icon/wheel validation
remains UI-08's open gate.

## 3. Guiding principles (what prevents future refactors)

1. **Declare intent through containers and slots.** They allocate outer regions/control targets;
   specialized rows/glyphs may draw within their allocated leaf or viewport.
2. **Containers own child placement.** A view never sets a child's bounds except as the root of a
   container subtree.
3. **One source per concern**: actions (registry), icons (IconRegistry), theme (UiTheme),
   editable data (reflection), edits (CommandService).
4. **Dialogs/viewers reuse the same chrome** (`ToolBar`/`MenuBar` + actions), not bespoke rows.
5. **Paint via UiSkin; sizes/colors via UiTheme.** No hard-coded pixels, no ad-hoc draws.
6. **Separate edit from commit** (buffer + Save), and never run heavy work inside event dispatch.
7. **Keep UI foundation implementation in `engine/ui`.** The user's explicit UI exception keeps
   `UI.Interface` contracts and separate runtime/editor/render/FreeType implementation targets in
   that foundation module. It does not require a UI feature plugin or relax layering for other features.

## 4. Implemented milestones

The earlier P0–P7 chrome plan delivered shared rules, containers, icons, action builders,
scoped accelerators, and plugin extension registration. Its P5 screen migration was partial:
AssetViewer top chrome used DockPanel and MaterialEditor rows used GridPanel. PreferencesDialog
subsequently completed composition in §16, and the frozen four-screen inventory completed in §18.
T10-A delivered action-state propagation in §14; remaining scope ownership stays T10-B/UI-12.

The subsequent six changes establish the following baseline. Their checked tasks describe the
implemented scope and recorded tests; §9 records the subsequent repairs and remaining gates.

| Change | Delivered behavior |
| --- | --- |
| `refactor-ui-foundations` | Interface/runtime/adaptor/editor target separation; parent-owned Arrange. |
| `extract-editor-widgets` | Shared EditorWidgets below Shell; form-owned interactions and injected services; common feature-editor controls. |
| `unify-ui-geometry` | Shared affine paint/input/clip contract with layout-space shape coordinates. |
| `constrain-ui-measurement` | Constraint budgets through containers/scroll/list; optional text wrapping; actual-width custom-content measurement. |
| `optimize-ui-invalidation` | Upward invalidation, bounded desired-size reuse, incremental allocation with conservative volatility. |
| `notify-editor-layout-dependencies` | Pre-layout form/document synchronization and full-tree shared-theme revision invalidation. |

See `openspec/changes/<change>/tasks.md` for the individual verification records. Geometry/render
caching, Unicode shaping, and the remaining screen migrations were not delivered by these changes.

## 5. Migration compatibility

- Keep `SetBounds` as a geometry setter during migration. Hosts assigning a final rectangle
  to a composed view call `Arrange`, so descendants are ready before input and paint.
- Keep existing external `Layout` / `Arrange` as forced entries while a view polls external
  state. Adopt `LayoutIfNeeded` / `ArrangeIfNeeded` and opt out of volatility only after all
  layout dependencies have notification paths. Known editor hosts synchronize their forms/documents
  before incremental allocation, while legacy providers and unknown derived/custom content retain
  volatility. Shared default-theme changes invalidate each affected surface tree by revision;
  localized text and independently mutable custom themes remain conservative.
- Migrate a frozen screen scope at a time. Containers allocate its outer regions/control slots;
  compatibility `*Rect()` queries read arranged leaves, and specialized row/glyph drawing remains
  within its owned allocation.
- Keep `UiTheme`/`UiSkin` as the only style source throughout — no parallel styling.
- Shared editor controls now use flat `editor/widgets/<Name>.h` headers from `EditorWidgets`.
  The old `editor/shell/...` control headers are removed; consumers update includes and link
  `EditorWidgets` explicitly instead of depending on Shell.

## 6. Code placement

- UI contracts, UIGeometry, UISize/UIConstraints, and referenced data/pure affine/layout utilities:
  root `engine/ui` (`UI.Interface`). This target contains only interfaces/data/pure utilities; the full
  UI foundation also contains implementation subtargets under the explicit exception in §3/§19.
- Layout + widgets + runtime document loading: `engine/ui/core` (`UI`; `ui/widgets/*`).
- Aurora renderer / FreeType provider: `engine/ui/{render,freetype}`
  (`UIRender` / optional `UI.FreeType`); runtime UI does not link either adaptor.
- Headless UI document editing: `engine/ui/editor` (`UI.Editor`), separate from runtime.
- Theme/painter primitives: `engine/sandbox/ui` (`EditorUi`; `UiTheme`, `UiDraw`), with only
  Core/UI dependencies. The default theme is shared across DLLs.
- Shared component skin, forms, property controls, and chrome: `engine/sandbox/widgets`
  (`EditorWidgets`; `editor/widgets/UiSkin.h`, `ReflectedFormView.h`, `ToolBar.h`, `MenuBar.h`,
  `ActionBarBuilder.h`, and related controls). This editor framework depends on `EditorUi`,
  `EditorCore`, `UI`, Core, and the required Framework reflection interfaces, never Shell/Aurora.
- Actions/icons: `engine/sandbox/core` (`EditorActionRegistry`), icons in `resources/icons`.
- Views and window/docking/modal composition: `engine/sandbox/shell`; asset views via
  `IAssetViewProvider` in their owning feature editor modules. Both use `EditorWidgets`;
  feature editor modules do not link Shell.

Forms own their `Widgets()` registry and popup/drag/text-edit state. `ReflectedFormContext`
injects host-owned command history, property metadata, change notification, and theme when a
document needs its own context. The default constructor uses shared editor services and the
shared default theme, without sharing interaction state between forms.

Placement update (2026-10-17): UI runtime, renderer, optional FreeType and headless editor sources
have returned from the earlier plugin layout to these four `engine/ui` subdirectories. Target names,
public `ui/...` includes and dependency separation remain. Current source references use the new
paths; historical verification counts in this plan retain their original execution context. The
relocation and latest review receive separate verification in §19.

## 7. Open decisions

- Constraint representation is now UISize/UIConstraints with normalized min/max bounds,
  an optional unbounded maximum, and centrally enforced finite desired size. Slot alignment/
  min/max property extensions remain a separate layout decision.
- Measurement reuse and invalidation now have an opt-in runtime contract. Migrating editor
  form/command/document dependencies has begun through explicit pre-layout synchronization;
  remaining custom providers, extension ticks, font/localization state, and arbitrary object writes
  still require notification or volatility. Shared default-theme revision changes invalidate whole
  live surface trees. Geometry/render caches, exact rotated polygon clipping, full Unicode shaping,
  and per-surface theme ownership remain deferred.
- Shared editor widgets/forms are below Shell in `EditorWidgets`; runtime UI remains independent
  of Sandbox. Document hosts can adopt explicit `ReflectedFormContext` ownership incrementally.
- Menu depth: keep 2 levels or generalize to N (affects `MenuBar`).

## 8. Scoped action contract

`EditorAction.scope` and `accelerator`, `EditorScopeStack`, and `ActionBarBuilder` are implemented.
The main shell and asset viewer use the active scope's toolbar/menu/shortcut declarations.
The viewer registers `assetViewer.*` actions; its former local menu and shortcut switch are gone.
The host clears the action registry before extension registration so plugin actions survive.

AssetViewer owns a fresh command session for each Open/close/switch boundary. All Undo/Redo
entries run the CommandService before-history callbacks before inspecting/traversing the stacks;
forms cancel previews there, including empty-stack requests. Clear generations prevent an executing
old-session command from being reinserted after a synchronous observer replaces the session.
Shell-owned asset actions use weak owner lifetimes so retained registry callbacks become inert
after destruction. Nested scope ownership and complete action presentation remain UI-12.

## 9. Reviewed backlog (2026-10-16)

This is the canonical follow-up list for the six uncommitted UI changes and their consumers.
See §10 for executable task boundaries, dependencies, and completion gates.
Review covered runtime layout/text, editor binding/history, surface resources, target dependencies,
and the related documentation. No new reverse engine/plugin or core/editor dependency was found
in the reviewed UI targets. Existing checked change tasks remain historical scope/verification
records; they do not close the issues below. No commit or OpenSpec archive was performed.

The findings below preserve the original review evidence. **Resolution** paragraphs describe the
subsequent implementation; unchecked resource issues retain their real GPU acceptance gate.

Priority: **P1** = fix before considering this stack ready; **P2** = correctness/contract follow-up;
**P3** = later design or feature work. “New” means introduced by this six-change stack; “existing”
means pre-existing behavior retained through the migration. Static findings are distinguished
from executed reproductions.

### 9.1 Correctness and lifetime fixes

- [x] **UI-01 · P1 · existing · detach font registries on surface destruction.**
  `UIFontAtlas::AddRegistry` retains raw pointers; `GetGlyph` updates all of them.
  `EditorRenderer::DestroySurface` and its closed-window loop erase a renderer without detaching it.
  A standalone ASan reproduction registered a second registry, cached A, destroyed that registry,
  then requested B: `heap-use-after-free` in `UIFontAtlas::GetGlyph`.
  Add a registration token or paired removal and detach before destruction. Acceptance: close/reopen
  a floating window, request new glyphs and new atlas pages in the main window, and verify only
  live registries receive calls. Sources: `engine/ui/core/src/text/UIFontAtlas.cpp` and
  `engine/sandbox/render/src/EditorRenderer.cpp`.
  **Resolution:** paired idempotent removal in UIFontAtlas/UITextSystem, detach before every
  renderer destruction, clear a removed primary, and replay current page pixels on reattachment.
  Persistent fake-registry tests include destroying the detached backend before glyph/page growth;
  normal and ASan UITextTest pass. Native window/glyph appearance remains UI-V3.

- [ ] **UI-02 · P1 · existing · retire floating-surface GPU resources safely.**
  The closed-surface loop calls UIRenderer::Shutdown/erase before `FrameContext::BeginFrame` waits
  for the preceding submission; explicit DestroySurface also releases immediately. UIRenderer
  clears GPU buffers/textures, and only global EditorRenderer::Shutdown currently calls WaitIdle.
  Static path confirmed; native GPU reproduction was not run. Add fence-based retirement or an
  appropriate safe wait before releasing surface resources. Acceptance: close a window with
  outstanding submissions under GPU validation, including explicit and OS-close paths.
  Sources: `engine/sandbox/render/src/EditorRenderer.cpp` and
  `engine/ui/render/src/UIRenderer.cpp`.
  **Resolution:** DestroySurface/OS-close now request closure and detach CPU bindings; frame-external
  retirement waits for the device before release. Reentrant frame shutdown is deferred. Per-window
  event proxies avoid multi-key listener detachment errors. EditorFrameSubmission always signals
  the reset frame fence, including minimized/acquire-failure frames, and balances successful acquires.
  CPU tests cover these seams; pending-submit native teardown remains required.

- [x] **UI-03 · P1 · new · distinguish live-model cancellation from abandoning invalid bindings.**
  Bind/destruction now reset interactions. ColorPicker::Close can invoke a preview rollback through
  a PropertyField descriptor to the previous model. WorldConfigPanel::RefreshDocument allows
  replacement of config storage before rebinding, so an active drag can write to released storage.
  AssetViewer's model members are also destroyed before its base-owned child forms. Static paths
  confirmed. Define a pre-replacement detach/cancel protocol and an abandon path that never calls
  the invalid model; ensure child cleanup precedes model destruction. Acceptance: active color
  preview during same-address document reload, model replacement, and viewer destruction under
  ASan; ordinary cancellation still restores a live model. Sources:
  `engine/sandbox/widgets/src/{ReflectedFormView,ColorPicker,ColorReflectedWidget}.cpp`,
  `engine/sandbox/shell/src/{WorldConfigPanel,AssetViewerWidget}.cpp`.
  **Resolution:** cancel restores a live model; abandon drops callbacks without touching invalid
  storage. Binding generations guard retained callbacks. Viewer/WorldConfig hosts follow explicit
  replacement/destruction ordering, and committing Viewer edits defer synchronous observer-driven
  replacement until the widget event returns. See `fix-editor-binding-lifetime` and T04.

- [x] **UI-04 · P1 · existing · make asset history own stable targets and persistence.**
  AssetViewer's edit callback persists settings then Reload replaces the settings Any. Commands
  retain a raw target pointer; settings larger than Any's 24-byte inline storage leave history
  targeting released memory. Even with stable inline storage, Undo only runs the setter and does
  not rerun ApplyCookSettings, so cook.json/Save can retain the edited value. Static paths confirmed.
  Resolve commands through stable asset/target/property identity or another owned target protocol;
  make Do/Undo update the persisted model, and give document close/switch an explicit history
  lifetime. Acceptance: heap-backed settings edit→reload→undo/redo, cook.json assertions, Save/Cook
  after Undo, and close/switch with populated history. Sources:
  `engine/sandbox/shell/src/AssetViewerWidget.cpp`,
  `engine/sandbox/core/src/{property/ReflectedForm,command/PropertyEditCommand}.cpp`.
  **Resolution:** viewer-local sessions and asset-domain commands carry UUID/target/schema plus
  sparse override snapshots. Do/Undo/Redo persist and notify through the same catalog path;
  reload reconstructs disposable settings. Missing/schema/target invalidation and close/switch clear
  history. Clear generations prevent old in-flight commands from re-entering the replacement session.

- [x] **UI-05 · P2 · existing integration gap · cancel previews before every history entry point.**
  Shell accelerator/menu/toolbar actions call CommandService directly, bypassing ReflectedFormView's
  cancel-before-Undo logic. For committed A→B and preview C, Undo restores A but later cancel can
  restore B and silently overwrite Undo. The current widget regression calls OnKeyEvent directly
  and does not cover the shell route. Coordinate active edit sessions at the shared history boundary.
  Acceptance: shortcut, menu, toolbar, and external-service Undo/Redo all cancel the preview first,
  preserve the command cursor, and leave no delayed rollback. Sources:
  `engine/sandbox/shell/src/EditorShell.cpp`, `engine/sandbox/module/src/SandboxModule.cpp`,
  `engine/sandbox/widgets/src/ReflectedFormView.cpp`.
  **Resolution:** ordered before-history subscribers run at CommandService Undo/Redo, including
  empty requests. Forms cancel there; recursive traversal is rejected, removed subscriptions are
  skipped, and local key handling checks binding lifetime after dispatch. Actual Shell shortcut,
  menu, toolbar, direct-service, and asset-session regressions cover the integrated routes.

- [x] **UI-06 · P2 · new · stabilize fractional-width text wrapping.**
  Standalone reproduction: glyph advance 6.1, text AB, Fixed width 12.2, offsetLeft 500.1.
  Measurement reports 12.2×10; arranged edge subtraction yields width 12.19998169 and paint emits
  two lines (second glyph y=10). Width-only epsilon in UITextLayout does not cover error introduced
  by the absolute origin. Share a stable effective width or another consistent line-layout policy.
  Acceptance: fractional advance/origin cases agree in measure and emitted glyph positions, while
  genuinely narrower widths still wrap. Source: `engine/ui/core/src/text/UITextLayout.cpp` and
  `engine/ui/core/src/widgets/Text.cpp`.
  **Resolution:** logical allocation is propagated independently of positioned endpoints through
  all eight native containers, measurement, and text paint. Tests cover 175 fractional origin/padding/
  width combinations, genuinely narrower controls, nested containers, and equal rounded rectangles
  with distinct logical-size layout skip keys. Normal and ASan runtime/text suites pass.

- [x] **UI-07 · P2 · new public-API compatibility gap · define legacy subclass measurement.**
  A Text subclass overriding only legacy Measure to return 99×88 still returns that directly,
  but MeasureDesired/Layout use inherited Text::OnMeasure (6.1×10 in the standalone probe).
  Migrated containers have the same dispatch boundary. No production repository subclass was
  found; the broad compatibility promise still exceeds the implemented bridge. Either preserve
  these overrides or explicitly approve/document the migration requirement. Acceptance: cover
  direct UIElement-derived legacy widgets and legacy overrides below migrated built-ins, with
  consistent old/new entry behavior for the supported contract. Sources:
  `engine/ui/core/src/{UIElement.cpp,widgets/Text.cpp}` and
  `openspec/changes/constrain-ui-measurement/design.md`.
  **Resolution:** direct UIElement legacy overrides remain supported. The nine migrated native
  wrappers are final; their subclasses must override OnMeasure. This source-breaking requirement
  is documented in `fix-ui-measurement-consistency`; a negative compile probe rejects the old
  override, and supported direct/native old/new paths have persistent runtime tests.

### 9.2 Resource and architecture follow-ups

- [ ] **UI-08 · P2 · existing · mirror shared textures to every surface.** Icons and the immutable
  color wheel currently upload through the main text-system registry; floating renderers receive
  atlas pages and viewport images only. Missing ids fall back to a white texture. Static paths
  confirmed; native visual reproduction was not run. Define initial replay, late registration/update,
  and removal for shared resources. Acceptance: floating toolbar
  icons and wheel render correctly before/after new resources and surface recreation; closing one
  surface preserves others. Coordinate with UI-01/UI-02 before broadening registry ownership.
  **Resolution:** a backend-free CPU catalog replays shared images and propagates updates/removal
  to live registries. Icons, wheel, and the reflection demo use it; viewport ids remain local.
  GPU image/group generations are replaced during upload and retained until collection after
  the inflight=1 frame wait. Rendering handles the full uint32 texture-id range. CPU/ASan replay,
  detach, pixel-update, and wheel-reset regressions pass; native visuals remain pending.

- [ ] **UI-09 · P2 · ownership design.** Define document/tree, surface/input, and device/resource
  owners and their close/transfer/rebind order. Include focus/hover/capture cleanup on subtree
  removal and virtualization. Audit element/texture id allocation domains across statically linked
  modules and multiple text systems before promising globally unique handles. Introduce surface/
  renderer interfaces only where these ownership seams need them; fix UI-01–UI-05 first.
  Integration additionally fixed inline Any field relocation (`fix-reflected-field-storage-lifetime`)
  and retained Shell action callbacks (`fix-editor-action-owner-lifetime`). Nested fields are built
  in reserved final storage after sorting metadata, and expired action owners make copied callbacks
  inert without removing a newer owner's same-id actions. General scope ownership, DLL unload,
  and deletion of another listener during raw-pointer Event snapshots remain future contracts.
  Audit the duplicate `SkyCocoaView`/`SkyCocoaWindowDelegate` Objective-C class definitions observed
  in the host and SandboxModule as a separate platform/DLL ownership investigation. The resolved
  old Bullet ABI crash was independently identified; these warnings do not establish its cause or
  explain the unavailable Metal device.
  **Resolution:** T09-A1 subtree/input lifetime is complete with portable CPU/ASan evidence in §15.
  The remaining ownership, Core Event, module/DLL and renderer decisions retain their separate scope.

- [ ] **UI-10 · P2 · renderer specification decision.** `openspec/specs/ui-render/spec.md` requires
  PipelinePass/RDG/DrawItem/RgBlockDesc and double/triple-frame resources. Current UIRenderer uses
  a direct encoder and one VB/IB set; EditorRenderer sets inflight=1. The foundations design records
  this limitation. Choose a separately scoped RDG integration or an explicit specification amendment.
  Before increasing inflight, implement frame-owned buffers and texture retirement and validate them.
  Do not infer a current multi-frame buffer failure from the single-frame implementation alone.

- [x] **UI-11 · P2 session correctness / P3 layout · finish Preferences and frozen screen migration.**
  Preferences data/reflection registration already lives in EditorCore and fields already use
  ReflectedFormView (archived `editor-preferences-reflected`). Remaining work is dialog-local history,
  safe page-identity/type-aware snapshots, extensible persistence/application, and container layout
  for categories/content/footer. The current global history survives snapshot restoration; snapshots
  are indexed raw bytes, while host persistence covers only three built-in pages. These are reviewed
  code-path gaps, not newly executed reproductions. Audit other panels one screen at a time.
  Independently mutable custom providers/widgets/themes retain notifications or volatility until
  their dependencies are observable. Container delivery alone does not complete screen migration.
  **Resolution:** the global-history/raw-snapshot description above preserves the original review
  finding. T11-A is complete in §14 with session-owned working values/history, typed snapshots and
  page registration identity. T11-B container composition is complete with portable evidence in
  §16. T11-C portable persistence/application is complete in §17. The user-authorized T11
  implementation closeout is complete in §18: settings-only host wiring, ORDER metadata and the
  frozen four-screen migration, with 42 new cases and all 533 tests passing per normal/ASan profile.
  Only UI Scale has an immediate consumer; the other eight settings remain explicitly stored-only.
  Windows/native/display/GPU platform acceptance remains open in §12.3/T08.

- [ ] **UI-12 · P2 · action presentation and scope ownership.** MenuBar already paints checkboxes;
  add action-level checked/checkable/visibility and disabled-menu presentation through the builder,
  keeping invocation-time enabled enforcement. Test state refresh, nested dialogs/scopes, and
  removal of handlers whose document/window has closed. Shared EditorMenu/EditorGroup enums and
  data-driven accelerators are already delivered.
  **Resolution:** `unify-editor-action-state` delivers enabled/visible/checkable/checked state,
  copied registration generations, guarded dispatch and in-place Shell/Viewer chrome refresh.
  T10-A CPU/ASan acceptance passes. Scope-instance/registration ownership and DLL unload remain T10-B.

- [ ] **UI-13 · P3 · document definition versus instance.** UIDocument still combines its live tree,
  raw binding targets, and name index. Define serialized schema/stable ids separately from instance
  state before adding reusable document instances or live structural editing.

- [ ] **UI-14 · P3 · quantify theme/layout cost before caching more.** Runtime UITheme and editor
  UiTheme/UiSkin remain separate protocols. Alternating DPI surfaces can switch the shared default
  theme and invalidate whole trees. Measure that cost, then decide per-surface theme ownership.
  Geometry/render caches need explicit invalidation and a measured benefit; current Paint still
  emits the full visible draw list.

- [ ] **UI-15 · P3 · deferred geometry/text scope.** Exact rotated clipping, Unicode shaping/bidi,
  and language-aware line breaking remain separate work. macOS already integrates IME committed text;
  UI preedit/composition presentation, composition-aware editing, and unsupported-platform parity
  still need their own scope. The current wrap contract is byte-wise; retain that boundary until
  the broader text/input design and tests exist.

### 9.3 Validation still required

The pre-repair record reported **340 passed / 3 DXIL skipped** on macOS. Review probes exposed
paths outside that coverage; the integration results below supersede that total for this repair batch.
CPU regressions, address-safety checks, module startup, and real GPU acceptance remain distinct.

- [x] **UI-V1 · CPU lifetime/history:** focused replacement/destruction, real Shell dispatch, and
  combined asset persistence/preview tests passed normally and with ASan. UI-02's real GPU gate is separate.
- [ ] **UI-V2 · platform:** Win32/VS2022 configure/build of affected targets including SandboxModule;
  relevant EditorCore/Widgets/Shell/UI tests and the currently skipped DXIL cases.
- [ ] **UI-V3 · native rendering:** floating close/reopen/re-dock, new glyph/page upload, icons/wheel,
  cross-DPI movement, transformed clipping/input, and pending-submit shutdown with GPU validation.
- [x] **UI-V4 · headless:** the isolated Sandbox=OFF/FreeType=OFF full default build passed,
  followed by EditorCoreTest 117, UICoreTest 150, UITextTest 25, and UIEditorTest 3. Outputs are
  isolated from the regular/ASan trees; this is actual compilation rather than configure-only evidence.
- [ ] **UI-V5 · cook integration:** run a real asset through edit → Undo/Redo → Save/Cook and
  verify the resulting product uses the persisted target settings. Current CPU fixtures prove
  snapshots/cook.json and Save enqueue consistency; they do not validate cooked product output.

Integration record (2026-10-16):

| Check | Executed result |
| --- | --- |
| macOS default build | Full 263-step build passed; final affected editor consumers rebuilt after the last Shell repair. |
| Normal CPU suites | EditorCoreTest 117, EditorWidgetsTest 28, EditorShellTest 59, UICoreTest 150, UITextTest 25, UIEditorTest 3, EditorRenderTest 6, UIShaderTest 6, UITextFreeTypeTest 2: **396 passed / 3 DXIL skipped**. |
| AddressSanitizer | The seven suites excluding UIShaderTest/UITextFreeTypeTest: **388 passed**, `ASAN_OPTIONS=detect_leaks=0`; the isolated ASan configuration has FreeType disabled. macOS LeakSanitizer is unavailable; this records address safety. |
| Headless | Full isolated Sandbox=OFF/FreeType=OFF build and the four suites above: **295 passed**. These are overlapping checks, not additional unique scenarios. |
| Native module startup | First run failed with old Bullet IModule ABI (PostTick jumped into RTTI). After all runtime modules were rebuilt, bounded Metal startup exited 0. Metal device creation failed and no renderer was established: **no GPU/render acceptance claimed**. |
| Spec and whitespace | All six foundation changes plus ten repair changes passed strict OpenSpec validation. Tracked diff and all 232 untracked files passed whitespace checks. No commit/archive performed. |
| Still required | Win32/VS2022 and DXIL; real floating close/reopen/re-dock, pending-submit shutdown, new glyphs/pages, icons/wheel, cross-DPI and transformed clip/input under GPU validation. |

The original nested-field ASan failure was repaired by constructing inline-Any fields in final
storage; the full Widgets suite now passes. The original Shell ASan failure was repaired by weak
action-owner guards, including clearing the old viewer target before rebuilding the same Shell.
These results close CPU gates without closing UI-02/UI-08 or increasing inflight beyond one.

### 9.4 Later feature and gated work

- [ ] Material editor defaults/per-property reset, with a chosen schema format and add/delete for
  material-instance overrides; retain a deliberate transaction/Undo contract for dynamic rows.
- [ ] Asset-view source write-back and re-cook (deferred from archived `editor-asset-views`).
- [ ] Remove legacy `assets/techniques/*.tech` and preload entries now that the legacy render consumers are gone; audit before deletion
  no longer require them, paired with `docs/aurora/shader-design-and-migration.md`.
- [ ] Verify the reported ShaderHeaderTool CWD/PATH/slang-DLL fix after a clean Windows reconfigure.

### 9.5 Documentation reconciliation completed in this review

- [x] Replace contradictory “future/already done” chrome sections and separate container delivery
  from remaining screen migrations.
- [x] Fix the mandatory action example to use current enums and remove deleted menuOrder.
- [x] Document subscriptions/pre-layout synchronization and qualify model/history lifetime guarantees.
- [x] Record the original unsafe floating-close path and its subsequent deferred safe-wait repair,
  keeping real GPU acceptance open.
- [x] Update shared ToolBar include paths and replace the legacy Measure gap with an explicit
  native-subclass OnMeasure migration.


## 10. Task execution plan

T01–T07 now have implementation changes; T02/T07 still require real GPU acceptance. The UI-xx
entries in §9 retain the original review evidence followed by current resolution notes. Each task
includes its own spec/code/tests/documentation. T08 separates executed CPU/build checks from
unavailable platform/visual gates; commit and archive still require user confirmation.

### 10.1 Order and parallel work

| Task | Priority | Scope | Current state |
| --- | --- | --- | --- |
| T01 Font registry lifetime | P1 | UI-01 | Complete; CPU/ASan passed. |
| T02 Floating GPU teardown | P1 | UI-02 | Implemented; real GPU lifecycle gate pending. |
| T03 Binding/preview lifetime | P1 | UI-03 | Complete; CPU/ASan passed. |
| T04 Asset settings history | P1 | UI-04 | Complete; persistence/session and joint preview CPU/ASan passed. |
| T05 History traversal boundary | P2 | UI-05 | Complete; shared-service and actual Shell dispatch CPU/ASan passed. |
| T06 Measurement correctness | P2 | UI-06/UI-07 | Complete; logical allocations and explicit OnMeasure migration tested. |
| T07 Shared surface textures | P2 | UI-08 | Implemented; real floating icon/wheel gate pending. |
| T08 Platform/integration validation | Gate | UI-V1–UI-V5 | CPU/ASan/headless complete; Win32, real GPU and product cook acceptance remain. |

The recommended repair sequence has been implemented: T01/T03/T06, T02, T04/T05, then T07.
Integration also exposed skipped-frame fences, nested field storage, and retained action callbacks;
their repair changes belong to this batch. Next close the remaining T08 platform/GPU gates, then
advance T09/T10 ownership and action-state design before increasing renderer inflight count.

| Additional integration change | State |
| --- | --- |
| `fix-editor-skipped-frame-fence` | Implemented; CPU/ASan order tests pass. Real GPU recovery remains T02/T08. |
| `fix-reflected-field-storage-lifetime` | Complete; nested inline values, defaults, parent chains and edit/reset/Undo pass normally/ASan. |
| `fix-editor-action-owner-lifetime` | Complete; retained callbacks, newer owners, and live Shell rebuild pass normally/ASan. General scope/DLL-unload ownership remains T10. |

### T01 — Font registry lifetime

Change: `fix-ui-font-registry-lifetime`. Owns UI runtime text + EditorRender integration.

- [x] T01.1 Add a paired unregister operation or lifetime-bound registration; define duplicate
  registration, repeated removal, and replay behavior without a GPU/backend dependency in UI core.
- [x] T01.2 Route explicit surface destruction, OS-close, and renderer shutdown through cleanup
  that detaches the registry before destroying its renderer. Preserve the main registry.
- [x] T01.3 Add fake-registry regressions for remove/re-register, new glyphs, and allocation of a
  new atlas page after a secondary registry is destroyed; run them under ASan.
- [x] T01.4 Update the text/surface lifetime contract and record checks. Close UI-01 only when
  removed registries receive no calls and live registries continue to receive pages/updates.

Completion gate: the reproduced atlas UAF is covered by a persistent regression and no longer
occurs. GPU visual verification is additionally tracked in T08, not needed to prove CPU detach.

### T02 — Safe floating-surface GPU teardown

Change: `fix-editor-surface-gpu-teardown`. Owns EditorRender + Aurora UI adaptor.

- [x] T02.1 Define one closing sequence: stop new work for the surface, detach CPU registrations,
  wait for its outstanding GPU work or retire the resources, then release renderer/window state.
  Cover explicit DestroySurface and OS-close through the same path.
- [x] T02.2 Prefer a small safe-wait repair for the current single-frame renderer. If using fence
  retirement, specify who owns each retired resource and which completion signal releases it.
- [x] T02.3 Add focused teardown-order/idempotence coverage and preserve reopen/re-dock behavior.
- [ ] T02.4 Run pending-submit close/reopen and application shutdown with a real GPU and validation
  enabled; record results for every closing entry point and reconcile the renderer documentation.

Completion gate: real GPU lifecycle validation is required to close UI-02. If only code/unit checks
are available, report **implementation complete / GPU validation pending** and leave the task open.
A full RDG migration or increased inflight count belongs to later work.

### T03 — Binding invalidation and preview cancellation

Change: `fix-editor-binding-lifetime`. Owns EditorWidgets + document/viewer hosts.

- [x] T03.1 Distinguish cancel-with-live-model (restore preview) from abandon-invalid-binding
  (drop callbacks without touching model data). Give bindings an explicit invalidation boundary.
- [x] T03.2 Have hosts cancel/detach before replacing a valid model; use abandonment if storage
  has already been replaced. Cover WorldConfig same-address reload and AssetViewer reload/close.
- [x] T03.3 Ensure child forms release interactions while their model/services are still alive;
  retained callbacks must not access an expired binding after rebind or destruction.
- [x] T03.4 Add ASan regressions for active color drag during model replacement, same-address
  config reload, viewer destruction, and ordinary Escape cancellation. Update the binding contract.

Completion gate: invalidated models receive no access; a normal cancellation still restores the
original value. Historical command targets are handled separately by T04.

### T04 — Asset settings history and persistence

Change: `fix-asset-settings-history`. Owns EditorCore asset commands + AssetViewer integration.

- [x] T04.1 Create an explicit asset-edit session/history scope via ReflectedFormContext. Define
  close, switch, reopen, and missing-asset behavior; service/model lifetime must satisfy T03.
- [x] T04.2 Replace settings-Any raw-pointer history with an asset-domain command carrying stable
  asset UUID/target identity and old/new data, preferably target sparse-override snapshots.
  Resolve current state at execution; isolate changes from other targets and define stale-target handling.
- [x] T04.3 Route Do/Undo/Redo through the same target-specific persistence/notification path.
  Reload the view from that state; persistence must not depend solely on SetOnEdited.
- [x] T04.4 Add only the required form command-creation seam in the existing editor framework.
  Keep asset lookup/serialization in the asset domain; CommandService continues to manage history.
- [x] T04.5 Verify settings larger than 24 bytes through edit→Reload→Undo→Redo; assert UI value,
  cook.json, and subsequent Save/Cook agree. Cover multiple targets, asset removal, and close/switch.

Two checkpoints: first stable command targets/session ownership, then persistence consistency.
Both must pass before closing UI-04; merely keeping the old settings object alive is insufficient.

### T05 — One pre-history interaction boundary

Change: `fix-editor-history-preview-order`. Owns CommandService contract + form subscriptions.

- [x] T05.1 Add a minimal UI-independent before-Undo/Redo subscription seam to the command service;
  define callback order, safe removal, and reentrancy without introducing asset-specific behavior.
- [x] T05.2 Forms subscribe to their injected service, end live interactions before traversal, and
  unsubscribe on destruction. Default and document-local histories must use the same ordering.
- [x] T05.3 Verify Shell shortcut, menu, toolbar, and direct external-service calls. With committed
  A→B and active preview C, history traversal must not be overwritten by later Escape/mouse-up/refresh.
- [x] T05.4 Jointly test T04's real asset form, checking final persisted values and history cursor;
  document the common boundary instead of relying on the form-local key path.

Completion gate: every entry point gets cancellation-before-history, with no duplicate rollback,
stale callback, or changed final value after traversal. Can proceed alongside T04 after T03.

### T06 — Measurement correctness and legacy compatibility

Change: `fix-ui-measurement-consistency`. Owns UI runtime layout/text contract.

- [x] T06.1 Turn the advance=6.1, width=12.2, offsetLeft=500.1 reproduction into a permanent test;
  add varied fractional origins and a meaningfully narrower control case.
- [x] T06.2 Make measure/arrange/paint use a consistent effective width or line-layout policy.
  Verify emitted glyph positions and height, not only the desired-size result. Close UI-06 on this evidence.
- [x] T06.3 Specify the supported behavior of legacy Measure overrides below migrated Text/container
  classes. The delivered decision makes native wrappers final and requires OnMeasure overrides;
  the design, headers, and migration example document the source-breaking boundary.
- [x] T06.4 Cover both direct UIElement legacy subclasses and subclasses of migrated built-ins,
  including old/new entry behavior and recursion avoidance. Close UI-07 only for the stated contract.

UI-06 and UI-07 are separate checkpoints; the wrap repair can land without waiting for a larger
compatibility design. Geometry/render caching and Unicode shaping stay outside this repair.

### T07 — Shared textures on every surface

Change: `sync-editor-surface-textures`. Owns shared resource registration + renderer replay.

- [x] T07.1 Define shared versus surface-local texture ownership and id scope. Keep UI core free
  of renderer types; retain local viewport textures while sharing icons/color-wheel/font resources.
- [x] T07.2 Replay existing shared resources when a surface attaches; propagate later registration
  and updates, and detach safely using T01/T02's lifecycle. Define resource removal/recreation behavior.
- [x] T07.3 Add fake-registry tests for initial replay, late resources, update, remove/recreate,
  and closing one registry while others remain live.
- [ ] T07.4 Verify floating toolbar/menu icons and color wheel on a real GPU, including late uploads,
  reopening, and closing one window while another still uses the resources. Update resource docs.

Completion gate: CPU synchronization tests plus native visual/lifecycle validation. UI-08 remains
open if only registry tests pass; do not infer correct rendering from id equality alone.

### T08 — Platform and integration evidence

This is a validation task spanning the implementation changes, not a separate docs/archive change.
Record results in the owning change and keep unavailable checks explicitly pending.

- [x] T08.1 UI-V1: run focused ASan/lifetime and actual Shell history regressions as T01/T03/T04/T05
  are delivered. Complete T04/T05's combined preview/persistence check after their integration.
- [ ] T08.2 UI-V2: on Win32/VS2022, close the editor, configure/build affected targets including
  SandboxModule, run relevant Core/Widgets/Shell/UI regressions, and execute the DXIL cases skipped on macOS.
- [ ] T08.3 UI-V3: perform native close/reopen/re-dock, cross-DPI, transformed clip/input, new
  glyph/page, icon/wheel, and pending-submit teardown checks with validation enabled.
- [x] T08.4 UI-V4: perform a full supported runtime build with Sandbox and FreeType disabled;
  distinguish actual compilation from configure/dependency-only evidence.
- [x] T08.5 Run strict OpenSpec validation and whitespace checks per changed scope; report code,
  CPU tests, platform tests, and GPU checks separately. Historical passing totals do not close new gates.
- [ ] T08.6 UI-V5: with a real asset/builder, verify edited and Undo/Redo target settings through
  Save/Cook to the resulting product, preserving other targets. This is additional integration
  acceptance; it does not reopen T04's completed CPU/domain implementation.

### 10.2 Design and later tasks

These tasks may be refined after the correctness repairs. A design deliverable does not count as
implementation; open the implementation scope only after its decisions and acceptance criteria exist.

| Task | Issues / dependency | How to advance | Completion evidence |
| --- | --- | --- | --- |
| T09 Ownership and renderer contract | UI-09/UI-10; discovery can start now | Write two decisions: document/tree/surface/input/device owners and close/transfer order; then independent encoder versus RDG. Audit handle domains and focus/capture cleanup. Use repair findings as evidence. | Agreed ownership/API map and renderer spec direction, with separately scoped implementation tasks. Frame-owned buffers/retirement and multi-frame GPU validation precede any inflight increase. |
| T10 Action presentation and scopes | UI-12; core presentation can run independently, history actions use T05 | Add checked/checkable/visible and disabled-menu state through registry→builder→widgets; define state refresh and scope/handler removal. | Toolbar/menu/shortcut share enabled behavior; check state updates correctly; nested dialog close leaves no stale scope/handler. |
| T11 Preferences correctness and remaining screens | UI-11; use T03/T05 interaction contract | Preserve the delivered core reflection and generic fields. First isolate Preferences history/safe snapshots, then generalize persistence/application and replace category/content/footer rects with containers. Inventory other screens afterward. | Cancel/Apply/history and plugin-page persistence are correct; existing reflected fields remain shared; dialog composition follows theme/layout contracts. |
| T12 Document definition and instances | UI-13; informed by T09 | Separate serialized definition/stable ids from instance tree, bindings, and name index; migrate one loader/editor consumer. | Multiple instances have independent state; rebuild/reload has defined binding/selection identity and no dangling targets. |
| T13 Theme/layout performance | UI-14; measure after lifetime repairs | Profile alternating-DPI surfaces and clean/dirty frames; set a measured budget, then choose per-surface theme ownership and only justified caches. | Before/after work counts or timings plus theme/DPI invalidation regressions; full visible painting remains correct. |
| T14 Advanced geometry and text | UI-15; text/input design first | Split exact rotated clipping and Unicode/composition into separate scopes. Define shaping/bidi/line-break requirements, then UI composition presentation/editing and platform integration, preserving macOS committed text. | Per-scope geometry or multilingual/IME acceptance cases; explicit platform support and fallback behavior. |

The material defaults/reset, source write-back, legacy technique removal, and ShaderHeaderTool
verification in §9.4 retain their own feature/platform gates. They are not prerequisites for T01–T07.

## 11. Remaining execution queue

This is the consolidated queue after the repair batch. T01/T03–T06 and the field-storage/action-owner
CPU repairs are complete. T02/T07 and skipped-frame-fence have implemented code and passing CPU/ASan
coverage; only their native GPU acceptance remains. Do not recreate those fixes as new development.
T11-A/T10-A are complete with portable evidence in §14. T09-A1 is complete with normal/ASan
integration verification in §15. T11-B composition and normal/ASan verification are complete in §16.
T11-C's portable persistence/application implementation and normal/ASan verification are complete
in §17. T11 implementation closeout is complete in §18: ORDER metadata, real generic host persistence/application
and four frozen outer compositions, under a narrow settings-only SandboxModule exception. Final
normal/ASan/file/source verification passed; T10-B/T13 remain separate later work and native
display/GPU gates stay open.
The suffixes below split the existing T08–T14 scopes without renumbering their parent tasks.

### 11.1 Acceptance for the current batch

| Task | Priority / prerequisite | Work and completion evidence |
| --- | --- | --- |
| T08-A · Win32/VS2022 + DXIL | Platform gate; Windows toolchain and third-party packages | Close the editor, configure and build affected targets including SandboxModule; run Core/Widgets/Shell/UI tests and the three DXIL cases skipped on macOS. Record commands, configuration and results. Covers T08.2; the separate ShaderHeaderTool environment check can share this session. |
| T08-B · Native surface/fence lifecycle | P1 gate; an environment that can create a real GPU device | Validate explicit/OS/preview close, multiple floating windows, pending-submit shutdown, main-window rendering after close, minimize/restore and out-of-date/acquire recovery. Exercise acquired-but-unusable image recovery where reproducible. Require GPU validation results without resource/synchronization errors. Closes T02.4 and skipped-frame-fence 3.3. |
| T08-C · Shared resources and interaction | P2 gate; can share T08-B's environment/session | Verify actual floating icons/wheel, late image registration/update/removal, glyph/page growth, detach/reopen, cross-DPI movement and transformed clip/input. Closing one window must preserve the others. Record visible output and validation results; closes T07.4 and the remaining T08.3 cases. |
| T08-D · Real cooked product | P2 integration; representative asset, builder and output inspection | Verify edit → Undo/Redo → Save/Cook changes the actual product according to current target settings, while preserving other targets. Check both Undo and Redo products. Covers T08.6; CPU snapshot and enqueue tests already pass. |

The current macOS bounded startup exited successfully after stale modules were rebuilt, but Metal
device initialization failed. It supplies no real rendered-frame evidence for T08-B/C. Keep those
gates open until they run in an environment with a usable device.

### 11.2 Next development batch

| Task | Priority / dependency | First deliverable | Completion evidence |
| --- | --- | --- | --- |
| T09-A1 · Subtree detach/input lifetime | Complete; portable CPU/ASan evidence in §15 | End the old tree attachment on RemoveChild/ClearChildren; clear affected router/direct-child state before cancellation and guard destructive callbacks. | 35 new regressions pass in both profiles. Same/cross-context reattachment requires fresh input; unaffected siblings and MoveChild retain their state. |
| T09-A · Remaining ownership/lifecycle decisions | P2; separate bounded scopes | Map remaining document/tree/binding ownership and close/rebind/transfer order. Audit handle domains across DLLs/multiple text systems, raw Event snapshots when another pending listener is deleted, and duplicate Cocoa class ownership. Renderer/device/resource decisions remain deferred under §13. | Explicit supported lifetimes and unregister rules; concrete follow-up changes for gaps. T09-A1 does not close Core Event or native module ownership. |
| T09-B · Renderer specification decision | Deferred during the separate RDG/RHI refactor | After the refactor's contracts stabilize, reconcile the UI renderer/spec with its resource boundaries. Do not select or amend the renderer contract in this UI batch. | A coordinated decision and matching spec/proposal after the temporary exclusion in §13 is lifted. |
| T10-A · Action presentation | Complete; portable CPU/ASan evidence in §14 | Carry checked/checkable/visible and disabled presentation from action registry through builder to toolbar/menu/shortcut dispatch; define refresh notifications. | Consistent state and invocation behavior across all chrome, with state-change regressions. |
| T10-B · Scope and handler ownership | P2; depends on T09-A | Define scope instance identity and precise cleanup for nested dialogs/windows/documents and plugin unload. | Closing an older owner cannot remove a newer scope; no handlers/callback code remain callable after document/window close or DLL unload. Existing weak Shell action guards remain delivered. |
| T11-B · Preferences container composition | Complete; portable normal/ASan evidence in §16 | DockPanel header/body/footer, category ScrollView+VBox, stable reflected form and right-aligned 4/2/1-column footer use shared containers, theme metrics and explicit logical sizes. | 13 Preferences layout/input cases and 4 shared-skin scrollbar cases passed in both profiles; actual display/DPI appearance remains pending. |
| T11-C · Portable persistence/application protocols | Complete; portable normal/ASan evidence in §17 | Generic checked page persistence, injectable storage/results, validation/application callbacks and result-aware session/dialog integration are present. | 30 new Core/Shell cases passed in both profiles; all six suites passed 491 tests per profile. Settings-only host integration is now included in the authorized §18 closeout; actual native display/GPU acceptance stays open. |
| T11 closeout · ORDER, host wiring and frozen four-screen composition | Complete implementation/portable evidence in §18 | Effective ORDER, real file storage/generic host injection/plugin preservation, consistent user×DPI and the four named outer layout migrations are delivered. | 42 new cases; all six suites passed 533 tests in each normal/ASan profile. Settings-source syntax and scope checks passed; native execution/display gates remain distinct and eight settings remain stored-only. |

**T11 implementation closeout** is complete in §18. The next bounded development scopes are
T10-B scope ownership and T13 CPU baselines.
The foundation-placement and exception/reentry review follow-up is complete with portable
verification and final unified whitespace/scope checks in §19.
T11-B's completed portable evidence is recorded in §16; its Windows/native visual gates remain open.
T08-A stays a Windows backlog; T08-B/C need a usable GPU. Although T08-D is offline, its current
real-builder dependencies reach RHI, so it is now deferred under §13. A design task's completion
does not imply its proposed code has shipped.

### 11.3 Later design/features

| Task | Dependency / next step | Completion evidence |
| --- | --- | --- |
| T12 · Document definition and instances | After T09-A, separate serialized schema/stable ids from instance tree, bindings and name index; migrate one loader/editor consumer. | Two independent instances and defined rebuild/reload identity without dangling targets. |
| T13 · Theme/layout performance | Measure clean/dirty layout, full visible painting and alternating-DPI invalidation before deciding per-surface theme ownership or further caches. | Recorded baseline/budget and measured benefit, with invalidation correctness preserved. |
| T14-A · Exact rotated clipping | Open a separate geometry scope with render/input acceptance cases. | Rotated clip boundaries and hit testing agree for the stated geometry contract. |
| T14-B · Unicode text layout | Define shaping/bidi/language-aware line breaking and font fallback requirements before implementation. | Multilingual shaping, wrapping and directionality cases; replace the current byte-wise wrap contract explicitly. |
| T14-C · IME composition | Define preedit/composition rendering, editing/history behavior and platform support. | Composition-aware editing/platform tests. macOS committed-text integration already exists. |

Independent feature pool: material defaults/per-property reset and override add/delete; asset-source
write-back/re-cook; legacy technique removal after all consumers migrate; clean-Windows verification
of ShaderHeaderTool CWD/PATH/slang-DLL behavior. These are separate from T08-D's acceptance of the
existing cook-settings path.

### 11.4 Delivery closeout

- [ ] Attach remaining platform/GPU/cook evidence to the owning tasks and update this queue.
- [ ] Before OpenSpec archive, show the selected changes' status and obtain explicit user approval.
- [ ] Before commit, show the proposed message and file list and obtain explicit user approval.
  Keep each change's implementation and OpenSpec artifacts/main-spec updates together; do not
  create a separate docs/archive-only commit for the implementation stack.

## 12. Current-environment development assessment (2026-10-16)

The current host has the macOS/Xcode build, CPU UI/editor tests, ASan, FreeType, offline image
cook libraries and AssetTool. Metal device initialization failed in the last native run. This
permits implementation and CPU/address-safety evidence; it does not provide rendered-frame,
real cross-display DPI or GPU synchronization acceptance. Windows is recorded below as deferred
verification, not a prerequisite for continuing portable core/editor development.

### 12.1 Work that can proceed now

This table updates the original assessment without discarding its findings. T11-A/T10-A are complete
with verification in §14; T09-A1 is complete with verification in §15. T11-B's completed composition
and normal/ASan evidence are recorded in §16. T11-C's portable protocols and normal/ASan verification
are complete in §17. The completed T11 implementation closeout in §18 adds ORDER, file/host wiring and four named
outer compositions while retaining the settings-only exception and protected renderer scope in §13.

| Recommended order | Task / scope size | Current code evidence and proposed first slice | Local completion / remaining platform gate |
| --- | --- | --- | --- |
| Complete in §14 | T11-A · Preferences session correctness · medium | The original gap was shared global history plus indexed byte snapshots. Delivered dialog-owned working values/history, typed snapshots and page id/type/registration identity, with Open/Apply/Cancel/Close baselines. The prior reflection migration remains intact. | CPU/ASan passed for isolation, Apply/Cancel/reopen, cross-page edits, reorder/removal and nontrivial values. Full plugin-unload behavior still uses T09/T10 ownership decisions. |
| Complete in §14 | T10-A · Unified action presentation · medium | The original gap was enabled-only actions and inconsistent host refresh. Delivered shared visible/checkable/checked/enabled state, current-registration invocation checks and Shell/Viewer refresh while preserving open menus for ordinary state changes. | Portable Core/Widgets/Shell tests, CPU draw data and ASan passed for semantics, refresh and callback lifetime. Actual visual quality remains native acceptance. |
| Complete in §15 | T09-A1 · Subtree detach/input lifetime · medium | The original gap was missing detach notification and raw router references. Delivered attachment-scoped weak references, precise subtree cleanup, multiple-router/context lifetime handling and callback-safe direct/router dispatch. This resolves the scoped contract gap; it does not claim a newly observed production crash. | 35 new cases passed normally and under ASan for removal, clear, unaffected targets, same-context reparent, cross-context transfer and destructive callbacks. Core Event and native-window ownership remain separate. |
| Deferred under §13 | T08-D · Actual image cook product · medium integration | The real AuroraImageBuilder works offline but AuroraCook.Static links Aurora.Adaptor.Static and RHI. Keep the existing catalog/history/Save-enqueue coverage; defer real product integration until the separate refactor stabilizes. | The eventual gate remains edit/Undo/Redo → Save/Drain → wait → inspect real product maxSize/mips and target isolation. No new product acceptance is claimed. |
| Complete portable evidence in §17; host implementation complete in §18 | T11-C · Page persistence/application · medium | Portable codec/storage/results, page validation/application and session/dialog integration remain delivered. Real adapter and settings-only SandboxModule injection are complete in §18. | Prior 30 new cases/491 per-profile counts remain historical evidence; §18 records 533 per-profile tests with the new closeout coverage. Actual native/display acceptance stays open. |
| Complete in §18 | T11 ORDER / host / four-screen composition | Effective appearance>ORDER>zero sorting preserves existing key-order ties; file storage/generic host load/injection/plugin preservation and main/floating scale application are delivered. Outer migration is frozen to FileBrowser, NewWorld, ProjectManager and WorldConfig. | 42 new cases passed normally/under ASan with bounded settings-source verification. The scope covers these four compositions; eight settings keep stored-only status, and Windows/native/display/GPU gates remain separate. |
| Complete in §16 | T11-B · Preferences container composition · medium | Category/page/footer allocation now uses DockPanel, vertical ScrollView+VBox, stable ReflectedFormView and a right-aligned footer GridPanel with 4/2/1 columns. Logical sizes and UiTheme inputs replace parallel row/button formulas. | 17 new cases passed normally and under ASan, covering small windows, long labels, independent scroll, pointer capture/cancel, popup overflow, large-origin logical size, resize/theme queries, session behavior and scrollbar draw bounds. Real display/DPI presentation remains T08-C. |
| Ready as a bounded measurement task | T13 · CPU layout/paint baseline · small | Existing tests cover clean/dirty layouts, same-scale theme changes and alternating surface DPI. Add fixed deep/wide trees and work counters before changing theme ownership/caches. | Record measure/arrange/invalidation visits, visible draw-data counts and diagnostic CPU timing. No GPU performance claim; do not make timing thresholds brittle tests. |

Evidence entry points: `PreferencesDialog.cpp` Open/Commit and `PreferencesSession.cpp`;
`ReflectedFormView.cpp` default service injection; `SandboxModule.cpp` RegisterPreferencePages/LoadPreferences,
`PreferencesPersistence.cpp` and `FilePreferenceStorage.cpp`;
`EditorActionRegistry.h`, `ActionBarBuilder.cpp`, `MenuBar.h`, Shell::RefreshActions and
AssetViewer::RebuildChrome; UIElement child mutation and UIEventRouter state; AssetCookService::Drain,
AuroraImageBuilder::Request and the existing cook pipeline tests. The initial assessment preceded
implementation; delivered behavior and actual verification are recorded separately in §14/§15/§16/§17/§18.
Rows still described as proposals retain their own implementation and acceptance work.

### 12.2 Feasible next, with scope decisions first

- **T09-A2 · Core Event registration/dispatch:** stable registration identity and invalidation can
  be implemented/tested without GPU. Cover self-removal, deleting another pending listener, nested
  broadcast and same-address reconnect. Keep this separate from subtree input work because Event
  is a shared Core facility with consumers beyond UI; retain cross-DLL shared storage.
- **T10-B · Scope/action registration ownership:** define instance token, owner/generation and
  global fallback rules with T09-A first. Then CPU tests can verify nested same-name scopes,
  out-of-order closes and replacement owners. macOS dynamic-load checks are possible; Windows DLL
  unloading still needs its own platform evidence.
- **T09-B · Renderer contract/spec:** deferred during the separate RDG/RHI refactor. Do not amend
  encoder/RDG, resource or inflight contracts from this UI batch; reassess after the new seams stabilize.
- **T12 · Document identity:** begin with definition/schema/stable-id decisions and a narrow
  serialization/mutation vertical slice after T09-A. Avoid a simultaneous full definition/instance
  rewrite while detach/binding identity is still being settled.
- **Cocoa duplicate class ownership:** inspect static/shared linking and class definitions locally;
  use a separate platform fix if the audit identifies one. Do not conflate this warning with the
  resolved Bullet ABI failure or assume it explains unavailable Metal hardware.

Defer broad RDG/multi-frame implementation, full Unicode/IME work and exact rotated GPU clipping
until their contracts and suitable acceptance paths are defined. The present environment can
support design/CPU algorithms for them, but cannot close their complete product acceptance.

### 12.3 Windows backlog (recorded; not executed on this host)

- [ ] **WIN-01 · Toolchain/dependencies:** use the existing VS2022 `cmake-build-debug` and verify
  Win32 third-party versions with `python python/third_party.py -p Win32 -o D:/Code/sky3rd_win32_output --check`.
- [ ] **WIN-02 · Configure/build:** close SandboxEditor first; reconfigure for moved/new sources;
  rebuild the executable and all affected modules together. Include Framework, EditorCore,
  EditorWidgets, EditorShell, EditorRender, UI/UIRender, SandboxModule and feature editor modules;
  avoid mixing old IModule/event ABIs with a newly built host.
- [ ] **WIN-03 · CPU regression:** build/run EditorCoreTest, EditorWidgetsTest, EditorShellTest,
  EditorRenderTest, UICoreTest, UITextTest, UIEditorTest and enabled FreeType tests. Record actual
  counts after the next batch rather than assuming the macOS counts stay constant.
- [ ] **WIN-04 · DXIL/tool invocation:** run UIShaderTest's vs_main/fs_main/fs_round DXIL cases;
  verify ShaderHeaderTool from a clean reconfigure with the correct CWD/PATH and deployed Slang DLLs.
- [ ] **WIN-05 · Real GPU acceptance:** with a usable selected backend and validation enabled,
  execute T08-B/C: close/reopen/re-dock, preview/multi-window shutdown, minimized/acquire recovery,
  shared resources, glyph/page growth, cross-DPI movement and transformed clip/input. Record backend,
  device and validation results. CPU fake-backend tests are separate evidence.
- [ ] **WIN-06 · Module ownership:** after T10-B is implemented, verify extension unload removes
  handlers/registrations before DLL code or page models disappear; exercise older/newer owners and
  nested scope cleanup. Do not mark this future implementation's acceptance complete now.

Windows items remain open while portable development proceeds. None implies approval to commit
or archive; those retain the explicit confirmation rules in §11.4.

## 13. Temporary RDG/RHI exclusion (user constraint, 2026-10-16)

RDG and RHI are undergoing a separate refactor. This restriction applies to all subsequent changes
in this UI work and supersedes the earlier environment-only assessment. Existing uncommitted
renderer/surface fixes remain intact; do not revert, rewrite or adapt that stack without a new
coordinated scope. A renderer failure caused by moving contracts is recorded as a dependency,
not repaired opportunistically in a UI change.

### 13.1 Excluded implementations and contracts

- Do not change `engine/aurora/rhi`, `engine/aurora/rdg` or `engine/aurora/pipeline`, their public
  interfaces, backend implementations, dependency setup or related OpenSpec contracts.
- Leave `engine/sandbox/render`, `engine/ui/render` and related surface/frame/submission/
  GPU-resource synchronization code untouched. Preserve draw-data, texture-registry and resource-id
  contracts consumed by these renderers; no compatibility wrappers against intermediate RHI APIs.
- Defer T09-B, renderer/RDG spec reconciliation, inflight/buffering changes and T02/T07 native GPU
  acceptance until the separate refactor is stable and the integration scope is coordinated.
- Defer T08-D's real image-cook integration: `AuroraCook.Static → Aurora.Adaptor.Static → RHI`
  reaches the excluded dependency chain even though the cook itself is offline. Do not restructure
  that chain merely to unblock this UI batch.
- Keep SandboxModule renderer initialization/tick/surface callbacks, full native editor wiring,
  backend dependencies and Cocoa host/linking changes outside the current changes. Core Event
  registration changes also remain separate because renderer/backend consumers share that facility.

**Narrow T11 exception (user authorization, 2026-10-17):** `finish-editor-preferences` may change
only settings registration, generic preference file Load/service injection, UI-scale application,
necessary settings-service lifetime/header declarations, and retaining the existing GUI callback's
surfaceId through PaintGui into EditorShell::PaintSurface for that scale policy. These statements may
sit within startup/hub/editor functions that also contain renderer code; surrounding renderer
initialization, tick, surface creation/interfaces/resources and refactor statements remain protected. This exception
does not authorize RHI/RDG/pipeline/backends, Cocoa/native-window ownership or Core Event changes,
and does not request a full/native renderer build or count native execution as verified.

**UI foundation placement exception (user direction, 2026-10-17):** retained runtime, Aurora UI
renderer, optional FreeType and headless document editor implementations live in `engine/ui` under
the separate targets listed in §6. Moving the existing renderer/provider files and build paths back
into that foundation is allowed; renderer algorithms/interfaces/resources and RHI/RDG/backend/Core
Event contracts remain protected. Root `UI.Interface` still excludes implementation dependencies;
the entire foundation directory is no longer constrained to interfaces only. No UI plugin is added.

**Bounded host-input review exception (§19):** existing SandboxModule key-down/up and filtered-text
handlers may forward a known floating window's current surface identity to existing Shell dispatch
APIs. Main-window identity must not be learned from a known floating window's first event. This
does not change native-window ownership, event interfaces or Core Event registration.

### 13.2 Safe development queue

1. **T11-A — complete (§14):** Preferences-owned history and safe page snapshots in editor core/Shell.
2. **T10-A — complete (§14):** action state, builder, MenuBar/ToolBar, Shell/Viewer refresh and dispatch semantics.
3. **T09-A1 — complete (§15):** UI tree detach and router
   focus/hover/capture lifetime; no native-window/RHI interface changes.
4. **T11-B — complete (§16):** Preferences categories/content/footer containers and theme metrics;
   normal/ASan portable evidence is recorded, while Windows/native presentation stays pending.
5. **T11-C portable — complete (§17):** checked persistence/results, test storage, session/dialog and
   Shell/Theme behavior with normal/ASan evidence; actual settings-only host wiring is now in §18.
6. **T11 implementation closeout — complete (§18):** effective appearance>ORDER>zero with stable
   existing key-order ties; real file/generic host settings wiring, user×DPI and the frozen four
   outer compositions. Normal/ASan/file/source evidence passed; no Framework ordering ABI
   expansion or renderer-side setting consumers. Windows/native/display/GPU gates remain open.
7. **T10-B / T13 (later portable scopes):** action owner/scope decisions and
   measure/arrange/invalidation or UIPaintContext draw-data baselines;
   no renderer, texture-upload, fence or GPU measurement work.

The current §19 follow-up restores foundation placement and simplifies exception boundaries while
repairing confirmed copy/reentry lifetime gaps. Portable normal/ASan verification passed; the final
unified whitespace/scope checks passed. Completed T11 counts remain the §18 checkpoint.

Keep implementations in EditorCore/EditorUi/EditorWidgets/EditorShell and `engine/ui/core` as
appropriate to their existing layers. Ownership/schema decisions for editor/UI data may proceed;
renderer ownership decisions and global Core Event changes stay outside this batch.

### 13.3 Verification boundary

- Use targeted `EditorCoreTest`, `EditorWidgetsTest`, `EditorShellTest`, `UICoreTest`, `UITextTest`
  and `UIEditorTest` builds/tests and their relevant ASan variants. Their current target dependencies
  avoid Aurora/RHI. Retain headless/widget-probe coverage where relevant.
- Exclude `EditorRenderTest` despite its fake CPU fixtures: it explicitly links Aurora.RHI.Static.
  Do not request UIRender/AuroraCook/SandboxModule/SandboxEditor or full default builds merely to
  verify portable UI changes during this exclusion.
- If even a selected test starts pulling in the refactoring renderer because dependencies change,
  stop that dependency path and report it; do not repair RDG/RHI CMake or APIs to force the test through.
- Windows checks in §12.3 remain recorded and unfinished. Backend/DXIL/native/module-integration
  evidence will be collected after the corresponding refactor dependencies are ready.

## 14. Portable UI batch: T11-A and T10-A

This batch is authorized after the RDG/RHI exclusion in §13. It implements two independent changes
in EditorCore/Widgets/Shell and uses only the portable CPU targets listed there. Previous renderer
changes and their GPU gates remain separate; Windows tasks remain recorded in §12.3.

### 14.1 T11-A · Preferences working session

Change: `fix-preferences-session-history`.

- [x] Add UI-free session-owned working values and CommandService; edits no longer mutate live
  preferences until Apply/OK. Cancel/close discard unapplied work; Apply establishes the new baseline.
- [x] Identify pages by id/type/registration generation, preserve stable storage and cross-page
  Undo across reorder, and invalidate old history safely on membership/re-registration changes.
- [x] Replace indexed byte snapshots with type-aware deep copies supporting strings/sequences;
  protect Apply/close/rebind against synchronous callbacks and destroy forms before their services.
- [x] Cover working/live/global-history isolation, Apply/Cancel/reopen/reset, multiple/reordered/
  replaced pages and lifetime/reentrancy with CPU tests and ASan.
- [x] Record verification and remaining T11-B/T11-C work. Host persistence/renderer wiring is outside
  this change; the existing generic reflected field UI remains in use.

### 14.2 T10-A · Shared action state

Change: `unify-editor-action-state`.

- [x] Add visible/checkable/checked alongside enabled and resolve state from copied registrations
  with scope/generation checks; stale or hidden actions cannot invoke an old handler.
- [x] Carry the same state through toolbar, menus and accelerators. Disabled shortcuts retain
  their consume-without-invocation behavior; hidden shortcuts do not match.
- [x] Refresh Shell/Viewer chrome state without closing open menus for ordinary state changes;
  generation/lifetime guards cover predicates that mutate registrations or owned controls.
- [x] Test enable/visibility/check changes, menu/toolbar drawing and input, current invocation,
  state refresh and callback mutation/destruction with portable CPU suites and ASan.
- [x] Record verification and the separate T10-B scope/registration-owner work. Renderer contracts,
  Core Event and native module integration remain outside this change.

### 14.3 Integration evidence (2026-10-16, macOS)

- Targeted normal and ASan builds passed for EditorCoreTest, EditorWidgetsTest, EditorShellTest,
  UICoreTest, UITextTest and UIEditorTest. Build dry-runs included no Aurora/RDG/RHI, EditorRender,
  SandboxModule or SandboxEditor work; no full default build was requested.
- Each profile passed **409 tests**: Core 117, Widgets 28, Shell 86, UI 150, Text 25 and UI.Editor 3.
  Shell includes 13 new PreferencesSession cases, 13 action-state cases and one real Shell routing
  integration case. The new integration verifies Preferences Ctrl+Z/Y, key-up, Ctrl+S/F5 isolation
  and global history resuming after Cancel. These totals are overlapping normal/ASan evidence.
- ASan used `detect_leaks=0`; macOS LeakSanitizer remains unavailable. A new menu draw assertion was
  corrected to check the actual checked glyph instead of expecting an exact gradient endpoint color.
- All 18 foundation/repair/current-batch changes passed strict OpenSpec validation. Tracked and
  untracked whitespace checks passed. A before/after content hash of 255 excluded-scope files was
  unchanged, preserving the existing uncommitted renderer stack.
- Windows, native module integration and GPU acceptance remain pending. EditorAction/PreferencePage
  metadata changed: rebuild all consuming editor modules together when integration resumes after
  the separate RDG/RHI refactor, rather than mixing older modules with these editor contracts.
- T11-B layout, T11-C generic persistence/application and T10-B scope/plugin-unload ownership remain
  open at this historical checkpoint; T11-B was subsequently completed in §16. Reflection member
  `isStatic` metadata is unreliable in the existing TypeFactory; investigate
  that separately instead of filtering all Preferences fields by that flag in this batch.
- No commit or OpenSpec archive was performed.

## 15. Portable UI batch: T09-A1 subtree/input lifetime

Change: `fix-ui-subtree-input-lifetime`. Implementation and portable normal/ASan verification are
complete. At that historical checkpoint this batch used `plugins/ui/core` and portable UI tests;
the same runtime code now lives in `engine/ui/core` under the §19 foundation placement. The
RDG/RHI, renderer, native-window and Core Event exclusions in §13 remain in force.

### 15.1 Delivered contract

- Object lifetime and tree attachment have separate weak references. RemoveChild/ClearChildren
  end the old attachment throughout the subtree, even if the same objects are immediately
  reattached to the same context. Cross-context transfer also requires fresh focus/capture;
  MoveChild only reorders siblings and preserves interaction state.
- RemoveChild extracts the returned subtree before callbacks. ClearChildren extracts the old
  child collection, so callback-added children remain in the live parent. Layout/metadata updates
  finish before cancellation can destroy or reenter the old owner.
- A context tracks all registered routers. Before cancellation, each router forgets only focus,
  hover and per-pointer capture within the removed subtree; unaffected sibling targets remain.
  Native direct-container capture participates in the same collection. Actual target/pointer pairs
  are deduplicated before OnPointerCancel, without inventing coordinates or synthetic Leave events.
- Pending cancellation uses a shared delivery ticket, held weakly by its target. If the first
  cancellation callback removes/transfers another pending Button, that nested RemoveChild fulfills
  the old ticket before returning ownership, clearing the old press before a new attachment or
  DOWN can begin. Delivery is claimed before entering the callback, so nested/outer cancellation
  cannot deliver the old ticket twice or cancel a fresh DOWN in the destination context.
- Normal detach cancels still-live targets; reference invalidation prevents dispatch into destroyed
  objects. Reset remains the documented silent host reset. Detached/foreign elements cannot become
  a router's focus target, and either context/router destruction order is supported.
- Pointer/key/text bubbling, hover transitions and direct-child routing revalidate weak lifetime
  and attachment after callbacks. Destruction or reattachment stops the old event path rather than
  continuing into a new parent. Direct UP releases capture before invoking the handler; capture
  queries cannot leave a dangling child snapshot after structural mutation.
- This is input/tree lifetime work, not a general permission to destroy arbitrary layout/paint
  objects mid-pass. Core Event registration, scope/plugin ownership, document instances and native
  GPU/window lifetimes retain their separate tasks.

### 15.2 Integration evidence (2026-10-16, macOS)

| Evidence | Status | Recorded result / boundary |
| --- | --- | --- |
| Attachment/router/direct-container regression cases | 35 added; passed in both profiles | Precise cancellation, siblings/multiple pointers, clear/reparent/transfer, multiple routers, destruction order, destructive input/capture-query callbacks and old-ticket delivery before transferring a pending Button, with/without a fresh destination DOWN. |
| Targeted normal CPU build/tests | Build passed; 444 tests passed | EditorCoreTest 117, EditorWidgetsTest 28, EditorShellTest 86, UICoreTest 185, UITextTest 25 and UIEditorTest 3. UICoreTest includes the 35 new lifetime regressions. |
| Targeted ASan build/tests | Build passed; 444 tests passed | The same six-suite counts passed with `ASAN_OPTIONS=detect_leaks=0`. macOS LeakSanitizer remains unavailable; normal/ASan totals overlap rather than representing distinct cases. |
| OpenSpec strict validation / whitespace | Passed | All 19 UI changes passed strict validation. Tracked changes and 250 untracked files passed whitespace checks. |
| Excluded-scope preservation | Passed | Before/after hashes matched for all 255 excluded-scope files. The initial 79-step build dry-run contained no excluded targets; both final targeted build profiles passed without a full/native build. |
| Windows / native module integration | Recorded, not executed | Keep §12.3 open; perform coordinated consumer rebuild when integration resumes. |
| Real GPU / DPI presentation | Deferred | Existing T08-B/C gates remain open under §13; portable input tests do not supply rendered-frame evidence. |

UIElement's object layout/virtual contract and the UIContext/UIEventRouter layouts changed. When integration
resumes, rebuild **all consumers** together, including consuming plugins/editor modules, rather
than loading old binaries against the new UI classes. This records an integration requirement;
it does not request a full/native build during the RDG/RHI exclusion or execute the Windows backlog.

The historical next step was **T11-B Preferences container layout**, now completed in §16;
the subsequent **T11-C portable persistence/application protocols** are complete in §17. The current
bounded T11 implementation closeout is §18. T10-B owner/scopes and T13 CPU
measurement remain separate later bounded work. No
commit or OpenSpec archive is part of this batch; both still require the explicit approvals in §11.4.

## 16. Portable UI batch: T11-B Preferences container composition

Change: `compose-preferences-container-layout` (2026-10-17). Implementation and portable normal/ASan
verification are complete. This section advances the current
queue without changing the dated evidence in §14/§15. RDG/RHI, renderer, SandboxModule, Core Event,
Windows and native GPU exclusions remain in force.

### 16.1 Implemented composition and API

- Preferences owns a DockPanel panel with an actual themed header leaf, bottom footer DockPanel,
  and Fill body DockPanel. The header uses UiSkin's `panelHeaderHeight`; layout and paint no longer
  use different title offsets. The body allocates a vertical category ScrollView on the left and
  the existing stable ReflectedFormView in Fill.
- Categories are container-owned VBox rows, rendered single-line through UiSkin/text ellipsis.
  Their scroll state is independent of the reflected form's built-in scrolling. Wheel routing and
  category selection use the actual visible viewport; clipped rows cannot select through the footer.
  UiSkin paints the scrollbar inside its explicitly reserved track, including small/fractional-scale
  tracks. Form capture records the owning pointer id; foreign release/cancel cannot end it, while
  matching cancellation resets the form safely and permits a new pointer session.
  Popup input remains ahead of category/footer routing, and popup drawing may extend beyond the
  modal panel inside the window without an ancestor panel clip truncating it.
- The footer docks its action GridPanel to the right. Label measurement, themed minimum widths
  and available width select four, two or one columns. All slots/padding are clamped to the available
  viewport; tiny surfaces may yield empty regions without changing shared container algorithms.
- `SynchronizeLayout()` prepares safe pending session transitions, page/form changes, theme inputs
  and the composed allocation. Existing geometry queries now read this synchronized tree, while
  `CategoryViewportRect()` and `PageViewportRect()` expose its actual scrolling/page regions.
  Const queries synchronize UI state before returning, including before the first paint.
- Explicit logical sizes flow through container arrangement. Paint, clips and input share arranged
  bounds after resize, reopen and same-scale/scale changes. The generic reflected field UI and local
  PreferencesSession history remain intact; the form is still destroyed before its session services.
  Panel logical allocation remains exact at large absolute origins instead of being reconstructed
  by subtracting rounded rectangle endpoints.
  OnPaint consumes only the prepared tree and does not synchronize registrations, create rows or
  repair layout; layout/input/explicit synchronization and const geometry queries prepare it.
- Session working values, Apply/OK/Cancel/Reset, cross-page history, registry identity and guarded
  callback/reopen behavior keep the T11-A contract. This composition adds no generic persistence or
  SandboxModule application wiring.

### 16.2 Integration evidence (2026-10-17, macOS)

| Evidence | Recorded result / boundary |
| --- | --- |
| New Preferences layout/input/shared-skin cases | 17 added, passed in both profiles: 13 PreferencesLayout cases and 4 EditorScrollbar cases. Coverage includes small/degenerate windows, long-label clipping, late category selection, independent form scroll/edit, pointer isolation/cancel, popup input/paint beyond the panel, resize/theme queries before paint, large-origin exact logical allocation, session behavior, Paint-only consumption, and reserved-track geometry at 0.5x/1x/2x including tiny tracks/fitting content. |
| Targeted normal portable CPU build/tests | Final targeted build passed; **461 tests passed**: EditorCoreTest 117, EditorWidgetsTest 32, EditorShellTest 99, UICoreTest 185, UITextTest 25 and UIEditorTest 3. No full/native build was requested. |
| Targeted ASan portable build/tests | Final targeted build passed; the same **461 tests passed** with `ASAN_OPTIONS=detect_leaks=0`. macOS LeakSanitizer remains unavailable. Normal/ASan counts overlap rather than representing distinct cases. |
| Strict OpenSpec / tracked and untracked whitespace | All 20 UI changes passed strict validation. Tracked changes and all 257 untracked files passed the final whitespace check. |
| Excluded-scope preservation | All 264 excluded-scope file hashes matched. The earlier 40-step build dry-run contained zero excluded targets; final builds were targeted C++/test increments. No RDG/RHI/renderer/SandboxModule/Core Event edits or full/native build were introduced. |
| Windows / native module integration | Recorded, not executed; §12.3 remains open. |
| Actual display / cross-DPI / GPU presentation | Deferred; CPU geometry/input coverage cannot close T08-B/C. |

### 16.3 Independent portable finding still open

`ReflectedForm::OrderedMembers` uses `MemberAppearance.order` but does not consume reflected
`CommonPropertyKey::ORDER` metadata. This is a separate generic form-ordering gap. T11-B's fixture
used the existing reflection iteration order to target actual rows; this change does not repair
the shared field-ordering implementation or add a business-specific settings UI. Keep the finding
open for a separately scoped portable correction and regression.
The earlier description of this fallback as "registration order" was imprecise: TypeNode::members
is a std::map, so that existing iteration is currently key/dictionary order. T11 closeout retains
that stable fallback without adding registration-order metadata to Framework.

The next bounded scope at this checkpoint was T11-C's portable persistence/application protocol,
now complete with portable normal/ASan evidence in §17. Actual SandboxModule Load/Save/Apply wiring
and RDG-related integration remain deferred under §13. T10-B ownership and T13 CPU measurement
remain separate later scopes. No commit/archive is performed; approvals in §11.4 remain required.

## 17. Portable UI batch: T11-C preference persistence/application protocol

Change: `persist-preference-pages-portably` (2026-10-17). Portable implementation, normal/ASan
execution and closeout checks are complete. Existing §14–§16 evidence remains
unchanged. Actual SandboxModule/native integration, RDG/RHI/renderer, Core Event, Windows and real
display/GPU acceptance remain excluded or unfinished.

### 17.1 Implemented portable contract

- EditorCore supplies PreferenceResult/code/context with conservative stage-effect flags,
  IPreferenceStorage, checked PreferencesPersistence and owned capture/validation/publication helpers.
  The existing cross-DLL registry adds optional pure candidate validation and retry-safe application
  callbacks without moving settings data into UI.
- The document remains pageId → `{classId, elements}`. Checked JSON/Any/reflection decoding stages
  all candidates before publication, preserving missing-page/field provider values. Save merges a
  fresh read, retaining unknown object keys/absent plugin pages and replacing known arrays, including
  empty arrays. Unsupported/invalid data returns context instead of reaching asserting JSON loaders.
  Save explicitly Any::Create-clones candidates before storage/getter callbacks. Owned reads prefer
  supported nonconst getters, with guarded const-getter fallback.
- Load belongs at a host-controlled boundary with no open editing session. Storage is injectable;
  failed/throwing Write must preserve previous bytes. This adapter contract is not an atomic
  transaction across storage, reflected setters and runtime callbacks.
- ApplyDetailed orders validate → optional Save → guarded live writes/application → guarded
  history clear/completion check. Pre-publication failures retain live settings and working/history state; later
  failures may leave durable/live/application effects. storageWritten confirms successful Write;
  liveWritten/applicationApplied are set before entering potentially effectful setters/hooks and do
  not prove actual change/completion. Retry may repeat those stages; Cancel only discards working state.
- PreferencesDialog supports shared persistence injection, detailed result callbacks/access and
  generic failure text. Failed OK/Apply remains retryable without legacy success notification.
  EditorShell exposes optional injection and portable Theme notification tests; it does not wire
  actual SandboxModule storage/application.
- Candidate-copy failure is caught as InvalidData with no stage effects/history loss. A required
  local Any::Create correction frees raw storage and resets type state after a throwing placement
  copy rather than destroying an incomplete object. Its signature and generic JSON loader stay unchanged.
- Current guards are checked before/after their own callback: false/throw or self-invalidating
  close/reopen/destruction/edit produces Conflict. CommandService::GetRevision includes transaction-
  internal edits, so callback work from storage/application/guards invalidates the old Apply snapshot
  even when stack counts are unchanged. Completion accepts only Clear's own revision increment;
  new edits/history from Clear observers are preserved and stop the old operation. In that final
  conflict case old published history has already been cleared; the newer work is not cleared again.

### 17.2 Integration evidence (2026-10-17, macOS)

| Evidence | Recorded result / boundary |
| --- | --- |
| New portable preference regressions | 30 added cases passed in both profiles: 26 Core and 4 Shell. Coverage includes guard self-invalidation/throw, storage/application callback edits including pending transactions, explicit Save snapshots/nonconst getters, throwing nontrivial copies, round trips, unknown keys/defaults/arrays, checked failures, stage flags, retry/history, identity/lifetime and optional Theme application. |
| Targeted normal portable CPU build/tests | Final six-suite targeted build passed; **491 tests passed**: EditorCoreTest 143, EditorWidgetsTest 32, EditorShellTest 103, UICoreTest 185, UITextTest 25 and UIEditorTest 3. |
| Targeted ASan portable CPU build/tests | Final targeted build and the same **491 tests passed**, with `ASAN_OPTIONS=detect_leaks=0` and no reported errors. macOS LeakSanitizer remains unavailable; normal/ASan totals overlap rather than representing distinct cases. |
| Strict OpenSpec / whitespace | All 21 active UI changes passed strict validation. Tracked changes and all 270 untracked files passed whitespace checks. |
| Excluded-scope preservation | Both final targeted six-suite builds succeeded with 47 build commands each and zero excluded commands. All 264 excluded-scope file hashes matched; no full/native build or renderer/RDG/RHI/SandboxModule/Core Event change was introduced. |
| Actual SandboxModule generic Load/Save/Apply | Deferred. The native host still has its legacy three-built-in-page serializer/callback; opt-in portable APIs do not automatically generalize running native persistence. |
| Windows / native display/DPI/GPU | Recorded and pending; §12.3 and T08-B/C remain open. |

### 17.3 Next bounded work

Recommend **T11-D reflected ORDER metadata** first: resolve the independent §16.3 generic field-ordering
gap through EditorCore/property/form tests, using explicit appearance order over reflected ORDER
over zero and stable existing reflection iteration (currently dictionary/key order) for ties.
This is a small portable correction, not new settings UI. Follow with separately scoped
T10-B action owner/scopes and T13 CPU work-count/baseline tasks as appropriate. T11-C actual native
adapter/application wiring remains deferred under §13 alongside the renderer integration. No
Windows/native/GPU gate, commit approval or archive approval is closed by this portable change.

Subsequent authorization: these remaining T11 implementation pieces are now combined into the
bounded §18 closeout, including a narrow settings-only SandboxModule exception. The §17 counts and
unrun native/platform gates above remain its original checkpoint evidence.

## 18. T11 implementation closeout: host preferences and frozen remaining screens

Change: `finish-editor-preferences` (2026-10-17). This is one bounded implementation batch for the
remaining T11 code, following the delivered A/B/C changes. Implementation and portable
normal/ASan/file/source verification are complete. All six suites passed 533 tests per profile,
including 42 new cases. Native/Windows/display/GPU acceptance remains pending.

### 18.1 Frozen inventory and ownership

| View | State found before this batch | Included migration; unchanged responsibility |
| --- | --- | --- |
| FileBrowserDialog | Hand-computed sidebar/navigation/list/header/footer/control rectangles | Shared container outer regions/control slots; preserve FileBrowserModel, specialized rows/popups and navigation/filter/create/rename/accept/cancel behavior. |
| NewWorldDialog | Hand-computed name/location fields and Create/Cancel footer | Shared panel/header/body field rows/footer/control slots; preserve TextEditState, path validation and create/cancel callbacks. |
| ProjectManagerView | Hand-computed banner/actions/header/status/recent-list regions | Shared Dock/Grid outer allocations and responsive action slots; preserve recent/selection/open/add/new/remove/confirmed-delete operations and specialized row content. |
| WorldConfigPanel | Existing parent Arrange and stable generic form, but manual list/form allocation | Shared header/list/form regions; preserve reflected fields, explicit document/provider lifetime and enable/rebuild/save behavior. |

This list is frozen at four views. Specialized row/glyph painting may remain inside its allocated
leaf/viewport. It is not an assertion that all editor screens have been rewritten. Paint/query/input
use the same arranged outer slots, with explicit logical sizes, themed dimensions, clipped hits and
finite small/degenerate budgets. No generic layout algorithm or business model rewrite is included.

### 18.2 Settings and metadata closeout

- Generic ORDER uses explicit MemberAppearance.order when hasOrder, otherwise reflected ORDER,
  otherwise zero. Stable ties preserve existing reflection iteration, currently dictionary/key order.
  Sorting precedes field/owned-inline-value construction; no registration-order ABI is added.
- FilePreferenceStorage supplies real same-directory temporary write/close/replace behavior under
  the prior storage contract. Failed writes preserve bytes/clean temporary artifacts; concurrent
  writer CAS and power-loss durability are not promised.
- BuiltinPreferencePages tracks exact owned registrations and does not Clear plugins. Binding
  rejects unrelated reserved-id conflicts; reset/destruction cannot remove a newer owner.
- Host settings wiring registers built-ins/contributors before generic Load, injects the shared file
  service into Shell/Dialog, and loads hub settings. This replaces the old three-page Save-only hook
  under §13's narrow exception; actual native module execution remains unverified.
- Device DPI and user multiplier are independent. Main/floating queries multiply once and clamp
  effective scale to 0.5–4; supported user scale is 0.5–2. UI Scale applies through Shell/Theme;
  the developer override remains a device input rather than another multiplier.
- Existing host GUI dispatch preserves the actual surfaceId into EditorShell::PaintSurface. Shell
  pointer/key/text entry points select that target theme/layout before routing, guarded against
  owner/attachment changes; main modal input does not inherit the last floating paint's scale.
  Presentation/input hosts supply context, rather than an arbitrary const geometry query guessing it.

| Built-in value | Delivered application claim |
| --- | --- |
| editor.uiScale | Immediate Shell/Theme multiplier for main/floating; persisted startup/hub input |
| general.language / theme / autosave | Stored only; no current consumers or restart-effect claim |
| editor.gridSize / snap / undoDepth | Stored only; no current consumers or restart-effect claim |
| rendering.vsync / resolutionScale | Stored only; consumer work waits for renderer refactor coordination |

All remain reflected/editable/validated/stored. General/Rendering titles and relevant Editor
labels/tooltips state the limitation. Implementing those eight feature consumers is separate from
closing persistence/application plumbing; no rendering feature is silently enabled here.

### 18.3 Verification and remaining platform acceptance

| Evidence | Current state / boundary |
| --- | --- |
| New regressions | 42 passed in both profiles: Core 17 (7 built-in ownership/validation/legacy archive, 7 real-file storage, 3 ORDER), Widgets 1 nested metadata, Shell 24 (4 preference integration, 6 NewWorld/WorldConfig, 8 FileBrowser, 6 ProjectManager). Includes checked Load of legacy archive bytes, real-file persistence, plugin preservation, scale/cross-window input and each frozen composition. |
| Targeted normal CPU build/tests | Passed: EditorCoreTest 160, EditorWidgetsTest 33, EditorShellTest 127, UICoreTest 185, UITextTest 25 and UIEditorTest 3; 533 total. |
| Targeted ASan build/tests | The same six suites and 533 tests passed with `ASAN_OPTIONS=detect_leaks=0`, without ASan errors. macOS LeakSanitizer remains unavailable; the profiles cover overlapping cases. |
| Bounded build scope | Both targeted six-suite profiles passed. Each initial build had 58 commands and each final correction increment had 8; every checked command set contained 0 excluded targets. No full/native renderer build was run. |
| SandboxModule settings-source integration | `-fsyntax-only` passed using the configured compile arguments. The two-file narrow exception was reviewed as settings/lifetime and GUI scale routing only; no native link/run evidence or renderer/RHI/Core Event change. |
| Strict validation / whitespace / protected scope | All 22 active UI changes passed strict validation. Tracked changes plus 289 untracked files passed whitespace checks. All 262 protected-file hashes remained unchanged; the two SandboxModule exception files were reviewed separately. |
| T11 platform acceptance | Windows/native module/display/cross-DPI/GPU remain open in §12.3/T08. |

Final regression corrections include nested-field test lookup and initialized paint/whole-window
input fixtures, plus retained-layout dirty propagation and tab-drag handling. The completed result
above follows those corrections rather than treating an earlier failing run as acceptance.

**T11 implementation closeout is complete** with the portable/file/ASan/source evidence above.
**T11 platform acceptance remains pending** for Windows/native display/GPU execution. T10-B ownership
and T13 CPU measurement remain later separate scopes; settings without consumers keep their
explicit stored-only status. No commit or
OpenSpec archive is part of this batch without the approvals in §11.4.

## 19. UI foundation placement and exception/reentry review

Change: `simplify-ui-foundation-boundaries` (2026-10-17). This follow-up implements the user's
foundation placement decision and request to remove unnecessary try/catch blocks, alongside the
confirmed lifetime fixes from the overall review. Portable normal/ASan builds/tests, module syntax,
strict validation, source/scope audits and final unified whitespace checks passed.
Completed §14–§18 counts remain their original checkpoint evidence.

### 19.1 Foundation placement

The 75-file relocation returns retained runtime to `engine/ui/core`, Aurora UI rendering to
`engine/ui/render`, the optional FreeType provider to `engine/ui/freetype`, and headless document
editing to `engine/ui/editor`. The root `UI.Interface` target retains contracts/data/pure utilities;
the four implementation targets keep their names and public `ui/...` includes. Runtime still does
not depend on editor/render/FreeType implementations. No UI feature plugin is introduced.

This is the explicit UI foundation exception to the general feature placement rule. Other features
retain their engine/interface and plugin ownership rules. Active documents now use current paths;
the earlier plugin-layout build/test records were not originally run from these locations. Moving
renderer/provider files does not authorize renderer/RHI/RDG/backend/Core Event behavior changes.

### 19.2 Exception and lifetime boundaries

- `Any::Create` keeps raw allocation in an RAII owner and publishes type/storage only after a
  placement copy succeeds. A throwing constructor frees raw memory without invoking a destructor
  on an incomplete object; no local catch is needed. Its signature and unsupported-type behavior
  remain unchanged.
- File storage uses explicit filesystem error codes/errno for operating-system failures and RAII
  for open streams/temporary files. Temporary names use the existing core UUID utility with
  exclusive creation. Complete write/close precedes replacement, and allocation exceptions are
  not relabeled as file I/O failures. Read publishes caller bytes only after a complete read.
- The review found that reflected copy callbacks can remove registry metadata or close/reopen a
  session while a borrowed descriptor or working source is still in use. The fix retains owned
  descriptor snapshots and shared working-page lifetime through callback execution, then checks
  current state before accepting the old snapshot. A reentered session's new work/history survives.
- Session Apply uses one checked capture path instead of nested repeated copies/catches. Deliberate
  exception translation remains at declared reflection/provider/storage callback boundaries where
  the result protocol needs a failure; it is not added around ordinary internal work.
- OpenPreferences rechecks weak shell/dialog identity and chrome generation after callback-capable
  setup/layout/open operations. History observers may destroy or replace an owner; the stale call
  stops without continuing through released storage.
- Existing native floating-window key-down/up and filtered-text input now retain the known window's
  current surface id through Shell's existing dispatch APIs, including a restored window's first
  event. Main-window identity remains separate; native window/event ownership is unchanged.

### 19.3 Verification and remaining gates

| Evidence | Current state / boundary |
| --- | --- |
| Foundation source/target mapping | All 75 moved files have destinations; all 71 non-CMake files remain byte-identical, with no missing file or remaining plugins/ui directory. Independent targets and public include/target names are preserved. |
| Exception cleanup and regressions | Physical try blocks decreased from 14 to 8 intentional callback-boundary sites. Six new regressions passed in both profiles: five Core reflected-copy/source/session lifetime cases and one Shell Preferences-reopening destruction case. Prior throwing-copy and seven real-file tests also passed. |
| Normal portable suites | All 541 tests passed: Core 165, Widgets 33, Shell 128, UICore 185, Text 25, UIEditor 3 and optional FreeType 2. |
| ASan portable suites | All six suites and 539 tests passed: the same counts without FreeType, with `ASAN_OPTIONS=detect_leaks=0` and no ASan errors. macOS LeakSanitizer remains unavailable; normal/ASan cases overlap. |
| Bounded configuration/build | Both configurations and targeted builds passed: normal 129 commands, ASan 124. No actual renderer/RHI/RDG/SandboxModule build was run. |
| Protected scope | Of the 262 baseline protected files, 261 are byte-identical; the remaining moved render CMake file changes only its CURRENT_FOLDER label. Renderer/RHI/RDG/backend/Core Event behavior remains protected. |
| Bounded source integration | SandboxModule `-fsyntax-only` passed with configured flags and 10 existing warnings. Exceptions cover placement/build paths, previously authorized settings wiring and existing floating key/text forwarding; no native link/run was performed. |
| Strict validation | All 23 active changes in this UI batch passed strict validation. |
| Whitespace | Tracked `git diff --check` and all 233 untracked files' no-index whitespace checks passed. |
| Final source/build review | Read-only audit of 150 dirty/untracked production C++ and build-configuration files found no new catch outside the declared Preferences boundaries, static registration guard, obsolete UI plugin build/code path or reverse UI dependency. The color parser's unrelated catch already existed in HEAD. |
| Windows/native/display/cross-DPI/GPU | Still pending under §12.3/T08; this review does not execute or close those gates. |

Commit and OpenSpec archive remain separate user-confirmed steps. This follow-up does not broaden
the four-screen T11 migration or add consumers for the eight explicitly stored-only settings.

## 20. Debug Sandbox startup and configured runtime module builds

Change: `fix-sandbox-runtime-module-build` (2026-10-17). The user's Debug startup crash was traced
to a current host loading stale runtime modules. The host-only build dependency fix and first
bounded Debug startup passed, followed by a successful direct 600-frame run. The Debug hub is left
running for the user. Strict validation and tracked/new-change whitespace checks passed.
Previous portable/native checkpoints retain their original binaries and execution context.

### 20.1 Confirmed crash cause

| Evidence | Finding |
| --- | --- |
| Debug host | 2026-10-17 SandboxEditor, binary UUID prefix 70A751C3 |
| Loaded Bullet module | 2026-10-13 BulletPhysicsModule, binary UUID prefix 993666EC |
| Fault | Current IModule includes the added RegisterMeta virtual slot; the old module's virtual table does not. Host PostTick dispatch reaches RTTI data past its old function entries and raises SIGBUS. |
| Other stale module | RecastNavigation was also old; it requires the same matching-header/configuration rebuild, although Bullet is the identified crashing dispatch. |
| Build gap | Dynamically loading modules does not create linker dependencies. Building only SandboxEditor could leave manifest-selected module binaries untouched. |

Dates/UUID prefixes identify the mismatched binaries; the virtual-table/crash evidence establishes
the ABI fault. The earlier resolved Bullet startup checkpoint does not establish freshness for
another configuration or later Debug module set.

### 20.2 Build coordination fix

After engine and plugin targets exist, top-level CMake reads `configs/modules_editor.json` and adds
SandboxEditor build-only dependencies for its existing module targets. It guards absent host and
module targets, and tracks the checked-in manifest through CMAKE_CONFIGURE_DEPENDS. Building the
Debug host now schedules enabled Bullet/Recast and the other available configured modules in the
same selected configuration. Missing source targets are not enabled or declared runtime-ready.

This does not add a plugin, link module implementations into the executable, change runtime loader
semantics, or introduce an ABI fingerprint system. External/replaced binaries remain the host's
deployment responsibility. The user's startup request permits the affected existing native target
rebuild/run; renderer/RHI/RDG/Cocoa/Core Event behavior changes remain outside this repair.

### 20.3 Recorded startup evidence

| Evidence | Current result / boundary |
| --- | --- |
| Xcode Debug host-only build | `--target SandboxEditor` automatically rebuilt Bullet/Recast and completed with BUILD SUCCEEDED. |
| Build graph / configuration | Xcode direct dependencies and Ninja link-order-only queries each asserted all 7 manifest modules; normal Ninja configuration passed. The generated RERUN_CMAKE inputs include the manifest, verifying change tracking. |
| Actual Debug bounded startup | Sandbox-outside LLDB run with `--frames 120`: all seven configured modules reported Load success; Metal instance/device initialization succeeded on Intel Iris Plus 655, FreeType initialized, and the application entered hub mode. Process 10584 exited with status 0. |
| LLDB wrapper exit | The outer LLDB command returned 1 because a later bt command had no live process; the launched application's exit status above is 0. This is not a Sandbox startup failure. |
| Direct longer run | `--frames 600` exited 0, with 7/7 module loads and successful Metal/FreeType initialization. No new SandboxEditor crash report appeared; only the user's 03:10 report and 03:11 sandbox-restricted reproduction remain. |
| Persistent hub | Debug SandboxEditor was relaunched without a frame limit in session 51508 and was still running after approximately one minute; it is left open for the user. |
| Protected scope | The 262-file audit again retains 261 byte-identical files plus the prior render CMake CURRENT_FOLDER-only exception. This repair changes the top-level build graph and docs/spec, with no renderer/RHI/RDG implementation change. |
| Strict validation / whitespace | New-change strict validation, tracked diff whitespace and the final unified no-index whitespace check of all 237 untracked files passed. |
| Windows/display/cross-DPI/GPU acceptance | Successful Metal initialization is startup evidence; the unexecuted lifecycle/visual/input/validation matrices in §12.3/T08 remain open. |

### 20.4 Separate follow-ups found during diagnosis

- ModuleManager currently calls RegisterMeta before Init. If Init returns false, the local dynamic
  library owner can unload while registered metadata remains, and the raw module instance leaks.
  The stale Recast module could expose that failure path. A separate lifecycle fix needs RAII
  instance ownership and a library/metadata cleanup or residency contract; rebuilding modules
  avoids this occurrence but does not repair that failure path.
- Duplicate Cocoa Objective-C class warnings remain an existing platform ownership investigation.
  This build fix does not claim to remove those warnings or attribute the confirmed Bullet ABI
  crash to them.

No commit or OpenSpec archive is part of this repair without the approvals in §11.4.

## 21. Confirmed archive closeout

On 2026-10-17 the user confirmed the reviewed archive and unified implementation/spec commit.
The 21 completed UI changes are archived under `openspec/changes/archive/2026-10-17-*` and their
requirements are merged into 25 canonical specifications. Archive replay preserved the cumulative
foundation, geometry, measurement and invalidation requirements; the 21 task lists retain all
301 completed entries. Earlier sections remain historical checkpoints with their original scope,
test counts and approval state.

Three changes remain active with one real GPU acceptance task each:

| Active change | Remaining acceptance |
| --- | --- |
| `fix-editor-skipped-frame-fence` | Native minimize/restore, acquire/skip and synchronization recovery. |
| `fix-editor-surface-gpu-teardown` | Repeated floating-window close/reopen and shutdown with in-flight work. |
| `sync-editor-surface-textures` | Floating texture/icon/wheel updates and close/reopen on real GPU surfaces. |

Their implementation and active specifications belong to the same unified change set; archiving
the other 21 changes does not mark those GPU gates complete. Windows/cross-DPI/full GPU matrices,
ModuleManager failed-Init ownership and duplicate Cocoa classes remain the follow-ups already
recorded above. No RHI/RDG/backend/Core Event implementation is added by this archive operation.
