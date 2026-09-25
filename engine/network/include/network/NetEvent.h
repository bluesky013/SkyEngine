//
// Created on 2026/09/25.
//

#pragma once

#include <network/ConnectionId.h>
#include <network/NetworkTypes.h>

#include <cstdint>
#include <span>

namespace sky::net {

    enum class NetEventType : uint8_t {
        Connected = 0,
        Disconnected,
        Message,
        Error
    };

    // Backend-facing sink. A backend pushes events here as it advances; the host owns the queue and
    // copies payloads so events stay valid after the backend's receive buffer is gone.
    class INetEventSink {
    public:
        virtual ~INetEventSink() = default;

        virtual void OnConnected(ConnectionId id) = 0;
        virtual void OnDisconnected(ConnectionId id, DisconnectReason reason) = 0;
        virtual void OnMessage(ConnectionId id, ChannelId channel, MessageSequence sequence,
                               std::span<const uint8_t> payload) = 0;
        virtual void OnError(ConnectionId id, NetResult error) = 0;
    };

} // namespace sky::net
