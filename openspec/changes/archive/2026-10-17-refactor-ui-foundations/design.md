## Context

The retained UI toolkit already has useful boundaries: render-agnostic `UIDrawData`, provider seams for fonts/textures/assets/data, a widget tree, and separate Aurora rendering. The original runtime target also contained document editing and an optional FreeType implementation; some slot containers arranged children from `OnPaint`. Those dependency and execution boundaries need to be explicit.

This change starts the larger UI architecture optimization with two independently verifiable foundations: separate target responsibilities, and make resolved child geometry available before paint. The user's explicit placement decision keeps UI implementation within `engine/ui` as foundation infrastructure. This is an exception to the general engine/interface-only feature rule, not permission to place unrelated feature implementations in engine modules. The toolkit remains usable by the Sandbox host and feature editor views.

## Goals / Non-Goals

**Goals:**

- Keep UI contracts and implementations within the `engine/ui` foundation while separating `UI.Interface`, runtime, document editing, FreeType and Aurora targets.
- Separate runtime, document editing, FreeType, and Aurora targets with one-way dependencies.
- Preserve existing `UI`, `UIRender`, and `ui/...` integration points where their responsibilities remain the same.
- Give slot containers and host compositions one recursive arrangement entry that completes geometry before paint and input hit-testing.
- Add focused regression coverage for nested containers, geometry before paint, and dependency separation.

**Non-Goals:**

- Replacing the current unconstrained `Measure(float &, float &)` API with constrained measurement.
- Introducing a new full Geometry type, transformed hit-test model, or hierarchy/layout/paint/resource cache system.
- Completing all widget invalidation propagation or migrating every custom editor view to a new layout model.
- Extracting EditorWidgets, redesigning surface/document/device ownership, separating document definitions from instances, or unifying themes in this change.
- Moving the entire Sandbox/editor framework or other engine features into plugins.
- Introducing a PipelinePass or RDG integration for the existing encoder-based UI renderer.

## Decisions

### Keep a small contract target within the UI foundation

The root `engine/ui` target `UI.Interface` exposes `IUIAssetResolver`, `IUIFontProvider`, `IUITextureRegistry`, `IUIDataProvider`, the types needed by those seams, `UIDrawData`, `UIRect`, `UILayout`, and `UIProperty`, plus pure utility/data construction code. That target does not expose widget-tree, document-loader, editor, font-rasterizer, or GPU implementation headers. This restriction applies to `UI.Interface`, not to the entire `engine/ui` directory.

`engine/ui/core` owns the retained toolkit, widgets, theme resolution, input routing, document schema/loading/bindings/builders, animation, localization, and fallback text implementation. It retains target `UI`, links `UI.Interface`, `Core`, and the document parser, and does not link Framework, editor, FreeType, or Aurora implementations. Public include prefixes remain `ui/...`; each consumer links the target whose API it uses.

The foundation root configures its four implementation subdirectories: `core`, `render`, optional `freetype`, and `editor` when the configured host/tests need it. There is no UI feature-plugin entry. Sandbox/editor hosts may continue direct C++ widget composition through `UI`; no abstract factory for every widget or interface-only host migration is required by this placement decision. General feature/plugin ownership rules elsewhere remain unchanged.

Alternative: move files while retaining a monolithic target. This would preserve build behavior but keep runtime transitively coupled to editor and concrete font backends. Alternative: introduce an abstract interface for every widget immediately. That would substantially change the extension and ownership model without addressing the immediate geometry-ordering bug.

### Separate implementation targets by responsibility

| Target | Location | Responsibility and dependencies |
|---|---|---|
| `UI.Interface` | `engine/ui` | Provider contracts, interface data, and pure utilities; no implementation dependencies |
| `UI` | `engine/ui/core` | Retained runtime toolkit; `UI.Interface`, Core, document parser |
| `UI.Editor` | `engine/ui/editor` | Headless document editing/serialization/undo; UI runtime, without a visual shell |
| `UIRender` | `engine/ui/render` | Aurora draw-data translation; `UI.Interface`, `Aurora.RHI.Static`, `Aurora.ShaderCompiler.Static` |
| `UI.FreeType` | `engine/ui/freetype` | Optional `IUIFontProvider`; `UI.Interface`, Core, FreeType |

`UI.FreeType` exists only when `SKY_BUILD_FREETYPE` is enabled. Its availability definition is propagated by the adaptor target, and callers using the provider link it explicitly. Runtime UI keeps the built-in fallback and remains buildable without this target. EditorRender and the FreeType tests opt into the adaptor; the document-editor tests opt into `UI.Editor`. Runtime tests no longer pull the editor implementation into `UI`.

Alternative: continue conditional FreeType sources in `UI`. This makes enabling a font backend change the core's dependency graph and prevents the build from proving that the core is backend-independent.

### Keep the renderer on the draw-data contract

`UIRender` consumes `UIDrawData` and implements `IUITextureRegistry`, so it only needs `UI.Interface` rather than the widget runtime. Remove the unused `Aurora.Pipeline.Static` dependency and keep the RHI/shader-compiler dependencies actually used by its graphics-encoder implementation. A producer can supply draw data without constructing widgets or linking `UI`.

This change preserves the renderer's current encoder-based integration. It does not implement or claim validation of the PipelinePass/RDG integration described elsewhere in the existing `ui-render` specification; that specification/implementation gap needs a separate rendering integration change. No renderer command-recording model change is part of this milestone.

### Distinguish layout requests from resolved arrangement

