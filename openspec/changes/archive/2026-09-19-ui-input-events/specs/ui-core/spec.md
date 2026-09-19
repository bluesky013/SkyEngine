## ADDED Requirements

### Requirement: Pointer enter and leave
The event router SHALL track the hovered element and notify the previous element with a leave and the new
element with an enter when the hovered element changes. Enter/leave SHALL NOT consume the move event.

#### Scenario: Enter on hover
- **WHEN** the pointer moves over an element that was not hovered
- **THEN** that element SHALL receive a pointer enter notification

#### Scenario: Leave on moving away
- **WHEN** the pointer moves from one element to another
- **THEN** the first element SHALL receive a pointer leave before the second receives enter

### Requirement: Unhandled event bubbling
An element that does not handle an event SHALL have the event propagate to its ancestors until one handles it
or the tree is exhausted.

#### Scenario: Parent handles child event
- **WHEN** a child does not handle a pointer event and its parent does
- **THEN** the parent's handler SHALL be invoked

### Requirement: Focus traversal
Elements SHALL carry a focusable flag (default false). The context SHALL provide next/previous focus lookup
over enabled, effectively-visible, focusable elements with wrap-around, in traversal order.

#### Scenario: Next focus
- **WHEN** focus traversal advances from a focusable element
- **THEN** the next focusable element in traversal order SHALL be returned

#### Scenario: Wrap around
- **WHEN** traversal advances from the last focusable element
- **THEN** the first focusable element SHALL be returned
