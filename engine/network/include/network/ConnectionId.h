//
// Created on 2026/09/25.
//

#pragma once

#include <cstdint>
#include <functional>

namespace sky::net {

    // Stable handle to a connection. index 0 with generation 0 is reserved so a default handle never
    // resolves. Mirrors phy::PhysicsObjectId.
    struct ConnectionId {
        uint64_t index      = 0;
        uint32_t generation = 0;

        constexpr bool operator==(const ConnectionId &rhs) const
        {
            return index == rhs.index && generation == rhs.generation;
        }

        constexpr bool operator!=(const ConnectionId &rhs) const { return !(*this == rhs); }

        constexpr bool operator<(const ConnectionId &rhs) const
        {
            return index != rhs.index ? index < rhs.index : generation < rhs.generation;
        }
    };

    inline constexpr ConnectionId INVALID_CONNECTION_ID = {0, 0};

    constexpr bool IsValid(ConnectionId id)
    {
        return id.index != 0 && id.generation != 0;
    }

    struct ConnectionIdHash {
        size_t operator()(const ConnectionId &id) const noexcept
        {
            return std::hash<uint64_t>()(id.index) ^ (std::hash<uint32_t>()(id.generation) << 1);
        }
    };

    // Logical session identity, independent of the physical connection so it survives reconnects.
    struct SessionId {
        uint64_t value = 0;

        constexpr bool operator==(const SessionId &rhs) const { return value == rhs.value; }
        constexpr bool operator!=(const SessionId &rhs) const { return value != rhs.value; }
    };

    inline constexpr SessionId INVALID_SESSION_ID = {0};

    constexpr bool IsValid(SessionId id) { return id.value != 0; }

} // namespace sky::net
