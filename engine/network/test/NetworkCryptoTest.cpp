//
// Created on 2026/09/25.
//

#include "crypto/Sha256.h"

#include <cstring>
#include <gtest/gtest.h>
#include <span>

using namespace sky::net;

TEST(NetworkCryptoTest, Sha256KnownVector)
{
    const char *message = "abc";
    const Sha256Digest digest =
        Sha256(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(message), 3));

    const uint8_t expected[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                  0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                  0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    EXPECT_EQ(0, std::memcmp(digest.data(), expected, sizeof(expected)));
}

TEST(NetworkCryptoTest, HmacSha256Rfc4231Case1)
{
    uint8_t key[20];
    std::memset(key, 0x0b, sizeof(key));
    const char *message = "Hi There";
    const Sha256Digest digest =
        HmacSha256(std::span<const uint8_t>(key, sizeof(key)),
                   std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(message), 8));

    const uint8_t expected[32] = {0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf,
                                  0xce, 0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83,
                                  0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
    EXPECT_EQ(0, std::memcmp(digest.data(), expected, sizeof(expected)));
}
