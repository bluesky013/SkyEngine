//
// Created by blues on 2024/12/29.
//

#include <core/crypto/md5/MD5.h>
#include "MD5.h"

namespace sky {

    MD5 MD5::CalculateMD5(const std::string &input)
    {
        MD5 res = {};

        MD5_CTX ctx = {};
        MD5Init(&ctx);
        MD5Update(&ctx, reinterpret_cast<const unsigned char *>(input.data()), static_cast<uint32_t>(input.length()));
        MD5Final(res.u8, &ctx);

        return res;
    }

    std::string MD5::ToString() const
    {
        char buf[33] = {};
        for (int i=0; i<16; i++) {
#if _MSC_VER
            sprintf_s(buf + i * 2, 33 - (i * 2), "%02x", u8[i]);
#else
            sprintf(buf+i*2, "%02x", u8[i]);
#endif
        }
        buf[32] = 0;

        return buf;
    }

} // namespace sky