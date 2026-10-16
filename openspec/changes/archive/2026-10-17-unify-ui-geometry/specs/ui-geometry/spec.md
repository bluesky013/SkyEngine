## ADDED Requirements

### Requirement: Pure geometry and affine utilities
UI.Interface SHALL expose a pure UIGeometry value containing absolute layout-surface bounds and a cumulative layout-to-rendered-surface affine transform. That target in engine/ui SHALL provide affine inversion and transformed-rectangle AABB operations without referencing UIElement, UIContext, renderer implementations, or plugin headers. Inversion SHALL report non-invertible transforms rather than produce undefined pointer coordinates.

#### Scenario: Rectangle maps through all corners
- **WHEN** an affine transform rotates or scales a rectangle
- **THEN** TransformRect SHALL return the axis-aligned bounds of all four transformed corners

#### Scenario: Inverse round-trip
- **WHEN** a point is mapped through an invertible affine transform and its inverse
- **THEN** the result SHALL recover the original point within floating-point tolerance

#### Scenario: Singular inverse is rejected
- **WHEN** an affine transform has no inverse
- **THEN** inverse computation SHALL report failure rather than return a usable pointer mapping

#### Scenario: Interface builds independently
- **WHEN** a consumer includes the geometry utilities using UI.Interface without UI runtime or renderer targets
- **THEN** it SHALL compile and use those pure data operations without plugin implementation dependencies

### Requirement: On-demand accumulated element geometry
UIElement::GetGeometry(extraTransform = {}) SHALL return its existing absolute layout bounds and the configured context surface base composed with all ancestor and element render transforms in paint order. The default query SHALL include that base without requiring the caller to pass it. An optional extra compositor transform SHALL premultiply the complete accumulated result rather than replace the base. The query SHALL work before paint after bounds have been assigned. It SHALL NOT change GetBounds/Arrange into parent-local coordinates, subtract the element origin during inverse mapping, or require a geometry cache or invalidation redesign.

#### Scenario: Nested transforms compose in paint order
- **WHEN** an element under transformed ancestors is queried
- **THEN** its geometry transform SHALL equal surfaceBase * root * successive ancestors * element

#### Scenario: Extra compositor preserves the configured base
- **WHEN** an element is queried with an extra compositor transform
- **THEN** that transform SHALL premultiply the result including the existing context surface base

#### Scenario: Geometry is available before painting
- **WHEN** a subtree has been arranged and GetGeometry is queried before first paint
- **THEN** it SHALL return the assigned bounds and current accumulated transform

#### Scenario: Absolute layout origin is preserved
- **WHEN** an element whose layout bounds start at x=100 is translated by 20 and a surface point at x=120 is mapped back
- **THEN** the layout x coordinate SHALL be 100 rather than zero

### Requirement: Persistent context surface transform
UIContext SHALL expose SetSurfaceTransform/GetSurfaceTransform with identity as the default and one authoritative surface-base state shared by context configuration, root painting, and default element geometry queries. Ordinary Paint and pointer routing SHALL use that same base. The compatibility Paint(context, rootTransform) entry SHALL store its transform before using the ordinary paint path. The context's root surface clip and existing content-size/DPI rules SHALL remain unchanged.

#### Scenario: Input before first paint
- **WHEN** the host arranges a context and sets its surface transform before any paint
- **THEN** its router SHALL use that transform for the first pointer hit-test and delivery

#### Scenario: Compatibility paint configures future input
- **WHEN** a caller paints with the explicit rootTransform overload
- **THEN** subsequent ordinary paint and input SHALL use the stored rootTransform

#### Scenario: Root surface clip is stable
- **WHEN** a surface transform translates or scales UI content
- **THEN** the root clip SHALL remain the configured surface rectangle rather than being transformed again

### Requirement: Transformed clipping and quad culling
PushClip SHALL transform its layout rectangle through the current cumulative matrix, convert it to a rendered-surface AABB, and intersect that with the active surface clip. Quad emission SHALL transform its corners before deciding whether its surface AABB is outside that clip. Rotated clipping SHALL be explicitly conservative and axis-aligned for scissor rendering rather than exact polygon clipping.

#### Scenario: Transformed content enters the clip
- **WHEN** a quad lies outside the clip in layout coordinates but its render transform moves it into view
- **THEN** it SHALL NOT be culled using its original layout rectangle

#### Scenario: Nested transformed clips intersect
- **WHEN** nested clipping rectangles have different accumulated transforms
- **THEN** the effective draw clip SHALL be the intersection of their transformed surface AABBs and the root surface clip

#### Scenario: Rotated clip is conservative
- **WHEN** a clipping rectangle is rotated
- **THEN** clipping SHALL use its surface AABB, with no claim of an exact rotated-polygon mask
