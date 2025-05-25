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
        auto *fp = fopen("/dev/urandom", "rb");
        if (fp == nullptr) {
            return false;
        }
        size_t res = fread(data, 1, static_cast<size_t>(dataSize), fp);
        fclose(fp);
        if (res != dataSize) {
            return false;
        }
#else
        HCRYPTPROV handle;
        if (!CryptAcquireContext(&handle, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
            return false;
        }

        if (!CryptGenRandom(handle, static_cast<DWORD>(dataSize), static_cast<PBYTE>(data))) {
            return false;
        }

        CryptReleaseContext(handle, 0);
#endif
        return true;
    }

} // namespace sky
