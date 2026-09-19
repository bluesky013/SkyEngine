## ADDED Requirements

### Requirement: DPI scaling
`UIContext` SHALL apply a DPI scale to the logical content size, so layout and painting operate in scaled
device coordinates.

#### Scenario: Scale applied to content
- **WHEN** the content size is 100 by 100 and the DPI scale is set to 2
- **THEN** the root bounds SHALL be 200 by 200

#### Scenario: Reset scale
- **WHEN** the DPI scale is set back to 1
- **THEN** the root bounds SHALL return to the logical size

### Requirement: Localization
The UI system SHALL provide per-locale string tables, a current locale, and key translation that falls back to
the key when a translation is missing.

#### Scenario: Translate in current locale
- **WHEN** a key has a value in the current locale
- **THEN** translation SHALL return that value

#### Scenario: Missing key falls back
- **WHEN** a key has no value in the current locale
- **THEN** translation SHALL return the key itself

### Requirement: Localized text
A `Text` widget SHALL be able to reference a localization key and resolve it when measuring and painting, so a
locale change is reflected without rebuilding the tree.

#### Scenario: Key resolves to text
- **WHEN** a text widget references a key present in the current locale
- **THEN** its measured/painted text SHALL be the translated value

#### Scenario: Locale change updates text
- **WHEN** the locale changes and the widget is measured again
- **THEN** the resolved text SHALL reflect the new locale
