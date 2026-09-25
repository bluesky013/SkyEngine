//
// Created on 2026/09/25.
//

#include <network/NetworkSession.h>

namespace sky::net {

    namespace {

        void WriteU64(uint8_t *out, uint64_t value)
        {
            for (uint32_t i = 0; i < 8; ++i) {
                out[i] = static_cast<uint8_t>((value >> (i * 8u)) & 0xFFu);
            }
        }

        void WriteU32(uint8_t *out, uint32_t value)
        {
            for (uint32_t i = 0; i < 4; ++i) {
                out[i] = static_cast<uint8_t>((value >> (i * 8u)) & 0xFFu);
            }
        }

        uint64_t ReadU64(const uint8_t *in)
        {
            uint64_t value = 0;
            for (uint32_t i = 0; i < 8; ++i) {
                value |= static_cast<uint64_t>(in[i]) << (i * 8u);
            }
            return value;
        }

        uint32_t ReadU32(const uint8_t *in)
        {
            uint32_t value = 0;
            for (uint32_t i = 0; i < 4; ++i) {
                value |= static_cast<uint32_t>(in[i]) << (i * 8u);
            }
            return value;
        }

        uint64_t KeyedHash(uint64_t seed, const uint8_t *data, uint32_t size)
        {
            // FNV-1a 64 with the secret folded into the initial state.
            uint64_t hash = 1469598103934665603ull ^ seed;
            for (uint32_t i = 0; i < size; ++i) {
                hash ^= data[i];
                hash *= 1099511628211ull;
            }
            return hash;
        }

    } // namespace

    uint32_t ResumeToken::Serialize(uint8_t *out, uint32_t capacity) const
    {
        if (out == nullptr || capacity < SERIALIZED_SIZE) {
            return 0;
        }
        WriteU64(out + 0, session);
        WriteU64(out + 8, expiresAtMs);
        WriteU32(out + 16, nonce);
        WriteU64(out + 20, signature);
        return SERIALIZED_SIZE;
    }

    bool ResumeToken::Deserialize(const uint8_t *data, uint32_t size, ResumeToken &out)
    {
        if (data == nullptr || size < SERIALIZED_SIZE) {
            return false;
        }
        out.session     = ReadU64(data + 0);
        out.expiresAtMs = ReadU64(data + 8);
        out.nonce       = ReadU32(data + 16);
        out.signature   = ReadU64(data + 20);
        return true;
    }

    uint64_t ResumeTokenCodec::Sign(const ResumeToken &token) const
    {
        uint8_t buffer[24] = {};
        WriteU64(buffer + 0, token.session);
        WriteU64(buffer + 8, token.expiresAtMs);
        WriteU32(buffer + 16, token.nonce);
        return KeyedHash(secret, buffer, sizeof(buffer));
    }

    ResumeToken ResumeTokenCodec::Issue(SessionId session, uint64_t nowMs, uint64_t ttlMs, uint32_t nonce) const
    {
        ResumeToken token;
        token.session     = session.value;
        token.expiresAtMs = nowMs + ttlMs;
        token.nonce       = nonce;
        token.signature   = Sign(token);
        return token;
    }

    bool ResumeTokenCodec::Verify(const ResumeToken &token, uint64_t nowMs) const
    {
        if (token.session == 0 || token.signature != Sign(token)) {
            return false;
        }
        return nowMs <= token.expiresAtMs;
    }

} // namespace sky::net
