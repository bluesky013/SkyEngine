## MODIFIED Requirements

### Requirement: Element transform
Elements SHALL support a 2D transform (translation, rotation, scale, pivot) that is applied to emitted vertex positions, and transforms SHALL compose down the element tree with the context surface transform. Paint, geometry queries, clipping, and hit-testing SHALL use that same composition. GetBounds/Arrange SHALL retain absolute layout-surface rectangles and the current transform/pivot convention; render transforms SHALL NOT implicitly move their origin into parent-local coordinates.

#### Scenario: Translation moves geometry
- **WHEN** an element with a translation transform is painted
- **THEN** its emitted vertices SHALL be offset by the translation

#### Scenario: Parent transform affects children
- **WHEN** a parent with a transform contains a child that emits geometry
- **THEN** the child's vertices SHALL be transformed by the parent transform as well

#### Scenario: Hit geometry matches paint geometry
- **WHEN** a child under nested transforms is painted and hit-tested
- **THEN** both operations SHALL use the same accumulated layout-to-surface matrix

### Requirement: Draw data emission
Core paint SHALL emit render-agnostic UIDrawData containing a batched triangle list (position, uv, color), indices, and draw commands with index range, clip rect, and an opaque texture handle. Vertex positions and draw clip rectangles SHALL be expressed in rendered-surface coordinates. Active layout clips SHALL be transformed to conservative surface AABBs before intersection. Core SHALL NOT emit Aurora or backend types.

#### Scenario: Draw commands carry clip and texture
- **WHEN** an element with a texture is painted inside a clipping container
- **THEN** the resulting draw command SHALL reference the element's texture handle and a clip rect equal to the intersection of active transformed surface clip AABBs

#### Scenario: No GPU types in draw data
- **WHEN** the UIDrawData type and its members are inspected
- **THEN** no Aurora, RHI, or backend type SHALL appear

### Requirement: Input event routing and hit-testing
The core SHALL accept input whose pointer positions are in rendered-surface coordinates and SHALL hit-test the element tree in reverse stable paint/z order, using effective transformed surface clip AABBs and inverse accumulated geometry. The topmost visible hit element SHALL receive the event. A non-invertible element and its subtree SHALL NOT be hit or receive pointer dispatch with undefined coordinates.

#### Scenario: Topmost element receives pointer
- **WHEN** two overlapping elements are hit by a pointer event
- **THEN** the element painted last SHALL receive the event

#### Scenario: Clipped element is not hit
- **WHEN** a pointer is outside a container's transformed surface clip AABB but inside a child's unclipped transformed bounds
- **THEN** the child SHALL NOT be hit

#### Scenario: Non-invertible subtree is excluded
- **WHEN** an element or its ancestor has a singular accumulated transform
- **THEN** neither that element nor its descendants SHALL be selected as a pointer hit target
