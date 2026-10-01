//
// Created on 2026/09/25.
//

#include "Sha256.h"

#include <cstring>
#include <vector>

#if defined(SKY_NETWORK_OPENSSL)
#include <openssl/hmac.h>
#include <openssl/sha.h>
#endif

namespace sky::net {

    namespace {

        constexpr uint32_t K[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

        constexpr uint32_t Rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32u - n)); }

        void Sha256Blocks(uint32_t state[8], const uint8_t *data, size_t blocks)
        {
            for (size_t b = 0; b < blocks; ++b) {
                uint32_t w[64];
                for (uint32_t i = 0; i < 16; ++i) {
                    const uint8_t *p = data + b * 64 + i * 4;
                    w[i] = (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
                           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
                }
                for (uint32_t i = 16; i < 64; ++i) {
                    const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                    const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
                }

                uint32_t a = state[0], bb = state[1], c = state[2], d = state[3];
                uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
                for (uint32_t i = 0; i < 64; ++i) {
                    const uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
                    const uint32_t ch = (e & f) ^ (~e & g);
                    const uint32_t t1 = h + s1 + ch + K[i] + w[i];
                    const uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
                    const uint32_t maj = (a & bb) ^ (a & c) ^ (bb & c);
                    const uint32_t t2 = s0 + maj;
                    h = g; g = f; f = e; e = d + t1;
                    d = c; c = bb; bb = a; a = t1 + t2;
                }
                state[0] += a; state[1] += bb; state[2] += c; state[3] += d;
                state[4] += e; state[5] += f; state[6] += g; state[7] += h;
            }
        }

        Sha256Digest Sha256Builtin(std::span<const uint8_t> data)
        {
            uint32_t state[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

            const size_t fullBlocks = data.size() / 64;
            Sha256Blocks(state, data.data(), fullBlocks);

            uint8_t tail[128] = {};
            const size_t remainder = data.size() - fullBlocks * 64;
            if (remainder > 0) {
                std::memcpy(tail, data.data() + fullBlocks * 64, remainder);
            }
            tail[remainder] = 0x80u;
            const size_t tailLen = (remainder < 56) ? 64 : 128;
            const uint64_t bitLen = static_cast<uint64_t>(data.size()) * 8u;
            for (uint32_t i = 0; i < 8; ++i) {
                tail[tailLen - 1 - i] = static_cast<uint8_t>((bitLen >> (i * 8u)) & 0xFFu);
            }
            Sha256Blocks(state, tail, tailLen / 64);

            Sha256Digest digest{};
            for (uint32_t i = 0; i < 8; ++i) {
                digest[i * 4 + 0] = static_cast<uint8_t>((state[i] >> 24) & 0xFFu);
                digest[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 16) & 0xFFu);
                digest[i * 4 + 2] = static_cast<uint8_t>((state[i] >> 8) & 0xFFu);
                digest[i * 4 + 3] = static_cast<uint8_t>(state[i] & 0xFFu);
            }
            return digest;
        }

    } // namespace

    Sha256Digest Sha256(std::span<const uint8_t> data)
    {
#if defined(SKY_NETWORK_OPENSSL)
        Sha256Digest digest{};
        SHA256(data.data(), data.size(), digest.data());
        return digest;
#else
        return Sha256Builtin(data);
#endif
    }

    Sha256Digest HmacSha256(std::span<const uint8_t> key, std::span<const uint8_t> data)
    {
#if defined(SKY_NETWORK_OPENSSL)
        Sha256Digest digest{};
        unsigned int length = 0;
        HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), data.data(), data.size(), digest.data(), &length);
        return digest;
#else
        uint8_t normalized[64] = {};
        if (key.size() > 64) {
            const Sha256Digest hashed = Sha256Builtin(key);
            std::memcpy(normalized, hashed.data(), hashed.size());
        } else if (!key.empty()) {
            std::memcpy(normalized, key.data(), key.size());
        }

        uint8_t inner[64];
        uint8_t outer[64];
        for (uint32_t i = 0; i < 64; ++i) {
            inner[i] = static_cast<uint8_t>(normalized[i] ^ 0x36u);
            outer[i] = static_cast<uint8_t>(normalized[i] ^ 0x5cu);
        }

        std::vector<uint8_t> innerInput;
        innerInput.reserve(64 + data.size());
        innerInput.insert(innerInput.end(), inner, inner + 64);
        innerInput.insert(innerInput.end(), data.begin(), data.end());
        const Sha256Digest innerHash = Sha256Builtin(innerInput);

        uint8_t outerInput[64 + 32];
        std::memcpy(outerInput, outer, 64);
        std::memcpy(outerInput + 64, innerHash.data(), innerHash.size());
        return Sha256Builtin(std::span<const uint8_t>(outerInput, sizeof(outerInput)));
#endif
    }

} // namespace sky::net
