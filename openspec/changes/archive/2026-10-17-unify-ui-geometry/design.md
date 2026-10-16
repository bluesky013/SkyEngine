## Context

The preceding UI changes establish parent-driven arrangement and reusable editor widgets. Layout rectangles are already absolute in the layout surface's coordinate system, and widgets compare incoming `event.x/y` directly with those rectangles. That compatibility boundary is useful and remains in place.

The current transform path is incomplete: UIElement pushes matrices during Paint, while UIEventRouter and direct child routing test raw bounds. UIPaintContext intersects untransformed clip rectangles and culls quads before transforming their vertices. Rounded-shape shader data is authored in layout coordinates, but its SDF uses pixel shader framebuffer position. A surface transform can also currently arrive only through a paint argument, leaving no shared configuration for input before first paint.

## Goals / Non-Goals

**Goals:**

- Use a single affine geometry contract for draw positions, transformed AABBs, hit-tests, and per-receiver pointer coordinates.
- Preserve absolute layout rectangles and existing identity-transform handler behavior.
- Configure a context's surface transform before input or paint and preserve the explicit Paint overload.
- Keep clipping compatible with axis-aligned scissor rendering, with conservative bounds for rotations.
- Make transformed rounded shapes evaluate their SDF in the same coordinate system as their authored shape data.
- Verify pure geometry without widget/render dependencies and validate the actual renderer shader source offline where the compiler is available.

**Non-Goals:**

- Constrained Measure, wrapping text, min/max or slot sizing redesign, and ScrollView content-extent changes.
- Parent-local layout coordinates, automatic origin subtraction, or a full editor view migration.
- Geometry caching, dirty/invalidation semantics or performance optimization, and replacing the current layout traversal.
- Exact polygon/stencil clipping, RDG/PipelinePass integration, frame-resource lifetime redesign, or host surface ownership.
- General pointer-capture policy or focus lifecycle redesign beyond transformed-coordinate delivery and unusable-geometry cancellation.

## Decisions

### Keep geometry pure and layout coordinates compatible

`engine/ui` exposes `UIGeometry` containing the element's existing bounds and a cumulative `layoutToSurface` affine transform. Affine inverse and TransformRect AABB operations are pure data utilities in `UI.Interface`. They do not reference UIElement, UIContext, a renderer, or plugin headers. Runtime tree traversal and painting/input behavior stay in `engine/ui/core`.

There are two coordinate spaces in this change:

- **Layout surface:** the absolute rectangles already returned by GetBounds and assigned by Arrange. They do not become parent-local.
- **Rendered surface:** the coordinates after applying the context surface transform and the element/ancestor render transforms, used by draw vertices, incoming router pointer positions, and scissor rectangles.

The transform's existing pivot/translation convention is retained; no implicit translation by an element's bounds origin is added. Mapping a pointer back into an element means applying the inverse accumulated transform, without subtracting `bounds.left/top`. For example, an element at layout x=100 translated by 20 paints at x=120; its handler receives x=100 for a pointer at rendered x=120.

`UIElement::GetGeometry(extraTransform = {})` computes its accumulated transform on demand, including its context's surface base by default, then composing ancestors and the element in paint order. The optional argument is an additional compositor transform, not a replacement for the configured base: a root-to-leaf chain uses `extra * surfaceBase * root * ... * element`. Callers and direct-routing helpers do not have to fetch and pass the context base manually. The query does not write a cache or change bounds, and it works before first paint once arrangement has supplied the bounds. Non-invertible geometry is detected rather than producing an invalid hit-test point.

Alternative: migrate all widgets to local-origin rectangles and introduce cached local/world Geometry immediately. That would expand the change into editor composition, measurement, and invalidation, while leaving more compatibility work than the current rendering/input mismatch requires.

### Store the surface transform in UIContext

Add SetSurfaceTransform/GetSurfaceTransform and default to identity. Keep a single authoritative surface-base state visible to context configuration, root painting, and default element geometry queries rather than storing separate context and element copies. The ordinary Paint entry uses that configured base; the compatibility `Paint(context, rootTransform)` entry first stores its argument and then paints through the same path. Input and direct helpers use geometry that includes the same base, so input before first paint works when the host sets the transform explicitly. The backing-state placement is internal and does not add a second public transform configuration path.

