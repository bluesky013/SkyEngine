//
// Created on 2026/09/25.
//

#pragma once

#include <network/NetworkTypes.h>

#include <cstdint>

namespace sky::net {

    // Traffic category a backend is attached under. Cluster and Control are reserved: they are part of
    // the role model but require no backend implementation in the current change.
    enum class NetworkRole : uint8_t {
        Client = 0,
        Server,
        Cluster,
        Control,
        Count
    };

    constexpr const char *ToString(NetworkRole role)
    {
        switch (role) {
        case NetworkRole::Client:  return "Client";
        case NetworkRole::Server:  return "Server";
        case NetworkRole::Cluster: return "Cluster";
        case NetworkRole::Control: return "Control";
        case NetworkRole::Count:   break;
        }
        return "Unknown";
    }

    // Host-owned threading policy. Backends declare what they can run under; the host selects it.
    enum class NetworkThreading : uint8_t {
        CallerPump = 0,
        OwnedThread
    };

    struct NetworkThreadingCaps {
        bool     supportsCallerPump = true;
        bool     supportsHostThread = false;
        bool     backendOwnsThreads = false;
        bool     threadSafeSend     = false;
        bool     wakeupSupport      = false;
        uint32_t ioThreadCount      = 1;
    };

    // Backend capability descriptor. Consumers select behavior from capabilities, never from a concrete
    // backend type.
    struct NetworkBackendCaps {
        bool reliable    = false;
        bool unreliable  = false;
        bool ordered     = false;
        bool encryption  = false;
        bool client      = false;
        bool server      = false;
        bool peerToPeer  = false;
        // True when sequenced delivery carries the sender's sequence (loss-detectable). A backend that
        // synthesizes the sequence on receive reports false.
        bool realSendSequence = false;

        DeliveryMode         defaultMode = DeliveryMode::ReliableOrdered;
        NetworkThreadingCaps threading{};

        uint32_t maxChannels = 1;
        uint32_t maxPayload  = 1200;
        uint32_t mtu         = 1400;

        bool Supports(DeliveryMode mode) const
        {
            switch (mode) {
            case DeliveryMode::ReliableOrdered:
            case DeliveryMode::ReliableUnordered:
                return reliable;
            case DeliveryMode::UnreliableSequenced:
            case DeliveryMode::Unreliable:
                return unreliable;
            }
            return false;
        }
    };

} // namespace sky::net
