## Context

T11-A establishes PreferencesSession-owned working pages, registration identity, local undo/redo, typed copies, and guarded transitions/callbacks. PreferencesDialog already binds the selected page into ReflectedFormView and must retain that generic field implementation. T11-B concerns the surrounding composition only.

Current PanelRect/ContentRect/CategoryRect/PageRect/ButtonRect calculate independent geometry. OnPaint derives another category/footer region from the panel painter, while input calls the former helpers. Some offsets/minimums are literal pixels, the category list has no bounded scroll model, and four fixed footer buttons can exceed the available width. The runtime now supplies constrained measurement, explicit logical allocation sizes, parent-owned arrangement, containers, clips, and scroll extents. The editor supplies theme metrics, UiSkin, reflected form synchronization, and safe session transitions.

## Goals / Non-Goals

**Goals:**

- Declare category/page/footer allocation using shared containers rather than separate per-control rectangle formulas.
- Finish arrangement before first paint or input and carry explicit logical sizes through the composed tree.
- Keep category and selected-page scroll state independent and bound paint/input to each allocated viewport.
- Respond consistently to small windows, long category labels, resized surfaces, page changes, and theme/scale changes.
- Preserve session/history behavior and all previously delivered ownership, cancellation, and callback lifetime guards.

**Non-Goals:**

- Replacing generic reflected field controls, introducing per-page settings forms, or changing settings schemas/defaults.
- Changing working-copy/session history, Apply/Cancel baselines, page registration identity, or provider/module-unload ownership.
- T11-C generic page persistence/application protocols or SandboxModule Load/Save/Apply wiring.
- New docking, general responsive-layout frameworks, Unicode shaping, renderer/RDG/RHI/surface work, or native DPI/GPU acceptance.

## Decisions

### Keep a composed tree beneath the modal host

PreferencesDialog remains the modal/session host. Its one root allocation determines a finite panel rectangle bounded by the available dialog surface using UiTheme dimensions. Shared DockPanel/StackPanel or equivalent existing containers own the header/body/footer and category/page subdivision. A footer action group is a container child rather than four independently positioned rectangles. The reflected form remains one page-host child bound to the selected session page.

The composition uses a panel DockPanel with a Top header leaf, Bottom footer DockPanel, and Fill body DockPanel. The body docks a vertical category ScrollView containing VBox rows on the Left, with the stable reflected form in Fill. The footer docks a GridPanel on the Right and selects four, two, or one columns from measured label/minimum-width budgets. Placement is expressed through slots, padding, gaps, and theme-derived extents. The host assigns the panel's outer final rectangle and configures pre-clamped container slots; it does not retain a parallel CategoryRect/ButtonRect/PageRect placement algorithm or change shared container algorithms. Existing public query helpers remain compatibility views over arranged element/viewport bounds and reject invalid indices safely. Tests and callers must not assume the form remains the modal's first direct child after composition.

### Keep logical allocation distinct from rounded endpoints

The current UI arrangement contract accepts final absolute bounds together with logical allocation size. Container/host integration carries that size explicitly so fractional origins and scale do not reintroduce measure/arrange drift through endpoint subtraction. Page form measurements and final allocations use the same allocated page width, with padding/header/footer reserved once. Theme-dependent preferred sizes are synchronized before allocation.

Painting uses arranged bounds and draws the modal frame/background through UiSkin. Category/footer leaf painters consume their allocated bounds rather than create rows or change layout. OnPaint consumes only the prepared composition: it does not synchronize page registrations, reconfigure containers, or create category/action nodes. The public SynchronizeLayout entry, executing layout, input entry, and compatibility const geometry queries prepare the safe composition before observation. Queries are therefore synchronized UI observations, not pure measurement calls. CategoryViewportRect and PageViewportRect expose the actual navigation/form regions; row rectangles may extend beyond the category viewport, while CategoryAt additionally checks the visible clip.

