## ADDED Requirements

### Requirement: Reveal a path in the OS file manager

The platform layer SHALL expose a native shell service to reveal a file path in the operating
system's file manager, implemented per desktop backend and defaulting to a no-op where unsupported.

#### Scenario: Desktop reveal
- **WHEN** a desktop host asks the platform to reveal an existing path
- **THEN** the OS file manager SHALL open with that path selected

#### Scenario: Unsupported backend
- **WHEN** a backend does not implement the service (e.g. mobile)
- **THEN** the call SHALL do nothing and SHALL NOT fail

#### Scenario: Interface home
- **WHEN** the service is declared
- **THEN** it SHALL live on `PlatformBase`/`Platform` alongside the other native shell services
  (open/save dialogs), so hosts do not call OS APIs directly
