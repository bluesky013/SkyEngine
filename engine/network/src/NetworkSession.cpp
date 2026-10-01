//
// Created on 2026/09/25.
//

#include <network/NetworkSession.h>

#include <network/detail/ByteCodec.h>

#include "crypto/Sha256.h"

#include <cstring>
#include <vector>

namespace sky::net {

    bool ResumeToken::IsZero() const
    {
        if (session != 0 || expiresAtMs != 0 || nonce != 0) {
            return false;
        }
        for (uint8_t byte : signature) {
            if (byte != 0) {
                return false;
            }
        }
        return true;
    }

    uint32_t ResumeToken::Serialize(uint8_t *out, uint32_t capacity) const
    {
        if (out == nullptr || capacity < SERIALIZED_SIZE) {
            return 0;
        }
        std::vector<uint8_t> buffer;
        buffer.reserve(SERIALIZED_SIZE);
        ByteWriter writer(buffer);
        writer.U64(session);
        writer.U64(expiresAtMs);
        writer.U32(nonce);
        writer.Bytes(signature);
        std::memcpy(out, buffer.data(), SERIALIZED_SIZE);
        return SERIALIZED_SIZE;
    }

    bool ResumeToken::Deserialize(const uint8_t *data, uint32_t size, ResumeToken &out)
    {
        if (data == nullptr || size < SERIALIZED_SIZE) {
            return false;
        }
        ByteReader reader(std::span<const uint8_t>(data, size));
        if (!reader.U64(out.session) || !reader.U64(out.expiresAtMs) || !reader.U32(out.nonce)) {
            return false;
        }
        std::span<const uint8_t> signatureBytes;
        if (!reader.Bytes(static_cast<uint32_t>(out.signature.size()), signatureBytes)) {
            return false;
        }
        std::memcpy(out.signature.data(), signatureBytes.data(), out.signature.size());
        return true;
    }

    ResumeTokenSignature ResumeTokenCodec::Sign(const ResumeToken &token) const
    {
        std::vector<uint8_t> message;
        ByteWriter writer(message);
        writer.U64(token.session);
        writer.U64(token.expiresAtMs);
        writer.U32(token.nonce);

        uint8_t key[8];
        for (uint32_t i = 0; i < 8; ++i) {
            key[i] = static_cast<uint8_t>((secret >> (i * 8u)) & 0xFFu);
        }

        return HmacSha256(std::span<const uint8_t>(key, sizeof(key)), message);
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
        if (token.session == 0) {
            return false;
        }
        const ResumeTokenSignature expected = Sign(token);
        uint8_t diff = 0;
        for (uint32_t i = 0; i < expected.size(); ++i) {
            diff |= static_cast<uint8_t>(token.signature[i] ^ expected[i]);
        }
        if (diff != 0) {
            return false;
        }
        return nowMs <= token.expiresAtMs;
    }

} // namespace sky::net
