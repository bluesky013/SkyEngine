## ADDED Requirements

### Requirement: Borrowed atlas registry lifetime

UIFontAtlas and UITextSystem SHALL support paired attachment and detachment of borrowed texture registries. The owner SHALL detach a registry before destroying it. Detachment SHALL stop all future glyph update and page registration calls to that registry without invoking it or releasing its textures. Duplicate attachment and unknown/null detachment SHALL be harmless.

#### Scenario: Detached renderer is destroyed
- **WHEN** a secondary atlas registry is detached and destroyed and new glyphs or pages are subsequently requested
- **THEN** only still-attached registries SHALL receive page registrations and updates
- **AND** cached glyph/page handles SHALL remain valid

#### Scenario: Registry is reattached
- **WHEN** a previously detached registry is attached again
- **THEN** all current pages SHALL be replayed under their existing ids exactly once
- **AND** a retained backend page SHALL receive current atlas pixels before cached glyphs are used
- **AND** duplicate attachment SHALL NOT replay pages or duplicate future update calls

#### Scenario: Default registry is removed
- **WHEN** UITextSystem's default registry is detached
- **THEN** GetRegistry SHALL return null until another registry is explicitly attached
- **AND** removing a secondary registry SHALL preserve an existing default

#### Scenario: Editor surface cleanup
- **WHEN** the editor explicitly destroys a floating surface, processes its OS-close, or shuts down all surfaces
- **THEN** each renderer SHALL be detached from the text system before that renderer is destroyed
