## MODIFIED Requirements

### Requirement: Visibility and dirty propagation
Elements SHALL support visibility and separate desired-size, arrangement, and paint invalidation. Layout invalidation SHALL propagate toward ancestors rather than recursively dirtying descendants. The explicit incremental layout entries SHALL skip only clean controlled branches with unchanged allocation inputs; volatile branches SHALL remain eligible for layout. Existing external forced layout calls SHALL remain supported. Visibility SHALL continue to suppress hidden elements and their descendants during paint.

#### Scenario: Hidden element is not painted
- **WHEN** an element's visibility is set to false
- **THEN** that element and its descendants SHALL emit no draw data

#### Scenario: Changed leaf invalidates its consumers
- **WHEN** a leaf changes its desired size
- **THEN** the leaf and ancestor allocation consumers SHALL become dirty while unrelated sibling branches SHALL not be recursively marked dirty

#### Scenario: Bounds changes update descendants during arrangement
- **WHEN** a host changes an element's bounds and requests layout
- **THEN** its descendant allocations SHALL update through arrangement while unrelated sibling branches remain eligible for reuse

#### Scenario: Clean controlled subtree skips incremental work
- **WHEN** an incremental pass reaches an unchanged controlled subtree with no pending layout work
- **THEN** that subtree SHALL NOT be re-laid-out solely because a new frame begins

### Requirement: Draw data emission
Core paint SHALL emit a render-agnostic UIDrawData containing a batched triangle list with position, UV, and color, indices, and draw commands with index range, clip rectangle, and an opaque texture handle. Vertex positions and draw clip rectangles SHALL be expressed in rendered-surface coordinates. Active layout clips SHALL be transformed to conservative surface AABBs before intersection. Core SHALL NOT emit Aurora or backend types. Each paint call SHALL rebuild complete visible output even when layout and paint flags were already clean; dirty state SHALL NOT be used to omit draw emission without a retained rendering system.

#### Scenario: Draw commands carry clip and texture
- **WHEN** an element with a texture is painted inside a clipping container
- **THEN** its draw command SHALL reference the texture handle and a clip rectangle equal to the intersection of active transformed surface clip AABBs

#### Scenario: No GPU types in draw data
- **WHEN** the UIDrawData type and its members are inspected
- **THEN** no Aurora, RHI, or backend type SHALL appear

#### Scenario: Unchanged frames retain full output
- **WHEN** a visible tree is painted twice with unchanged state and clean dirty flags
- **THEN** both frames SHALL contain complete equivalent draw output rather than an empty second frame
