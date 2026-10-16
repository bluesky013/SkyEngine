## Context

The previous five changes establish runtime/editor separation, shared editor controls, common geometry, constrained measurement, and opt-in incremental layout. Native controlled branches can now skip unchanged work, but editor hosts remain conservative because several layout inputs are observed implicitly. ReflectedFormView builds row rectangles before paint but only updates contentHeight while drawing. WorldConfigPanel discovers document changes inside ArrangeChildren. AssetViewer repeatedly forces its whole composition and consumes form heights that may describe the previous frame. The shared theme mutates at a stable address without a revision, so unchanged allocated rectangles cannot prove that metric-dependent descendants remain valid.

## Goals / Non-Goals

**Goals:**

- Make form geometry/extent ready before input and paint, including the first frame and structural changes.
- Represent injected property/command and known world-document dependencies explicitly before host layout.
- Reuse unchanged layout for audited editor hosts while preserving unknown/custom polling behavior.
- Invalidate cached measurements and internal arrangements when shared theme metrics/fonts change, including same-scale changes and floating surfaces.
- Preserve form edit/popup lifetime, undoable reflected editing, shared chrome, existing constrained geometry, and complete per-frame paint output.

**Non-Goals:**

- Automatically observing arbitrary writes to bound objects, custom themes, widget implementations, font providers, or subsystem registries.
- Replacing the runtime invalidation/cache design, introducing a general reactive binding/event system, or globally opting all editor widgets out of volatility.
- Rewriting remaining hand-computed editor layouts, adding per-config forms, changing asset persistence/cook semantics, or renderer/geometry/draw-list caching.

## Decisions

### Keep dependency ownership in the editor framework

Reflected form synchronization and preferred-extent logic remain in EditorWidgets. Shared default-theme state remains in EditorUi, and document/surface orchestration remains in EditorShell. No runtime UI class depends on editor state. The existing ReflectedFormContext continues to inject host-owned command history, property metadata/change notifier, and theme; those services must outlive the view. No new process-global form state is introduced.

### Compute form extent from rows rather than the previous paint

The form's row construction calculates its full content extent using the same sections, expansion state, widget row counts, and theme metrics that painting consumes. GetPreferredHeight is therefore valid immediately after Bind/Refresh and before OnPaint. OnPaint consumes prepared row/extent state and does not supply the only correct height for the next frame. Bounds, scroll origin, or a previous narrow viewport must not truncate the natural extent.

Bind, Refresh, header visibility, expanded/collapsed structures, sequence membership, and widget registry changes affecting row count notify desired-size/layout invalidation. Each form's registry reports successful additions/replacements through a per-instance callback that invalidates layout without rebuilding the form or ending an active interaction. Changes inside an already-registered custom widget's arbitrary RowCount state are not automatically observed: the owner calls Refresh or keeps the form volatile. Scrolling changes row placement and invalidates arrangement/paint, while hover/caret/pressed appearance remains paint-only where possible. Existing mutable model access cannot silently discover arbitrary external changes; callers retain the explicit Refresh contract.

The structural regressions also require correcting indexed PropertyDescriptor semantics: an element descriptor retains its owning member metadata, but represents that element rather than the container. IsSequence excludes IsSequenceElement, matching the existing GetSequenceCount/BuildSequenceChildren boundary. The indexed descriptor's type is the sequence valueType's TypeInfo rather than the owning container's type; otherwise the form's editor-kind inference still falls back to Unknown after excluding the fictitious nested sequence. A vector<float> child exposes float type/value, selects the Float editor, is not presented as another sequence, and cannot produce an add-container command. Otherwise form add/remove input targets fictitious sequence rows and corrupts the preferred row extent. This targeted model correction remains in EditorCore and does not add UI dependencies or redesign nested-container editing.

### Synchronize external form changes before host allocation

Expose a public SynchronizeLayout step. PropertyChangeNotifier subscriptions and CommandService change callbacks mark a pending refresh and invalidate the view's layout path immediately. The pre-layout step consumes pending changes at a safe point, before a host asks for preferred height or allocates children. The property revision remains a defensive consistency check. Subscriptions are removed when the view is destroyed, and callbacks do not rebuild fields synchronously during a command or property-notification dispatch.

