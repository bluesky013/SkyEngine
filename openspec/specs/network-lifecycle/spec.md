# network-lifecycle Specification

## Purpose
TBD - created by archiving change add-network-core. Update Purpose after archive.
## Requirements
### Requirement: Host exposes an explicit lifecycle state machine

The host SHALL expose lifecycle states `Accepting`, `Draining`, and `Closed`. While `Draining`, the host SHALL stop accepting new connections but SHALL keep existing sessions served until they end or a timeout elapses.

#### Scenario: Drain stops new connections

- **WHEN** the host enters `Draining`
- **THEN** it SHALL reject new incoming connections while continuing to serve existing ones

### Requirement: Redirect moves clients during drain

While draining, the host SHALL be able to ask connected clients to reconnect to another endpoint by sending a redirect carrying a resume token.

#### Scenario: Client is redirected

- **WHEN** the host calls redirect for a connection with a target address and resume token
- **THEN** the client SHALL receive the redirect and be able to reconnect to the target endpoint using the token

### Requirement: Shutdown ordering is defined and safe

Host shutdown SHALL stop producers before releasing resources: it SHALL first signal and join the I/O thread when running in `OwnedThread` mode, then stop delivering events, and only then release transport resources. The backend `Pump` SHALL return within its `waitMs` bound so shutdown cannot stall indefinitely.

#### Scenario: No callbacks after shutdown begins

- **WHEN** host shutdown is initiated
- **THEN** no user callback SHALL be invoked after shutdown completes

### Requirement: Host reports statistics for orchestration

The host SHALL report per-backend and per-lane statistics including active connections, round-trip time, bandwidth, and packet loss, intended as input for an external orchestrator.

#### Scenario: Stats are available while serving

- **WHEN** the host is serving connections
- **THEN** it SHALL expose current connection counts and network statistics that can be read without stopping the host

