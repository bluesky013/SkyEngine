# network-session Specification

## Purpose
TBD - created by archiving change add-network-core. Update Purpose after archive.
## Requirements
### Requirement: Session identity is separate from the physical connection

The host SHALL represent a logical session by a stable `SessionId` that is independent of the physical `ConnectionId`, so a session can survive a disconnect and be re-bound to a new connection.

#### Scenario: Session survives reconnect

- **WHEN** a client disconnects and reconnects with a valid resume token
- **THEN** the host SHALL re-bind the existing `SessionId` to the new connection

### Requirement: Resume tokens are stateless, signed, and expiring

A `ResumeToken` SHALL be verifiable by any server holding the shared signing key, SHALL carry an expiration, and SHALL require no cluster or directory service to validate. The signature SHALL be an HMAC-SHA256 over the token fields, and verification SHALL use a constant-time comparison.

#### Scenario: Any server can validate a token

- **WHEN** a server receives a resume token issued by another server that shares the signing key
- **THEN** it SHALL validate the token without contacting a directory service

#### Scenario: Expired token is rejected

- **WHEN** a resume token is presented after its expiration
- **THEN** the host SHALL reject the resume attempt

#### Scenario: Tampered token is rejected

- **WHEN** any signed field or signature byte of a token is modified
- **THEN** verification SHALL fail

### Requirement: Reconnection uses bounded backoff

A client SHALL be able to reconnect automatically using bounded, increasing backoff and a resume token.

#### Scenario: Backoff grows then stops

- **WHEN** repeated connection attempts fail
- **THEN** the host SHALL increase the retry interval up to a configured maximum and SHALL report attempts through its statistics

### Requirement: Handover may hold multiple connections

During a redirect or re-homing, the host SHALL allow more than one connection to be active for the same session until the handover completes.

#### Scenario: Overlap during handover

- **WHEN** a session is being moved to a new endpoint
- **THEN** the host SHALL keep the old connection usable until the new connection is established or a timeout elapses

