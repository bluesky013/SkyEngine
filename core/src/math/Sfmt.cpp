//
// Created by Zach Lee on 2021/12/4.
//

#include <core/math/Sfmt.h>
#include <core/math/Random.h>

namespace sky {

    SFMTRandom::SFMTRandom()
    {
        Random::Gen(&seed, sizeof(uint32_t));
        sfmt_init_gen_rand(&sfmt, seed);
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

}