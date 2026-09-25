//
// Created on 2026/09/25.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sky::net {

    // Transport endpoint. Kept as text plus a parsed family so IPv4, IPv6, and hostnames are all
    // representable without pulling in a socket library at the contract level.
    struct NetworkAddress {
        enum class Family : uint8_t {
            Unknown = 0,
            IPv4,
            IPv6,
            Host
        };

        std::string host;
        uint16_t    port   = 0;
        Family      family = Family::Unknown;

        bool IsValid() const { return !host.empty() && port != 0; }

        // Accepts "host:port", "ipv4:port", and "[ipv6]:port".
        static NetworkAddress Parse(std::string_view text);
        std::string ToString() const;

        bool operator==(const NetworkAddress &rhs) const
        {
            return host == rhs.host && port == rhs.port;
        }

        bool operator!=(const NetworkAddress &rhs) const { return !(*this == rhs); }
    };

} // namespace sky::net