The context root surface bounds remain the viewport clip in rendered-surface coordinates; the root clip is not transformed a second time. SetContentSize/SetDpiScale retain their current behavior. A surface transform is not another DPI rule and does not multiply logical content size again.

Alternative: remember a transform only in paint/router internals. That leaves first-input behavior dependent on whether paint has already occurred and permits the two paths to disagree.

### Transform clipping and culling into the rendered surface

UIPaintContext continues to accumulate a matrix stack. PushClip converts the incoming layout rectangle to the AABB of its four transformed corners, then intersects that AABB with the active rendered-surface clip. Begin installs the unchanged root surface clip. Quad emission first transforms its corners, derives the rendered AABB, and tests that against the active clip, so geometry translated into view is not discarded using its old layout position.

Element hit-testing applies the same accumulated transforms and conservative clip intersection. Rotated clip bounds are explicitly an AABB approximation, not a rotated polygon mask. The existing renderer consumes the resulting surface clip as a scissor. Exact clipping is deferred rather than approximated silently.

Alternative: transform vertices only. That preserves the current invisible/misaligned input and clip behavior. Alternative: introduce stencil/polygon clipping. It requires a wider renderer contract and is not necessary to fix the geometry seam.

### Map pointer coordinates independently for every receiver

UIEventRouter accepts pointer x/y in rendered-surface coordinates. Hit-testing walks reverse stable paint/z order, applies the effective clip in rendered space, and inverse-maps the point into each element's layout bounds. A non-invertible element excludes its subtree from hit-testing and pointer dispatch.

For dispatch, preserve the original surface point and derive a receiver-specific event copy for the target and every bubbling ancestor. OnPointerEnter/Leave use each hovered receiver's own geometry. Captured move/up use the captured receiver's current transform, preserving pointer id/button/action/wheel fields and per-pointer capture isolation. If a captured target becomes non-invertible, do not call its pointer handler with undefined coordinates; this change does not otherwise redesign capture policy.

Receiver `event.x/y` stays in that receiver's untransformed absolute layout-surface coordinates. Converting only the initial hit and then forwarding its mapped event unchanged to ancestors would be wrong when ancestor and target transforms differ. Converting only hit-testing without mapping events would also leave Button and editor fields comparing surface points against untransformed bounds.

### Cancel a captured gesture when geometry becomes unusable

Skipping an unmappable captured move/up is insufficient: a Button can remain pressed, or a form can retain drag/live-preview state after the release is discarded. Add `UIElement::OnPointerCancel(pointerId)` as a coordinate-free notification, with a no-op default. When the router cannot map a captured pointer through its current geometry, it clears that pointer's capture before callbacks and notifies the target and its ancestors without inventing x/y coordinates. Normal mapped move/up delivery does not occur for that failed dispatch.

Direct ChildRouting likewise clears an unmappable captured child and calls its cancellation notification. Slot containers can forward cancellation to their own directly captured child, clearing that reference first. Button clears its pressed gesture without producing a click; ReflectedFormView cancels active drag/popup/live-preview state so preview cancellation restores uncommitted data while its fields still exist. Cancellation handlers must be safe when the same pointer has no active gesture.

This is bounded cleanup for geometry becoming non-invertible, including a singular context surface base. It does not introduce a general capture-reply protocol, change focus ownership, or redesign normal input policy. Other pointers retain independent router captures.

### Direct routing uses parent-layout input

Direct ChildRouting has a different entry contract: it is called from a parent's handler with parent-layout coordinates. It applies each child's local inverse, not the full accumulated inverse again, before the child's hit-test and handler; deeper direct routes repeat that relative conversion. The parent's default GetGeometry can map the incoming point back into the rendered surface to check the transformed AABBs of ancestors that declare ClipsChildren for uncaptured target selection; this includes the configured surface base automatically. The helper has no UIContext/paint-context viewport rectangle and does not treat a detached root's GetBounds as one. Its caller enforces the actual viewport through the host or UIEventRouter's fixed context surface clip. A non-invertible surface base or accumulated path excludes dispatch. Captured direct-child input follows the same relative-coordinate rule without turning capture into an ordinary clipped hit-test. Child order uses reverse stable paint/z order rather than vector-order switches that can disagree with painting.

