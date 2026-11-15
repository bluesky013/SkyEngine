## ADDED Requirements

### Requirement: Catalog-only asset identity tooltips

Asset picker entries and asset reference controls SHALL provide tooltip content containing the complete logical path, type, available catalog status and GUID for the hovered identity. Content SHALL be obtained from current catalog/metadata snapshots without loading a product, generating a preview or invoking an asset action. Unresolved identities SHALL remain explicit and SHALL NOT be replaced with unrelated asset data. The display SHALL preserve the full logical path even when the underlying row or field label is abbreviated.

#### Scenario: Asset label is abbreviated
- **WHEN** an asset picker entry or asset reference displays an abbreviated path
- **THEN** its tooltip exposes the complete logical path together with type, available status and GUID

#### Scenario: Asset has no loaded product
- **WHEN** catalog metadata is available but no product is resident
- **THEN** tooltip display uses that metadata and does not request product loading or preview generation

### Requirement: Picker result tooltip identity

An asset picker SHALL expose tooltip metadata for the hovered visible result independently of the current selection. Target identity SHALL remain bound to that asset and catalog/list generation rather than a recycled row index. Filtering, result replacement, clipping, popup close or catalog invalidation SHALL discard the former target. Covered controls SHALL NOT supply a tooltip through the open picker.

#### Scenario: Filtering reuses a row position
- **WHEN** a filter replaces the asset at the pending tooltip's row position
- **THEN** the previous tooltip is discarded and the replacement starts a new target lifetime and hover delay

#### Scenario: Hover does not select
- **WHEN** a picker result is hovered long enough to show its details
- **THEN** the current selection and referenced property remain unchanged
