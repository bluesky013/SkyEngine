## ADDED Requirements

### Requirement: Post-opaque scene render extension stage

The production scene feature SHALL expose an ordered extension stage that runs after the scene opaque pass and before tonemap, and SHALL provide the current view, scene color handle and scene depth handle to each extension.

#### Scenario: Extension runs before tonemap

- **WHEN** a scene extension is registered and the scene renders
- **THEN** the extension's graph work SHALL be declared after the opaque pass and before the tonemap pass

#### Scenario: Shared scene attachments

- **WHEN** an extension builds in the post-opaque stage
- **THEN** it SHALL receive the scene color and depth handles created for the current view and SHALL use `LoadOp::LOAD` for both attachments

#### Scenario: Extension requires depth

- **WHEN** an extension requires depth and the scene view supports depth
- **THEN** the scene feature SHALL create a depth attachment before invoking the extension

#### Scenario: No registered extension

- **WHEN** no scene extension is registered
- **THEN** the scene feature SHALL produce the same pass order and output as before this change

#### Scenario: Deterministic extension order

- **WHEN** multiple extensions are registered
- **THEN** they SHALL execute in ascending order value with a stable name tie-break, independent of registration order

#### Scenario: Invalid attachment usage

- **WHEN** an extension requests a clear of scene color or depth, or requests an unsupported attachment
- **THEN** the scene feature SHALL reject the extension before native encoding

### Requirement: Scene extension failure propagation

A scene extension failure SHALL fail the scene build explicitly and SHALL NOT produce a successful partial frame.

#### Scenario: Extension build failure

- **WHEN** an extension returns a frame operation error
- **THEN** the scene feature SHALL propagate the error and the frame SHALL NOT be submitted as successful

#### Scenario: Extension requires unavailable depth

- **WHEN** an extension requires depth but the active scene view has no supported depth format
- **THEN** validation SHALL reject the extension with an explicit error

#### Scenario: Extension context lifetime

- **WHEN** an extension callback returns
- **THEN** it SHALL NOT retain the borrowed build context, graph handles or view reference
