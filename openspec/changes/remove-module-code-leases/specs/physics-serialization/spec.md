## MODIFIED Requirements

### Requirement: Serialization implementation lives in the plugin

Reflection registration and asset/component serialization for physics SHALL be implemented in the physics plugin, while engine descriptor structs and their field names SHALL remain backend-neutral. Metadata SHALL be registered only by the unique module definition owner and SHALL rely on process-resident DLL code for persistent callbacks, retaining any captured data through actual owners. Runtime Init/Shutdown SHALL NOT repeat reflection registration.

#### Scenario: Engine stays implementation-free
- **WHEN** the engine physics module is inspected
- **THEN** it SHALL define descriptors and field names but SHALL NOT perform reflection registration or asset serialization

#### Scenario: Plugin registers serialization
- **WHEN** the physics module definition is created
- **THEN** its metadata owner SHALL register the physics components, descriptors and config types once

#### Scenario: Runtime is restarted
- **WHEN** the same module definition begins a new runtime session
- **THEN** its existing metadata SHALL be reused without duplicate reflection or an idempotent registration guard