An active popup, drag, or text edit keeps its existing field/widget lifetime. Pending external refresh stays pending while the interaction makes rebuild unsafe and is consumed on a later safe synchronization even if the service revision does not advance again. Bind/Refresh explicitly reset interactions as before. Undo/redo invoked from a different host sharing the command service must become visible before the next layout, rather than requiring a preliminary Paint.

Audited host-owned forms explicitly adopt controlled layout after their synchronization path is wired. Default standalone forms and unknown derived forms retain conservative volatility where extension hooks or mutable local theme/widget dependencies are unnotified. A derived form introducing OnViewTick layout changes must preserve that conservative policy or provide its own notification contract. Callback registration/removal does not replace per-form interaction ownership.

### Make world-document binding explicit without breaking provider callers

WorldConfigPanel gains an explicit document setter and a refresh notification for changes that can replace subsystem config storage while the WorldDocument pointer remains the same. Explicit binding resets selection/rebinds the reflected config as necessary, invalidates layout, and can adopt the controlled host policy. SynchronizeLayout prepares its child form before allocation. EditorShell forwards document open/close/replacement through this explicit path, including floating/detached panel instances.

The existing DocumentProvider constructor remains supported and layout-volatile: a provider may return a different document without notification. Its synchronization/arrangement keeps observing that provider, so custom hosts retain their existing behavior. Explicit refresh is required when an owner mutates/rebuilds a document's config storage externally; pointer equality is insufficient to validate a bound Any data address. Repeating EditorShell::SetWorldDocument with the same pointer forwards a refresh to its applicable views, so the shell also provides this notification boundary. Selection and enable toggles continue to rebind after mutations that may relocate config data.

### Migrate AssetViewer's audited composition

AssetViewer's own structural nodes, owned forms, and chrome adopt controlled policy only after their relevant mutations notify layout. Before measuring/stacking forms, the viewer synchronizes them so contentTotal, scrollbar reservation, scroll clamp, and final child rectangles reflect the current model in the same layout pass. Existing custom provider measurement uses the actual content width after scrollbar reservation.

Shell synchronizes before ArrangeIfNeeded so pending work is visible before a skip decision. AssetViewer also synchronizes inside an executing ArrangeChildren before extent calculation, preserving existing standalone callers that use forced Arrange followed by Paint. This defensive arrangement step does not make measurement queries mutate the form and does not prevent unchanged controlled branches from skipping.

Open/reload/rebuilt content and resized panel dimensions invalidate desired-size/allocation work. Outer scroll changes invalidate arrangement without unnecessarily discarding desired sizes. A later unchanged frame can use ArrangeIfNeeded without re-running viewer arrangement. Reopening at the same outer bounds still lays out new content because its explicit invalidation survives the unchanged rectangle. Theme synchronization also refreshes copied chrome inputs, including the toolbar DockSlot extent and menu bar height; marking nodes dirty alone cannot update constructor-copied values. DockPanel exposes a live-child slot update entry with the same ownership/validation semantics as its existing slot model.

Unknown custom provider elements keep their own layout policy; default volatility flows through the subtree count and prevents unsafe skipping of the viewer branch. No host forcibly declares custom provider content stable. This preserves providers that read external state without notifications while allowing an audited provider to opt in through the runtime contract.

### Synchronize known hosts at the shell boundary

Before requesting an incremental allocation of a known migrated form/document/viewer host, EditorShell calls its explicit synchronization entry. The shell uses ArrangeIfNeeded for the migrated composition path, including applicable floating-surface views. Unchanged controlled child branches can reuse their allocation; unknown views remain default-volatile and execute through the same compatible entry. Existing public forced Arrange/Layout APIs retain their previous contract.

Shell notification reaches both attached main-window views and floating/preserved views. A document change or pending form refresh must not be invisible because its previous rectangle is unchanged. Remaining unrelated editor polls are deliberately left conservative.

### Track default-theme changes across DLLs and surfaces

Add a monotonically advancing revision beside the default UiTheme value in its existing sky::Singleton state. SetDefaultUiTheme keeps the value's stable address and advances that shared revision on every setter call, including same-scale updates. Modules linked to separate static EditorUi copies must observe the same value and revision; no function-local static counter is used.

