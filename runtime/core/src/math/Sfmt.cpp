//
// Created by Zach Lee on 2021/12/4.
//

#include <core/math/Random.h>
#include <core/math/Sfmt.h>

namespace sky {

    SFMTRandom::SFMTRandom()
    {
        uint32_t seed[32] = {0};
        Random::Gen(&seed, sizeof(seed));
        sfmt_init_by_array(&sfmt, seed, 32);
    }

    SFMTRandom::SFMTRandom(uint32_t s) : seed(s)
    {
        sfmt_init_gen_rand(&sfmt, seed);
    }

    uint32_t SFMTRandom::GenU32()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return sfmt_genrand_uint32(&sfmt);
    }

    uint64_t SFMTRandom::GenU64()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return sfmt_genrand_uint64(&sfmt);
    }

} // namespace sky