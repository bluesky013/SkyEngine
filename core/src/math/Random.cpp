//
// Created by Zach Lee on 2021/12/4.
//

#include <core/math/Random.h>
#include <cstdint>
#include <stdio.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <wincrypt.h>
#endif

namespace sky {

    bool Random::Gen(void *data, uint32_t dataSize)
    {
#if defined(__linux__) || defined(__APPLE__)
        int   res;
        auto *fp = fopen("/dev/urandom", "rb");
        if (fp == nullptr) {
            return false;
        }
        res = fread(data, 1, dataSize, fp);
        fclose(fp);
        if (res != dataSize) {
            return false;
        }
#else
#endif
        return true;
    }

} // namespace sky