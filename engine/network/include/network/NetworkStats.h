//
// Created on 2026/09/25.
//

#pragma once

#include <cstdint>

namespace sky::net {

    struct NetworkLaneStats {
        uint32_t connections = 0;
        uint64_t bytesIn     = 0;
        uint64_t bytesOut    = 0;
    };

    inline constexpr uint32_t NETWORK_MAX_LANES = 64;

    // Aggregate host statistics intended as input for an external orchestrator. Readable while serving.
    struct NetworkHostStats {
        uint32_t activeConnections = 0;
        uint32_t laneCount         = 0;
        uint32_t rttMs             = 0;
        uint64_t bytesIn           = 0;
        uint64_t bytesOut          = 0;
        uint64_t messagesDropped   = 0;
        uint64_t reconnectAttempts = 0;

        NetworkLaneStats lanes[NETWORK_MAX_LANES] = {};
    };

} // namespace sky::net
