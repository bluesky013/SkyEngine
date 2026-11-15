## ADDED Requirements

### Requirement: Reflected field metadata tooltips

Reflected forms SHALL expose existing TOOLTIP metadata for the hovered visible field or label through the common tooltip protocol. Missing or empty metadata SHALL produce no fabricated description. Bind, Unbind, Refresh and field replacement SHALL invalidate tooltip bindings before old field storage or model references are released. Tooltip construction SHALL NOT edit the property, create an undo command or access an obsolete bound object.

#### Scenario: Field defines help text
- **WHEN** a visible field with nonempty TOOLTIP metadata is hovered through the shared delay
- **THEN** the metadata is displayed without changing the field value or command history

#### Scenario: Form is rebound during hover
- **WHEN** a form changes its bound object or refreshes its field storage while a tooltip is pending or visible
- **THEN** the former tooltip target expires and cannot read or display stale field metadata

#### Scenario: Field has no help metadata
- **WHEN** a visible field has no TOOLTIP metadata
- **THEN** it does not create a metadata tooltip
