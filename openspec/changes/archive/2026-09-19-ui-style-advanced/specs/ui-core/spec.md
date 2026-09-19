## ADDED Requirements

### Requirement: Per-field style cascade
Theme style resolution SHALL merge style classes field by field, so a class that sets only some fields
overrides those fields while earlier classes keep the rest.

#### Scenario: Overlay overrides one field
- **WHEN** a base class sets the background and an overlay class sets only the border width
- **THEN** the resolved style SHALL have the base background and the overlay border width

#### Scenario: Later class wins per field
- **WHEN** two classes set different fields
- **THEN** the resolved style SHALL contain both

### Requirement: Nine-slice images
An image SHALL support per-edge slice insets and, when set, SHALL render nine quads so corners stay fixed
while edges and center stretch.

#### Scenario: Nine quads emitted
- **WHEN** an image with non-zero slice insets is painted
- **THEN** it SHALL emit nine quads

#### Scenario: No insets means single quad
- **WHEN** an image has zero slice insets
- **THEN** it SHALL emit a single quad
