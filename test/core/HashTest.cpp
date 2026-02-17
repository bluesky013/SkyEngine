//
// Created by blues on 2025/2/11.
//

#include <gtest/gtest.h>
#include <core/hash/Hash.h>

using namespace sky;

TEST(HashTest, Murmur3Test)
{
    std::vector<uint32_t> data = {
        1, 2, 3, 4, 5
    };

    uint32_t hash1 = Murmur3Hash32(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(uint32_t), 0);
    uint32_t hash2 = Murmur3Hash32({1, 2, 3, 4, 5}, 0);

    ASSERT_EQ(hash1, hash2);
}