Keep `Layout(parentRect)` as the compatibility path that resolves an element's existing anchor/size request. Add `Arrange(finalRect)` for a parent that has already resolved the child's rectangle. The common entry stores that rectangle and recursively invokes the element's `ArrangeChildren` hook with its content area. Slot containers call `child->Arrange(rect)` rather than writing only `SetBounds(rect)`; this allows nested containers to complete their descendants during the same traversal and avoids applying anchor/size rules a second time to an already allocated slot.

These rectangles keep the toolkit's existing absolute surface-coordinate convention. Arrangement does not introduce parent-local Geometry, transform accumulation, or a new coordinate-conversion API. A complete Geometry system remains follow-up work alongside constrained measurement and transformed hit-testing.

StackPanel, GridPanel, DockPanel, and Overlay implement their allocation in `ArrangeChildren`. Any retained zero-argument convenience `Layout()` delegates to the common arrangement path. The current HBox/VBox and default element layout semantics continue to work through the same hook.

`Measure(float &, float &)` remains the existing intrinsic-size query. This change does not represent it as an available-size constraint pass or promise that fill, percent, wrapping, and min/max rules have all been unified. Existing slot sizing is preserved except for corrections necessary to make the same declared allocation available before paint.

Alternative: call the old private arrange helpers from `OnPaint` and add more manual pre-paint calls. This retains multiple execution paths and leaves nested descendants dependent on which host remembered to force layout.

### Make paint consume completed geometry in the migrated paths

`UIElement::Paint` and the built-in slot containers emit draw data using completed bounds; they do not arrange children as a side effect of drawing. Sandbox and material-editor compositions affected by the slot containers move their layout calls into their layout/arrangement path so that reading child bounds, routing input, and first paint all observe the same allocation.

Painting twice without an intervening layout change must not move those children or trigger a new arrangement. Resizing is handled by arranging the parent with its new rectangle. This establishes an execution-order contract; it is not a new dirty-subtree/cache optimization, and this change does not claim to have eliminated all pre-existing custom view invalidation issues.

## Risks / Trade-offs

- [CMake ordering and transitive include propagation change] -> Preserve the established `UI`/`UIRender` names and `ui/...` includes, add explicit editor/font dependencies, reconfigure before building, and verify runtime-only and editor configurations.
- [Previously implicit editor/font dependencies become missing symbols or headers] -> Build separate `UIEditorTest`/FreeType tests and affected EditorRender/Sandbox/material-editor targets.
- [Nested layout exposes previously paint-corrected geometry] -> Test resolved bounds before any paint, nested slot containers, a second arrangement after resize, and paint stability.
- [Partial architecture milestone could be mistaken for the complete UI redesign] -> Keep deferred decisions in the roadmap below, and do not mark them as implementation tasks or claim constrained measurement/Geometry/cache completion.
- [Foundation exception becomes an accidental general layering exemption] -> Limit it to UI foundation targets; retain backend/editor separation and the general feature/plugin ownership rules elsewhere.

## Migration Plan

1. Keep root contracts/pure functions in `UI.Interface`, with runtime/editor/render/font-provider implementations in their separate `engine/ui` subtargets, without changing include prefixes.
2. Rewire CMake dependencies, explicit FreeType/editor callers, and separate document-editor tests.
3. Add the common resolved-rectangle arrangement entry, migrate built-in slot containers, and update affected host compositions.
4. Reconfigure and run focused UI and editor integration builds/tests; verify strict OpenSpec validation. Record unavailable platform validation instead of treating it as passed.

No persistent document format changes. Rollback is a source/build rollback of this change as one unit; moving only the spec or one side of the target split would leave dependencies inconsistent. Commit and OpenSpec archive remain separate user-confirmed actions under `AGENTS.md`.

Placement revision (2026-10-17): this active design now reflects the user-directed return from the earlier plugin placement to `engine/ui`. The prior test/build record remains historical; it does not establish that those runs originally used these paths. The relocation and latest review are verified separately in `simplify-ui-foundation-boundaries` and canonical §19.

## Follow-up Roadmap

These are architectural next steps, not this change's delivery checklist:

1. **Measurement and geometry:** design available-size constraints, intrinsic/desired size semantics, container/slot ownership, and a single geometry representation shared by paint and transformed hit-testing. Define invalidation propagation before selecting caches.
2. **Shared editor widgets:** extract reusable reflected forms, field editors, UiSkin, and action-driven chrome behind a module that feature editors can use without the Shell. Preserve reflected configuration editing, CommandService undo, EditorActionRegistry, and theme metrics.
3. **Context ownership:** define surface focus/capture/modal state, document binding/undo/dirty state, and device texture/atlas/pipeline lifetime. Use `Singleton<T>` only for genuinely process-shared services and `sky::Once` for cross-DLL one-time registration.
4. **Document definition and instances:** separate serialized node definitions with stable IDs from live widgets/subscriptions; keep JSON and C++ builders on one instantiation/property protocol before adding templates or overrides.
5. **Theme protocol:** share type/state/variant/override resolution while allowing different game and editor theme data; retain a clear render-agnostic draw-data boundary.
6. **Renderer integration:** reconcile the existing encoder renderer with the `ui-render` PipelinePass/RDG requirements in a separate change, including frame-data ownership and validation of the chosen recording path.

## Open Questions

- No question blocks this milestone. The abstract widget-factory/host boundary and the exact constrained-measurement API require dedicated follow-up designs.
