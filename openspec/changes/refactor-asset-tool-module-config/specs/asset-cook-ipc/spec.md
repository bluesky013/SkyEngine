## MODIFIED Requirements

### Requirement: Worker capability parity

The worker SHALL provide the same enabled asset-building capabilities as the editor through configuration-selected builder modules loaded before serving requests; linking Framework alone SHALL NOT be assumed to provide builders. Selection SHALL use the shared module manifest contract rather than a C++ list of business modules. A configured enabled module that cannot load SHALL fail worker startup. A valid worker selection with no builder for a requested asset SHALL complete that request with an observable failure.

#### Scenario: Worker has builders
- **WHEN** the worker starts with a valid cook manifest
- **THEN** it SHALL register the corresponding enabled editor builder capabilities before accepting cook requests
- **AND** loading SHALL retain each builder's existing metadata owner and code lifetime

#### Scenario: Missing builders fail the cook
- **WHEN** an asset is requested on a successfully started worker whose effective selection has no matching builder
- **THEN** the request SHALL complete with a failure result and the pending load SHALL become failed

#### Scenario: Enabled builder module is missing
- **WHEN** the manifest selects a builder module that is unavailable or fails initialization
- **THEN** worker startup SHALL fail and the parent SHALL complete affected requests through the existing worker-failure path
- **AND** the worker SHALL NOT report a ready handshake with a silently reduced capability set
