## ADDED Requirements

### Requirement: Builders self-describe effective settings

An asset builder SHALL be able to describe, for a given product bundle, the effective settings it
would apply, as ordered key/value pairs. The base implementation SHALL return no settings so existing
builders remain valid.

#### Scenario: Default is empty
- **WHEN** a builder does not override the settings description
- **THEN** describing any bundle SHALL return an empty list

#### Scenario: Builder reports its settings
- **WHEN** an image builder that encodes per bundle is asked to describe a bundle
- **THEN** it SHALL return the effective settings (e.g. encode, srgb, max size, mip generation) as
  key/value pairs

### Requirement: Settings queryable by asset extension

The builder manager SHALL expose a lookup that returns the settings description for an extension and
bundle, resolving to the builder registered for that extension.

#### Scenario: Lookup by extension
- **WHEN** the manager is queried with a registered extension and a bundle
- **THEN** it SHALL return that builder's settings description for the bundle

#### Scenario: Unknown extension
- **WHEN** the manager is queried with an extension that has no builder
- **THEN** it SHALL return an empty list
