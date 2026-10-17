## 1. Regression test

- [x] 1.1 Add an IPv6 loopback conformance case to `plugins/network-enet/test/EnetConformanceTest.cpp` using a free port.

## 2. Implement the macOS bind fix

- [x] 2.1 In `EnetBackend::Listen`, resolve the requested address and, on macOS, bind non-IPv6 listen addresses with `ENET_HOST_ANY` while preserving the port.
- [x] 2.2 Keep explicit IPv6 listen addresses bound to the requested host.
- [x] 2.3 Store the original `ListenDesc::address` in the listener so `GetAddress()` still reports the requested endpoint.
- [x] 2.4 Add a comment explaining the macOS dual-stack ENet limitation and the interface-selection trade-off.

## 3. Verify

- [x] 3.1 Build `EnetNetworkTest` and run it; confirm the IPv4 loopback case passes.
- [x] 3.2 Run the full test suite on macOS and record any unrelated failures.
- [x] 3.3 Run `openspec validate fix-enet-ipv4-listen --strict --no-interactive` and `git diff --check`.
- [x] 3.4 Record verification results in `tasks.md`.

## Verification record

- Reproduced the failure with a standalone ENet probe: binding the server to `127.0.0.1` never completes the handshake on macOS, while `ENET_HOST_ANY` and `::1` both work. This is an ENet dual-stack socket behavior, not a firewall or hardware issue.
- `EnetBackend::Listen` now binds non-IPv6 endpoints to `ENET_HOST_ANY` on macOS while keeping the port and reporting the requested address.
- Added `ConformanceOverUdpIpv6Loopback`; both IPv4 and IPv6 conformance cases pass.
- Full `ctest` on macOS: 37/37 passed.
- `openspec validate fix-enet-ipv4-listen --strict --no-interactive` and `git diff --check` passed.
- Windows was not run on this host; the workaround is behind `__APPLE__`, so Windows binding behavior is unchanged.
