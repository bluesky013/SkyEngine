## MODIFIED Requirements

### Requirement: Multi-pointer capture
Pointer events SHALL carry a pointer id, and the router SHALL track capture per pointer so that each pointer's move/release is delivered to the element it pressed, independently of other pointers. Captured delivery SHALL map the rendered-surface pointer through the target's current inverse accumulated transform. When captured geometry is non-invertible, the router SHALL clear that pointer's capture and send coordinate-free pointer cancellation rather than deliver undefined coordinates or leave the gesture captured.

#### Scenario: Independent captures
- **WHEN** two pointers press two different elements and then move
- **THEN** each element SHALL receive only its own pointer's events

#### Scenario: Release clears one capture
- **WHEN** a captured pointer is released
- **THEN** only that pointer's capture SHALL be cleared

#### Scenario: Captured geometry changes
- **WHEN** a captured element's invertible transform changes before a subsequent move or release
- **THEN** that event SHALL be mapped using its current geometry rather than the transform at the original press

#### Scenario: Capture becomes non-invertible
- **WHEN** a captured target's accumulated transform becomes singular
- **THEN** the router SHALL clear that pointer's capture, notify cancellation without x/y, and SHALL NOT invoke its ordinary pointer handler with undefined coordinates

## ADDED Requirements

### Requirement: Per-receiver pointer coordinates
UIEventRouter SHALL map pointer x/y separately for every target, bubbling ancestor, and hover enter/leave receiver using that receiver's inverse accumulated geometry. Handler x/y SHALL remain untransformed absolute layout-surface coordinates without subtracting the receiver's bounds origin. The original rendered-surface point SHALL be preserved for deriving each receiver's event copy, and pointer id, action, button, and wheel data SHALL be preserved.

#### Scenario: Transformed button receives layout coordinates
- **WHEN** a pointer presses and releases inside the painted bounds of a transformed Button
- **THEN** its handler SHALL receive points in its original absolute layout bounds and SHALL apply its existing inside/outside click behavior

#### Scenario: Bubbling remaps for each ancestor
- **WHEN** an event bubbles from a transformed child to an ancestor with different accumulated geometry
- **THEN** the ancestor SHALL receive coordinates mapped from the original surface point through its own inverse rather than a copy of the child's mapped coordinates

#### Scenario: Hover transitions map independently
- **WHEN** the hovered element changes between differently transformed elements
- **THEN** leave and enter callbacks SHALL each receive coordinates appropriate to their respective receiver

### Requirement: Direct child routing follows relative geometry
Direct ChildRouting SHALL accept pointer x/y in the calling parent's layout coordinates and SHALL map them with each child's local inverse before its hit-test and delivery. Uncaptured target selection SHALL use reverse stable paint/z order and the conservative rendered-surface AABBs of ancestors that declare ClipsChildren, obtained through geometry including the configured surface base. The helper SHALL NOT infer a viewport from a detached root's GetBounds; its caller SHALL enforce viewport boundaries through its host or UIEventRouter, which owns the fixed context surface clip. Captured child input SHALL use the same relative coordinate mapping without requiring a new clipped hit-test. Direct routing SHALL NOT apply the full accumulated inverse again to an already parent-mapped point or dispatch through a non-invertible surface base or accumulated child path. An unmappable direct capture SHALL be cleared before coordinate-free cancellation is sent to the captured child.

#### Scenario: Direct route maps one transform step
- **WHEN** a parent's handler forwards an already mapped pointer event to a transformed child
- **THEN** the child SHALL receive the result of its local inverse only, with no duplicate ancestor/surface conversion

#### Scenario: Direct overlap follows paint order
- **WHEN** overlapping children differ in z or equal-z insertion order
- **THEN** direct routing SHALL consider them in reverse of their stable paint order

#### Scenario: Direct captured child remains mapped
- **WHEN** a direct-route child is captured and receives later move/release outside its bounds
- **THEN** the captured input SHALL use its current local inverse without requiring a new clipped hit-test, and a non-invertible accumulated path SHALL prevent undefined dispatch

#### Scenario: Direct route respects surface-base clipping
- **WHEN** a direct child route receives a parent-layout point under a configured surface transform
- **THEN** uncaptured target selection SHALL check the corresponding rendered-surface point against ClipsChildren ancestor AABBs using the parent's default geometry

#### Scenario: Detached bounds do not invent a viewport
- **WHEN** a detached root does not clip children and a child extends beyond that root's bounds
- **THEN** direct routing SHALL NOT reject the child solely because of those root bounds, and the caller SHALL remain responsible for any actual viewport restriction

### Requirement: Unusable capture sends coordinate-free cancellation
UIElement SHALL provide OnPointerCancel(pointerId) with a default no-op implementation. If a router's captured target cannot map a pointer due to non-invertible geometry, the router SHALL clear that pointer's capture before notifying the target and its ancestors through OnPointerCancel. Direct-child capture owners SHALL clear and notify an unmappable captured child, and slot containers SHALL be able to forward cancellation through their direct capture. Cancellation SHALL require no mapped x/y, SHALL NOT produce a Button click, and SHALL clear active pressed/drag/preview state in the affected Button or reflected form. This requirement SHALL be limited to invalid geometry cleanup and SHALL NOT change focus policy or other pointers' captures.

#### Scenario: Singular release does not leave a Button pressed
- **WHEN** a Button is captured on press and its accumulated geometry becomes singular before release
- **THEN** cancellation SHALL clear its pressed state and capture without emitting a click, allowing a later invertible gesture to work normally

#### Scenario: Cancellation traverses target and ancestors
- **WHEN** a router cannot map a captured pointer because an element, ancestor, or configured surface transform is singular
- **THEN** it SHALL clear the capture and notify target-to-ancestor cancellation without attempting to construct local coordinates

#### Scenario: Direct capture cancels safely
- **WHEN** a direct-child capture becomes unmappable
- **THEN** its owner SHALL clear the captured reference before notifying that child, including forwarding through a slot container's own direct capture where applicable

#### Scenario: Reflected preview and drag are canceled
- **WHEN** a reflected form has an active drag or live preview and its captured geometry becomes unusable
- **THEN** pointer cancellation SHALL clear that interaction and restore an uncommitted preview without retaining obsolete capture or field references

#### Scenario: Other pointers remain captured
- **WHEN** one pointer's unusable capture is canceled while a different pointer retains valid captured geometry
- **THEN** the second pointer's router capture SHALL remain active
