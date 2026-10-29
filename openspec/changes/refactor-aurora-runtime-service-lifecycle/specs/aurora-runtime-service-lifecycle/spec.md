## ADDED Requirements

### Requirement: Renderer adapter is independent of Skeleton service publication

`AuroraModule` SHALL manage its renderer provider, host and launcher lifecycle without creating, registering, inspecting or withdrawing `IAnimationSkeletonSource`.

#### Scenario: Direct renderer adapter without animation services
- **WHEN** a caller initializes the renderer adapter with no Skeleton source or module manager
- **THEN** Skeleton source availability does not affect provider-only or host initialization

#### Scenario: External Skeleton owner
- **WHEN** an external Skeleton service exists during direct adapter initialization or host failure
- **THEN** the adapter preserves that service through initialization, rollback and shutdown

### Requirement: Runtime assembly owns the Skeleton service session

The existing Aurora runtime entry SHALL compose the Skeleton asset source and renderer adapter with one explicit registration owner and a real defining-module lease. Provider-only loading SHALL still publish both required providers without creating a RendererHost.

#### Scenario: Provider-only module session
- **WHEN** the runtime module loads in provider-only mode
- **THEN** renderer and Skeleton providers are available to dependent modules
- **AND** shutdown withdraws the session's providers

#### Scenario: Renderer initialization fails after source publication
- **WHEN** renderer initialization fails after this session registers its Skeleton source
- **THEN** the session withdraws and releases its source
- **AND** services owned by others remain registered

#### Scenario: Duplicate runtime owner
- **WHEN** another runtime instance attempts to publish a Skeleton source over the active owner
- **THEN** initialization fails explicitly
- **AND** cleanup does not withdraw the active owner's services

### Requirement: Service withdrawal and code lifetime are distinct

Runtime shutdown SHALL withdraw its Skeleton interface before draining the renderer host and SHALL release its source after host shutdown. Outstanding request/result leases SHALL keep defining code available for calls and destruction after service withdrawal or module manager destruction.

#### Scenario: Request outlives its module session
- **WHEN** a caller retains a Skeleton request or result across runtime shutdown and module manager destruction
- **THEN** the global source interface is withdrawn
- **AND** the retained object's defining code remains valid until its final lease is released

#### Scenario: Repeated module session
- **WHEN** initialization/shutdown is repeated or cleanup follows partial failure
- **THEN** the session neither duplicates registration nor unregisters another owner
