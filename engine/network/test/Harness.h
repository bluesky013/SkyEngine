//
// Created on 2026/09/25.
//

#pragma once

#include <network/NetworkHost.h>

#include "LoopbackBackend.h"
#include "LoopbackHarness.h"

#include <memory>

namespace sky::net::test {

    inline constexpr const char *LOOPBACK_ADDR = "loopback:9000";

    // Builds a connected client/server pair over the loopback transport.
    struct NetPair {
        LoopbackBackend serverBackend;
        LoopbackBackend clientBackend;

        std::unique_ptr<NetworkHost> server;
        std::unique_ptr<NetworkHost> client;

        ConnectionId serverConn = INVALID_CONNECTION_ID;
        ConnectionId clientConn = INVALID_CONNECTION_ID;

        int serverConnected = 0;
        int clientConnected = 0;
        int serverDisconnected = 0;
        DisconnectReason lastServerReason = DisconnectReason::Unknown;

        void Init(uint32_t serverQueueCapacity = 4096)
        {
            server = std::make_unique<NetworkHost>(ServerHostConfig(serverQueueCapacity));
            server->SetServer(true);
            server->AttachBackend(NetworkRole::Server, &serverBackend);
            server->SetConnectHandler([this](ConnectionId id) {
                ++serverConnected;
                serverConn = id;
            });
            server->SetDisconnectHandler([this](ConnectionId, DisconnectReason reason) {
                ++serverDisconnected;
                lastServerReason = reason;
            });

            client = std::make_unique<NetworkHost>(ClientHostConfig());
            client->AttachBackend(NetworkRole::Client, &clientBackend);
            client->SetConnectHandler([this](ConnectionId id) {
                ++clientConnected;
                clientConn = id;
            });

            server->Listen(NetworkAddress::Parse(LOOPBACK_ADDR));
            client->Connect(NetworkAddress::Parse(LOOPBACK_ADDR));
        }

        void Pump(int iterations) { PumpBoth(*client, *server, iterations); }

        void ConnectAndSettle()
        {
            Init();
            Pump(20);
        }
    };

} // namespace sky::net::test
