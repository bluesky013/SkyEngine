//
// Created by Zach Lee on 2021/12/4.
//

#include <core/math/Sfmt.h>
#include <core/util/Uuid.h>
#include <core/math/Random.h>

namespace sky {

    Uuid Uuid::Create()
    {
        Uuid res;
        Random::Gen(&res, sizeof(Uuid));

        res.data[8] &= 0xBF;
        res.data[8] |= 0x80;
        res.data[6] &= 0x4F;
        res.data[6] |= 0x40;
        return res;
    }

    Uuid Uuid::CreateWithSeed(uint32_t seed)
    {
        Uuid       res;
        SFMTRandom random(seed);
        uint64_t  *lPtr = reinterpret_cast<uint64_t *>(&res);
        lPtr[0]         = random.GenU64();
        lPtr[1]         = random.GenU64();

        res.data[8] &= 0xBF;
        res.data[8] |= 0x80;
        res.data[6] &= 0x4F;
        res.data[6] |= 0x40;
        return res;
    }

    std::string Uuid::ToString() const
    {
        std::string res = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
        char       *out = res.data();
        for (uint32_t i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                *out++ = '-';
            }

            unsigned char ch = data[i];
            *out++           = DIGITS[(ch >> 4)];
            *out++           = DIGITS[(ch & 15)];
        }
        return res;
    }

} // namespace sky