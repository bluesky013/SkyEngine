//
// Created on 2026/09/25.
//

#include <network/NetworkAddress.h>

#include <array>
#include <cstdio>
#include <cstdlib>

namespace sky::net {

    namespace {

        bool LooksLikeIPv4(const std::string &host)
        {
            int dots = 0;
            for (char c : host) {
                if (c == '.') {
                    ++dots;
                } else if (c < '0' || c > '9') {
                    return false;
                }
            }
            return dots == 3;
        }

        bool LooksLikeIPv6(const std::string &host)
        {
            bool hasColon = false;
            for (char c : host) {
                if (c == ':') {
                    hasColon = true;
                    continue;
                }
                const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
                if (!hex) {
                    return false;
                }
            }
            return hasColon;
        }

        bool ParsePort(std::string_view text, uint16_t &out)
        {
            if (text.empty()) {
                return false;
            }
            unsigned long value = 0;
            for (char c : text) {
                if (c < '0' || c > '9') {
                    return false;
                }
                value = value * 10u + static_cast<unsigned long>(c - '0');
                if (value > 65535ul) {
                    return false;
                }
            }
            out = static_cast<uint16_t>(value);
            return true;
        }

    } // namespace

    NetworkAddress NetworkAddress::Parse(std::string_view text)
    {
        NetworkAddress result;
        if (text.empty()) {
            return result;
        }

        if (text.front() == '[') {
            const size_t close = text.find(']');
            if (close == std::string_view::npos || close + 1 >= text.size() || text[close + 1] != ':') {
                return result;
            }
            result.host = std::string(text.substr(1, close - 1));
            if (!ParsePort(text.substr(close + 2), result.port)) {
                return result;
            }
        } else {
            const size_t colon = text.rfind(':');
            if (colon == std::string_view::npos) {
                return result;
            }
            result.host = std::string(text.substr(0, colon));
            if (!ParsePort(text.substr(colon + 1), result.port)) {
                return result;
            }
        }

        if (LooksLikeIPv4(result.host)) {
            result.family = Family::IPv4;
        } else if (LooksLikeIPv6(result.host)) {
            result.family = Family::IPv6;
        } else {
            result.family = Family::Host;
        }
        return result;
    }

    std::string NetworkAddress::ToString() const
    {
        std::array<char, 16> portText{};
        std::snprintf(portText.data(), portText.size(), "%u", static_cast<unsigned>(port));
        if (family == Family::IPv6) {
            return "[" + host + "]:" + portText.data();
        }
        return host + ":" + portText.data();
    }

} // namespace sky::net
