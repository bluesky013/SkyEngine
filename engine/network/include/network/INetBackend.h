//
// Created on 2026/09/25.
//

#pragma once

#include <network/ConnectionId.h>
#include <network/NetworkAddress.h>
#include <network/NetworkCaps.h>
#include <network/NetworkTypes.h>
#include <network/NetEvent.h>

#include <cstdint>
#include <span>

namespace sky::net {

    struct ListenDesc {
        NetworkAddress address;
        uint32_t       maxConnections = 0;
    };

    struct ConnectDesc {
        NetworkAddress address;
        uint32_t       timeoutMs = 0;
    };

    // A live connection. Consumers reference it by ConnectionId; they never touch a backend type.
    class INetConnection {
    public:
        virtual ~INetConnection() = default;

        virtual ConnectionId GetId() const = 0;
        virtual NetResult Send(ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode) = 0;
        virtual void Close(DisconnectReason reason) = 0;
        virtual uint32_t GetRttMs() const = 0;
    };

    class INetListener {
    public:
        virtual ~INetListener() = default;

        virtual void Broadcast(ChannelId channel, std::span<const uint8_t> payload, DeliveryMode mode) = 0;
        virtual NetworkAddress GetAddress() const = 0;
        virtual void Close() = 0;
    };

    // The only I/O advancement entry point. waitMs == 0 must be non-blocking; waitMs > 0 may block up to
    // waitMs so an OwnedThread loop can sleep efficiently. Returns the number of events delivered.
    class INetBackend {
    public:
        virtual ~INetBackend() = default;

        virtual const NetworkBackendCaps &GetCaps() const = 0;

        virtual bool Init() = 0;
        virtual void Shutdown() = 0;

        virtual INetListener *Listen(const ListenDesc &desc) = 0;
        virtual ConnectionId  Connect(const ConnectDesc &desc) = 0;

        virtual uint32_t Pump(INetEventSink &sink, uint32_t maxEvents, uint32_t waitMs) = 0;

        virtual INetConnection *GetConnection(ConnectionId id) = 0;
    };

} // namespace sky::net
