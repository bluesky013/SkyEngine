//
// Created on 2026/09/25.
//

#pragma once

#include <network/ConnectionId.h>

#include <cstdint>

namespace sky::net {

    // Stateless, expiring, keyed-hash-signed resume token. Any server holding the shared key can verify
    // it without a directory service. (Placeholder keyed hash; to be replaced by a crypto HMAC.)
    struct ResumeToken {
        uint64_t session     = 0;
        uint64_t expiresAtMs = 0;
        uint32_t nonce       = 0;
        uint64_t signature   = 0;

        static constexpr uint32_t SERIALIZED_SIZE = 8 + 8 + 4 + 8;

        bool IsZero() const { return session == 0 && expiresAtMs == 0 && nonce == 0 && signature == 0; }

        uint32_t Serialize(uint8_t *out, uint32_t capacity) const;
        static bool Deserialize(const uint8_t *data, uint32_t size, ResumeToken &out);
    };

    class ResumeTokenCodec {
    public:
        explicit ResumeTokenCodec(uint64_t secret = 0) : secret(secret) {}

        ResumeToken Issue(SessionId session, uint64_t nowMs, uint64_t ttlMs, uint32_t nonce) const;
        bool        Verify(const ResumeToken &token, uint64_t nowMs) const;

        uint64_t GetSecret() const { return secret; }

    private:
        uint64_t Sign(const ResumeToken &token) const;

        uint64_t secret;
    };

} // namespace sky::net
