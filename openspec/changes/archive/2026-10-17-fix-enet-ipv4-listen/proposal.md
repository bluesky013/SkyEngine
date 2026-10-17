## Why

`EnetNetworkTest.ConformanceOverUdp` fails consistently on macOS at the client/server handshake. The test listens on `127.0.0.1:39555`, and `EnetBackend::Listen` passes that address straight to `enet_host_create`. The vendored ENet (zpl-c/enet v2.7.0) creates an IPv6 dual-stack socket and maps `127.0.0.1` to `::ffff:127.0.0.1`; on macOS, binding that specific IPv4 address leaves the socket unable to complete an ENet handshake. Binding `ENET_HOST_ANY` or using an IPv6 address works, and the same standalone ENet program reproduces the failure outside the engine, so it is not a firewall or hardware issue. Windows is unaffected.

## What Changes

- Make the ENet backend bind non-IPv6 listen endpoints to `ENET_HOST_ANY` on macOS while preserving the requested address and port for `INetListener::GetAddress()`.
- Keep explicit IPv6 listen endpoints bound to their requested address (this path works today).
- Add an IPv6 loopback conformance case so the workaround cannot regress explicit IPv6 listening.
- Document the macOS dual-stack limitation and the fact that a specific IPv4 listen address may accept traffic on all IPv4 interfaces.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `network-transport`: adds the requirement that the ENet backend completes handshakes for IPv4 loopback/listen endpoints on macOS without giving up IPv6 listening.

## Impact

- `plugins/network-enet/src/EnetBackend.cpp` and `plugins/network-enet/test/EnetConformanceTest.cpp`.
- No public interface change; `ListenDesc` still carries the requested address and `GetAddress()` still reports it.
- On macOS, an IPv4 listen address is advisory for interface selection (the port and requested-address reporting are preserved). Windows behavior is unchanged.
- No new dependencies; no third-party patch.
