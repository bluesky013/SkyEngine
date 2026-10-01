//
// Created on 2026/09/25.
//

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <span>

namespace sky::net {

    using Sha256Digest = std::array<uint8_t, 32>;

    // SHA-256 and HMAC-SHA256. Uses OpenSSL when SKY_NETWORK_OPENSSL is defined; otherwise a
    // self-contained implementation so the module keeps no mandatory third-party dependency.
    Sha256Digest Sha256(std::span<const uint8_t> data);
    Sha256Digest HmacSha256(std::span<const uint8_t> key, std::span<const uint8_t> data);

} // namespace sky::net
