## Context

The ENet transport backend uses zpl-c/enet v2.7.0. On non-Windows POSIX builds ENet creates a `PF_INET6` datagram socket with `IPV6_V6ONLY=0`. `enet_address_set_host("127.0.0.1")` produces the IPv4-mapped address `::ffff:127.0.0.1`. Standalone ENet probes on macOS show:

| Server bind | Client connect | Handshake |
|---|---|---|
| `ENET_HOST_ANY` | `127.0.0.1` | works |
| `::1` | `::1` | works |
| `127.0.0.1` | `127.0.0.1` | fails |
| `127.0.0.1` | `localhost` | fails |
| `localhost` | `127.0.0.1` | fails |

The engine test reproduces the last failure at `engine/network/test/Conformance.h:80`. Windows completes the same bind successfully, so this is an OS/socket-stack difference rather than a hardware or network-permission issue.

## Goals / Non-Goals

**Goals:**

- Make ENet loopback/listen tests pass on macOS for IPv4 endpoints.
- Preserve explicit IPv6 listening.
- Preserve the requested port and `GetAddress()` reporting.
- Add a regression test for the IPv6 path.

**Non-Goals:**

- No ENet version update or third-party patch.
- No switch to IPv4-only builds.
- No public network API change.
- No change to Windows binding behavior.

## Decisions

### 1. Widen non-IPv6 binds to `ENET_HOST_ANY` on macOS

`EnetBackend::Listen` resolves the requested address, then on macOS (`__APPLE__`) replaces the host with `ENET_HOST_ANY` for any family other than `IPv6`, while keeping the port. The listener stores the original `ListenDesc::address`, so `GetAddress()` is unchanged.

Alternative considered: compile ENet with `ENET_IPV4_ONLY`. Rejected because it removes IPv6 support from the whole build.

Alternative considered: patch ENet's socket creation to select `AF_INET` per address. Rejected for now because it requires changing ENet's compile-time socket path and all send/receive helpers; a local backend workaround is smaller and reversible.

Alternative considered: change the test to listen on `0.0.0.0`. Rejected because it hides a real backend failure and would break any API user that passes a specific IPv4 address.

### 2. Keep IPv6 endpoints explicit

Explicit `IPv6` addresses (`::1`, `[::1]:port`) keep the requested host. The current ENet path works for them and the new conformance case locks that in.

### 3. Document the macOS limitation

Because IPv4 binds widen to all interfaces, the backend and design record must state that the host part is advisory on macOS while the port is honored. A future change can replace this with an ENet patch or a family-aware socket path if per-interface IPv4 binding becomes required.

## Risks / Trade-offs

- **[macOS IPv4 listener accepts traffic on all interfaces]** → Document the limitation; the requested address is preserved for reporting, and servers that must restrict interfaces need a future family-aware bind.
- **[Hostname listen addresses]** → The workaround covers `Host`/`Unknown` families as well, since they may resolve to IPv4; IPv6 literals remain explicit.
- **[Test only covers loopback]** → Add an IPv6 conformance case; full `ctest` verifies no other network behavior changed.
- **[Platform drift]** → The workaround is behind `__APPLE__`; Windows keeps binding the requested address.

## Migration Plan

1. Add the IPv6 conformance case.
2. Implement the macOS IPv4 bind widening in `EnetBackend::Listen`.
3. Run `EnetNetworkTest` and the full suite on macOS.
4. Record results in `tasks.md`.

Rollback is a one-line removal of the macOS branch.

## Open Questions

None. If per-interface IPv4 binding is required later, replace the workaround with an ENet patch or a family-aware socket implementation.
