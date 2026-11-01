## ADDED Requirements

### Requirement: iOS native framework linkage

The Framework iOS target SHALL declare the Apple system frameworks used by its native platform implementation, including CoreGraphics for rectangle operations. Applications SHALL receive these dependencies through the existing target rather than application-specific linker workarounds.

#### Scenario: Link native text input in an iOS application

- **WHEN** an arm64 iOS application links Framework's native text-input implementation
- **THEN** the link SHALL resolve its CoreGraphics rectangle APIs through Framework's dependency interface
