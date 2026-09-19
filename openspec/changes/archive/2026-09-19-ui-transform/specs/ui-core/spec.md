## ADDED Requirements

### Requirement: Element transform
Elements SHALL support a 2D transform (translation, rotation, scale, pivot) that is applied to emitted vertex
positions, and transforms SHALL compose down the element tree (a parent transform affects its subtree).

#### Scenario: Translation moves geometry
- **WHEN** an element with a translation transform is painted
- **THEN** its emitted vertices SHALL be offset by the translation

#### Scenario: Parent transform affects children
- **WHEN** a parent with a transform contains a child that emits geometry
- **THEN** the child's vertices SHALL be transformed by the parent transform as well

### Requirement: Hierarchical opacity
Elements SHALL carry an opacity in the range 0 to 1 that multiplies into emitted vertex alpha, and a parent's
opacity SHALL apply to its subtree.

#### Scenario: Opacity scales alpha
- **WHEN** an element with opacity 0.5 emits opaque geometry
- **THEN** the emitted vertex alpha SHALL be half

#### Scenario: Parent opacity cascades
- **WHEN** a parent with opacity less than 1 contains a child that emits geometry
- **THEN** the child's vertex alpha SHALL be reduced by the parent's opacity
