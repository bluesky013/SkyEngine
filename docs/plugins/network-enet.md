---
title: "ENet Network Backend"
description: "First real network transport backend: reliable/unreliable UDP channels over ENet."
module: "network-enet"
updated: "2026-10-17"
---

## Overview

`plugins/network-enet` is the first real transport backend for the [Network module](../modules/network.md).
It implements `sky::net::INetBackend` over **ENet** (zpl-c/enet v2.7.0) and registers itself for the `Client`
and `Server` roles. Consumers resolve it through `NetworkBackendRegistry` and never name the concrete backend.

| Build switch | Source | Default |
|---|---|---|
| `SKY_BUILD_NETWORK_ENET` | `plugins/plugins.json` | `ON` |

## Targets

| Target | Type | Links | Contents |
|---|---|---|---|
| `EnetNetwork` | SHARED | `Network`, `Framework`, `3rdParty::enet` | `EnetBackend`, `EnetNetworkModule`, `plugin.json` |
| `EnetNetworkTest` | TEST | `Network`, `Framework`, `3rdParty::enet`, googletest | shared conformance suite over real UDP |

The plugin recompiles its sources into `EnetNetworkTest` rather than linking the shared library, matching the
existing plugin test pattern.

## Backend contract

`EnetBackend` maps the engine delivery modes onto ENet channels and reliability flags:

| `DeliveryMode` | ENet channel | Flags |
|---|---|---|
| `ReliableOrdered` | 0 | `ENET_PACKET_FLAG_RELIABLE` |
| `ReliableUnordered` | 1 | `RELIABLE | UNSEQUENCED` |
| `UnreliableSequenced` | 2 | (none) |
| `Unreliable` | 3 | `UNSEQUENCED` |

Its capability descriptor reports `reliable`, `unreliable`, `ordered`, client/server roles, `maxPayload = 1024`,
`wakeupSupport = false`, no backend-owned threads, and `realSendSequence = false` (ENet does not expose the
sender's packet sequence, so the reported sequence is synthesized on receive and must not be used for loss
detection).

## Listening and macOS IPv4 binding

`EnetBackend::Listen` resolves `ListenDesc::address` and creates the ENet host on that port. ENet v2.7.0 uses an
IPv6 dual-stack socket on POSIX and maps IPv4 literals to `::ffff:a.b.c.d`. On macOS, binding that socket to a
specific IPv4 address (for example `127.0.0.1`) leaves it unable to complete ENet handshakes, even though the port
is bound.

To keep IPv4 loopback and dedicated-server flows working, macOS binds non-IPv6 listen addresses to
`ENET_HOST_ANY` and keeps the requested port. The listener still reports the address passed to `Listen()` via
`INetListener::GetAddress()`, but interface selection is advisory on macOS: an IPv4 listener accepts traffic on
all IPv4 interfaces. Explicit IPv6 addresses (for example `[::1]`) keep their requested bind address. Windows is
not affected and continues to bind the requested IPv4 address.

## Module registration

`EnetNetworkModule` (registered with `REGISTER_MODULE`) registers an `EnetBackend` instance for the `Client` and
`Server` roles on start and unregisters them on shutdown.

## Third-party packaging

ENet is declared in `cmake/thirdparty.json` and discovered through `cmake/thirdparty/Findenet.cmake`
(static `enet` plus `ws2_32`/`winmm` on Windows). Build it with:

```bash
python3 python/third_party.py -p Win32 -o build_3rd -t enet
```

## Tests

`EnetNetworkTest` runs the shared `RunBackendConformance` suite from `engine/network/test/Conformance.h` against
the ENet backend over real UDP loopback connections, covering both `127.0.0.1` and `[::1]`.
