//
// Created on 2026/09/25.
//

#pragma once

#include <network/NetworkHost.h>

#include "LoopbackBackend.h"

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
            NetworkHostConfig serverConfig;
            serverConfig.role              = NetworkRole::Server;
            serverConfig.eventQueueCapacity = serverQueueCapacity;
            server = std::make_unique<NetworkHost>(serverConfig);
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

            NetworkHostConfig clientConfig;
            clientConfig.role = NetworkRole::Client;
            client = std::make_unique<NetworkHost>(clientConfig);
            client->AttachBackend(NetworkRole::Client, &clientBackend);
            client->SetConnectHandler([this](ConnectionId id) {
                ++clientConnected;
                clientConn = id;
            });

            server->Listen(NetworkAddress::Parse(LOOPBACK_ADDR));
            client->Connect(NetworkAddress::Parse(LOOPBACK_ADDR));
        }

        void Pump(int iterations)
        {
            for (int i = 0; i < iterations; ++i) {
                client->Update();
                server->Update();
            }
        }

        void ConnectAndSettle()
        {
            Init();
            Pump(20);
        }
    };

} // namespace sky::net::test
