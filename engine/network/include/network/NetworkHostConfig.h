//
// Created on 2026/09/25.
//

#pragma once

#include <network/NetworkCaps.h>

#include <cstdint>

namespace sky::net {

    enum class NetworkLifecycle : uint8_t {
        Accepting = 0,
        Draining,
        Closed
    };

    constexpr const char *ToString(NetworkLifecycle state)
    {
        switch (state) {
        case NetworkLifecycle::Accepting: return "Accepting";
        case NetworkLifecycle::Draining:  return "Draining";
        case NetworkLifecycle::Closed:    return "Closed";
        }
        return "Unknown";
    }

    struct NetworkHostConfig {
        NetworkRole      role = NetworkRole::Client;
        uint32_t         laneCount = 1;

        // Bounded inbound event queue. Overflow closes the affected connection with Backpressure.
        uint32_t         eventQueueCapacity = 4096;
        uint32_t         pumpEventBudget    = 256;

        // OwnedThread: how long each Pump may block waiting for I/O.
        uint32_t         ioWaitMs = 1;

        // Timers (real monotonic time, independent of the world tick).
        uint32_t         heartbeatIntervalMs = 1000;
        uint32_t         timeoutMs           = 5000;

        // Resume token lifetime.
        uint32_t         resumeTokenTtlMs    = 60000;

        // Client reconnect backoff.
        bool             autoReconnect            = false;
        uint32_t         reconnectInitialBackoffMs = 250;
        uint32_t         reconnectMaxBackoffMs     = 5000;
    };

} // namespace sky::net
