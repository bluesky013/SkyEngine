//
// Created on 2026/09/25.
//

#pragma once

#include <network/ConnectionId.h>

#include <array>
#include <cstdint>

namespace sky::net {

    using ResumeTokenSignature = std::array<uint8_t, 32>;

    // Stateless, expiring, HMAC-SHA256-signed resume token. Any server holding the shared key can verify
    // it without a directory service.
    struct ResumeToken {
        uint64_t             session     = 0;
        uint64_t             expiresAtMs = 0;
        uint32_t             nonce       = 0;
        ResumeTokenSignature signature{};

        static constexpr uint32_t SERIALIZED_SIZE = 8 + 8 + 4 + 32;

        bool IsZero() const;

        uint32_t    Serialize(uint8_t *out, uint32_t capacity) const;
        static bool Deserialize(const uint8_t *data, uint32_t size, ResumeToken &out);
    };

    class ResumeTokenCodec {
    public:
        explicit ResumeTokenCodec(uint64_t secret = 0) : secret(secret) {}

        ResumeToken Issue(SessionId session, uint64_t nowMs, uint64_t ttlMs, uint32_t nonce) const;

        // Constant-time signature comparison to avoid a timing side channel.
        bool Verify(const ResumeToken &token, uint64_t nowMs) const;

    private:
        ResumeTokenSignature Sign(const ResumeToken &token) const;

        uint64_t secret;
    };

} // namespace sky::net
