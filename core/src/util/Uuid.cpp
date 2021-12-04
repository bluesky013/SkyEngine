//
// Created by Zach Lee on 2021/12/4.
//

#include <core/util/Uuid.h>
#include <core/math/Sfmt.h>

namespace sky {

    SFMTRandom random;

    Uuid::Uuid()
    {
        word[0] = 0;
        word[1] = 0;
    }

    Uuid Uuid::Create()
    {
        Uuid res;
        res.word[0] = random.GenU64();
        res.word[1] = random.GenU64();

        res.data[8] &= 0xBF;
        res.data[8] |= 0x80;

        res.data[6] &= 0x4F;
        res.data[6] |= 0x40;
        return res;
    }

    std::string Uuid::ToString() const
    {
        static const char *DIGITS = "0123456789abcdef";
        std::string res = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
        char* out = res.data();
        for (uint32_t i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                *out++ = '-';
            }

            unsigned char val = data[i];
            *out++ = DIGITS[(val >> 4)];
            *out++ = DIGITS[(val & 15)];
        }
        return res;
    }

}