Main and floating surfaces remember the revision last applied to their layout. PaintSurface selects the effective surface DPI/theme before synchronization and allocation. A changed revision invalidates layout/measurement state for every node in that surface's live tree before incremental layout. Invalidating only the root is insufficient: a child whose final rectangle stays equal can otherwise skip despite changed row metrics, fonts, or padding. The deliberately infrequent whole-tree walk trades theme-change work for safe ordinary-frame reuse.

Preserved hidden/detached views may miss that live-tree walk. When a view is adopted into the main tree or transferred into a surface, its entire subtree is synchronized against the active theme even if the destination surface revision is already current. Invalidating only the reattached view root cannot discard unchanged descendants' old measurements. Main-window MenuBar also updates its stored barHeight from the active theme before allocation, just as the viewer refreshes its copied chrome inputs.

Different-DPI surfaces may switch the shared default theme between their paints. Each affected surface must invalidate against the active revision before reuse; this can cause conservative work when alternating scales and makes no claim of cross-DPI layout-cache reuse. A same-DPI unchanged frame with no theme setter keeps its revision and remains eligible for reuse. Local independently mutable themes remain the owner's notification responsibility or retain volatile policy.

## Risks / Trade-offs

- [Form geometry is still one frame late] -> Derive extent during row preparation and synchronize external changes before host height queries; test without any preliminary paint.
- [Refresh invalidates an active field/widget pointer] -> Notification callbacks only defer refresh and invalidate; the safe pre-layout step retains pending work through active interactions and unsubscribes at destruction.
- [Document pointer stays equal while config storage moves] -> Provide an explicit refresh/rebind notification rather than treating pointer identity as a revision.
- [Opt-in hides unknown provider/derived behavior] -> Host-owned audited nodes opt in deliberately; legacy provider/unknown custom descendants remain volatile.
- [Theme changes leave nested caches stale] -> Keep a shared revision and invalidate every live surface node, not only its ancestor root.
- [Multiple DPI surfaces cause extra invalidation] -> Accept conservative re-layout when the shared default theme switches; per-surface theme ownership is deferred.
- [Dirty state raised during synchronization/layout is lost] -> Retain the preceding generation-aware runtime contract and verify a later unchanged pass settles without repeated rebuilds.

## Migration Plan

1. Add form row extent, notification subscriptions, safe synchronization, structural mutation invalidation, and an explicit controlled-host policy.
2. Add explicit WorldConfigPanel document/refresh integration and retain its legacy provider path.
3. Migrate AssetViewer's owned nodes/setters and shell's known-host synchronization/incremental calls.
4. Add shared theme revision, complete per-surface invalidation, and regressions for same-scale and floating/DPI behavior.
5. Build affected editor/runtime consumers, run focused regressions and strict validation, and record actual unavailable platform/GPU checks.

No stored data migration is required. The five preceding uncommitted changes remain intact. Commit and OpenSpec archive require separate explicit user confirmation.

## Validation Strategy

- Query preferred height after Bind, expanded/collapsed nested fields, sequence changes, widget row-count replacement, and header toggling without first painting; compare actual host allocations and scroll extents. Exercise actual form sequence add/remove/undo with Float child editors, and assert indexed scalar descriptors expose their element type/value, are not sequences, and reject container-add commands in EditorCoreTest.
- Notify injected property changes and execute external undo/redo, synchronize and lay out, and verify updated rows in the same pass. Keep an interaction active across notification, then finish it and confirm pending refresh is consumed once. Destroy views and verify subscriptions no longer call them.
- Count arrangement/measurement hooks for controlled forms/viewer branches over unchanged frames, resize/reopen/reload/scroll mutations, and branches containing default-volatile custom providers.
- Switch world documents through explicit binding, refresh relocated config storage at the same document address, and retain provider compatibility without a setter notification.
- Verify default theme value/revision is shared across DLLs; change metrics at the same scale after a clean layout and confirm nested bounds/heights and stored chrome heights update. Exercise main/floating surface revision application, alternating surface DPI, and hide/theme-change/show reattachment conservatively.
- Build relevant editor consumers, run core/property/UI/editor/widget/shell/render regression suites, validate the change strictly, and check whitespace/layering. Record unrun native GPU/Win32 checks without claiming visual or timing benchmarks.