### Make the two scroll regions independent

The category area uses a vertical ScrollView (or existing shared bounded scrolling container) around a container-owned row list. Its intrinsic content extent comes from registered pages and theme row/gap metrics, and its viewport remains the category column's final allocation. The selected reflected page continues to use ReflectedFormView's existing internal scroll behavior; wrapping it in a second competing form scroll model is unnecessary.

Wheel/input routing chooses the visible region under the pointer. Category scrolling cannot change the form scroll offset, and form scrolling cannot move category rows or the footer. CategoryAt checks viewport membership/clipping as well as row bounds, so a translated offscreen row is not clickable through another region. Selecting the last category after scrolling binds that page through the same session and preserves local cross-page history. Page switch/reopen/registry reconciliation may reset or clamp relevant scroll positions deliberately, but must not produce an offset outside the current extent.

Active reflected popups/capture retain their established routing priority and lifetime rules. Generic form input continues to handle enum/color/text/drag interactions, with the footer/category route unable to steal an active captured interaction. The dialog tracks the pointer id owning a captured form interaction; a foreign pointer's MOVE/UP/cancel cannot end or modify that session. Matching pointer cancellation clears the dialog capture and forwards cancellation to the form before a new pointer can capture. Cancellation/forwarding may destroy the owner, so no unsafe member access follows it. Category mutations/rebuilds cancel stale row input before releasing affected row nodes using the current UI subtree-input contract.

The category scrollbar reserves its own narrow track beside the content viewport. UiSkin::DrawScrollbar consumes that actual track rectangle, with height equal to the viewport height, rather than deriving another inset track from a whole view. Theme-sized thumb geometry is clamped within the track, including sub-1x and 2x themes and tiny tracks; content that fits draws no thumb. Independent shared-skin draw-data regressions verify this bounded painter contract without changing runtime ScrollView layout or introducing another scroll owner.

### Derive responsive bounds from the theme

Use UiTheme/UiMetrics for panel preferred dimensions, outer margin, header/footer extents, category width/row height, gaps/padding, button minimums, and control heights. The actual header leaf uses panelHeaderHeight consistently with UiSkin instead of the former titleBarHeight/panel painter disagreement. New metrics are acceptable when existing ones cannot express a reusable dimension; their scaling belongs in UiMetrics::Scale. No Preferences-only literal-pixel placement constants replace the removed rectangle formulas.

The panel's preferred size is constrained by actual available bounds. Category width leaves a bounded page allocation instead of forcing the full nominal category width into an undersized body. Footer layout adapts when one row of the themed action controls cannot fit; an existing grid/vertical arrangement or another small container configuration may be used. It must avoid negative/NaN extents and overlapping action targets. At degenerate sizes, clipped or empty regions are valid; invisible/outside regions are not active input targets. This does not promise comfortable access to all controls in a zero-sized surface.

Long category labels are clipped or ellipsized within their arranged row; they do not extend into the reflected page or change the hit target. The label painter and row use the same theme/font/clip width. Footer labels remain within their allocated controls. Same-scale metric changes and 1x/fractional/2x scale changes refresh copied container inputs and the full affected subtree before reuse, without requiring a preliminary paint or dialog reconstruction.

### Use small editor adapters only for presentation or routing

If a category/action leaf is needed, it lives in the editor UI/widgets or shell composition layer and only reports theme-based desired size, draws via UiSkin, and invokes the host's existing operation. It does not implement another layout engine or settings editor. Shared existing controls should be reused where they satisfy the required behavior; a small generic shared setter/adapter may be introduced only for this composition's concrete need.

ScrollView/container algorithms stay in engine/ui/core, theme/skin in the existing editor framework, and PreferencesSession/registration in EditorCore. No UI dependency is added to the session or settings model, and no renderer/backend dependency is added to the dialog. There is no new process-wide registry or per-DLL singleton.

### Preserve the session and safe transition boundary

