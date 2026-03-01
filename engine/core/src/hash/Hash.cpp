//
// Created by blues on 2025/2/11.
//

#include <core/hash/Hash.h>
#include <cstring>
#include <unordered_map>
namespace sky {

    uint32_t Murmur3Hash32(const uint8_t* data, size_t length, uint32_t seed)
    {
        const size_t blocks = length / 4;

        static constexpr uint32_t C1 = 0xcc9e2d51;
        static constexpr uint32_t C2 = 0x1b873593;

        uint32_t hash = seed;
        for (size_t i = 0; i < blocks; ++i) {
            uint32_t k1 = 0;
            memcpy(&k1, data + i * 4, sizeof(uint32_t));

            k1 *= C1;
            k1 = (k1 << 15) | (k1 >> 17);       // ROL32(k1, 15)
            k1 *= C2;

            hash ^= k1;
            hash = (hash << 13) | (hash >> 19); // ROL32(h1, 13)
            hash = hash * 5 + 0xe6546b64;
        }

        // tail
        const uint8_t* tail = data + blocks * 4;
        uint32_t k1 = 0;

        switch (length & 3) {
        case 3: k1 ^= tail[2] << 16; // fall through
        case 2: k1 ^= tail[1] << 8;  // fall through
        case 1: k1 ^= tail[0];
            k1 *= C1;
            k1 = (k1 << 15) | (k1 >> 17); // ROL32(k1, 15)
            k1 *= C2;
            hash ^= k1;
        }

        // finalize
        hash ^= length;
        hash ^= hash >> 16;
        hash *= 0x85ebca6b;
        hash ^= hash >> 13;
        hash *= 0xc2b2ae35;
        hash ^= hash >> 16;

        return hash;
    }

    uint32_t Murmur3Hash32(std::initializer_list<uint32_t> u32List, uint32_t seed)
    {
        const auto* data = reinterpret_cast<const uint8_t*>(u32List.begin());
        size_t length = u32List.size() * sizeof(uint32_t);

        return Murmur3Hash32(data, length, seed);
    }

} // namespace sky