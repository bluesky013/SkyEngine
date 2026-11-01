## ADDED Requirements

### Requirement: Successfully loaded module code is process resident

DynamicModule SHALL keep successfully loaded native module code resident until process exit through a shared process owner. Wrapper release, session shutdown and manager destruction SHALL NOT physically unload that code. Binary updates and removal require process restart.

#### Scenario: Manager is destroyed
- **WHEN** initialized module sessions shut down and their manager is destroyed
- **THEN** their services are withdrawn and runtime instances are released
- **AND** native code remains available without per-object code tokens

#### Scenario: Another session uses the same module
- **WHEN** a later session loads the same module name
- **THEN** it reuses resident code
- **AND** metadata registration is not repeated

### Requirement: Singleton lookup observes the current shared object

Singleton access SHALL resolve through the shared Environment rather than a DLL-local object cache. First construction SHALL be serialized across callers, and constructors SHALL be able to resolve another singleton type. Destroy SHALL remove the shared registry entry before deleting the instance. Object users SHALL be drained before destruction.

#### Scenario: Another DLL destroys a service
- **WHEN** a service is removed and deleted through the shared Environment while its code stays resident
- **THEN** subsequent Singleton access constructs or obtains the current instance instead of returning a cached pointer to the previous object

#### Scenario: Concurrent first lookup
- **WHEN** multiple callers first resolve the same singleton type
- **THEN** they obtain one shared constructed instance

### Requirement: Dependencies use initialized session state

Module dependency checks SHALL require successful initialization in the current manager independently of code residency. Start/Tick callbacks SHALL only run for initialized sessions. Failed initialization SHALL call Shutdown to clean its partial instance before destruction and prevent dependent consumers from starting. Cold instances whose Init was never attempted SHALL receive no service callbacks.

#### Scenario: A provider fails initialization
- **WHEN** provider code is resident but Init fails
- **THEN** its dependent consumer is not initialized
- **AND** retry does not repeat process metadata registration

#### Scenario: Lifecycle reentry
- **WHEN** an Init, Start or Tick-phase callback attempts nested loading, unloading or callback dispatch
- **THEN** the operation returns failure without publishing a successful session, repeating callbacks or destroying the executing instance
- **AND** registration and topology mutation SHALL return failure during every lifecycle callback, including shutdown
- **AND** the host SHALL be able to retry after the active dispatch returns

#### Scenario: Manager shutdown
- **WHEN** shutdown begins
- **THEN** each session becomes unavailable before its callback runs and consumers stop before providers
- **AND** consumer instances are destroyed before their providers shut down
- **AND** repeated shutdown does not call callbacks again

### Requirement: Real object ownership survives code-lease removal

Pure DLL code leases SHALL NOT be required by source requests, Feature/editor registrations, World factories, shader compiler handles or physics bridges. Actual data/instance ownership and task/GPU completion constraints SHALL remain enforced.

#### Scenario: Shader service stops with a pending task
- **WHEN** a shader compiler is unregistered while a future still needs it
- **THEN** the future retains the compiler and writer objects until safe completion
- **AND** releasing those objects does not unload their code

#### Scenario: A Feature is disabled with frames in flight
- **WHEN** its registration is disabled or retired
- **THEN** accepted frame borrowers retain the Feature instance until their existing completion/retirement boundary

#### Scenario: An editor document or animation snapshot outlives its source registration
- **WHEN** a source/editor service is withdrawn
- **THEN** owned snapshots and permitted outstanding operations remain safe through their real owners
- **AND** stale actions/provider generations remain rejected
