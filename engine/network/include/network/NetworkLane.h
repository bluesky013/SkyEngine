//
// Created on 2026/09/25.
//

#pragma once

#include <network/ConnectionId.h>

#include <cstdint>
#include <unordered_set>

namespace sky::net {

    // A connection partition fixed at startup. Owns its connection set and counters so per-lane load can
    // be reported and lanes can be added/removed for new connections later. Per-lane I/O threads are
    // deferred: the backend contract advances all connections in one Pump.
    struct NetworkLane {
        uint32_t           id = 0;
        uint64_t           bytesIn  = 0;
        uint64_t           bytesOut = 0;
        std::unordered_set<ConnectionId, ConnectionIdHash> connections;

        void Add(ConnectionId connection) { connections.insert(connection); }

        void Remove(ConnectionId connection) { connections.erase(connection); }
    };

} // namespace sky::net
