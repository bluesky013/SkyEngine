## MODIFIED Requirements

### Requirement: Single active backend registration

The engine registry SHALL maintain exactly one active backend at a time. Registering a new backend SHALL unregister the previous factory and release the registry's ownership of it without invalidating existing worlds' owned SDK state; defining DLL code SHALL remain process resident. World destruction SHALL use its creating owner's lifetime contract, not whichever backend is currently active.

#### Scenario: Replace the active backend
- **WHEN** a new backend is registered while one is active
- **THEN** the previous backend SHALL stop accepting creation, while outstanding worlds SHALL remain safely destructible
