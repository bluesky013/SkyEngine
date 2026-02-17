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

    MD5 MD5::CalculateMD5(const char* data, size_t length)
    {
        MD5 res = {};

        MD5_CTX ctx = {};
        MD5Init(&ctx);
        MD5Update(&ctx, reinterpret_cast<const unsigned char *>(data), static_cast<uint32_t>(length));
        MD5Final(res.u8, &ctx);

        return res;
    }

    bool MD5::operator == (const MD5 &val) const
    {
        return (u64[0] == val.u64[0]) && (u64[1] == val.u64[1]);
    }

    bool MD5::operator != (const MD5 &val) const
    {
        return (u64[0] != val.u64[0]) || (u64[1] != val.u64[1]);
    }

    std::string MD5::ToString() const
    {
        char buf[33] = {};
        for (int i=0; i<16; i++) {
#ifdef _MSC_VER
            sprintf_s(buf + i * 2, 33 - (i * 2), "%02x", u8[i]);
#else
            snprintf(buf + i * 2, 33 - (i * 2), "%02x", u8[i]);
//            sprintf(buf+i*2, "%02x", u8[i]);
#endif
        }
        buf[32] = 0;

        return buf;
    }

} // namespace sky
