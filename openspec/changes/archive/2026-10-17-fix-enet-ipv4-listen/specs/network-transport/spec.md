## ADDED Requirements

### Requirement: IPv4 listen endpoints complete handshakes on macOS

The ENet transport backend SHALL complete client/server handshakes when listening on an IPv4 loopback or IPv4 listen address on macOS, without requiring `ENET_IPV4_ONLY`, and SHALL continue to support explicit IPv6 listen addresses. The requested port SHALL be preserved, and `INetListener::GetAddress()` SHALL report the address passed to `Listen()`.

#### Scenario: IPv4 loopback handshake

- **WHEN** a server listens on `127.0.0.1:<port>` and a client connects to the same endpoint
- **THEN** both sides SHALL report a connected peer within the conformance timeout

#### Scenario: Explicit IPv6 listen

- **WHEN** a server listens on `[::1]:<port>` and a client connects to the same endpoint
- **THEN** both sides SHALL report a connected peer within the conformance timeout

#### Scenario: Requested address is reported

- **WHEN** `Listen()` succeeds for an IPv4 address
- **THEN** `INetListener::GetAddress()` SHALL return the requested address, including its port

### Requirement: macOS IPv4 binding limitation is documented

The backend SHALL document that, on macOS with the dual-stack ENet socket, a specific IPv4 listen address binds to all IPv4 interfaces while the requested address is retained for reporting. It SHALL NOT silently drop the requested port or switch the whole build to IPv4-only.

#### Scenario: Port is honored

- **WHEN** the server is configured for port `39555`
- **THEN** clients connecting to port `39555` SHALL reach it, regardless of the macOS bind widening

#### Scenario: IPv6 remains available

- **WHEN** the project is configured with the ENet backend on macOS
- **THEN** explicit IPv6 listeners SHALL still work and the build SHALL NOT define `ENET_IPV4_ONLY`