### Evaluate rounded shapes in layout coordinates

Rounded-shape center, half-size, and radius remain authored in layout coordinates. A shape quad's UV carries its original layout corner positions; the fragment shader interpolates those positions and evaluates its SDF with them rather than framebuffer `SV_Position.xy`. Render transforms affect rasterized positions; the shape's own distance calculation remains coherent under translation, rotation, and scale. Texture quads retain their existing texture UV meaning; the layout-coordinate interpretation applies to the shape path.

Keep the actual shader source in one shared renderer source artifact consumed by runtime shader compilation and an offline compiler check. Do not validate a separately copied test shader that can drift from runtime. This shader adjustment does not change the current encoder-based renderer into a PipelinePass or RDG integration.

## Risks / Trade-offs

- [Existing handlers assume absolute layout x/y] -> Preserve that convention and test translated elements with nonzero bounds origins and independently transformed bubbling ancestors.
- [Rotated clip AABBs include pixels outside the rotated rectangle] -> Specify the conservative scissor behavior in requirements and tests; exact polygon masks remain out of scope.
- [Singular transforms yield invalid inverses or leave a captured gesture pressed] -> Detect inversion failure, exclude affected hit targets, and clear unusable capture with coordinate-free cancellation of the active gesture.
- [Surface transforms accidentally affect root clipping or DPI twice] -> Keep Begin's surface clip unchanged and test configured transforms before first paint and through the compatibility Paint overload.
- [Direct child routes apply the full transform twice] -> Treat their input explicitly as parent-layout coordinates and test nested direct routes/capture with z overlap and clipping.
- [Rounded-shape CPU data and shader coordinates diverge] -> Test shape quad payload and compile the actual shared shader source offline, with unavailable backend/compiler checks recorded explicitly.
- [On-demand ancestor composition repeats work] -> Accept the small correctness-first traversal; caching/invalidation is a later, separately measured change.

## Migration Plan

1. Preserve both preceding changes and add pure geometry utilities to UI.Interface.
2. Connect UIElement/GetGeometry and UIContext surface configuration without changing layout or Measure contracts.
3. Update paint clipping/culling and pointer/direct-child delivery to use the same affine geometry.
4. Adjust rounded-shape coordinate interpolation and share its actual shader source.
5. Update the layout/chrome plan's completed state and geometry conventions; run focused tests, offline shader validation, relevant integration builds, and strict OpenSpec validation.

No persisted format migration. Rollback this geometry/shader/input change together while keeping the preceding framework changes intact. Commit and archive remain user-confirmed operations.

## Validation Strategy

- Pure inverse/composition/AABB tests, including identity, nonzero translation, rotation/scale, and singular transforms.
- Pre-paint GetGeometry and UIContext configured-surface input tests; compatibility Paint stores the same transform used afterward by input.
- Button click, bubbling, hover enter/leave, captured movement after transform changes, multiple pointers, non-invertible targets, and direct-child routing tests.
- Capture a Button or reflected drag/preview, make the element/ancestor/surface transform singular, dispatch move/up, and verify coordinate-free cancellation clears capture and gesture state without a click or a stale preview; verify a different pointer's capture remains intact.
- Paint tests for clip transformation, nested conservative AABB intersections, transformed-into-view culling, stable root surface clipping, and shape coordinate payload.
- Offline compilation of the actual shared runtime shader source for supported available targets, plus affected UI/editor renderer builds and retained integration tests.
- Strict OpenSpec validation; record unavailable GPU/backend/platform checks as limitations, not passes.

## Follow-up Work

- Define constrained measurement and ScrollView content extents separately.
- Design invalidation propagation and any geometry/paint caches after correctness is established.
- Reconcile RDG integration and host surface/device ownership in their own changes.

## Open Questions

- No question blocks this scope; exact rotated clipping, constrained measurement, and cached geometry remain deferred.
