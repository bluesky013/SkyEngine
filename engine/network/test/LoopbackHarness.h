//
// Created on 2026/09/25.
//

#pragma once

#include <network/NetworkCaps.h>
#include <network/NetworkHost.h>

namespace sky::net::test {

    inline NetworkHostConfig ClientHostConfig(uint32_t queueCapacity = 4096)
    {
        NetworkHostConfig config;
        config.role = NetworkRole::Client;
        config.eventQueueCapacity = queueCapacity;
        return config;
    }

    inline NetworkHostConfig ServerHostConfig(uint32_t queueCapacity = 4096)
    {
        NetworkHostConfig config;
        config.role = NetworkRole::Server;
        config.eventQueueCapacity = queueCapacity;
        return config;
    }

    // Pumps a client/server host pair in lockstep for a number of iterations.
    inline void PumpBoth(NetworkHost &client, NetworkHost &server, int iterations)
    {
        for (int i = 0; i < iterations; ++i) {
            client.Update();
            server.Update();
        }
    }

} // namespace sky::net::test