Open/Close/SetModel, pending transitions, registry reconciliation, BindPage, Reset, Apply/OK and external callbacks retain their existing ordering and weak lifetime/generation checks. Rebuilding category visual nodes does not replace session working objects, clear otherwise valid history, or create a new session. Pure registration reorder preserves selected page identity and history according to T11-A; removal/replacement still uses its existing reconciliation rules.

Before measuring/arranging, drain only safe pending transitions, synchronize page registrations, and synchronize the reflected form and default theme. These operations can notify observers or destroy/reopen the dialog, so the existing service retention and lifetime guards remain around the new composition work. The child form is still unbound/destroyed before its session/history services and working storage are released. Moving it deeper into a container must not bypass that destruction order.

Changing page, applying/resetting values, reopening, resizing, or changing theme notifies layout when its allocated content changes. Unknown dynamic dependencies remain conservative unless their notification path is explicitly audited; T11-B does not require a new global retained-layout policy. The host persistence callback remains exactly the T11-A callback and may still close/reopen/replace/destroy the dialog.

## Risks / Trade-offs

- [New containers preserve the old visual formulas in parallel] -> Query helpers read the arranged tree; paint/input use those same bounds and clips.
- [First frame repairs layout during paint] -> Test layout and input before any Paint, and keep tree/row construction at safe pre-layout/session boundaries.
- [Small surfaces overflow nominal button/category minimums] -> Constrain the panel, adapt shared container configuration, and validate finite nonoverlapping clipped targets.
- [Nested scrolling moves both regions] -> Keep a category scroll model separate from the existing reflected form scroll model and route by viewport/capture.
- [Moving the form changes lifetime or history] -> Preserve its injected local session, guarded transition ordering, and explicit child destruction before services.
- [Theme slots copy stale metrics] -> Reconfigure copied extents/padding/gaps when the theme revision changes and invalidate the affected subtree before layout reuse.
- [Long labels alter geometry inconsistently] -> Keep clipping and text measurement tied to the same allocated row/page widths.

## Validation Strategy

- Assert arranged category/page/footer/control bounds after Open and Arrange, before first Paint, using explicit logical sizes at ordinary and fractional origins.
- Resize from normal to small/degenerate bounds and back; verify finite nonnegative extents, region containment, nonoverlapping footer hit targets, usable constrained body allocations where space permits, and unchanged paint/input geometry.
- Register enough pages to overflow categories, scroll to and select a late row, and prove offscreen/clipped rows are not clickable. Populate a long reflected page and prove wheel routing scrolls only the hovered region while the footer remains fixed. Cover foreign-pointer release/isolation and matching cancellation followed by a new pointer session.
- Capture shared UiSkin scrollbar draw data for explicitly reserved tracks at 0.5x, 1x and 2x themes, including tiny tracks and fitting content, and assert thumb geometry stays in its reserved rectangle.
- Change page registrations after a prepared layout and invoke Paint without a preparation step; prove Paint consumes only the previous prepared composition, then synchronize/query/layout and verify the change becomes visible without tree mutation during paint.
- Cover long labels, page switching, dynamic registry reorder/removal/replacement, reopen, and same-scale/scale metric changes before first paint.
- Retain actual PreferencesSession/portable shell regressions for local cross-page Undo/Redo, Apply/Cancel/OK/Reset baselines, callback destruction/reopen, deferred transitions, and editor-wide history isolation.
- Root configures/builds affected portable CPU targets, runs focused normal and ASan tests plus strict OpenSpec/whitespace checks, and records actual results. No performance, native visual, Windows, or GPU claim follows from CPU geometry tests.

## Rollout / Remaining Work

This change is limited to T11-B and requires no stored-data migration. T11-C persistence/application, other dialog composition, and Windows/native display/DPI/GPU acceptance remain separate pending work. Prior changes and main specifications are not archived or rewritten during implementation. Commit and archive still require explicit user confirmation.
